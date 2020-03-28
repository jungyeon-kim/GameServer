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

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket() -> 소켓 생성
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind() -> 지역 IP / port 번호 결정
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen() -> 대기상태
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	char CSBuf[BUFSIZE + 1]{}, SCBuf[BUFSIZE + 1]{};
	int addrlen;

	while (1) {
		// accept() -> 접속한 클라이언트와 통신 가능하도록 새로운 소켓을 만들어 리턴
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// 접속한 클라이언트 정보 출력
		printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// 클라이언트와 데이터 통신
		while (1) {
			// 데이터 받기
			retval = recv(client_sock, CSBuf, BUFSIZE, 0);
			if (retval == SOCKET_ERROR) {
				err_display("recv()");
				break;
			}
			else if (retval == 0)
				break;

			// 데이터 처리
			Process(CSBuf, SCBuf);

			// 데이터 보내기
			retval = send(client_sock, SCBuf, BUFSIZE, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send()");
				break;
			}
			ZeroMemory(SCBuf, BUFSIZE);
		}

		// closesocket()
		closesocket(client_sock);
		printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
			inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	}

	// closesocket()
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}