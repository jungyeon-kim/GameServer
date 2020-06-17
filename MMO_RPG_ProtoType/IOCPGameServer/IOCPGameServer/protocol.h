#pragma once

constexpr int MAX_BUF_SIZE{ 1024 };
constexpr int MAX_PACKET_SIZE{ 255 };
constexpr int MAX_USER_COUNT{ 20000 };
constexpr int MAX_NPC_COUNT{ 50000 };
constexpr int MAX_ID_LEN{ 50 };
constexpr int MAX_STR_LEN{ 50 };
constexpr int NPC_ID_START{ MAX_USER_COUNT };

#define WORLD_WIDTH		2000
#define WORLD_HEIGHT	2000

#define SERVER_PORT		9000

#define SC_LOG			1
#define SC_LOGIN_OK		2
#define SC_MOVE			3
#define SC_ENTER		4
#define SC_LEAVE		5
#define SC_CHAT			6

#define CS_SIGNUP		1
#define CS_SIGNOUT		2
#define CS_LOGIN		3
#define CS_MOVE			4

#pragma pack(push ,1)

struct SC_Packet_Log
{
	char Size{};
	char Type{};
	char Msg[MAX_STR_LEN]{};
};

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
	unsigned MoveTime{};
};

constexpr unsigned char O_HUMAN{ 0 };
constexpr unsigned char O_ELF{ 1 };
constexpr unsigned char O_ORC{ 2 };

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

struct SC_Packet_Chat
{
	char Size{};
	char Type{};
	int ID{};
	char Msg[MAX_STR_LEN]{};
};

struct CS_Packet_SignUp
{
	char Size{};
	char Type{};
	char Name[MAX_ID_LEN]{};
	char Password[MAX_STR_LEN]{};
};

struct CS_Packet_SignOut
{
	char Size{};
	char Type{};
	char Name[MAX_ID_LEN]{};
	char Password[MAX_STR_LEN]{};
};

struct CS_Packet_Login 
{
	char Size{};
	char Type{};
	char Name[MAX_ID_LEN]{};
	char Password[MAX_STR_LEN]{};
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
	unsigned MoveTime{};
};

#pragma pack (pop)