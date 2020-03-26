#include "stdafx.h"
#include "GameManager.h"
#include "NetworkManager.h"
#include "Renderer.h"
#include "Player.h"
#include "MapTile.h"

using namespace std;

CGameManager::CGameManager(const char* ServerIP)
{
	Network = make_unique<CNetworkManager>(ServerIP);
	Renderer = make_shared<CRenderer>(WndSizeX, WndSizeY);

	Player = make_unique<CPlayer>(Renderer);
	MapTile = make_unique<CMapTile>(Renderer);
}

CGameManager::~CGameManager()
{
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
	PACKET Packet{};

	switch (key)
	{
	case GLUT_KEY_UP:
		Packet = { VK_UP, Player->GetPosX(), Player->GetPosY() };
		break;
	case GLUT_KEY_DOWN:
		Packet = { VK_DOWN, Player->GetPosX(), Player->GetPosY() };
		break;
	case GLUT_KEY_LEFT:
		Packet = { VK_LEFT, Player->GetPosX(), Player->GetPosY() };
		break;
	case GLUT_KEY_RIGHT:
		Packet = { VK_RIGHT, Player->GetPosX(), Player->GetPosY() };
		break;
	}

	Packet = Network->SendRecv(Packet);
	Player->SetPosX(Packet.PosX);
	Player->SetPosY(Packet.PosY);

	Render();
}
