#pragma once

#include <WS2tcpip.h>

#define SERVERPORT 9000
#define MAX_BUFFER 1024

#define SC_POS 0
#define CS_MOVE 0

constexpr int WndSizeX{ 400 };
constexpr int WndSizeY{ 400 };

struct PacketBase
{
	char Type{}, Size{};
};

// Server to Client //

#pragma pack (push, 1)
struct SC_PosPacket
{
	char Type{}, Size{};
	float PosX{}, PosY{};
};
#pragma pack (pop)

// Client to Server //

#pragma pack (push, 1)
struct CS_MovePacket
{
	char Type{}, Size{};
	char Key{};
};
#pragma pack (pop)