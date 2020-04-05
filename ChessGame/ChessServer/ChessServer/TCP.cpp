#include <iostream>
#include <map>
#include "Protocol.h"

using namespace std;

void err_quit(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(SUBLANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPTSTR)lpMsgBuf, (LPTSTR)msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

struct SOCKETINFO
{
	WSAOVERLAPPED Overlapped{};
	WSABUF DataBuffer{};
	SOCKET Socket{};
	char MsgBuffer[MAX_BUFFER]{};
	float PosX{ WndSizeX / 16.0f }, PosY{ WndSizeY / 16.0f };
};

map<SOCKET, SOCKETINFO> Clients{};

void Process(SOCKET Socket, char* CSBuf, char* SCBuf);
void CALLBACK Recv_CallBack(DWORD Error, DWORD DataBytes, LPWSAOVERLAPPED Overlapped, DWORD lnFlags);
void CALLBACK Send_CallBack(DWORD Error, DWORD DataBytes, LPWSAOVERLAPPED Overlapped, DWORD lnFlags);

void CALLBACK Recv_CallBack(DWORD Error, DWORD DataBytes, LPWSAOVERLAPPED Overlapped, DWORD lnFlags)
{
	int Client_S{ reinterpret_cast<int>(Overlapped->hEvent) };
	char CSBuf[MAX_BUFFER + 1]{};
	char SCBuf[MAX_BUFFER + 1]{};

	if (!DataBytes)
	{
		closesocket(Clients[Client_S].Socket);
		Clients.erase(Client_S);
		return;
	}  // 클라이언트가 closesocket을 했을 경우
	
	Clients[Client_S].MsgBuffer[DataBytes] = 0;
	cout << "From client : " << Clients[Client_S].MsgBuffer << " (" << DataBytes << " bytes)\n";

	// 받은 데이터 처리후 전송
	memcpy(&CSBuf, Clients[Client_S].MsgBuffer, MAX_BUFFER);
	Process(Client_S, CSBuf, SCBuf);
	for (auto& Client : Clients)
	{
		Client.second.DataBuffer.len = SCBuf[1];
		Client.second.DataBuffer.buf = SCBuf;
		memset(&(Client.second.Overlapped), 0, sizeof(WSAOVERLAPPED));
		Client.second.Overlapped.hEvent = (HANDLE)Client.first;
		WSASend(Client.first, &(Client.second.DataBuffer), 1, NULL, 0,
			&(Client.second.Overlapped), Send_CallBack);
	}
}

void CALLBACK Send_CallBack(DWORD Error, DWORD DataBytes, LPWSAOVERLAPPED Overlapped, DWORD lnFlags)
{
	DWORD Flags{};
	int Client_S{ reinterpret_cast<int>(Overlapped->hEvent) };

	if (!DataBytes) 
	{
		closesocket(Clients[Client_S].Socket);
		Clients.erase(Client_S);
		return;
	}  // 클라이언트가 closesocket을 했을 경우

	// WSASend(응답에 대한)의 콜백일 경우
	Clients[Client_S].DataBuffer.len = MAX_BUFFER;
	Clients[Client_S].DataBuffer.buf = Clients[Client_S].MsgBuffer;
	cout << "TRACE - Send message : " << Clients[Client_S].MsgBuffer << " (" << DataBytes << " bytes)\n";
	memset(&(Clients[Client_S].Overlapped), 0, sizeof(WSAOVERLAPPED));
	Clients[Client_S].Overlapped.hEvent = (HANDLE)Client_S;
	WSARecv(Client_S, &Clients[Client_S].DataBuffer, 1, 0, &Flags, &(Clients[Client_S].Overlapped), Recv_CallBack);
}

void Process(SOCKET Socket, char* CSBuf, char* SCBuf)
{
	switch (CSBuf[0])
	{
	case CS_INIT:
	{
		SC_InitPacket* SCPacket{ reinterpret_cast<SC_InitPacket*>(SCBuf) };

		// 새로운 클라이언트에게 기존 데이터 전송
		//for (auto& Client : Clients)
		//{
		//	if (Client.first != Socket)
		//	{
		//		SCPacket->Type = SC_INIT;
		//		SCPacket->Size = sizeof(SC_InitPacket);
		//		SCPacket->ClientID = Client.second.Socket;
		//		SCPacket->PosX = Client.second.PosX;
		//		SCPacket->PosY = Client.second.PosY;

		//		Clients[Socket].DataBuffer.len = SCPacket->Size;
		//		Clients[Socket].DataBuffer.buf = (char*)SCPacket;
		//		memset(&(Clients[Socket].Overlapped), 0, sizeof(WSAOVERLAPPED));
		//		Clients[Socket].Overlapped.hEvent = (HANDLE)Socket;
		//		WSASend(Socket, &(Clients[Socket].DataBuffer), 1, NULL, 0,
		//			&(Clients[Socket].Overlapped), Send_CallBack);
		//	}
		//}

		SCPacket->Type = SC_INIT;
		SCPacket->Size = sizeof(SC_InitPacket);
		SCPacket->ClientID = Socket;
		SCPacket->PosX = Clients[Socket].PosX;
		SCPacket->PosY = Clients[Socket].PosY;
		break;
	}
	case CS_MOVE:
	{
		CS_MovePacket* CSPacket{ reinterpret_cast<CS_MovePacket*>(CSBuf) };
		SC_PosPacket* SCPacket{ reinterpret_cast<SC_PosPacket*>(SCBuf) };

		SCPacket->Type = SC_POS;
		SCPacket->Size = sizeof(SC_PosPacket);
		SCPacket->ClientID = Socket;

		switch (CSPacket->Key)
		{
		case VK_UP:
			if (WndSizeX / 2.0f > Clients[Socket].PosY) Clients[Socket].PosY += WndSizeY / 8.0f;
			if (WndSizeX / 2.0f <= Clients[Socket].PosY) Clients[Socket].PosY -= WndSizeY / 8.0f;
			break;
		case VK_DOWN:
			if (-WndSizeX / 2.0f < Clients[Socket].PosY) Clients[Socket].PosY -= WndSizeY / 8.0f;
			if (-WndSizeX / 2.0f >= Clients[Socket].PosY) Clients[Socket].PosY += WndSizeY / 8.0f;
			break;
		case VK_LEFT:
			if (-WndSizeX / 2.0f < Clients[Socket].PosX) Clients[Socket].PosX -= WndSizeX / 8.0f;
			if (-WndSizeX / 2.0f >= Clients[Socket].PosX) Clients[Socket].PosX += WndSizeX / 8.0f;
			break;
		case VK_RIGHT:
			if (WndSizeX / 2.0f > Clients[Socket].PosX) Clients[Socket].PosX += WndSizeX / 8.0f;
			if (WndSizeX / 2.0f <= Clients[Socket].PosX) Clients[Socket].PosX -= WndSizeX / 8.0f;
			break;
		}
		SCPacket->PosX = Clients[Socket].PosX;
		SCPacket->PosY = Clients[Socket].PosY;
		break;
	}
	}
}

int main(int argc, char* argv[])
{
	int Retval;

	// 윈속 초기화
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0) return 1;

	// socket() -> 소켓 생성
	SOCKET ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (ListenSocket == INVALID_SOCKET) err_quit("socket()");

	// bind() -> 지역 IP / port 번호 결정
	SOCKADDR_IN ServerAddr;
	memset(&ServerAddr, 0, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	ServerAddr.sin_port = htons(SERVERPORT);
	Retval = bind(ListenSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (Retval == SOCKET_ERROR) err_quit("bind()");

	// listen() -> 대기상태
	Retval = listen(ListenSocket, SOMAXCONN);
	if (Retval == SOCKET_ERROR) err_quit("listen()");

	// 데이터 통신에 사용할 변수
	SOCKADDR_IN ClientAddr;
	int AddrLen{ sizeof(ClientAddr) };

	while (true) 
	{
		SOCKET ClientSocket{ accept(ListenSocket, (SOCKADDR*)&ClientAddr, &AddrLen) };
		SOCKETINFO SocketInfo{};
		Clients.emplace(ClientSocket, SocketInfo);
		Clients[ClientSocket].Socket = ClientSocket;
		Clients[ClientSocket].DataBuffer.len = MAX_BUFFER;
		Clients[ClientSocket].DataBuffer.buf = Clients[ClientSocket].MsgBuffer;
		memset(&Clients[ClientSocket].Overlapped, 0, sizeof(WSAOVERLAPPED));
		Clients[ClientSocket].Overlapped.hEvent = (HANDLE)Clients[ClientSocket].Socket;

		DWORD Flags{};
		WSARecv(Clients[ClientSocket].Socket, &Clients[ClientSocket].DataBuffer, 1, NULL,
			&Flags, &(Clients[ClientSocket].Overlapped), Recv_CallBack);
	}

	// closesocket()
	closesocket(ListenSocket);
	// 윈속 종료
	WSACleanup();
}