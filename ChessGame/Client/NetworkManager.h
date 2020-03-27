#pragma once
#include <WinSock2.h>

class CNetworkManager
{
private:
	SOCKET Socket{};
	PACKET Packet{};
public:
	CNetworkManager(const char* ServerIP);
	~CNetworkManager();

	void err_quit(const char* Msg);
	void err_display(const char* Msg);
	PACKET SendRecv(PACKET NewPacket);
};

