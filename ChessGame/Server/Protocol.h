#pragma once

#define SERVERPORT 9000
#define BUFSIZE 512

#define SC_POS 0
#define CS_MOVE 0

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