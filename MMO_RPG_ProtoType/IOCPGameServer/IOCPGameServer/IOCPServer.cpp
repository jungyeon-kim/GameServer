#include "IOCPServer.h"

void InitClients()
{
    for (int i = 0; i < MAX_USER_COUNT; ++i) Clients[i].ID = i;
}

void InitNPCs()
{
    for (int i = NPC_ID_START; i < NPC_ID_START + MAX_NPC_COUNT; ++i)
    {
        Clients[i].Socket = 0;
        Clients[i].ID = i;
        sprintf_s(Clients[i].Name, "NPC%d", i);
        Clients[i].Status = ClientStat::SLEEP;
        Clients[i].PosX = rand() % WORLD_WIDTH;
        Clients[i].PosY = rand() % WORLD_HEIGHT;

        lua_State* L{ Clients[i].L = luaL_newstate() };
        luaL_openlibs(L);
        luaL_loadfile(L, "NPC.LUA");
        lua_pcall(L, 0, 0, 0);
        lua_getglobal(L, "SetID");   // �Լ��� push
        lua_pushnumber(L, i);      // SetID�Լ��� ���� push
        lua_pcall(L, 1, 0, 0);      // ���ÿ��� ���ϰ��� ���Ե�
        lua_pop(L, 1);            // ���ϰ� pop
        // lua�� �Լ� ���
        lua_register(L, "API_GetX", API_GetX);
        lua_register(L, "API_GetY", API_GetY);
        lua_register(L, "API_SendMsg", API_SendMsg);

    }
}

void Disconnect(int UserID)
{
    // ������ �ش� ������ ���� ��ġ�� DB�� Set
    if (Clients[UserID].Status == ClientStat::ACTIVE)
        DBHandler.SetDBPosition(Clients[UserID].Name, Clients[UserID].PosX, Clients[UserID].PosY);

    Send_Packet_Leave(UserID, UserID);

    Clients[UserID].Mutex.lock();
    Clients[UserID].Status = ClientStat::ALLOCATED;
    closesocket(Clients[UserID].Socket);

    for (int i = 0; i < NPC_ID_START; ++i)
    {
        if (Clients[i].ID == UserID) continue;   // ����� ����
        //Client.Mutex.lock();
        if (Clients[i].Status == ClientStat::ACTIVE)
            Send_Packet_Leave(Clients[i].ID, UserID);
        //Client.Mutex.unlock();
    }
    Clients[UserID].Status = ClientStat::FREE;
    Clients[UserID].Mutex.unlock();
}

bool IsNear(int UserID, int OtherObjectID)
{
    return abs(Clients[UserID].PosX - Clients[OtherObjectID].PosX) <= VIEW_RANGE &&
        abs(Clients[UserID].PosY - Clients[OtherObjectID].PosY) <= VIEW_RANGE;
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

int API_SendMsg(lua_State* L)
{
    int MyID{ (int)lua_tointeger(L, -3) };
    int UserID{ (int)lua_tointeger(L, -2) };
    char* Msg{ (char*)lua_tostring(L, -1) };
    Send_Packet_Chat(UserID, MyID, Msg);
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

void Send_Packet_Log(int UserID, char Msg[])
{
    SC_Packet_Log Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_LOG;
    strcpy_s(Packet.Msg, Msg);

    Send_Packet(UserID, &Packet);
}

void Send_Packet_Login_Ok(int UserID)
{
    SC_Packet_Login_OK Packet{};

    Packet.Size = sizeof(Packet);
    Packet.Type = SC_LOGIN_OK;
    Packet.ID = UserID;
    Packet.Exp = 0;
    Packet.Level = 0;
    Packet.HP = 0;
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
    Packet.ObjectType = O_HUMAN;
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

void RecvPacketAssemble(int UserID, int RecvByte)
{
    Client& Client{ Clients[UserID] };
    int RestByte{ RecvByte };                  // ó���ؾ��� ���� ������ ũ��
    char* PosToProcess{ Client.RecvOver.IOBuf };   // ó���� ��ġ
    int PacketSize{};

    if (Client.PrevRecvSize) PacketSize = Client.PacketBuf[0];

    while (RestByte)   // �������� ��Ŷ�� �� ���� �����Ƿ� ������ ó��
    {
        if (!PacketSize) PacketSize = *PosToProcess;
        // ��Ŷ�� �ϼ��� �� �ִٸ�
        if (PacketSize <= RestByte + Client.PrevRecvSize)
        {
            memcpy(Client.PacketBuf + Client.PrevRecvSize, PosToProcess, PacketSize - Client.PrevRecvSize);
            PosToProcess += PacketSize - Client.PrevRecvSize;
            RestByte -= PacketSize - Client.PrevRecvSize;
            PacketSize = 0;
            ProcessPacket(UserID, Client.PacketBuf);
            Client.PrevRecvSize = 0;
        }
        // ��Ŷ�� �ϼ��� �� ���ٸ�
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
        CS_Packet_SignUp* Packet{ reinterpret_cast<CS_Packet_SignUp*>(Buf) };
        bool IsValid{ DBHandler.SignUp(Packet->Name, Packet->Password) };

        if (IsValid)
        {
            char Msg[]{ "ȸ�������� �Ϸ�Ǿ����ϴ�." };
            Send_Packet_Log(UserID, Msg);
        }
        else
        {
            char Msg[]{ "�̹� �����ϴ� ID�Դϴ�." };
            Send_Packet_Log(UserID, Msg);
        }
        break;
    }
    case CS_SIGNOUT:
    {
        CS_Packet_SignOut* Packet{ reinterpret_cast<CS_Packet_SignOut*>(Buf) };
        bool IsValid{ DBHandler.SignOut(Packet->Name, Packet->Password) };

        if (IsValid)
        {
            char Msg[]{ "ȸ��Ż�� �Ϸ�Ǿ����ϴ�." };
            Send_Packet_Log(UserID, Msg);
        }
        else
        {
            char Msg[]{ "���������ʴ� ID�̰ų� Password�� Ʋ�Ƚ��ϴ�." };
            Send_Packet_Log(UserID, Msg);
        }
        break;
    }
    case CS_LOGIN:
    {
        CS_Packet_Login* Packet{ reinterpret_cast<CS_Packet_Login*>(Buf) };

        // ��ȿ�� ID, Password���� �˻�
        if (!DBHandler.IsValidedUser(Packet->Name, Packet->Password))
        {
            char Msg[]{ "�α��� ����! ID�� Password�� Ȯ�����ּ���." };
            Send_Packet_Log(UserID, Msg);
            Disconnect(UserID);
            break;
        }

        Clients[UserID].Mutex.lock();
        strcpy_s(Clients[UserID].Name, Packet->Name);
        Clients[UserID].Name[MAX_ID_LEN] = NULL;

        // DB���� Ŭ���̾�Ʈ�� ��ġ�� �޾ƿ� Set
        DBHandler.SetClientPosition(Clients[UserID].Name, Clients[UserID].PosX, Clients[UserID].PosY);

        Send_Packet_Login_Ok(UserID);

        Clients[UserID].Status = ClientStat::ACTIVE;
        Clients[UserID].Mutex.unlock();

        for (auto& Client : Clients)
        {
            if (UserID == Client.ID) continue;   // ����� & �ڽſ��� ������ �� ����
            if (IsNear(UserID, Client.ID))
            {
                //Clients[i].Mutex.lock();
                if (Client.Status == ClientStat::SLEEP) ActivateNPC(Client.ID);
                if (Client.Status == ClientStat::ACTIVE)
                {
                    Send_Packet_Enter(UserID, Client.ID);
                    if (IsPlayer(Client.ID)) Send_Packet_Enter(Client.ID, UserID);
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

        short& PosX{ Clients[UserID].PosX };
        short& PosY{ Clients[UserID].PosY };

        switch (Packet->Dir)
        {
        case D_UP:      if (PosY > 0)               --PosY; break;
        case D_DOWN:   if (PosY < WORLD_HEIGHT - 1)   ++PosY; break;
        case D_LEFT:   if (PosX > 0)               --PosX; break;
        case D_RIGHT:   if (PosX < WORLD_WIDTH - 1)      ++PosX; break;
        default:
            cout << "Unknown Direction From Client Move Packet." << endl;
            DebugBreak();
            exit(-1);
        }

        Clients[UserID].Mutex.lock();
        unordered_set<int> OldViewList{ Clients[UserID].ViewList };
        Clients[UserID].Mutex.unlock();
        unordered_set<int> NewViewList{};

        for (auto& Client : Clients)
        {
            if (!IsNear(UserID, Client.ID)) continue;
            if (Client.Status == ClientStat::SLEEP) ActivateNPC(Client.ID);
            if (Client.Status != ClientStat::ACTIVE || Client.ID == UserID) continue;
            if (!IsPlayer(Client.ID))
            {
                ExOverlapped* ExOver{ new ExOverlapped{} };
                ExOver->Op = EnumOp::PLAYER_MOVE;
                ExOver->PlayerID = UserID;
                PostQueuedCompletionStatus(IOCP, 1, Client.ID, &ExOver->Over);
            }
            NewViewList.emplace(Client.ID);
        }

        Send_Packet_Move(UserID, UserID);

        for (auto& NewVisibleObject : NewViewList)
        {
            if (!OldViewList.count(NewVisibleObject))   // ������Ʈ�� ���� �þ߿� ������ ��
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
            else                              // ������Ʈ�� ��� �þ߿� ������ ��
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

        for (auto& OldVisibleObject : OldViewList)      // ������Ʈ�� �þ߿��� ������� ��
        {
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
        // �׹�° ���ڴ� �Ϸ�� IO ������ ���۵Ǿ��� �� ������ OVERLAPPED ����ü�� �ּҸ� �޴� ������ ���� ������
        GetQueuedCompletionStatus(IOCP, &IOByte, &CompletionKey, &Over, INFINITE);   // ���⼭ Blocking

        ExOverlapped* ExOver{ reinterpret_cast<ExOverlapped*>(Over) };
        int ObjectID{ static_cast<int>(CompletionKey) };

        switch (ExOver->Op)
        {
        case EnumOp::RECV:
        {
            if (!IOByte) Disconnect(ObjectID);      // ���� �����Ͱ� 0Byte��� �ش� Ŭ���̾�Ʈ ����
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
            if (!IOByte) Disconnect(ObjectID);      // ���� �����Ͱ� 0Byte��� �ش� Ŭ���̾�Ʈ ����
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

            // ���߿� �α��� ���� ��Ŷ�� ���� ����
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
            switch (rand() % 4)
            {
            case 0:      if (Clients[ObjectID].PosY > 0) --Clients[ObjectID].PosY;               break;
            case 1:      if (Clients[ObjectID].PosY < WORLD_HEIGHT - 1) ++Clients[ObjectID].PosY;   break;
            case 2:      if (Clients[ObjectID].PosX > 0) --Clients[ObjectID].PosX;               break;
            case 3:      if (Clients[ObjectID].PosX < WORLD_WIDTH - 1) ++Clients[ObjectID].PosX;      break;
            }
            for (int PlayerID = 0; PlayerID < NPC_ID_START; ++PlayerID)
            {
                if (Clients[PlayerID].Status != ClientStat::ACTIVE) continue;

                if (IsNear(PlayerID, ObjectID))
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

            bool Flag{};
            for (int PlayerID = 0; PlayerID < NPC_ID_START; ++PlayerID)
                if (IsNear(PlayerID, ObjectID) && Clients[PlayerID].Status == ClientStat::ACTIVE)
                {
                    Flag = true;
                    break;
                }
            if (Flag) AddTimerQueue(ObjectID, ExOver->Op, 1000);
            else Clients[ObjectID].Status = ClientStat::SLEEP;

            delete ExOver;
            break;
        }
        case EnumOp::PLAYER_MOVE:
        {
            Clients[ObjectID].LuaMutex.lock();
            lua_State* L{ Clients[ObjectID].L };
            lua_getglobal(L, "BeginOverlap");
            lua_pushnumber(L, ExOver->PlayerID);
            int Error{ lua_pcall(L, 1, 0, 0) };
            if (Error) cout << lua_tostring(L, -1) << endl;
            // EventPlayerMove�� ���ϰ��� �����Ƿ� pop�Ұ� ����. (lua_pcall()���� ��� pop�Ǳ� ����)
            Clients[ObjectID].LuaMutex.unlock();

            delete ExOver;
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
                ExOverlapped* ExOver{ new ExOverlapped{} };
                ExOver->Op = Event.Op;
                PostQueuedCompletionStatus(IOCP, 1, Event.ObjectID, &ExOver->Over);
                break;
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
    // ���� PC�� �ھ 6���̹Ƿ� 6���� ������ ����
    for (int i = 0; i < 6; ++i) Workers.emplace_back(WorkerThread);

    thread Timer{ TimerThread };

    for (auto& Worker : Workers) Worker.join();
    Timer.join();
}