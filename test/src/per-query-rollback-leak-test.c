/*
 * Bug verification: PER_QUERY_ROLLBACK memory leak in CC_internal_rollback.
 *
 * When rollback_on_error=2 (statement rollback) is active, each failed
 * statement triggers CC_internal_rollback(PER_QUERY_ROLLBACK), which sends
 * "ROLLBACK TO _per_query_svp_; RELEASE _per_query_svp_".
 *
 * The while loop in CC_internal_rollback for PER_QUERY_ROLLBACK did NOT call
 * PQclear(pgres) inside the loop body.  For a two-statement SQL (ROLLBACK TO
 * + RELEASE), PQgetResult is called twice; the first PGresult pointer was
 * overwritten by the second without being freed, leaking one PGresult per
 * failed statement.
 *
 * This test hammers the error path 500 times.  When run under Valgrind with
 * the unfixed driver, Valgrind reports "definitely lost" blocks proportional
 * to the iteration count.  With the fix, there should be 0 bytes lost.
 *
 * Pass criterion: no crash, all statements complete, [No crash - PASS] printed.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"

int main(int argc, char **argv)
{
	SQLRETURN	rc;
	SQLHSTMT	hstmt = SQL_NULL_HSTMT;
	int		i;
	int		iterations = 500;

	/* Connect with Protocol=7.4-2 to force statement-level rollback (rollback_on_error=2) */
	test_connect_ext("Protocol=7.4-2");

	rc = SQLAllocStmt(conn, &hstmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLAllocStmt failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Disable autocommit so per-query savepoints are used */
	rc = SQLSetConnectAttr(conn, SQL_ATTR_AUTOCOMMIT,
						   (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLSetConnectAttr(AUTOCOMMIT_OFF) failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/* Create a temporary table inside the transaction */
	rc = SQLExecDirect(hstmt, (SQLCHAR *) "CREATE TEMP TABLE leak_test (id int PRIMARY KEY)", SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("CREATE TABLE failed", SQL_HANDLE_STMT, hstmt);
		exit(1);
	}
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("SQLEndTran (commit) failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	/*
	 * Hammer the error path.  Each iteration:
	 *   1. Insert a valid row (succeeds)
	 *   2. Insert a duplicate row (fails → triggers PER_QUERY_ROLLBACK)
	 *   3. Insert the valid row again (succeeds; demonstrates the statement
	 *      was rolled back cleanly)
	 * Without the fix, each iteration leaks one PGresult from the rollback
	 * loop.
	 */
	for (i = 0; i < iterations; i++)
	{
		SQLCHAR sql[256];

		/* 1. Insert a valid row */
		snprintf((char *) sql, sizeof(sql),
				 "INSERT INTO leak_test VALUES (%d)", i);
		rc = SQLExecDirect(hstmt, sql, SQL_NTS);
		if (!SQL_SUCCEEDED(rc))
		{
			printf("FAIL: first insert %d failed unexpectedly\n", i);
			print_diag("INSERT 1 failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}

		/* 2. Duplicate insert → expected failure */
		rc = SQLExecDirect(hstmt, sql, SQL_NTS);
		if (SQL_SUCCEEDED(rc))
		{
			printf("FAIL: duplicate insert %d should have failed\n", i);
			exit(1);
		}
		/* Clear the error state from the statement handle */
		SQLFreeStmt(hstmt, SQL_CLOSE);

		/* 3. Insert a different row to prove rollback succeeded */
		snprintf((char *) sql, sizeof(sql),
				 "INSERT INTO leak_test VALUES (%d)", iterations + i);
		rc = SQLExecDirect(hstmt, sql, SQL_NTS);
		if (!SQL_SUCCEEDED(rc))
		{
			printf("FAIL: recovery insert %d failed after rollback\n", i);
			print_diag("INSERT recovery failed", SQL_HANDLE_STMT, hstmt);
			exit(1);
		}
	}

	/* Commit everything */
	rc = SQLEndTran(SQL_HANDLE_DBC, conn, SQL_COMMIT);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag("Final SQLEndTran failed", SQL_HANDLE_DBC, conn);
		exit(1);
	}

	SQLFreeStmt(hstmt, SQL_DROP);
	test_disconnect();

	printf("Completed %d iterations of error+rollback without crash.\n", iterations);
	printf("[No crash - PASS]\n");
	return 0;
}
