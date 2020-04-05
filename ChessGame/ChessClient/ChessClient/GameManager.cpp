#include "stdafx.h"
#include "GameManager.h"
#include "Renderer.h"
#include "Player.h"
#include "MapTile.h"

using namespace std;

CGameManager::CGameManager(const char* ServerIP)
{
	Renderer = make_shared<CRenderer>(WndSizeX, WndSizeY);
	MapTile = make_unique<CMapTile>(Renderer);

	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0) return;

	// socket()
	Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (Socket == INVALID_SOCKET) err_quit("socket()");
	
	// connect()
	SOCKADDR_IN ServerAddr;
	memset(&ServerAddr, 0, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(SERVERPORT);
	inet_pton(AF_INET, ServerIP, &ServerAddr.sin_addr);
	int Retval = connect(Socket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (Retval == SOCKET_ERROR) err_quit("connect() - falied");

	// Make Thread
	thread{ &CGameManager::Receive, this }.detach();

	SendInit();
}

CGameManager::~CGameManager()
{
	closesocket(Socket);
	WSACleanup();
}

void CGameManager::Update()
{
}

void CGameManager::Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0f, 0.3f, 0.3f, 1.0f);

	MapTile->Render();
	for (const auto& Player : Players) Player.second->Render();

	glutSwapBuffers();
}

void CGameManager::Idle()
{
	Update();
	Render();
}

void CGameManager::MouseInput(int button, int state, int x, int y)
{
	Update();
	Render();
}

void CGameManager::KeyInput(unsigned char key, int x, int y)
{
	Update();
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

	int Retval{ send(Socket, Packet, BasicPacket->Size, 0) };
	if (Retval == SOCKET_ERROR) err_display("send()");
}

void CGameManager::SendInit()
{
	CS_InitPacket Packet{};

	Packet.Type = CS_INIT;
	Packet.Size = sizeof(Packet);

	Send((char*)&Packet);
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
	while (true)
	{
		static char Buf[MAX_BUFFER + 1]{};

		int Retval{ recv(Socket, Buf, MAX_BUFFER, 0) };
		if (Retval == SOCKET_ERROR) err_display("recv()");

		switch (Buf[0])
		{
		case SC_INIT:
		{
			SC_InitPacket* Packet{ reinterpret_cast<SC_InitPacket*>(Buf) };
			if (Players.find(Packet->ClientID) == Players.end())
			{
				Players.emplace(Packet->ClientID, make_unique<CPlayer>(Renderer));
				Players[Packet->ClientID]->SetPosX(Packet->PosX);
				Players[Packet->ClientID]->SetPosY(Packet->PosY);

				//SendInit();
			}
			break;
		}
		case SC_POS:
		{
			SC_PosPacket* Packet{ reinterpret_cast<SC_PosPacket*>(Buf) };
			Players.emplace(Packet->ClientID, make_unique<CPlayer>(Renderer));	// 임시코드
			Players[Packet->ClientID]->SetPosX(Packet->PosX);
			Players[Packet->ClientID]->SetPosY(Packet->PosY);
			break;
		}
		}
	}
}