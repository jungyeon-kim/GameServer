#pragma once

#include <windows.h>  
#include <sqlext.h>  
#include <string>

class CDBHandler
{
private:
    static constexpr int MAX_ID_LEN{ 50 };

    SQLHENV henv{};
    SQLHDBC hdbc{};
    SQLHSTMT hstmt{};
    SQLRETURN retcode{};
    SQLWCHAR UserID[MAX_ID_LEN]{};
    SQLINTEGER UserPosX{}, UserPosY{}, UserLevel{}, UserExp{}, UserHP{};
    SQLLEN cbUserID{}, cbUserPosX{}, cbUserPosY{}, cbUserLevel{}, cbUserExp{}, cbUserHP{};
public:
    CDBHandler();
    ~CDBHandler();

    void show_error();
    void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);

    std::wstring StringToWstring(const std::string& str);

    bool SignUp(std::string ID, std::string Password);
    bool SignOut(std::string ID, std::string Password);
    bool IsValidedUser(std::string ID, std::string Password);
    void SetClientData(std::string ID, short& PosX, short& PosY, short& Level, short& Exp, short& HP);
    void SetDBData(std::string ID, short& PosX, short& PosY, short& Level, short& Exp, short& HP);
};