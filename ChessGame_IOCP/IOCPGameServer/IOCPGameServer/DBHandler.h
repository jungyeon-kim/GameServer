#pragma once

#include <windows.h>  
#include <sqlext.h>  
#include <string>

#define NAME_LEN 50  

class CDBHandler
{
private:
    SQLHENV henv{};
    SQLHDBC hdbc{};
    SQLHSTMT hstmt{};
    SQLRETURN retcode{};
    SQLWCHAR UserID[NAME_LEN]{};
    SQLINTEGER UserPosX{}, UserPosY{};
    SQLLEN cbUserID{}, cbUserPosX{}, cbUserPosY{};
public:
    CDBHandler();
    ~CDBHandler();

    void show_error();
    void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);

    std::wstring StringToWstring(const std::string& str);

    bool IsValidedUserID(std::string ID);
    void SetClientPosition(std::string ID, short& X, short& Y);
    void SetDBPosition(std::string ID, short X, short Y);
};