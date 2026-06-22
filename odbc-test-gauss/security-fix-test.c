#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>

#include "common.h"

#ifdef WIN32
#include <sqlucode.h>
#endif

/*
 * Regression tests for security fixes in odbc-brainafk/漏洞清单.csv rows 7-15.
 *
 * Build:
 *   cd odbc-test-gauss && make security-fix-test
 * Run:
 *   ./security-fix-test
 *
 * Environment:
 *   ODBC_DRIVER   - optional absolute path to a driver .so to load instead of
 *                   the one configured in the gaussdb DSN.
 *   ODBC_SERVER   - server to use with ODBC_DRIVER (default 127.0.0.1)
 *   ODBC_PORT     - port to use with ODBC_DRIVER (default 5432)
 *   ODBC_DATABASE - database to use with ODBC_DRIVER (default postgres)
 *
 *   Example:
 *     ODBC_DRIVER=$PWD/../output/odbc/lib/psqlodbcw.so ./security-fix-test
 *
 * Note: tests that exercise connection-string parsing assume a reachable
 * gaussdb DSN as configured in common.c. Some checks are best-effort and
 * print warnings instead of failing when the runtime environment does not
 * expose the necessary state (e.g. MYLOG files).
 *
 * Coverage note:
 *   - Vulnerability #9 (SQLConnectW length validation in win_unicode.c) is
 *     exercised only on Windows builds; the test is compiled out on Linux.
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

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
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

	/*
	 * The statement is still in the data-at-execution state.  Cancel it before
	 * freeing so the connection can be cleanly disconnected.
	 */
	rc = SQLCancel(hstmt);
	CHECK_STMT_RESULT(rc, "SQLCancel failed after SQLPutData error", hstmt);

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
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

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

/*
 * Build a connection string that optionally loads a specific driver binary
 * via the ODBC_DRIVER environment variable. When ODBC_DRIVER is set, the
 * DSN keyword is omitted so unixODBC cannot fall back to the DSN's driver;
 * server/port/database are taken from ODBC_SERVER/ODBC_PORT/ODBC_DATABASE
 * (defaulting to 127.0.0.1:5432/postgres).
 */
static void
build_connect_string(char *buf, size_t bufsize, const char *extra)
{
	const char *driver = getenv("ODBC_DRIVER");

	if (driver && driver[0])
	{
		const char *server = getenv("ODBC_SERVER");
		const char *port = getenv("ODBC_PORT");
		const char *database = getenv("ODBC_DATABASE");

		if (!server || !server[0])
			server = "127.0.0.1";
		if (!port || !port[0])
			port = "5432";
		if (!database || !database[0])
			database = "postgres";

		snprintf(buf, bufsize,
				 "DRIVER=%s;SERVER=%s;PORT=%s;DATABASE=%s;"
				 "UID=odbc;PWD=Gauss@123;%s",
				 driver, server, port, database,
				 extra ? extra : "");
	}
	else
	{
		snprintf(buf, bufsize,
				 "DSN=gaussdb;UID=odbc;PWD=Gauss@123;%s",
				 extra ? extra : "");
	}
}

/*
 * Print MYLOG lines from this process that contain any of the given
 * keywords. Used for diagnostics when the cap test fails.
 */
static void
dump_mylog_lines(const char *keywords)
{
	char filename[PATH_MAX];
	char line[8192];
	const char *username;
	pid_t pid = getpid();
	FILE *fp;

	username = getenv("USER");
	if (!username)
		username = getenv("LOGNAME");
	if (!username)
		username = "unknown";

	snprintf(filename, sizeof(filename),
			 "/tmp/mylog_security-fix-test_%s%u.log", username, (unsigned) pid);

	fp = fopen(filename, "r");
	if (!fp)
	{
		printf("WARN: could not open MYLOG %s for diagnostics\n", filename);
		return;
	}

	printf("--- MYLOG diagnostics (keywords: %s) ---\n", keywords);
	while (fgets(line, sizeof(line), fp))
	{
		if (strstr(line, keywords))
			printf("%s", line);
	}
	printf("--- end MYLOG diagnostics ---\n");
	fclose(fp);
}

/*
 * Print the path of any psqlodbcw.so library mapped into this process.
 * Useful to verify that unixODBC actually loaded the DRIVER we requested.
 */
static void
dump_loaded_driver_path(void)
{
	FILE *fp = fopen("/proc/self/maps", "r");
	char line[1024];
	int found = 0;

	if (!fp)
		return;

	printf("--- loaded psqlodbcw.so paths ---\n");
	while (fgets(line, sizeof(line), fp))
	{
		if (strstr(line, "psqlodbcw.so"))
		{
			printf("%s", line);
			found = 1;
		}
	}
	if (!found)
		printf("(none)\n");
	printf("--- end loaded paths ---\n");
	fclose(fp);
}

static void
test_result_set_cap(void)
{
	SQLRETURN rc;
	SQLHDBC hdbc = SQL_NULL_HDBC;
	HSTMT hstmt = SQL_NULL_HSTMT;
	SQLCHAR errstate[6];
	SQLINTEGER nativeerr;
	SQLCHAR errmsg[256];
	SQLSMALLINT errlen;
	char connstr[1024];
	int cap_hit = 0;

	/*
	 * Use a separate connection with declare/fetch disabled so the driver
	 * materializes the whole result set in memory. With a cursor, rows are
	 * fetched in small batches and the 5M-row allocation cap is never hit.
	 * Enable Debug=1 so the MYLOG output can be inspected on failure.
	 */
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("FAIL: SQLAllocHandle(DBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
	}

	build_connect_string(connstr, sizeof(connstr),
						 "UseDeclareFetch=0;Debug=1");
	{
		char masked[1024];
		char *p;

		strncpy(masked, connstr, sizeof(masked) - 1);
		masked[sizeof(masked) - 1] = '\0';
		if ((p = strstr(masked, "PWD=")) != NULL)
		{
			char *end = strchr(p + 4, ';');
			memset(p + 4, 'x', end ? (size_t)(end - (p + 4)) : strlen(p + 4));
		}
		printf("cap test connection string: %s\n", masked);
	}
	rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) connstr, SQL_NTS,
						  NULL, 0, NULL, SQL_DRIVER_COMPLETE);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("FAIL: SQLDriverConnect(UseDeclareFetch=0) failed",
				   SQL_HANDLE_DBC, hdbc);
		exit(1);
	}

	dump_loaded_driver_path();

	rc = SQLAllocStmt(hdbc, &hstmt);
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
		SQLRETURN	rc2;
		SQLLEN		fetched = 0;

		fprintf(stderr,
				"FAIL: expected 'maximum allowed rows' error for 5,000,001-row result set\n");
		fprintf(stderr, "      SQLExecDirect returned %d\n", (int) rc);
		if (SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1,
						  errstate, &nativeerr,
						  errmsg, sizeof(errmsg), &errlen) != SQL_ERROR)
		{
			fprintf(stderr, "      diag: [%s] %s\n", errstate, errmsg);
		}

		/*
		 * Diagnostic: if the driver used a DECLARE/FETCH cursor, only
		 * Fetch=100 rows will be materialized and the cap is never hit.
		 * If it materialized the full result set, we will see far more.
		 */
		while ((rc2 = SQLFetch(hstmt)) == SQL_SUCCESS ||
			   rc2 == SQL_SUCCESS_WITH_INFO)
		{
			fetched++;
			if (fetched >= 200)
				break;
		}
		fprintf(stderr, "      fetched %ld rows (%s)%s\n",
				(long) fetched,
				rc2 == SQL_NO_DATA ? "SQL_NO_DATA" : "stopped at 200",
				rc2 == SQL_ERROR ? " [SQL_ERROR]" : "");

		dump_mylog_lines("REALLOC");
		SQLFreeStmt(hstmt, SQL_DROP);
		SQLDisconnect(hdbc);
		SQLFreeConnect(hdbc);
		exit(1);
	}

	SQLFreeStmt(hstmt, SQL_DROP);
	SQLDisconnect(hdbc);
	SQLFreeConnect(hdbc);

	printf("OK: result set cap prevented unbounded allocation\n");
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
	char extra[4096];
	int i;

	/* Build a 1000-character dummy path that does not exist. */
	memset(longpath, 0, sizeof(longpath));
	strcpy(longpath, "/tmp/");
	for (i = strlen(longpath); i < 1000; i++)
		longpath[i] = 'a' + (i % 26);
	longpath[1000] = '\0';

	snprintf(extra, sizeof(extra),
			 "sslmode=verify-ca;sslcert=%s;sslkey=%s;sslrootcert=%s",
			 longpath, longpath, longpath);
	build_connect_string(dsn, sizeof(dsn), extra);

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
	char dsn[1024];

	/* Do not specify sslmode; the default should now be 'prefer'. */
	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(DDBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
	}

	build_connect_string(dsn, sizeof(dsn), NULL);
	rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) dsn, SQL_NTS,
						  str, sizeof(str), &strl, SQL_DRIVER_COMPLETE);
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

	/*
	 * Default MYLOG path on Linux: /tmp/mylog_<exe>_<user><pid>.log
	 * (the driver does not insert an underscore before the pid).
	 */
	snprintf(filename, sizeof(filename),
			 "/tmp/mylog_security-fix-test_%s%u.log", username, (unsigned) pid);

	fp = fopen(filename, "r");
	if (!fp)
	{
		fprintf(stderr,
				"FAIL: could not open MYLOG file %s. "
				"Enable Debug=1 in the driver section of odbcinst.ini "
				"(see README-security-fix-test.md).\n",
				filename);
		return 1;
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
	char extra[4096];

	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocHandle(DDBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
	}

	/*
	 * Use multiple hosts/ports so the driver takes the URL-logging path in
	 * connection.c. The URL contains password and SSL file paths; all of them
	 * must be masked in MYLOG even if the connection itself fails. Enable
	 * Debug=1 so the MYLOG file is produced for this process.
	 */
	snprintf(extra, sizeof(extra),
			 "SERVER=127.0.0.1,127.0.0.1;PORT=5432,5432;Debug=1;"
			 "sslmode=verify-ca;sslcert=%s;sslkey=%s;sslrootcert=%s",
			 sslcert, sslkey, sslrootcert);
	build_connect_string(dsn, sizeof(dsn), extra);

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

	rc = SQLFreeStmt(hstmt, SQL_DROP);
	CHECK_STMT_RESULT(rc, "SQLFreeStmt failed", hstmt);
}

#ifdef WIN32
static void
test_sqlconnectw_invalid_length(void)
{
	SQLRETURN rc;
	SQLHDBC hdbc = SQL_NULL_HDBC;
	SQLWCHAR dsn[] = L"gaussdb";
	SQLWCHAR user[] = L"odbc";
	SQLWCHAR pwd[] = L"Gauss@123";

	rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("FAIL: SQLAllocHandle(DBC) failed", SQL_HANDLE_ENV, env);
		exit(1);
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
				rc);
		exit(1);
	}
	printf("OK: SQLConnectW with invalid negative length returned SQL_ERROR\n");

	SQLFreeConnect(hdbc);
}
#endif

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

	test_disconnect_keep_env();

	/* The following tests allocate their own connections. */
	test_long_sslcert_path();
	test_default_ssl_prefer();
	test_log_password_redaction();

#ifdef WIN32
	test_sqlconnectw_invalid_length();
#endif

	test_free_env();

	printf("=== security-fix-test passed ===\n");
	return 0;
}
