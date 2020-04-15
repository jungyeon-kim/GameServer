#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")

#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include "protocol.h"

using namespace std;

enum class EnumOp { RECV, SEND, ACCEPT };

struct ExOverlapped
{
	WSAOVERLAPPED Over{};		// 실제 Overlapped 구조체
	EnumOp Op{};				// IO 완료후 Send, Recv, Accept중 어떤 것이었는지 판단하기위한 변수
	char IOBuf[MAX_BUF_SIZE]{};
	WSABUF WSABuf{};
};

struct Client
{
	SOCKET Socket{};
	int ID{};
	ExOverlapped RecvOver{};	// Recv용 Overlapped구조체 (Send와 달리 하나만 필요)
	int PrevRecvSize{};			// 조각난 데이터를 Recv했을 경우 해당 데이터의 사이즈를 저장하는 변수
	char PacketBuf[MAX_PACKET_SIZE]{};

	char Name[MAX_ID_LEN + 1]{};
	short PosX{}, PosY{};
};

HANDLE IOCP{};
Client Clients[MAX_USER_COUNT]{};
int CurrentUserID{};

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

void Send_Packet_Move(int UserID, int Dir)
{
	short& PosX{ Clients[UserID].PosX };
	short& PosY{ Clients[UserID].PosY };

	switch (Dir)
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

	SC_Packet_Move Packet{};

	Packet.Size = sizeof(Packet);
	Packet.Type = SC_LOGIN_OK;
	Packet.ID = UserID;
	Packet.PosX = Clients[UserID].PosX;
	Packet.PosY = Clients[UserID].PosY;

	Send_Packet(UserID, &Packet);
}

void ProcessPacket(int UserID, char* Buf)
{
	switch (Buf[1])
	{
	case CS_LOGIN:
	{
		CS_Packet_Login* Packet{ reinterpret_cast<CS_Packet_Login*>(Buf) };

		strcpy_s(Clients[UserID].Name, Packet->Name);
		Clients[UserID].Name[MAX_ID_LEN] = NULL;

		Send_Packet_Login_Ok(UserID);
		break;
	}
	case CS_MOVE:
	{
		CS_Packet_Move* Packet{ reinterpret_cast<CS_Packet_Move*>(Buf) };

		Send_Packet_Move(UserID, Packet->Dir);
		break;
	}
	default:
		cout << "Unknown PacketType Error." << endl;
		DebugBreak();
		exit(-1);
	}
}

int main()
{
	WSADATA WSAData{};
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	SOCKET ListenSocket{ WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED) };

	SOCKADDR_IN SocketAddr{};
	SocketAddr.sin_family = AF_INET;
	SocketAddr.sin_port = htons(SERVER_PORT);
	SocketAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	::bind(ListenSocket, reinterpret_cast<sockaddr*>(&SocketAddr), sizeof(SocketAddr));

	listen(ListenSocket, SOMAXCONN);

	IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(ListenSocket), IOCP, 999, 0);
	SOCKET ClientSocket{ WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED) };
	ExOverlapped AcceptOver{};
	AcceptOver.Op = EnumOp::ACCEPT;
	AcceptEx(ListenSocket, ClientSocket, AcceptOver.IOBuf, NULL, 
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &AcceptOver.Over);

	while (true)
	{
		DWORD IOByte{};
		ULONG_PTR CompletionKey{};
		WSAOVERLAPPED* Over{};
		GetQueuedCompletionStatus(IOCP, &IOByte, &CompletionKey, &Over, INFINITE);	// 여기서 Blocking

		ExOverlapped* ExOver{ reinterpret_cast<ExOverlapped*>(Over) };
		int UserID{ static_cast<int>(CompletionKey) };

		switch (ExOver->Op)
		{
		case EnumOp::RECV:
		{
			ProcessPacket(UserID, ExOver->IOBuf);

			Clients[UserID].RecvOver.Over = {};
			DWORD Flags{};
			WSARecv(Clients[UserID].Socket, &Clients[UserID].RecvOver.WSABuf, 1, NULL, &Flags, &Clients[UserID].RecvOver.Over, NULL);
			break;
		}
		case EnumOp::SEND:
			delete ExOver;
			break;
		case EnumOp::ACCEPT:
		{
			int UserID{ CurrentUserID++ };
			CurrentUserID = CurrentUserID % MAX_USER_COUNT;

			CreateIoCompletionPort(reinterpret_cast<HANDLE>(ClientSocket), IOCP, UserID, 0);

			Client& NewClient{ Clients[UserID] };
			NewClient.Socket = ClientSocket;
			NewClient.ID = UserID;
			NewClient.RecvOver.Op = EnumOp::RECV;
			NewClient.RecvOver.WSABuf.buf = NewClient.RecvOver.IOBuf;
			NewClient.RecvOver.WSABuf.len = MAX_BUF_SIZE;
			NewClient.PosX = rand() % WORLD_WIDTH;
			NewClient.PosY = rand() % WORLD_HEIGHT;

			DWORD Flags{};
			WSARecv(ClientSocket, &NewClient.RecvOver.WSABuf, 1, NULL, &Flags, &NewClient.RecvOver.Over, NULL);

			ClientSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			AcceptOver.Over = {};
			AcceptEx(ListenSocket, ClientSocket, AcceptOver.IOBuf, NULL,
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &AcceptOver.Over);
			break;
		}
		}
	}
}