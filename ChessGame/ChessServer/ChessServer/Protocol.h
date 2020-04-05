#pragma once
#pragma comment(lib, "ws2_32.lib")

#include <WS2tcpip.h>

#define SERVERPORT 9000
#define MAX_BUFFER 1024

#define SC_INIT 0
#define SC_POS 1

#define CS_INIT 0
#define CS_MOVE 1

constexpr int WndSizeX{ 400 };
constexpr int WndSizeY{ 400 };

struct PacketBase
{
	char Type{}, Size{};
};

// Server to Client //

#pragma pack (push, 1)
struct SC_InitPacket
{
	char Type{}, Size{};
	char ClientID{};
	float PosX{}, PosY{};
};
#pragma pack (pop)

#pragma pack (push, 1)
struct SC_PosPacket
{
	char Type{}, Size{};
	char ClientID{};
	float PosX{}, PosY{};
};
#pragma pack (pop)

// Client to Server //

#pragma pack (push, 1)
struct CS_InitPacket
{
	char Type{}, Size{};
};
#pragma pack (pop)

#pragma pack (push, 1)
struct CS_MovePacket
{
	char Type{}, Size{};
	char Key{};
};
#pragma pack (pop)