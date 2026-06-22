#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>

#include "common.h"

/*
 * Regression tests for security fixes in odbc-brainafk/漏洞清单.csv rows 7-15.
 *
 * Build:
 *   cd odbc-test-gauss && make security-fix-test
 * Run:
 *   ./security-fix-test
 *
 * Note: tests that exercise connection-string parsing assume a reachable
 * gaussdb DSN as configured in common.c. Some checks are best-effort and
 * print warnings instead of failing when the runtime environment does not
 * expose the necessary state (e.g. MYLOG files).
 *
 * Coverage note:
 *   - Vulnerability #9 (SQLConnectW length validation in win_unicode.c) is
 *     Windows-specific and not exercised by this Linux gcc/unixODBC test set.
 */

static void
test_bind_parameter_zero(void)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLINTEGER dummy = 0;
	SQLLEN cb = SQL_NTS;

	rc = SQLAllocStmt(conn, &hstmt);
	CHECK_STMT_RESULT(rc, "SQLAllocStmt failed", hstmt);

	/* Parameter number 0 is invalid (ODBC params are 1-based). */
	rc = SQLBindParameter(hstmt, 0, SQL_PARAM_INPUT,
						  SQL_C_SLONG, SQL_INTEGER,
						  0, 0, &dummy, 0, &cb);
	if (rc != SQL_ERROR)
	{
		fprintf(stderr, "FAIL: SQLBindParameter(ipar=0) should return SQL_ERROR, got %d\n", rc);
		exit(1);
	}
	printf("OK: SQLBindParameter(ipar=0) returned SQL_ERROR as expected\n");

	SQLFreeStmt(hstmt, SQL_DROP);
}

static void
test_sqlputdata_negative_length(void)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	char buf[16] = "dummy";
	SQLLEN cb = SQL_DATA_AT_EXEC;

	rc = SQLAllocStmt(conn, &hstmt);
	CHECK_STMT_RESULT(rc, "SQLAllocStmt failed", hstmt);

	/* Prepare a statement with a data-at-execution parameter. */
	rc = SQLPrepare(hstmt,
					(SQLCHAR *) "SELECT id FROM byteatab WHERE t = ?",
					SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLPrepare failed", hstmt);

	rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
						  SQL_C_BINARY, SQL_VARBINARY,
						  sizeof(buf), 0,
						  (void *) 1, 0, &cb);
	CHECK_STMT_RESULT(rc, "SQLBindParameter failed", hstmt);

	rc = SQLExecute(hstmt);
	if (rc != SQL_NEED_DATA)
		CHECK_STMT_RESULT(rc, "SQLExecute failed", hstmt);

	/* SQLPutData with an invalid negative length (-2) must fail. */
	rc = SQLPutData(hstmt, buf, -2);
	if (rc != SQL_ERROR)
	{
		fprintf(stderr, "FAIL: SQLPutData(..., -2) should return SQL_ERROR, got %d\n", rc);
		exit(1);
	}
	printf("OK: SQLPutData(..., -2) returned SQL_ERROR as expected\n");

	SQLFreeStmt(hstmt, SQL_DROP);
}

static void
test_descriptor_uaf(void)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLHDESC hdesc = SQL_NULL_HDESC;
	SQLINTEGER val = 0;
	SQLLEN cb = SQL_NTS;

	rc = SQLAllocStmt(conn, &hstmt);
	CHECK_STMT_RESULT(rc, "SQLAllocStmt failed", hstmt);

	rc = SQLAllocHandle(SQL_HANDLE_DESC, conn, &hdesc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("FAIL: SQLAllocHandle(DESC) failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Replace the statement's application row descriptor with a user desc. */
	rc = SQLSetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, (SQLPOINTER) hdesc, SQL_IS_POINTER);
	CHECK_STMT_RESULT(rc, "SQLSetStmtAttr(SQL_ATTR_APP_ROW_DESC) failed", hstmt);

	/* Free the user descriptor. The driver must clear stmt->ard to avoid UAF. */
	rc = SQLFreeHandle(SQL_HANDLE_DESC, hdesc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("FAIL: SQLFreeHandle(DESC) failed", SQL_HANDLE_DESC, hdesc);
		exit(1);
	}

	/*
	 * Re-using the statement after its ARD was freed should not crash.
	 * If stmt->ard still points to freed memory, SQLExecDirect/SQLFetch
	 * would dereference a dangling pointer.
	 */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "SELECT 1 AS id", SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed after descriptor free", hstmt);

	rc = SQLBindCol(hstmt, 1, SQL_C_SLONG, &val, 0, &cb);
	CHECK_STMT_RESULT(rc, "SQLBindCol failed", hstmt);

	rc = SQLFetch(hstmt);
	if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
		CHECK_STMT_RESULT(rc, "SQLFetch failed after descriptor free", hstmt);

	if (val != 1)
	{
		fprintf(stderr, "FAIL: expected 1, got %d\n", (int) val);
		exit(1);
	}
	printf("OK: statement reused after freeing user descriptor without UAF\n");

	SQLFreeStmt(hstmt, SQL_DROP);
}

static void
test_result_set_cap(void)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLCHAR errstate[6];
	SQLINTEGER nativeerr;
	SQLCHAR errmsg[256];
	SQLSMALLINT errlen;
	int cap_hit = 0;

	rc = SQLAllocStmt(conn, &hstmt);
	CHECK_STMT_RESULT(rc, "SQLAllocStmt failed", hstmt);

	/*
	 * The driver caps in-memory materialization at QR_MAX_BACKEND_TUPLES
	 * rows (currently 10,000,000, with reallocation failing at half that).
	 * Executing a query that returns 5,000,001 rows must fail before
	 * allocating without bound and crashing the client.
	 */
	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "SELECT 1 FROM generate_series(1, 5000001) AS id",
					   SQL_NTS);
	if (rc == SQL_ERROR)
	{
		if (SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1,
						  errstate, &nativeerr,
						  errmsg, sizeof(errmsg), &errlen) != SQL_ERROR)
		{
			if (strstr((char *) errmsg, "maximum allowed rows") != NULL)
				cap_hit = 1;
		}
	}

	if (!cap_hit)
	{
		fprintf(stderr, "FAIL: expected 'maximum allowed rows' error for 5,000,001-row result set\n");
		exit(1);
	}
	printf("OK: result set cap prevented unbounded allocation\n");

	SQLFreeStmt(hstmt, SQL_DROP);
}

static void
test_long_sslcert_path(void)
{
	SQLRETURN rc;
	SQLHDBC hdbc = SQL_NULL_HDBC;
	SQLCHAR str[1024];
	SQLSMALLINT strl;
	char dsn[4096];
	char longpath[1100];
	int i;

	/* Build a 1000-character dummy path that does not exist. */
	memset(longpath, 0, sizeof(longpath));
	strcpy(longpath, "/tmp/");
	for (i = strlen(longpath); i < 1000; i++)
		longpath[i] = 'a' + (i % 26);
	longpath[1000] = '\0';

	snprintf(dsn, sizeof(dsn),
			 "DSN=gaussdb;UID=odbc;PWD=Gauss@123;sslmode=verify-ca;sslcert=%s;sslkey=%s;sslrootcert=%s",
			 longpath, longpath, longpath);

	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(DDBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
	}

	/* The connection will fail because the files do not exist, but it must
	 * not crash due to URL buffer overflow. */
	rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) dsn, SQL_NTS,
						  str, sizeof(str), &strl, SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(rc))
	{
		printf("WARN: long sslcert path unexpectedly succeeded; disconnecting\n");
		SQLDisconnect(hdbc);
	}
	else
	{
		printf("OK: long sslcert path returned error without crashing\n");
	}

	SQLFreeConnect(hdbc);
}

static void
test_default_ssl_prefer(void)
{
	SQLRETURN rc;
	SQLHDBC hdbc = SQL_NULL_HDBC;
	SQLCHAR str[1024];
	SQLSMALLINT strl;

	/* Do not specify sslmode; the default should now be 'prefer'. */
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(DDBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
	}

	rc = SQLDriverConnect(hdbc, NULL,
						  (SQLCHAR *) "DSN=gaussdb;UID=odbc;PWD=Gauss@123",
						  SQL_NTS, str, sizeof(str), &strl,
						  SQL_DRIVER_COMPLETE);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("FAIL: default SSL mode 'prefer' should allow connection",
				   SQL_HANDLE_DBC, hdbc);
		exit(1);
	}
	printf("OK: connection succeeded with default SSL mode (prefer)\n");

	SQLDisconnect(hdbc);
	SQLFreeConnect(hdbc);
}

/*
 * Check that the MYLOG file produced by this process does not contain
 * plaintext sensitive values and does contain masked equivalents.
 * Returns 0 if no log file was found or if all checks passed, 1 on failure.
 */
static int
check_log_redaction(const char *password,
					const char *sslcert,
					const char *sslkey,
					const char *sslrootcert)
{
	char filename[PATH_MAX];
	char line[8192];
	const char *username;
	pid_t pid = getpid();
	FILE *fp;
	int found_plain = 0;
	int found_pwd_masked = 0;
	int found_sslcert_masked = 0;
	int found_sslkey_masked = 0;
	int found_sslrootcert_masked = 0;

	username = getenv("USER");
	if (!username)
		username = getenv("LOGNAME");
	if (!username)
		username = "unknown";

	/* Default MYLOG path on Linux: /tmp/mylog_<exe>_<user>_<pid>.log */
	snprintf(filename, sizeof(filename),
			 "/tmp/mylog_security-fix-test_%s_%u.log", username, (unsigned) pid);

	fp = fopen(filename, "r");
	if (!fp)
	{
		printf("WARN: could not open MYLOG file %s (is Debug=1 set in odbcinst.ini?)\n",
			   filename);
		return 0;
	}

	while (fgets(line, sizeof(line), fp))
	{
		if ((password && strstr(line, password)) ||
			(sslcert && strstr(line, sslcert)) ||
			(sslkey && strstr(line, sslkey)) ||
			(sslrootcert && strstr(line, sslrootcert)))
		{
			fprintf(stderr, "FAIL: MYLOG contains plaintext secret: %s\n", line);
			found_plain = 1;
		}

		if ((strstr(line, "PWD=") && strstr(line, "PWD=xxxxx")) ||
			strstr(line, "password=xxxxx"))
			found_pwd_masked = 1;
		if (strstr(line, "sslcert=") && strstr(line, "sslcert=xxxx"))
			found_sslcert_masked = 1;
		if (strstr(line, "sslkey=") && strstr(line, "sslkey=xxxx"))
			found_sslkey_masked = 1;
		if (strstr(line, "sslrootcert=") && strstr(line, "sslrootcert=xxxx"))
			found_sslrootcert_masked = 1;
	}
	fclose(fp);

	if (!found_plain)
		printf("OK: no plaintext password/SSL values found in MYLOG\n");
	if (found_pwd_masked)
		printf("OK: MYLOG masks password\n");
	if (found_sslcert_masked)
		printf("OK: MYLOG masks sslcert\n");
	if (found_sslkey_masked)
		printf("OK: MYLOG masks sslkey\n");
	if (found_sslrootcert_masked)
		printf("OK: MYLOG masks sslrootcert\n");

	return found_plain ? 1 : 0;
}

static void
test_log_password_redaction(void)
{
	SQLRETURN rc;
	SQLHDBC hdbc = SQL_NULL_HDBC;
	SQLCHAR str[1024];
	SQLSMALLINT strl;
	const char *password = "Gauss@123";
	const char *sslcert = "/secret/odbc-cert.pem";
	const char *sslkey = "/secret/odbc-key.pem";
	const char *sslrootcert = "/secret/odbc-root.crt";
	char dsn[4096];

	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(DDBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
	}

	/*
	 * Use multiple hosts/ports so the driver takes the URL-logging path in
	 * connection.c. The URL contains password and SSL file paths; all of them
	 * must be masked in MYLOG even if the connection itself fails.
	 */
	snprintf(dsn, sizeof(dsn),
			 "DSN=gaussdb;UID=odbc;PWD=%s;SERVER=127.0.0.1,127.0.0.1;PORT=5432,5432;"
			 "sslmode=verify-ca;sslcert=%s;sslkey=%s;sslrootcert=%s",
			 password, sslcert, sslkey, sslrootcert);

	rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) dsn, SQL_NTS,
						  str, sizeof(str), &strl, SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(rc))
	{
		printf("WARN: multi-host connection unexpectedly succeeded; disconnecting\n");
		SQLDisconnect(hdbc);
	}
	SQLFreeConnect(hdbc);

	if (check_log_redaction(password, sslcert, sslkey, sslrootcert) != 0)
		exit(1);
}

static void
test_large_result_set(void)
{
	SQLRETURN rc;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLLEN rows = 0;

	rc = SQLAllocStmt(conn, &hstmt);
	CHECK_STMT_RESULT(rc, "SQLAllocStmt failed", hstmt);

	/*
	 * Fetch 100k rows. This exercises QR_prepare_for_tupledata growth
	 * without hitting the 10M cap, and verifies normal large result sets
	 * still work.
	 */
	rc = SQLExecDirect(hstmt,
					   (SQLCHAR *) "SELECT generate_series(1, 100000) AS id",
					   SQL_NTS);
	CHECK_STMT_RESULT(rc, "SQLExecDirect failed", hstmt);

	while ((rc = SQLFetch(hstmt)) == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
		rows++;

	if (rows != 100000)
	{
		fprintf(stderr, "FAIL: expected 100000 rows, got %ld\n", (long) rows);
		exit(1);
	}
	printf("OK: fetched 100000 rows without crash or runaway allocation\n");

	SQLFreeStmt(hstmt, SQL_DROP);
}

int main(int argc, char **argv)
{
	printf("=== security-fix-test start ===\n");

	/* Connect once for tests that need a live connection. */
	test_connect();

	test_bind_parameter_zero();
	test_sqlputdata_negative_length();
	test_descriptor_uaf();
	test_large_result_set();
	test_result_set_cap();

	test_disconnect();

	/* The following tests allocate their own connections. */
	test_long_sslcert_path();
	test_default_ssl_prefer();
	test_log_password_redaction();

	printf("=== security-fix-test passed ===\n");
	return 0;
}
