#include "IOCPServer.h"

void InitClients()
{
    for (int i = 0; i < MAX_USER_COUNT; ++i) Clients[i].ID = i;
}

void ResetClient(int UserID)
{
    Clients[UserID].PosX = 100;
    Clients[UserID].PosY = 100;

    // 기존 장소 Leave
    for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
        for (auto& ObjectID : Sector)
        {
            if (!IsPlayer(ObjectID) || ObjectID == UserID) continue;
            Send_Packet_Leave(ObjectID, UserID);
        }
    Send_Packet_Move(UserID, UserID);
    Send_Packet_Data(UserID, UserID, SC_EXP, Clients[UserID].Exp /= 2);
    Send_Packet_Data(UserID, UserID, SC_HP, Clients[UserID].HP = Clients[UserID].MaxHP);

    // 초기 장소 Enter
    SetSector(UserID, Clients[UserID].PosX, Clients[UserID].PosY);
    for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
        for (auto& ObjectID : Sector)
        {
            if (!IsPlayer(ObjectID) || ObjectID == UserID) continue;
            Send_Packet_Enter(ObjectID, UserID);
            Send_Packet_Enter(UserID, ObjectID);
        }
}

void InitNPCs()
{
    for (int i = NPC_ID_START; i < NPC_ID_START + MAX_NPC_COUNT; ++i)
    {
        Clients[i].Socket = 0;
        Clients[i].ID = i;
        sprintf_s(Clients[i].Name, "NPC%d", i - NPC_ID_START);
        Clients[i].Status = ClientStat::SLEEP;
        Clients[i].PosX = rand() % WORLD_WIDTH;
        Clients[i].PosY = rand() % WORLD_HEIGHT;
        Clients[i].Level = rand() % 19 + 1;
        Clients[i].MaxHP = Clients[i].Level * 10;
        Clients[i].HP = Clients[i].MaxHP;

        // 섹터 배정
        SetSector(i, Clients[i].PosX, Clients[i].PosY);

        lua_State* L{ Clients[i].L = luaL_newstate() };
        luaL_openlibs(L);
        luaL_loadfile(L, "NPC.LUA");
        lua_pcall(L, 0, 0, 0);
        lua_getglobal(L, "SetID");  // 함수를 push
        lua_pushnumber(L, i);       // SetID함수의 인자 push
        lua_pcall(L, 1, 0, 0);      // 스택에는 리턴값만 남게됨
        lua_pop(L, 1);              // 리턴값 pop
        // lua에 함수 등록
        lua_register(L, "API_GetX", API_GetX);
        lua_register(L, "API_GetY", API_GetY);
        lua_register(L, "API_IsDead", API_IsDead);
        lua_register(L, "API_TakeDamage", API_TakeDamage);

    }
}

void Disconnect(int UserID)
{
    if (Clients[UserID].Status == ClientStat::ACTIVE)
    {
        // 종료전 해당 유저의 현재 상태를 DB에 Set
        DBHandler.SetDBData(Clients[UserID].Name, Clients[UserID].PosX, Clients[UserID].PosY, 
            Clients[UserID].Level, Clients[UserID].Exp, Clients[UserID].HP);
        // 종료전 해당 유저의 섹터 정보를 초기화
        Sectors[Clients[UserID].CurrentSector].erase(UserID);
        Clients[UserID].CurrentSector = -1;
    }

    Send_Packet_Leave(UserID, UserID);

    Clients[UserID].Mutex.lock();
    Clients[UserID].Status = ClientStat::ALLOCATED;
    closesocket(Clients[UserID].Socket);

    for (int i = 0; i < NPC_ID_START; ++i)
    {
        if (Clients[i].ID == UserID) continue;   // 데드락 방지
        //Client.Mutex.lock();
        if (Clients[i].Status == ClientStat::ACTIVE)
            Send_Packet_Leave(Clients[i].ID, UserID);
        //Client.Mutex.unlock();
    }
    Clients[UserID].Status = ClientStat::FREE;
    Clients[UserID].Mutex.unlock();
}

bool IsNear(int UserID, int OtherObjectID, int Range)
{
    return abs(Clients[UserID].PosX - Clients[OtherObjectID].PosX) <= Range &&
        abs(Clients[UserID].PosY - Clients[OtherObjectID].PosY) <= Range;
}

bool IsPlayer(int ObjectID)
{
    return ObjectID < NPC_ID_START;
}

void ActivateNPC(int NPCID)
{
    ClientStat OldStat{ ClientStat::SLEEP };
    if (atomic_compare_exchange_strong(&Clients[NPCID].Status, &OldStat, ClientStat::ACTIVE))
        AddTimerQueue(NPCID, EnumOp::RANDOM_MOVE, 1000);
}

int GetSectorIdx(int PosX, int PosY)
{
    return (PosX / SECTOR_WIDTH) + (PosY / SECTOR_HEIGHT * SECTOR_HEIGHT * 2);
}

const vector<unordered_set<int>> GetNearSectors(int CurrentSectorIdx)
{
    static int NumOfColumn{ WORLD_WIDTH / SECTOR_WIDTH };
    vector<unordered_set<int>> NearSectors{};

    SectorLock.lock();
    for (int Column = -NumOfColumn - 1; Column < NumOfColumn; Column += NumOfColumn)
        for (int Row = Column; Row < Column + 3; ++Row)
            if (0 <= CurrentSectorIdx + Row && CurrentSectorIdx + Row < NumOfSector)
                NearSectors.emplace_back(Sectors[CurrentSectorIdx + Row]);
    SectorLock.unlock();

    return NearSectors;
}

void SetSector(int ObjectID, int NewPosX, int NewPosY)
{
    const int CurrentSector{ GetSectorIdx(NewPosX, NewPosY) };

    if (Sectors[CurrentSector].find(ObjectID) == Sectors[CurrentSector].end())
    {
        if (NewPosX != Clients[ObjectID].PosX || NewPosY != Clients[ObjectID].PosY)
        {
            const int PrevSectorIdx{ GetSectorIdx(Clients[ObjectID].PosX, Clients[ObjectID].PosY) };
            Sectors[PrevSectorIdx].erase(ObjectID);
        }

        Sectors[CurrentSector].emplace(ObjectID);
        Clients[ObjectID].CurrentSector = CurrentSector;
    }
}

void AddTimerQueue(int ObjectID, EnumOp Op, int Duration)
{
    TimerLock.lock();
    Event Event{ ObjectID, Op, high_resolution_clock::now() + milliseconds{ Duration }, 0 };
    TimerQueue.push(Event);
    TimerLock.unlock();
}

int API_GetX(lua_State* L)
{
    int ObjectID{ (int)lua_tointeger(L, -1) };
    lua_pushnumber(L, Clients[ObjectID].PosX);
    return 1;
}

int API_GetY(lua_State* L)
{
    int ObjectID{ (int)lua_tointeger(L, -1) };
    lua_pushnumber(L, Clients[ObjectID].PosY);
    return 1;
}

int API_IsDead(lua_State* L)
{
    int ObjectID{ (int)lua_tointeger(L, -1) };
    if (Clients[ObjectID].Status == ClientStat::DEAD) lua_pushboolean(L, true);
    else lua_pushboolean(L, false);
    return 1;
}

int API_TakeDamage(lua_State* L)
{
    int UserID{ (int)lua_tointeger(L, -3) };
    int NPCID{ (int)lua_tointeger(L, -2) };
    int Damage{ (int)lua_tointeger(L, -1) };
    if (Clients[UserID].HP > 0)
    {
        short CurrentHP{ Clients[UserID].HP -= Damage };
        for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
            for (auto& PlayerID : Sector)
                if (IsPlayer(PlayerID))
                    Send_Packet_Data(PlayerID, UserID, SC_HP, CurrentHP);

        string Msg{ static_cast<string>(Clients[NPCID].Name)
            + " was attacked with " + to_string(Damage) + " damage." };
        Send_Packet_Log(UserID, Msg.c_str(), 1);
    }
    if (Clients[UserID].HP < 0) Clients[UserID].HP = 0;
    if (!Clients[UserID].HP) ResetClient(UserID);
    return 0;
}

void Send_Packet(int UserID, void* BufPointer)
{
    char* Buf{ reinterpret_cast<char*>(BufPointer) };
    ExOverlapped* ExOver{ new ExOverlapped };
    ExOver->Op = EnumOp::SEND;
    ExOver->Over = {};
    ExOver->WSABuf.buf = ExOver->IOBuf;
    ExOver->WSABuf.len = Buf[0];
    memcpy(ExOver->IOBuf, Buf, Buf[0]);

    WSASend(Clients[UserID].Socket, &ExOver->WSABuf, 1, NULL, 0, &ExOver->Over, NULL);
}

void Send_Packet_Log(int UserID, const char* Msg, char IsInGame)
{
    SC_Packet_Log Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_LOG;
    strcpy_s(Packet.Msg, Msg);
    Packet.IsInGame = IsInGame;

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Login_Ok(int UserID)
{
    SC_Packet_Login_OK Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_LOGIN_OK;
    Packet.ID = UserID;
    Packet.Level = Clients[UserID].Level;
    Packet.Exp = Clients[UserID].Exp;
    Packet.HP = Clients[UserID].HP;
    Packet.PosX = Clients[UserID].PosX;
    Packet.PosY = Clients[UserID].PosY;

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Enter(int UserID, int OtherObjectID)
{
    SC_Packet_Enter Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_ENTER;
    Packet.ID = OtherObjectID;
    Packet.Level = Clients[OtherObjectID].Level;
    Packet.HP = Clients[OtherObjectID].HP;
    Packet.ObjectType = O_PLAYER;
    Packet.PosX = Clients[OtherObjectID].PosX;
    Packet.PosY = Clients[OtherObjectID].PosY;
    strcpy_s(Packet.Name, Clients[OtherObjectID].Name);

    Clients[UserID].Mutex.lock();
    Clients[UserID].ViewList.emplace(OtherObjectID);
    Clients[UserID].Mutex.unlock();

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Leave(int UserID, int OtherObjectID)
{
    SC_Packet_Leave Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_LEAVE;
    Packet.ID = OtherObjectID;

    Clients[UserID].Mutex.lock();
    Clients[UserID].ViewList.erase(OtherObjectID);
    Clients[UserID].Mutex.unlock();

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Chat(int UserID, int Chatter, char Msg[])
{
    SC_Packet_Chat Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_CHAT;
    Packet.ID = Chatter;
    strcpy_s(Packet.Msg, Msg);

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Move(int UserID, int MovedUserId)
{
    SC_Packet_Move Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_MOVE;
    Packet.ID = MovedUserId;
    Packet.PosX = Clients[MovedUserId].PosX;
    Packet.PosY = Clients[MovedUserId].PosY;
    Packet.MoveTime = Clients[MovedUserId].MoveTime;

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Attack(int UserID, int OtherUserID, char Type)
{
    SC_Packet_Attack Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = Type;
    Packet.ID = OtherUserID;

    Send_Packet(UserID, &Packet);
}

void Send_Packet_DeadorAlive(int UserID, int OtherUserID, char Type)
{
    SC_Packet_Attack Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = Type;
    Packet.ID = OtherUserID;

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Data(int UserID, int OtherObjectID, char Type, short Data)
{
    SC_Packet_Data Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = Type;
    Packet.ID = OtherObjectID;
    Packet.Data = Data;

    Send_Packet(UserID, &Packet);
}

void RecvPacketAssemble(int UserID, int RecvByte)
{
    Client& Client{ Clients[UserID] };
    int RestByte{ RecvByte };                  // 처리해야할 남은 데이터 크기
    char* PosToProcess{ Client.RecvOver.IOBuf };   // 처리할 위치
    int PacketSize{};

    if (Client.PrevRecvSize) PacketSize = Client.PacketBuf[0];

    while (RestByte)   // 여러개의 패킷이 올 수도 있으므로 루프로 처리
    {
        if (!PacketSize) PacketSize = *PosToProcess;
        // 패킷을 완성할 수 있다면
        if (PacketSize <= RestByte + Client.PrevRecvSize)
        {
            memcpy(Client.PacketBuf + Client.PrevRecvSize, PosToProcess, PacketSize - Client.PrevRecvSize);
            PosToProcess += PacketSize - Client.PrevRecvSize;
            RestByte -= PacketSize - Client.PrevRecvSize;
            PacketSize = 0;
            ProcessPacket(UserID, Client.PacketBuf);
            Client.PrevRecvSize = 0;
        }
        // 패킷을 완성할 수 없다면
        else
        {
            memcpy(Client.PacketBuf + Client.PrevRecvSize, PosToProcess, RestByte);
            Client.PrevRecvSize += RestByte;
            RestByte = 0;
            PosToProcess += RestByte;
        }
    }
}

void ProcessPacket(int UserID, char* Buf)
{
    switch (Buf[1])
    {
    case CS_SIGNUP:
    {
        CS_Packet_Login* Packet{ reinterpret_cast<CS_Packet_Login*>(Buf) };
        bool IsValid{ DBHandler.SignUp(Packet->Name, Packet->Password) };

        if (IsValid)
        {
            char Msg[]{ "회원가입이 완료되었습니다." };
            Send_Packet_Log(UserID, Msg);
        }
        else
        {
            char Msg[]{ "이미 존재하는 ID입니다." };
            Send_Packet_Log(UserID, Msg);
        }
        break;
    }
    case CS_SIGNOUT:
    {
        CS_Packet_Login* Packet{ reinterpret_cast<CS_Packet_Login*>(Buf) };
        bool IsValid{ DBHandler.SignOut(Packet->Name, Packet->Password) };

        if (IsValid)
        {
            char Msg[]{ "회원탈퇴가 완료되었습니다." };
            Send_Packet_Log(UserID, Msg);
        }
        else
        {
            char Msg[]{ "존재하지않는 ID이거나 Password가 틀렸습니다." };
            Send_Packet_Log(UserID, Msg);
        }
        break;
    }
    case CS_LOGIN:
    {
        CS_Packet_Login* Packet{ reinterpret_cast<CS_Packet_Login*>(Buf) };

        // 유효한 ID, Password인지 검사
        if (!DBHandler.IsValidedUser(Packet->Name, Packet->Password))
        {
            char Msg[]{ "로그인 실패! ID나 Password를 확인해주세요." };
            Send_Packet_Log(UserID, Msg);
            Disconnect(UserID);
            break;
        }

        Clients[UserID].Mutex.lock();
        strcpy_s(Clients[UserID].Name, Packet->Name);
        Clients[UserID].Name[MAX_ID_LEN] = NULL;

        // DB에서 클라이언트의 상태를 받아와 Set
        DBHandler.SetClientData(Clients[UserID].Name, Clients[UserID].PosX, Clients[UserID].PosY,
            Clients[UserID].Level, Clients[UserID].Exp, Clients[UserID].HP);
        // 섹터 배정
        SetSector(UserID, Clients[UserID].PosX, Clients[UserID].PosY);
        // 일정시간마다 HP 회복
        AddTimerQueue(UserID, EnumOp::RECOVERY, 5000);

        Send_Packet_Login_Ok(UserID);

        Clients[UserID].Status = ClientStat::ACTIVE;
        Clients[UserID].Mutex.unlock();

        for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
            for (auto& ObjectID : Sector)
            {
                if (UserID == ObjectID) continue;   // 데드락 & 자신에게 보내는 것 방지
                if (IsNear(UserID, ObjectID, VIEW_RANGE))
                {
                    //Clients[i].Mutex.lock();
                    if (Clients[ObjectID].Status == ClientStat::SLEEP) ActivateNPC(ObjectID);
                    if (Clients[ObjectID].Status == ClientStat::ACTIVE)
                    {
                        Send_Packet_Enter(UserID, ObjectID);
                        if (IsPlayer(ObjectID)) Send_Packet_Enter(ObjectID, UserID);
                    }
                    //Clients[i].Mutex.unlock();
                }
            }
        break;
    }
    case CS_MOVE:
    {
        CS_Packet_Move* Packet{ reinterpret_cast<CS_Packet_Move*>(Buf) };

        Clients[UserID].MoveTime = Packet->MoveTime;

        short PosX{ Clients[UserID].PosX };
        short PosY{ Clients[UserID].PosY };

        switch (Packet->Dir)
        {
        case D_UP:      if (PosY > 0)                   --PosY; break;
        case D_DOWN:    if (PosY < WORLD_HEIGHT - 1)    ++PosY; break;
        case D_LEFT:    if (PosX > 0)                   --PosX; break;
        case D_RIGHT:   if (PosX < WORLD_WIDTH - 1)     ++PosX; break;
        default:
            cout << "Unknown Direction From Client Move Packet." << endl;
            DebugBreak();
            exit(-1);
        }

        // 섹터 재배정
        SetSector(UserID, PosX, PosY);

        Clients[UserID].PosX = PosX;
        Clients[UserID].PosY = PosY;

        Clients[UserID].Mutex.lock();
        unordered_set<int> OldViewList{ Clients[UserID].ViewList };
        Clients[UserID].Mutex.unlock();
        unordered_set<int> NewViewList{};

        for(auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
            for (auto& ObjectID : Sector)
            {
                if (!IsNear(UserID, ObjectID, VIEW_RANGE) || ObjectID == UserID) continue;
                if (!IsPlayer(ObjectID))
                {
                    if (Clients[ObjectID].Status == ClientStat::SLEEP) ActivateNPC(ObjectID);

                    ExOverlapped* ExOver{ new ExOverlapped{} };
                    ExOver->Op = EnumOp::OVERLAP;
                    ExOver->PlayerID = UserID;
                    PostQueuedCompletionStatus(IOCP, 1, ObjectID, &ExOver->Over);
                }
                NewViewList.emplace(ObjectID);
            }

        Send_Packet_Move(UserID, UserID);

        for (auto& NewVisibleObject : NewViewList)
        {
            if (Clients[NewVisibleObject].Status == ClientStat::DEAD) continue;

            // 오브젝트가 새로 시야에 들어왔을 때
            if (!OldViewList.count(NewVisibleObject))
            {
                Send_Packet_Enter(UserID, NewVisibleObject);
                if (!IsPlayer(NewVisibleObject)) continue;

                Clients[NewVisibleObject].Mutex.lock();
                if (!Clients[NewVisibleObject].ViewList.count(UserID))
                {
                    Clients[NewVisibleObject].Mutex.unlock();
                    Send_Packet_Enter(NewVisibleObject, UserID);
                }
                else
                {
                    Clients[NewVisibleObject].Mutex.unlock();
                    Send_Packet_Move(NewVisibleObject, UserID);
                }
            }
            // 오브젝트가 계속 시야에 존재할 때
            else
            {
                if (!IsPlayer(NewVisibleObject)) continue;

                Clients[NewVisibleObject].Mutex.lock();
                if (Clients[NewVisibleObject].ViewList.count(UserID))
                {
                    Clients[NewVisibleObject].Mutex.unlock();
                    Send_Packet_Move(NewVisibleObject, UserID);
                }
                else
                {
                    Clients[NewVisibleObject].Mutex.unlock();
                    Send_Packet_Enter(NewVisibleObject, UserID);
                }
            }
        }

        // 오브젝트가 시야에서 사라졌을 때
        for (auto& OldVisibleObject : OldViewList)
        {
            if (Clients[OldVisibleObject].Status == ClientStat::DEAD) continue;

            if (!NewViewList.count(OldVisibleObject))
            {
                Send_Packet_Leave(UserID, OldVisibleObject);
                if (!IsPlayer(OldVisibleObject)) continue;

                Clients[OldVisibleObject].Mutex.lock();
                if (Clients[OldVisibleObject].ViewList.count(UserID))
                {
                    Clients[OldVisibleObject].Mutex.unlock();
                    Send_Packet_Leave(OldVisibleObject, UserID);
                }
                else Clients[OldVisibleObject].Mutex.unlock();
            }
        }
        break;
    }
    case CS_ATTACK_START:
    {
        Clients[UserID].Mutex.lock();
        unordered_set<int> ViewList{ Clients[UserID].ViewList };
        Clients[UserID].Mutex.unlock();

        for (auto& VisibleObject : ViewList)
        {
            if (!IsPlayer(VisibleObject))
            {
                if (IsNear(UserID, VisibleObject, 1) && Clients[VisibleObject].Status != ClientStat::DEAD)
                {
                    if (Clients[VisibleObject].HP > 0) Clients[VisibleObject].HP -= 20;
                    if (Clients[VisibleObject].HP < 0) Clients[VisibleObject].HP = 0;
                    for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
                        for (auto& PlayerID : Sector)
                            if (IsPlayer(PlayerID))
                                Send_Packet_Data(PlayerID, VisibleObject, SC_HP, Clients[VisibleObject].HP);

                    string Msg{ "Player hit " + static_cast<string>(Clients[VisibleObject].Name) + " dealing 20 damage." };
                    Send_Packet_Log(UserID, Msg.c_str(), 1);

                    if (!Clients[VisibleObject].HP) // 적 사망시
                    {
                        Clients[VisibleObject].Status = ClientStat::DEAD;

                        // 경험치 획득
                        Send_Packet_Data(UserID, UserID, SC_EXP, Clients[UserID].Exp += Clients[VisibleObject].Level * 5);
                        string Msg{ "Player Killed " + static_cast<string>(Clients[VisibleObject].Name) 
                            + " to gain " + to_string(Clients[VisibleObject].Level * 5) + " exp." };
                        Send_Packet_Log(UserID, Msg.c_str(), 1);
                        // 레벨업
                        if (Clients[UserID].Exp >= 100 * pow(2, Clients[UserID].Level - 1))
                        {
                            short CurrentLevel{ Clients[UserID].Level += 1 };

                            Send_Packet_Data(UserID, UserID, SC_EXP, Clients[UserID].Exp = 0);
                            for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
                                for (auto& PlayerID : Sector)
                                    if (IsPlayer(PlayerID))
                                        Send_Packet_Data(PlayerID, UserID, SC_LEVEL, CurrentLevel);
                        }
                        // 주변 플레이어에게 적이 사망했음을 알리고 적을 리스폰 타이머큐에 추가
                        for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
                            for (auto& PlayerID : Sector)
                            {
                                if (!IsPlayer(PlayerID)) continue;
                                Send_Packet_DeadorAlive(PlayerID, VisibleObject, SC_DEAD);
                            }
                        AddTimerQueue(VisibleObject, EnumOp::RESPAWN, 5000);
                    }
                }
                continue;
            }

            Send_Packet_Attack(VisibleObject, UserID, CS_ATTACK_START);
        }
        break;
    }
    case CS_ATTACK_END:
    {
        Clients[UserID].Mutex.lock();
        unordered_set<int> ViewList{ Clients[UserID].ViewList };
        Clients[UserID].Mutex.unlock();

        for (auto& VisibleObject : ViewList)
        {
            if (!IsPlayer(VisibleObject)) continue;
            Send_Packet_Attack(VisibleObject, UserID, CS_ATTACK_END);
        }
        break;
    }
    case CS_CHAT:
    {
        CS_Packet_Chat* Packet{ reinterpret_cast<CS_Packet_Chat*>(Buf) };

        for (auto& Sector : GetNearSectors(Clients[UserID].CurrentSector))
            for (auto& ObjectID : Sector)
                if (IsPlayer(ObjectID))
                    Send_Packet_Chat(ObjectID, UserID, Packet->Msg);
    }
    break;
    default:
        cout << "Unknown PacketType Error." << endl;
        DebugBreak();
        exit(-1);
    }
}

void WorkerThread()
{
    while (true)
    {
        DWORD IOByte{};
        ULONG_PTR CompletionKey{};
        WSAOVERLAPPED* Over{};
        // 네번째 인자는 완료된 IO 조작이 시작되었을 때 지정된 OVERLAPPED 구조체의 주소를 받는 변수에 대한 포인터
        GetQueuedCompletionStatus(IOCP, &IOByte, &CompletionKey, &Over, INFINITE);   // 여기서 Blocking

        ExOverlapped* ExOver{ reinterpret_cast<ExOverlapped*>(Over) };
        int ObjectID{ static_cast<int>(CompletionKey) };

        switch (ExOver->Op)
        {
        case EnumOp::RECV:
        {
            if (!IOByte) Disconnect(ObjectID);      // 받은 데이터가 0Byte라면 해당 클라이언트 종료
            else
            {
                RecvPacketAssemble(ObjectID, IOByte);

                Clients[ObjectID].RecvOver.Over = {};
                DWORD Flags{};
                WSARecv(Clients[ObjectID].Socket, &Clients[ObjectID].RecvOver.WSABuf, 1, NULL, &Flags, &Clients[ObjectID].RecvOver.Over, NULL);
            }
            break;
        }
        case EnumOp::SEND:
            if (!IOByte) Disconnect(ObjectID);      // 보낸 데이터가 0Byte라면 해당 클라이언트 종료
            delete ExOver;
            break;
        case EnumOp::ACCEPT:
        {
            int UserID{ -1 };

            for (int i = 0; i < MAX_USER_COUNT; ++i)
            {
                lock_guard<mutex> LockGuard{ Clients[i].Mutex };
                if (Clients[i].Status == ClientStat::FREE)
                {
                    UserID = i;
                    Clients[i].Status = ClientStat::ALLOCATED;
                    break;
                }
            }

            SOCKET ClientSocket{ ExOver->ClientSocket };

            // 나중에 로그인 실패 패킷을 보낼 예정
            if (UserID == -1) closesocket(ClientSocket);
            else
            {
                CreateIoCompletionPort(reinterpret_cast<HANDLE>(ClientSocket), IOCP, UserID, 0);

                Client& NewClient{ Clients[UserID] };
                NewClient.Socket = ClientSocket;
                NewClient.RecvOver.Op = EnumOp::RECV;
                NewClient.RecvOver.WSABuf.buf = NewClient.RecvOver.IOBuf;
                NewClient.RecvOver.WSABuf.len = MAX_BUF_SIZE;
                NewClient.ViewList.clear();

                DWORD Flags{};
                WSARecv(ClientSocket, &NewClient.RecvOver.WSABuf, 1, NULL, &Flags, &NewClient.RecvOver.Over, NULL);
            }

            ClientSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
            ExOver->ClientSocket = ClientSocket;
            ExOver->Over = {};
            AcceptEx(ListenSocket, ClientSocket, ExOver->IOBuf, NULL,
                sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &ExOver->Over);
            break;
        }
        case EnumOp::RANDOM_MOVE:
        {
            short PosX{ Clients[ObjectID].PosX };
            short PosY{ Clients[ObjectID].PosY };

            switch (rand() % 4)
            {
            case 0:      if (PosY > 0)                  --PosY;     break;
            case 1:      if (PosY < WORLD_HEIGHT - 1)   ++PosY;     break;
            case 2:      if (PosX > 0)                  --PosX;     break;
            case 3:      if (PosX < WORLD_WIDTH - 1)    ++PosX;     break;
            }

            // 섹터 재배정
            SetSector(ObjectID, PosX, PosY);

            Clients[ObjectID].PosX = PosX;
            Clients[ObjectID].PosY = PosY;

            for (auto& Sector : GetNearSectors(Clients[ObjectID].CurrentSector))
                for (auto& PlayerID : Sector)
                {
                    if (IsPlayer(PlayerID))
                    {
                        ExOverlapped* ExOver{ new ExOverlapped{} };
                        ExOver->Op = EnumOp::OVERLAP;
                        ExOver->PlayerID = PlayerID;
                        PostQueuedCompletionStatus(IOCP, 1, ObjectID, &ExOver->Over);
                    }
                }

            for (auto& Sector : GetNearSectors(Clients[ObjectID].CurrentSector))
                for (auto& PlayerID : Sector)
                {
                    if (!IsPlayer(Clients[PlayerID].ID)) continue;

                    if (IsNear(PlayerID, ObjectID, VIEW_RANGE))
                    {
                        Clients[PlayerID].Mutex.lock();
                        if (Clients[PlayerID].ViewList.count(ObjectID))
                        {
                            Clients[PlayerID].Mutex.unlock();
                            Send_Packet_Move(PlayerID, ObjectID);
                        }
                        else
                        {
                            Clients[PlayerID].Mutex.unlock();
                            Send_Packet_Enter(PlayerID, ObjectID);
                        }
                    }
                    else
                    {
                        Clients[PlayerID].Mutex.lock();
                        if (Clients[PlayerID].ViewList.count(ObjectID))
                        {
                            Clients[PlayerID].Mutex.unlock();
                            Send_Packet_Leave(PlayerID, ObjectID);
                        }
                        else Clients[PlayerID].Mutex.unlock();
                    }
                }

            // NPC가 플레이어 근처에 있고 살아있다면 다시 타이머 큐에 넣고 그렇지 않다면 잠재운다.
            bool Flag{};
            for (auto& Sector : GetNearSectors(Clients[ObjectID].CurrentSector))
                for (auto& PlayerID : Sector)
                {
                    if (!IsPlayer(Clients[PlayerID].ID)) continue;

                    if (IsNear(PlayerID, ObjectID, VIEW_RANGE) && Clients[ObjectID].Status == ClientStat::ACTIVE)
                    {
                        Flag = true;
                        break;
                    }
                }
            if (Flag) AddTimerQueue(ObjectID, ExOver->Op, 1000);
            else if (Clients[ObjectID].Status != ClientStat::DEAD) Clients[ObjectID].Status = ClientStat::SLEEP;

            delete ExOver;
            break;
        }
        case EnumOp::OVERLAP:
        {
            Clients[ObjectID].LuaMutex.lock();
            lua_State* L{ Clients[ObjectID].L };
            lua_getglobal(L, "BeginOverlap");
            lua_pushnumber(L, ExOver->PlayerID);
            int Error{ lua_pcall(L, 1, 0, 0) };
            //#if defined(_DEBUG)
            //if (Error) cout << lua_tostring(L, -1) << endl;
            //#endif
            // EventPlayerMove은 리턴값이 없으므로 pop할게 없다. (lua_pcall()에서 모두 pop되기 때문)
            Clients[ObjectID].LuaMutex.unlock();

            delete ExOver;
            break;
        }
        case EnumOp::RESPAWN:
        {
            bool IsInPlayerView{};

            for (auto& Sector : GetNearSectors(Clients[ObjectID].CurrentSector))
                for (auto& PlayerID : Sector)
                {
                    if (!IsPlayer(PlayerID)) continue;
                    if (!IsInPlayerView) IsInPlayerView = true;
                    Send_Packet_DeadorAlive(PlayerID, ObjectID, SC_RESPAWN);
                    Send_Packet_Data(PlayerID, ObjectID, SC_HP, Clients[ObjectID].MaxHP);
                }

            Clients[ObjectID].Status = ClientStat::SLEEP;
            Clients[ObjectID].HP = Clients[ObjectID].MaxHP;
            if (IsInPlayerView) ActivateNPC(ObjectID);
            break;
        }
        case EnumOp::RECOVERY:
        {
            short& RecoveredHP{ Clients[ObjectID].HP };

            if (Clients[ObjectID].HP < Clients[ObjectID].MaxHP) RecoveredHP += Clients[ObjectID].MaxHP / 10;
            if (Clients[ObjectID].HP > Clients[ObjectID].MaxHP) RecoveredHP = Clients[ObjectID].MaxHP;
            for (auto& Sector : GetNearSectors(Clients[ObjectID].CurrentSector))
                for (auto& PlayerID : Sector)
                    if(IsPlayer(PlayerID))
                        Send_Packet_Data(PlayerID, ObjectID, SC_HP, RecoveredHP);


            AddTimerQueue(ObjectID, ExOver->Op, 5000);
            break;
        }
        default:
            cout << "Unknown EnumOp." << endl;
            while (true);
        }
    }
}

void TimerThread()
{
    while (true)
    {
        this_thread::sleep_for(1ms);

        while (true)
        {
            TimerLock.lock();
            if (TimerQueue.empty()) { TimerLock.unlock(); break; }
            if (TimerQueue.top().WakeUpTime > high_resolution_clock::now()) { TimerLock.unlock(); break; }
            Event Event{ TimerQueue.top() };
            TimerQueue.pop();
            TimerLock.unlock();

            switch (Event.Op)
            {
            case EnumOp::RANDOM_MOVE:
            case EnumOp::RESPAWN:
            case EnumOp::RECOVERY:
            {
                ExOverlapped* ExOver{ new ExOverlapped{} };
                ExOver->Op = Event.Op;
                PostQueuedCompletionStatus(IOCP, 1, Event.ObjectID, &ExOver->Over);
                break;
            }
            }
        }
    }
}

int main()
{
    WSADATA WSAData{};
    WSAStartup(MAKEWORD(2, 2), &WSAData);

    cout << "Initializing NPC..." << endl;
    InitNPCs();
    cout << "NPC is Initialized!" << endl;

    ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    SOCKADDR_IN SocketAddr{};
    SocketAddr.sin_family = AF_INET;
    SocketAddr.sin_port = htons(SERVER_PORT);
    SocketAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    ::bind(ListenSocket, reinterpret_cast<sockaddr*>(&SocketAddr), sizeof(SocketAddr));

    listen(ListenSocket, SOMAXCONN);

    IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

    InitClients();

    CreateIoCompletionPort(reinterpret_cast<HANDLE>(ListenSocket), IOCP, 999, 0);
    SOCKET ClientSocket{ WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED) };
    ExOverlapped AcceptOver{};
    AcceptOver.Op = EnumOp::ACCEPT;
    AcceptOver.ClientSocket = ClientSocket;
    AcceptEx(ListenSocket, ClientSocket, AcceptOver.IOBuf, NULL,
        sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &AcceptOver.Over);

    vector<thread> Workers{};
    // 현재 PC의 코어가 6개이므로 6개의 스레드 생성
    for (int i = 0; i < 6; ++i) Workers.emplace_back(WorkerThread);

    thread Timer{ TimerThread };

    for (auto& Worker : Workers) Worker.join();
    Timer.join();
}