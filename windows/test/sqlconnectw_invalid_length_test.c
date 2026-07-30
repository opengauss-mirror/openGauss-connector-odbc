/*
 * Minimal Windows regression test for SQLConnectW invalid length handling.
 *
 * Vulnerability #9 in odbc-brainafk/漏洞清单.csv: without the fix,
 * win_unicode.c would call ucs2strlen on the wide string and read out of
 * bounds when given a negative length other than SQL_NTS.
 *
 * Build (MinGW 32-bit):
 *   i686-w64-mingw32-gcc -o sqlconnectw_invalid_length_test.exe \
 *       sqlconnectw_invalid_length_test.c -lodbc32
 *
 * Run:
 *   .\sqlconnectw_invalid_length_test.exe
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>

int main(void)
{
    SQLRETURN rc;
    SQLHENV henv = SQL_NULL_HENV;
    SQLHDBC hdbc = SQL_NULL_HDBC;
    SQLWCHAR dsn[] = L"gaussdb";
    SQLWCHAR user[] = L"odbc";
    SQLWCHAR pwd[] = L"Gauss@123";

    rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (!SQL_SUCCEEDED(rc))
    {
        fprintf(stderr, "FAIL: SQLAllocHandle(ENV) failed\n");
        return 1;
    }

    rc = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION,
                       (SQLPOINTER) SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(rc))
    {
        fprintf(stderr, "FAIL: SQLSetEnvAttr failed\n");
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        return 1;
    }

    rc = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
    if (!SQL_SUCCEEDED(rc))
    {
        fprintf(stderr, "FAIL: SQLAllocHandle(DBC) failed\n");
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        return 1;
    }

    /*
     * SQLConnectW with an invalid negative length (not SQL_NTS) must fail.
     * Without the fix, win_unicode.c would call ucs2strlen on the wide
     * string and read out of bounds.
     */
    rc = SQLConnectW(hdbc, dsn, -2, user, SQL_NTS, pwd, SQL_NTS);
    if (rc != SQL_ERROR)
    {
        fprintf(stderr,
                "FAIL: SQLConnectW with invalid negative length should return SQL_ERROR, got %d\n",
                (int) rc);
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        return 1;
    }

    printf("OK: SQLConnectW with invalid negative length returned SQL_ERROR\n");

    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, henv);
    return 0;
}
