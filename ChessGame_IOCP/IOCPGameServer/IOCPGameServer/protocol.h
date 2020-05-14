#pragma once

constexpr int MAX_BUF_SIZE{ 1024 };
constexpr int MAX_PACKET_SIZE{ 255 };
constexpr int MAX_USER_COUNT{ 10 };
constexpr int MAX_ID_LEN{ 50 };
constexpr int MAX_STR_LEN{ 255 };

#define WORLD_WIDTH		8
#define WORLD_HEIGHT	8

#define SERVER_PORT		9000

#define CS_LOGIN		1
#define CS_MOVE			2

#define SC_LOGIN_OK		1
#define SC_MOVE			2
#define SC_ENTER		3
#define SC_LEAVE		4

#pragma pack(push ,1)

struct SC_Packet_Login_OK 
{
	char Size{};
	char Type{};
	int ID{};
	short PosX{}, PosY{};
	short HP{};
	short Level{};
	int	Exp{};
};

struct SC_Packet_Move 
{
	char Size{};
	char Type{};
	int ID{};
	short PosX{}, PosY{};
};

constexpr unsigned char O_PLAYER{ 0 };
constexpr unsigned char O_NPC{ 1 };

struct SC_Packet_Enter 
{
	char Size{};
	char Type{};
	int ID{};
	char Name[MAX_ID_LEN]{};
	char ObjectType{};
	short PosX{}, PosY{};
};

struct SC_Packet_Leave 
{
	char Size{};
	char Type{};
	int ID{};
};

struct CS_Packet_Login 
{
	char Size{};
	char Type{};
	char Name[MAX_ID_LEN]{};
};

constexpr unsigned char D_UP{ 0 };
constexpr unsigned char D_DOWN{ 1 };
constexpr unsigned char D_LEFT{ 2 };
constexpr unsigned char D_RIGHT{ 3 };

struct CS_Packet_Move 
{
	char Size{};
	char Type{};
	char Dir{};
};

#pragma pack (pop)