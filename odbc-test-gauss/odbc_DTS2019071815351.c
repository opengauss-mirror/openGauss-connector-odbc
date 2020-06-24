/*
 * 测试odbc的rollback，开源及早期版本，在create语句执行时，如果成功，事务就提交了，如果失败，事务就回滚了。
 * 这样会完全破坏用户的事务状态。虽然此问题在开源代码里好多年了，但是仍然是一个大问题。我们决定修改。
 * 如果此修改引起与开源行为不一致，那么在生态问题上，请使用开源驱动。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include "common.h"
int Exec(SQLHDBC hdbc, SQLCHAR* sql)
{
	SQLHSTMT        hStmt   = SQL_NULL_HSTMT;
	int 			rc = 0;
	/*****************************************************************/
	/* Allocate statement handle                                     */
	/*****************************************************************/
	rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt);
	if (!SQL_SUCCEEDED(rc))
	{
		printf("Failed to alloc hstmt\n");
		return rc;
	}	
	
	rc = SQLPrepare(hStmt, sql, SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag(sql, SQL_HANDLE_STMT, hStmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		return rc;
	}
	
	rc = SQLExecute(hStmt);
	if (!SQL_SUCCEEDED(rc))
	{
		print_diag(sql, SQL_HANDLE_STMT, hStmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		return rc;
	}
	
	
	/*****************************************************************/
	/* Free statement handle                                         */
	/*****************************************************************/
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	if (!SQL_SUCCEEDED(rc))       // An advertised API failed
	{
		printf("Failed to free hstmt.\n");
		return rc;
	}
}
int main( )
{
	SQLHENV         hEnv    = SQL_NULL_HENV;
	SQLHDBC         hDbc    = SQL_NULL_HDBC;
	SQLHSTMT        hStmt   = SQL_NULL_HSTMT;
	SQLINTEGER      RETCODE = SQL_SUCCESS;
	
	TIMESTAMP_STRUCT sTime ;
	SQLLEN cbID = 0;
	SQLCHAR result[200]={0};
		
	(void) printf ("**** Entering odbc_transaction_crashed.\n\n");
	/*****************************************************************/
	/* Allocate environment handle                                   */
	/*****************************************************************/
	RETCODE = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
	if (!SQL_SUCCEEDED(RETCODE))
		goto dberror;
	SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0); 
	
	/*****************************************************************/
	/* Allocate connection handle to DSN                             */
	/*****************************************************************/
	RETCODE = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
	if( !SQL_SUCCEEDED(RETCODE) )      // Could not get a Connect Handle
		goto dberror;

	/*****************************************************************/
	/* CONNECT TO data source (STLEC1)                               */
	/*****************************************************************/
	RETCODE = SQLConnect(hDbc,        // Connect handle
			                  (SQLCHAR *)"gaussdb", // DSN
			                  SQL_NTS,     // DSN is nul-terminated
			                  NULL,        // Null UID
			                  0   ,
			                  NULL,        // Null Auth string
			                  0);
		
	if( !SQL_SUCCEEDED(RETCODE))      // Connect failed
		goto dberror;
 
	RETCODE = Exec(hDbc,"set application_name = 'odbc_transaction_crashed';");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;
	
	RETCODE = Exec(hDbc,"set session_timeout = 0;");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	RETCODE = Exec(hDbc,"drop table IF EXISTS odbc_transaction_crashed;");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	/*-----------------------------------------------------------------------
	 *                     begin a transaction
	 *----------------------------------------------------------------------*/
	RETCODE = Exec(hDbc, "   \n\t\rbegin;");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;
	
	RETCODE = Exec(hDbc, "create table odbc_transaction_crashed(tm smalldatetime,name varchar(20));");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	RETCODE = Exec(hDbc,"insert into odbc_transaction_crashed values('2012-12-14 12:12','Joe');");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	RETCODE = Exec(hDbc,"insert into odbc_transaction_crashed values('2013-09-01 12:12:12','Mike');");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;


	RETCODE = Exec(hDbc,"insert into odbc_transaction_crashed values('2013-09-01 12:12:36','Mary');");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	/* This "rollback" should also rolls back the creation of table. */
	RETCODE = SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_ROLLBACK);
	if (!SQL_SUCCEEDED(RETCODE))
	{
		print_diag("SQLEndTran", SQL_HANDLE_STMT, hStmt);
		goto exit;
	}
	
	/*****************************************************************/
	/* Allocate statement handle                                     */
	/*****************************************************************/
	RETCODE = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;
	/* This selection should fail, as the definition of table has been rolled back. */
	RETCODE = SQLExecDirect(hStmt,"select tm,name from odbc_transaction_crashed order by tm;", SQL_NTS);
	
	if (SQL_SUCCEEDED(RETCODE))
	{
		print_result(hStmt);
		(void) printf ("Expected: SQL_ERROR, but get a %d. This operation should always fail, as the definition rolled back.\n", RETCODE);
		goto exit;
	}	

	/*****************************************************************/
	/* Free statement handle                                         */
	/*****************************************************************/
	RETCODE = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	if (!SQL_SUCCEEDED(RETCODE))       // An advertised API failed
		goto dberror;
	/*-----------------------------------------------------------------------
	 *                     start a transaction
	 *----------------------------------------------------------------------*/
	RETCODE = Exec(hDbc, "\r\n\t start\r\n\t transaction;");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;
	
	RETCODE = Exec(hDbc, "create table odbc_transaction_crashed(tm smalldatetime,name varchar(20));");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	RETCODE = Exec(hDbc,"insert into odbc_transaction_crashed values('2012-12-14 12:12','Joe');");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	RETCODE = Exec(hDbc,"insert into odbc_transaction_crashed values('2013-09-01 12:12:12','Mike');");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;


	RETCODE = Exec(hDbc,"insert into odbc_transaction_crashed values('2013-09-01 12:12:36','Mary');");
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;

	/* This "rollback" should also rolls back the creation of table. */
	RETCODE = SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_ROLLBACK);
	if (!SQL_SUCCEEDED(RETCODE))
	{
		print_diag("SQLEndTran", SQL_HANDLE_STMT, hStmt);
		goto exit;
	}
	
	/*****************************************************************/
	/* Allocate statement handle                                     */
	/*****************************************************************/
	RETCODE = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;
	/* This selection should fail, as the definition of table has been rolled back. */
	RETCODE = SQLExecDirect(hStmt,"select tm,name from odbc_transaction_crashed order by tm;", SQL_NTS);
	
	if (SQL_SUCCEEDED(RETCODE))
	{
		print_result(hStmt);
		(void) printf ("Expected: SQL_ERROR, but get a %d. This operation should always fail, as the definition rolled back.\n", RETCODE);
		goto exit;
	}	

	/*****************************************************************/
	/* Free statement handle                                         */
	/*****************************************************************/
	RETCODE = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	if (!SQL_SUCCEEDED(RETCODE))       // An advertised API failed
		goto dberror;
	/*****************************************************************/
	/* DISCONNECT from data source                                   */
	/*****************************************************************/
	RETCODE = SQLDisconnect(hDbc);
	if (!SQL_SUCCEEDED(RETCODE))
		goto dberror;
	/*****************************************************************/
	/* Deallocate connection handle                                  */
	/*****************************************************************/
	RETCODE = SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
	if (!SQL_SUCCEEDED(RETCODE))
		goto dberror;
	/*****************************************************************/
	/* Free environment handle                                       */
	/*****************************************************************/
	RETCODE = SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
	goto exit;

dberror:
	RETCODE=12;
exit:
	if (!(SQL_SUCCEEDED(RETCODE)))
		printf("Failed to test odbc_transaction_crashed\n");
	(void) printf ("**** Exiting  odbc_transaction_crashed.\n\n");
	return(RETCODE);
}
