#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")

#include <WS2tcpip.h>
#include <MSWSock.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <unordered_set>
#include "protocol.h"

using namespace std;

constexpr int VIEW_RANGE{ 6 };

enum class EnumOp { RECV, SEND, ACCEPT };
enum class ClientStat { FREE, ALLOCATED, ACTIVE };

struct ExOverlapped
{
	WSAOVERLAPPED Over{};		// 실제 Overlapped 구조체
	EnumOp Op{};				// IO 완료후 Send, Recv, Accept중 어떤 것이었는지 판단하기위한 변수
	char IOBuf[MAX_BUF_SIZE]{};
	union						// 같은 메모리공간을 공유
	{
		WSABUF WSABuf{};
		SOCKET ClientSocket;
	};
};

struct Client
{
	mutex Mutex{};
	SOCKET Socket{};
	int ID{};
	ExOverlapped RecvOver{};	// Recv용 Overlapped구조체 (Send와 달리 하나만 필요)
	int PrevRecvSize{};			// 조각난 데이터를 Recv했을 경우 해당 데이터의 사이즈를 저장하는 변수
	char PacketBuf[MAX_PACKET_SIZE]{};
	atomic<ClientStat> Status{};
	unordered_set<int> ViewList{};	// 시야범위 내에 존재하는 클라이언트를 담기위한 자료구조

	char Name[MAX_ID_LEN + 1]{};
	short PosX{}, PosY{};

	unsigned MoveTime{};
};

HANDLE IOCP{};
SOCKET ListenSocket{};
Client Clients[MAX_USER_COUNT]{};

void InitClients()
{
	for (int i = 0; i < MAX_USER_COUNT; ++i) Clients[i].ID = i;
}

bool IsNear(int UserID, int OtherObjectID)
{
	return abs(Clients[UserID].PosX - Clients[OtherObjectID].PosX) <= VIEW_RANGE &&
		abs(Clients[UserID].PosY - Clients[OtherObjectID].PosY) <= VIEW_RANGE;
}

void Send_Packet(int UserID, void* BufPointer)
{
	char* Buf{ reinterpret_cast<char*>(BufPointer) };
	ExOverlapped* ExOver{ new ExOverlapped };
	ExOver->Op = EnumOp::SEND;
	ExOver->Over = {};
	ExOver->WSABuf.buf = ExOver->IOBuf;
	ExOver->WSABuf.len = Buf[0];
	memcpy(ExOver->IOBuf, Buf, Buf[0]);

	WSASend(Clients[UserID].Socket, &ExOver->WSABuf, 1, NULL, 0, &ExOver->Over, NULL);
}

void Send_Packet_Login_Ok(int UserID)
{
	SC_Packet_Login_OK Packet{};

	Packet.Size = sizeof(Packet);
	Packet.Type = SC_LOGIN_OK;
	Packet.ID = UserID;
	Packet.Exp = 0;
	Packet.Level = 0;
	Packet.HP = 0;
	Packet.PosX = Clients[UserID].PosX;
	Packet.PosY = Clients[UserID].PosY;

	Send_Packet(UserID, &Packet);
}

void Send_Packet_Enter(int UserID, int OtherObjectID)
{
	SC_Packet_Enter Packet{};
	
	Packet.Size = sizeof(Packet);
	Packet.Type = SC_ENTER;
	Packet.ID = OtherObjectID;
	Packet.ObjectType = O_PLAYER;
	Packet.PosX = Clients[OtherObjectID].PosX;
	Packet.PosY = Clients[OtherObjectID].PosY;
	strcpy_s(Packet.Name, Clients[OtherObjectID].Name);

	Clients[UserID].Mutex.lock();
	Clients[UserID].ViewList.emplace(OtherObjectID);
	Clients[UserID].Mutex.unlock();

	Send_Packet(UserID, &Packet);
}

void Send_Packet_Leave(int UserID, int OtherObjectID)
{
	SC_Packet_Leave Packet{};

	Packet.Size = sizeof(Packet);
	Packet.Type = SC_LEAVE;
	Packet.ID = OtherObjectID;

	Clients[UserID].Mutex.lock();
	Clients[UserID].ViewList.erase(OtherObjectID);
	Clients[UserID].Mutex.unlock();

	Send_Packet(UserID, &Packet);
}

void Send_Packet_Move(int UserID, int MovedUserId)
{
	SC_Packet_Move Packet{};

	Packet.Size = sizeof(Packet);
	Packet.Type = SC_MOVE;
	Packet.ID = MovedUserId;
	Packet.PosX = Clients[MovedUserId].PosX;
	Packet.PosY = Clients[MovedUserId].PosY;
	Packet.MoveTime = Clients[MovedUserId].MoveTime;

	Send_Packet(UserID, &Packet);
}

void ProcessPacket(int UserID, char* Buf)
{
	switch (Buf[1])
	{
	case CS_LOGIN:
	{
		CS_Packet_Login* Packet{ reinterpret_cast<CS_Packet_Login*>(Buf) };

		Clients[UserID].Mutex.lock();
		strcpy_s(Clients[UserID].Name, Packet->Name);
		Clients[UserID].Name[MAX_ID_LEN] = NULL;

		Send_Packet_Login_Ok(UserID);

		Clients[UserID].Status = ClientStat::ACTIVE;
		Clients[UserID].Mutex.unlock();

		for (int i = 0; i < MAX_USER_COUNT; ++i)
		{
			if (UserID == i) continue;	// 데드락 & 자신에게 보내는 것 방지
			if (IsNear(UserID, i))
			{
				//Clients[i].Mutex.lock();
				if (Clients[i].Status == ClientStat::ACTIVE)
				{
					Send_Packet_Enter(UserID, i);
					Send_Packet_Enter(i, UserID);
				}
				//Clients[i].Mutex.unlock();
			}
		}
		break;
	}
	case CS_MOVE:
	{
		CS_Packet_Move* Packet{ reinterpret_cast<CS_Packet_Move*>(Buf) };

		Clients[UserID].MoveTime = Packet->MoveTime;

		short& PosX{ Clients[UserID].PosX };
		short& PosY{ Clients[UserID].PosY };

		switch (Packet->Dir)
		{
		case D_UP:		if (PosY > 0)					--PosY; break;
		case D_DOWN:	if (PosY < WORLD_HEIGHT - 1)	++PosY; break;
		case D_LEFT:	if (PosX > 0)					--PosX; break;
		case D_RIGHT:	if (PosX < WORLD_WIDTH - 1)		++PosX; break;
		default:
			cout << "Unknown Direction From Client Move Packet." << endl;
			DebugBreak();
			exit(-1);
		}

		Clients[UserID].Mutex.lock();
		unordered_set<int> OldViewList{ Clients[UserID].ViewList };
		Clients[UserID].Mutex.unlock();
		unordered_set<int> NewViewList{};

		for (auto& Client : Clients)
		{
			if (Client.Status != ClientStat::ACTIVE || Client.ID == UserID) continue;
			if (IsNear(UserID, Client.ID)) NewViewList.emplace(Client.ID);
		}

		Send_Packet_Move(UserID, UserID);

		for (auto& NewVisibleObject : NewViewList)
		{
			if (!OldViewList.count(NewVisibleObject))
			{
				Send_Packet_Enter(UserID, NewVisibleObject);
				Clients[NewVisibleObject].Mutex.lock();
				if (!Clients[NewVisibleObject].ViewList.count(UserID))
				{
					Clients[NewVisibleObject].Mutex.unlock();
					Send_Packet_Enter(NewVisibleObject, UserID);
				}
				else
				{
					Clients[NewVisibleObject].Mutex.unlock();
					Send_Packet_Move(NewVisibleObject, UserID);
				}
			}
			else
			{
				Clients[NewVisibleObject].Mutex.lock();
				if (Clients[NewVisibleObject].ViewList.count(UserID))
				{
					Clients[NewVisibleObject].Mutex.unlock();
					Send_Packet_Move(NewVisibleObject, UserID);
				}
				else
				{
					Clients[NewVisibleObject].Mutex.unlock();
					Send_Packet_Enter(NewVisibleObject, UserID);
				}
			}
		}

		for (auto& OldVisibleObject : OldViewList)
		{
			if (!NewViewList.count(OldVisibleObject))
			{
				Send_Packet_Leave(UserID, OldVisibleObject);
				Clients[OldVisibleObject].Mutex.lock();
				if (Clients[OldVisibleObject].ViewList.count(UserID))
				{
					Clients[OldVisibleObject].Mutex.unlock();
					Send_Packet_Leave(OldVisibleObject, UserID);
				}
				else Clients[OldVisibleObject].Mutex.unlock();
			}
		}
		break;
	}
	default:
		cout << "Unknown PacketType Error." << endl;
		DebugBreak();
		exit(-1);
	}
}

void RecvPacketAssemble(int UserID, int RecvByte)
{
	Client& Client{ Clients[UserID] };
	int RestByte{ RecvByte };						// 처리해야할 남은 데이터 크기
	char* PosToProcess{ Client.RecvOver.IOBuf };	// 처리할 위치
	int PacketSize{};

	if (Client.PrevRecvSize) PacketSize = Client.PacketBuf[0];

	while (RestByte)	// 여러개의 패킷이 올 수도 있으므로 루프로 처리
	{
		if (!PacketSize) PacketSize = *PosToProcess;
		// 패킷을 완성할 수 있다면
		if (PacketSize <= RestByte + Client.PrevRecvSize)
		{
			memcpy(Client.PacketBuf + Client.PrevRecvSize, PosToProcess, PacketSize - Client.PrevRecvSize);
			PosToProcess += PacketSize - Client.PrevRecvSize;
			RestByte -= PacketSize - Client.PrevRecvSize;
			PacketSize = 0;
			ProcessPacket(UserID, Client.PacketBuf);
			Client.PrevRecvSize = 0;
		}
		// 패킷을 완성할 수 없다면
		else
		{
			memcpy(Client.PacketBuf + Client.PrevRecvSize, PosToProcess, RestByte);
			Client.PrevRecvSize += RestByte;
			RestByte = 0;
			PosToProcess += RestByte;
		}
	}
}

void Disconnect(int UserID)
{
	Send_Packet_Leave(UserID, UserID);

	Clients[UserID].Mutex.lock();
	Clients[UserID].Status = ClientStat::ALLOCATED;
	closesocket(Clients[UserID].Socket);

	for (auto& Client : Clients)
	{
		if (Client.ID == UserID) continue;	// 데드락 방지
		//Client.Mutex.lock();
		if (Client.Status == ClientStat::ACTIVE)
			Send_Packet_Leave(Client.ID, UserID);
		//Client.Mutex.unlock();
	}
	Clients[UserID].Status = ClientStat::FREE;
	Clients[UserID].Mutex.unlock();
}

void WorkerThread()
{
	while (true)
	{
		DWORD IOByte{};
		ULONG_PTR CompletionKey{};
		WSAOVERLAPPED* Over{};
		// 네번째 인자는 완료된 IO 조작이 시작되었을 때 지정된 OVERLAPPED 구조체의 주소를 받는 변수에 대한 포인터
		GetQueuedCompletionStatus(IOCP, &IOByte, &CompletionKey, &Over, INFINITE);	// 여기서 Blocking

		ExOverlapped* ExOver{ reinterpret_cast<ExOverlapped*>(Over) };
		int UserID{ static_cast<int>(CompletionKey) };

		switch (ExOver->Op)
		{
		case EnumOp::RECV:
		{
			if (!IOByte) Disconnect(UserID);		// 받은 데이터가 0Byte라면 해당 클라이언트 종료
			else
			{
				RecvPacketAssemble(UserID, IOByte);

				Clients[UserID].RecvOver.Over = {};
				DWORD Flags{};
				WSARecv(Clients[UserID].Socket, &Clients[UserID].RecvOver.WSABuf, 1, NULL, &Flags, &Clients[UserID].RecvOver.Over, NULL);
			}
			break;
		}
		case EnumOp::SEND:
			if (!IOByte) Disconnect(UserID);		// 보낸 데이터가 0Byte라면 해당 클라이언트 종료
			delete ExOver;
			break;
		case EnumOp::ACCEPT:
		{
			int UserID{ -1 };

			for (int i = 0; i < MAX_USER_COUNT; ++i)
			{
				lock_guard<mutex> LockGuard{ Clients[i].Mutex };
				if (Clients[i].Status == ClientStat::FREE)
				{
					UserID = i;
					Clients[i].Status = ClientStat::ALLOCATED;
					break;
				}
			}

			SOCKET ClientSocket{ ExOver->ClientSocket };

			// 나중에 로그인 실패 패킷을 보낼 예정
			if (UserID == -1) closesocket(ClientSocket);
			else
			{
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(ClientSocket), IOCP, UserID, 0);

				Client& NewClient{ Clients[UserID] };
				NewClient.Socket = ClientSocket;
				NewClient.RecvOver.Op = EnumOp::RECV;
				NewClient.RecvOver.WSABuf.buf = NewClient.RecvOver.IOBuf;
				NewClient.RecvOver.WSABuf.len = MAX_BUF_SIZE;
				NewClient.ViewList.clear();
				NewClient.PosX = rand() % WORLD_WIDTH;
				NewClient.PosY = rand() % WORLD_HEIGHT;

				DWORD Flags{};
				WSARecv(ClientSocket, &NewClient.RecvOver.WSABuf, 1, NULL, &Flags, &NewClient.RecvOver.Over, NULL);
			}

			ClientSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			ExOver->ClientSocket = ClientSocket;
			ExOver->Over = {};
			AcceptEx(ListenSocket, ClientSocket, ExOver->IOBuf, NULL,
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &ExOver->Over);
			break;
		}
		}
	}
}

int main()
{
	WSADATA WSAData{};
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN SocketAddr{};
	SocketAddr.sin_family = AF_INET;
	SocketAddr.sin_port = htons(SERVER_PORT);
	SocketAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(ListenSocket, reinterpret_cast<sockaddr*>(&SocketAddr), sizeof(SocketAddr));

	listen(ListenSocket, SOMAXCONN);

	IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	InitClients();

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(ListenSocket), IOCP, 999, 0);
	SOCKET ClientSocket{ WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED) };
	ExOverlapped AcceptOver{};
	AcceptOver.Op = EnumOp::ACCEPT;
	AcceptOver.ClientSocket = ClientSocket;
	AcceptEx(ListenSocket, ClientSocket, AcceptOver.IOBuf, NULL, 
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &AcceptOver.Over);

	vector<thread> WorkerThreads{};
	// 현재 PC의 코어가 6개이므로 6개의 스레드 생성
	for (int i = 0; i < 6; ++i) WorkerThreads.emplace_back(WorkerThread);
	for (auto& Thread : WorkerThreads) Thread.join();
}