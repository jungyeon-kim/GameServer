#pragma once

class CGameManager
{
private:
	SOCKET Socket{};

	std::map<SOCKET, std::unique_ptr<class CPlayer>> Players{};
	std::shared_ptr<class CRenderer> Renderer{};
	std::unique_ptr<class CMapTile> MapTile{};
public:
	CGameManager(const char* ServerIP);
	~CGameManager();

	void Update();
	void Render();
	void Idle();
	void MouseInput(int button, int state, int x, int y);
	void KeyInput(unsigned char key, int x, int y);
	void SpecialKeyInput(int key, int x, int y);

	void err_quit(const char* Msg);
	void err_display(const char* Msg);

	void Send(char* Packet);
	void SendInit();
	void SendMovement(char Key);
	void Receive();
};

