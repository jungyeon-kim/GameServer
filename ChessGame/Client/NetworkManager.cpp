#include "stdafx.h"
#include "NetworkManager.h"

CNetworkManager::CNetworkManager(const char* ServerIP)
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;

	// socket()
	Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Socket == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(ServerIP);
	serveraddr.sin_port = htons(SERVERPORT);
	int retval = connect(Socket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect() - falied");
}

CNetworkManager::~CNetworkManager()
{
	closesocket(Socket);
	WSACleanup();
}

void CNetworkManager::err_quit(const char* Msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPTSTR)lpMsgBuf, (LPCWSTR)Msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

void CNetworkManager::err_display(const char* Msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", Msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

PACKET CNetworkManager::SendRecv(PACKET NewPacket)
{
	int retval = send(Socket, (char*)&NewPacket, sizeof(PACKET), 0);
	if (retval == SOCKET_ERROR) err_display("send()");

	retval = recv(Socket, (char*)&Packet, sizeof(PACKET), 0);
	if (retval == SOCKET_ERROR) err_display("recv()");

	return Packet;
}
