#pragma once

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")
#pragma comment (lib, "lua53.lib")

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <WS2tcpip.h>
#include <MSWSock.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <array>
#include <vector>
#include <unordered_set>
#include <queue>
#include "protocol.h"
#include "DBHandler.h"

using namespace std;
using namespace chrono;

constexpr int VIEW_RANGE{ 8 }; 
constexpr int NumOfSector{ (WORLD_WIDTH / SECTOR_WIDTH) * (WORLD_HEIGHT / SECTOR_HEIGHT) };

enum class EnumOp { RECV, SEND, ACCEPT, RANDOM_MOVE, OVERLAP, RESPAWN, RECOVERY };
enum class ClientStat { FREE, ALLOCATED, ACTIVE, SLEEP, DEAD };

struct ExOverlapped
{
    WSAOVERLAPPED Over{};       // 실제 Overlapped 구조체
    EnumOp Op{};                // IO 완료후 Send, Recv, Accept중 어떤 것이었는지 판단하기위한 변수
    char IOBuf[MAX_BUF_SIZE]{};
    union                       // 같은 메모리공간을 공유
    {
        WSABUF WSABuf{};
        SOCKET ClientSocket;
        int PlayerID;
    };
};

struct Client
{
    mutex Mutex{};
    SOCKET Socket{};
    int ID{};
    ExOverlapped RecvOver{};            // Recv용 Overlapped구조체 (Send와 달리 하나만 필요)
    int PrevRecvSize{};                 // 조각난 데이터를 Recv했을 경우 해당 데이터의 사이즈를 저장하는 변수
    char PacketBuf[MAX_PACKET_SIZE]{};
    atomic<ClientStat> Status{};
    unordered_set<int> ViewList{};      // 시야범위 내에 존재하는 클라이언트를 담기위한 자료구조
    int CurrentSector{ -1 };            // 현재 속해있는 섹터

    char Name[MAX_ID_LEN + 1]{};
    short PosX{}, PosY{};
    short MaxHP{ 100 }, HP{}, Level{}, Exp{};

    unsigned MoveTime{};
    high_resolution_clock::time_point LastMoveTime{};

    mutex LuaMutex{};
    lua_State* L{};
};

struct Event
{
    int ObjectID{};
    EnumOp Op{};
    high_resolution_clock::time_point WakeUpTime{};
    int TargetID{};

    constexpr bool operator>(const Event& rhs) const
    {
        return WakeUpTime > rhs.WakeUpTime;
    }
};

HANDLE IOCP{};
SOCKET ListenSocket{};

CDBHandler DBHandler{};
priority_queue<Event, vector<Event>, greater<Event>> TimerQueue;
array<unordered_set<int>, NumOfSector> Sectors{};
mutex TimerLock{}, SectorLock{};

Client Clients[NPC_ID_START + MAX_NPC_COUNT]{};

void InitClients();
void ResetClient(int UserID);
void InitNPCs();
void Disconnect(int UserID);

bool IsNear(int UserID, int OtherObjectID, int Range);
bool IsPlayer(int ObjectID);
void ActivateNPC(int NPCID);
void AddTimerQueue(int ObjectID, EnumOp Op, int Duration);

int GetSectorIdx(int PosX, int PosY);
const vector<unordered_set<int>> GetNearSectors(int CurrentSectorIdx);
void SetSector(int ObjectID, int NewPosX, int NewPosY);

int API_GetX(lua_State* L);
int API_GetY(lua_State* L);
int API_IsDead(lua_State* L);
int API_TakeDamage(lua_State* L);

void Send_Packet(int UserID, void* BufPointer);
void Send_Packet_Log(int UserID, const char* Msg, char IsInGame = 0);
void Send_Packet_Login_Ok(int UserID);
void Send_Packet_Enter(int UserID, int OtherObjectID);
void Send_Packet_Leave(int UserID, int OtherObjectID);
void Send_Packet_Chat(int UserID, int Chatter, char Msg[]);
void Send_Packet_Move(int UserID, int MovedUserId);
void Send_Packet_Attack(int UserID, int OtherUserID, char Type);
void Send_Packet_DeadorAlive(int UserID, int OtherUserID, char Type);
void Send_Packet_Data(int UserID, int OtherObjectID, char Type, short Data);
void RecvPacketAssemble(int UserID, int RecvByte);

void ProcessPacket(int UserID, char* Buf);

void WorkerThread();
void TimerThread();