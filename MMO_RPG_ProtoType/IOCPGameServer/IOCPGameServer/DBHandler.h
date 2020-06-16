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
    SQLINTEGER UserPosX{}, UserPosY{};
    SQLLEN cbUserID{}, cbUserPosX{}, cbUserPosY{};
public:
    CDBHandler();
    ~CDBHandler();

    void show_error();
    void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);

    std::wstring StringToWstring(const std::string& str);

    bool SignUp(std::string ID, std::string Password);
    bool SignOut(std::string ID, std::string Password);
    bool IsValidedUser(std::string ID, std::string Password);
    void SetClientPosition(std::string ID, short& X, short& Y);
    void SetDBPosition(std::string ID, short X, short Y);
};