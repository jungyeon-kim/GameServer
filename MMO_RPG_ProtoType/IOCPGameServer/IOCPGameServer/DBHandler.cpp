#include "DBHandler.h"
#include "protocol.h"
#include <locale.h>
#include <iostream>
#include <codecvt>

using namespace std;

CDBHandler::CDBHandler()
{
    setlocale(LC_ALL, "korean");

    // Allocate environment handle  
    retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

    // Set the ODBC version environment attribute  
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

        // Allocate connection handle  
        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
            retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

            // Set login timeout to 5 seconds  
            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
                SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

                // Connect to data source  
                retcode = SQLConnect(hdbc, (SQLWCHAR*)L"GameServer_ODBC", SQL_NTS, (SQLWCHAR*)NULL, SQL_NTS, NULL, SQL_NTS);
                if (retcode == SQL_SUCCESS_WITH_INFO)
                    cout << "DB connected." << endl;
            }
        }
    }
}

CDBHandler::~CDBHandler()
{
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    {
        SQLCancel(hstmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    }
    SQLDisconnect(hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

void CDBHandler::show_error()
{
    cout << "error" << endl;
}
void CDBHandler::HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
    SQLSMALLINT iRec = 0;
    SQLINTEGER  iError;
    WCHAR       wszMessage[1000];
    WCHAR       wszState[SQL_SQLSTATE_SIZE + 1];

    if (RetCode == SQL_INVALID_HANDLE) 
    {
        fwprintf(stderr, L"Invalid handle!\n");
        return;
    }
    while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage, 
        (SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS)
        if (wcsncmp(wszState, L"01004", 5)) fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
}

wstring CDBHandler::StringToWstring(const string& str)
{
    typedef codecvt_utf8<wchar_t> convert_type;
    wstring_convert<convert_type, wchar_t> converter;

    return converter.from_bytes(str);
}

bool CDBHandler::SignUp(string ID, string Password)
{
    wstring Query{ L"EXEC SignUp " + StringToWstring(ID) + L", " + StringToWstring(Password) };
    int Flag{};
    SQLLEN cbFlag{};

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
        retcode = SQLExecDirect(hstmt, (SQLWCHAR*)Query.c_str(), SQL_NTS);

        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
        {
            retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &Flag, 100, &cbFlag);
            retcode = SQLFetch(hstmt);

            if (retcode == SQL_ERROR) show_error();

            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
                return (bool)Flag;
        }
        else
            HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
    }

    return false;
}

bool CDBHandler::SignOut(std::string ID, std::string Password)
{
    wstring Query{ L"EXEC SignOut " + StringToWstring(ID) + L", " + StringToWstring(Password) };
    int Flag{};
    SQLLEN cbFlag{};

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
        retcode = SQLExecDirect(hstmt, (SQLWCHAR*)Query.c_str(), SQL_NTS);

        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
        {
            retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &Flag, 100, &cbFlag);
            retcode = SQLFetch(hstmt);

            if (retcode == SQL_ERROR) show_error();

            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
                return (bool)Flag;
        }
        else
            HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
    }

    return false;
}

bool CDBHandler::IsValidedUser(string ID, string Password)
{
    if (ID == "adminKJY") return true;

    wstring Query{ L"EXEC IsValidedUser " + StringToWstring(ID) + L", " + StringToWstring(Password) };
    int Flag{};
    SQLLEN cbFlag{};

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
        retcode = SQLExecDirect(hstmt, (SQLWCHAR*)Query.c_str(), SQL_NTS);

        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
        {
            retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &Flag, 100, &cbFlag);
            retcode = SQLFetch(hstmt);

            if (retcode == SQL_ERROR) show_error();

            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
                return (bool)Flag;
        }
        else
            HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
    }

    return false;
}

void CDBHandler::SetClientPosition(string ID, short& X, short& Y)
{
    wstring Query{ L"EXEC SetClientPosition " + StringToWstring(ID) };

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
        retcode = SQLExecDirect(hstmt, (SQLWCHAR*)Query.c_str(), SQL_NTS);

        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
        {
            retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &UserPosX, 100, &cbUserPosX);
            retcode = SQLBindCol(hstmt, 2, SQL_C_LONG, &UserPosY, 100, &cbUserPosY);
            retcode = SQLFetch(hstmt);

            if (retcode == SQL_ERROR) show_error();

            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
            {
                if (ID == "adminKJY")   // 더미 클라이언트의 경우
                {
                    X = rand() % WORLD_WIDTH;
                    Y = rand() % WORLD_HEIGHT;
                }
                else
                {
                    X = UserPosX;
                    Y = UserPosY;
                }
            }
        }
        else
            HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
    }
}

void CDBHandler::SetDBPosition(string ID, short X, short Y)
{
    if (ID == "adminKJY") return;

    wstring Query{ L"EXEC SetDBPosition " + StringToWstring(ID) + L", " + to_wstring(X) + L", " + to_wstring(Y) };

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
        retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
        retcode = SQLExecDirect(hstmt, (SQLWCHAR*)Query.c_str(), SQL_NTS);

        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
        {
            if (retcode == SQL_ERROR) show_error();

            if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
                cout << "Saved Position of " << ID << "." << endl;
        }
        else
            HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
    }
}