#include "stdafx.h"
#include "GameManager.h"
#include "Renderer.h"
#include "Player.h"
#include "MapTile.h"

using namespace std;

CGameManager::CGameManager(const char* ServerIP)
{
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 0), &WSAData) != 0) return;

	// socket()
	Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
	if (Socket == INVALID_SOCKET) err_quit("socket()");
	
	// connect()
	SOCKADDR_IN ServerAddr;
	memset(&ServerAddr, 0, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr(ServerIP);
	ServerAddr.sin_port = htons(SERVERPORT);
	int Retval = connect(Socket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (Retval == SOCKET_ERROR) err_quit("connect() - falied");

	Renderer = make_shared<CRenderer>(WndSizeX, WndSizeY);
	Player = make_unique<CPlayer>(Renderer);
	MapTile = make_unique<CMapTile>(Renderer);
}

CGameManager::~CGameManager()
{
	closesocket(Socket);
	WSACleanup();
}

void CGameManager::Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0f, 0.3f, 0.3f, 1.0f);

	MapTile->Render();
	Player->Render();

	glutSwapBuffers();
}

void CGameManager::Idle()
{
	Render();
}

void CGameManager::MouseInput(int button, int state, int x, int y)
{
	Render();
}

void CGameManager::KeyInput(unsigned char key, int x, int y)
{
	Render();
}

void CGameManager::SpecialKeyInput(int key, int x, int y)
{
	switch (key)
	{
	case GLUT_KEY_UP:
		SendMovement(VK_UP);
		break;
	case GLUT_KEY_DOWN:
		SendMovement(VK_DOWN);
		break;
	case GLUT_KEY_LEFT:
		SendMovement(VK_LEFT);
		break;
	case GLUT_KEY_RIGHT:
		SendMovement(VK_RIGHT);
		break;
	}

	Receive();
}

void CGameManager::err_quit(const char* Msg)
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

void CGameManager::err_display(const char* Msg)
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

void CGameManager::Send(char* Packet)
{
	auto BasicPacket{ reinterpret_cast<PacketBase*>(Packet) };

	int Retval = send(Socket, Packet, BasicPacket->Size, 0);
	if (Retval == SOCKET_ERROR) err_display("send()");
}

void CGameManager::SendMovement(char Key)
{
	CS_MovePacket Packet{};

	Packet.Type = CS_MOVE;
	Packet.Size = sizeof(Packet);
	Packet.Key = Key;

	Send((char*)&Packet);
}

void CGameManager::Receive()
{
	static char Buf[MAX_BUFFER + 1]{};

	int Retval = recv(Socket, Buf, MAX_BUFFER, 0);
	if (Retval == SOCKET_ERROR) err_display("recv()");
	
	switch (Buf[0])
	{
	case SC_POS:
		SC_PosPacket* Packet{ reinterpret_cast<SC_PosPacket*>(Buf) };
		Player->SetPosX(Player->GetPosX() + Packet->PosX);
		Player->SetPosY(Player->GetPosY() + Packet->PosY);
		break;
	}
}