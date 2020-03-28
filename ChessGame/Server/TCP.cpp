#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <WS2tcpip.h>
#include "Protocol.h"

using namespace std;

constexpr int WndSizeX{ 400 };
constexpr int WndSizeY{ 400 };

void Process(char* CSBuf, char* SCBuf)
{
	switch (CSBuf[0])
	{
	case CS_MOVE:
	{
		CS_MovePacket* CSPacket{ reinterpret_cast<CS_MovePacket*>(CSBuf) };
		SC_PosPacket* SCPacket{ reinterpret_cast<SC_PosPacket*>(SCBuf) };

		SCPacket->Type = SC_POS;
		SCPacket->Size = sizeof(SC_PosPacket);
		switch (CSPacket->Key)
		{
		case VK_UP:
			SCPacket->PosY = WndSizeY / 8.0f;
			break;
		case VK_DOWN:
			SCPacket->PosY = -WndSizeY / 8.0f;
			break;
		case VK_LEFT:
			SCPacket->PosX = -WndSizeX / 8.0f;
			break;
		case VK_RIGHT:
			SCPacket->PosX = WndSizeX / 8.0f;
			break;
		}
		break;
	}
	}
}

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

int main(int argc, char* argv[])
{
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket() -> ���� ����
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind() -> ���� IP / port ��ȣ ����
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen() -> ������
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	char CSBuf[BUFSIZE + 1]{}, SCBuf[BUFSIZE + 1]{};
	int addrlen;

	while (1) {
		// accept() -> ������ Ŭ���̾�Ʈ�� ��� �����ϵ��� ���ο� ������ ����� ����
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// ������ Ŭ���̾�Ʈ ���� ���
		printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// Ŭ���̾�Ʈ�� ������ ���
		while (1) {
			// ������ �ޱ�
			retval = recv(client_sock, CSBuf, BUFSIZE, 0);
			if (retval == SOCKET_ERROR) {
				err_display("recv()");
				break;
			}
			else if (retval == 0)
				break;

			// ������ ó��
			Process(CSBuf, SCBuf);

			// ������ ������
			retval = send(client_sock, SCBuf, BUFSIZE, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send()");
				break;
			}
			ZeroMemory(SCBuf, BUFSIZE);
		}

		// closesocket()
		closesocket(client_sock);
		printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	}

	// closesocket()
	closesocket(listen_sock);

	// ���� ����
	WSACleanup();
	return 0;
}