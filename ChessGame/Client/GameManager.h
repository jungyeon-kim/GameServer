#pragma once

class CGameManager
{
private:
	std::unique_ptr<class CNetworkManager> Network{};
	std::shared_ptr<class CRenderer> Renderer{};
	std::unique_ptr<class CPlayer> Player{};
	std::unique_ptr<class CMapTile> MapTile{};
public:
	CGameManager(const char* ServerIP);
	~CGameManager();

	void Render();
	void Idle();
	void MouseInput(int button, int state, int x, int y);
	void KeyInput(unsigned char key, int x, int y);
	void SpecialKeyInput(int key, int x, int y);
};

