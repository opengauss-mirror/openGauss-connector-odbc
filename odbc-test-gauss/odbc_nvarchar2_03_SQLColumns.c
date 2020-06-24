/*
测试odbc支持nvarchar2类型
test: SQLColumns()函数
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

int main( )
{
	SQLHENV			hEnv	 = SQL_NULL_HENV;
	SQLHDBC			hDbc	 = SQL_NULL_HDBC;
	SQLHSTMT			hStmt	= SQL_NULL_HSTMT;
	SQLRETURN		 rc		= SQL_SUCCESS;
	SQLINTEGER		RETCODE = 0;
	
	SQLCHAR typename[200];
	SQLCHAR schema[200] = "PUBLIC";
	SQLCHAR table[200] = "ODBC_NVARCHAR2_03";
	SQLCHAR col[200] = "NAME";
	 
	SQLSMALLINT dateType = 0;
	SQLSMALLINT sqlDateType = 0;
	
	SQLINTEGER buffLen = 0;
	SQLINTEGER columnSize = 0;
	SQLINTEGER octLen = 0;
	
	SQLLEN cbDateType = 0;
	SQLLEN cbSqlDateType = 0;
	SQLLEN cbBuffLen = 0;
	SQLLEN cbTypeName = 0;
	SQLLEN cbColSize = 0;
	SQLLEN cbOctLen = 0;
	 
	(void) printf ("**** Entering CLIP06.\n\n");
	/*****************************************************************/
	/* Allocate environment handle												*/
	/*****************************************************************/
	RETCODE = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
	if (!SQL_SUCCEEDED(RETCODE))
		goto dberror;
	SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0); 
	
	/*****************************************************************/
	/* Allocate connection handle to DSN										*/
	/*****************************************************************/
	RETCODE = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
	if( !SQL_SUCCEEDED(RETCODE) )		// Could not get a Connect Handle
		goto dberror;

	/*****************************************************************/
	/* CONNECT TO data source (STLEC1)										 */
	/*****************************************************************/
	RETCODE = SQLConnect(hDbc,			// Connect handle
								(SQLCHAR *)"gaussdb", // DSN
								SQL_NTS,		// DSN is nul-terminated
								NULL,			// Null UID
								0	,
								NULL,			// Null Auth string
								0);
	 
	if( !SQL_SUCCEEDED(RETCODE))		// Connect failed
		goto dberror;
 
	/*****************************************************************/
	/* Allocate statement handle												 */
	/*****************************************************************/
	rc = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	 
	
	/****************************************************************/
	/*                                                              */
	/* Retrieve native SQL types from DSN ------------>             */
	/*	The result set consists of 15 columns. We only bind         */
	/*	serveral we modified for tinyint. Note: Need                */
	/*	not bind all columns of result set -- only those required.  */
	/*****************************************************************/
	
	rc = SQLExecDirect(hStmt,"drop table IF EXISTS	odbc_nvarchar2_03",SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	 
	rc = SQLExecDirect(hStmt,"create table odbc_nvarchar2_03(name nvarchar2(20))",SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
		goto exit;

	rc = SQLExecDirect(hStmt,"insert into odbc_nvarchar2_03 values('nimei')",SQL_NTS);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	 
	rc = SQLColumns(hStmt, NULL, 0, schema, sizeof(schema),table, sizeof(table),col, sizeof(col));
	if (!SQL_SUCCEEDED(rc))
	{
		 char			 V_OD_stat[10];		// Status SQL
		SQLINTEGER		 V_OD_rowanz1,V_OD_id,V_ID,V_OD_err;
		char			 V_OD_msg[200];
		SQLSMALLINT		 V_OD_mlen;
		SQLGetDiagRec(SQL_HANDLE_STMT, hStmt,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
	 	printf("SQLColumns error!\n");
		goto exit;	
	}
	printf("bengin sqlbindcol:");
	rc = SQLBindCol(hStmt,5,SQL_C_SSHORT, (SQLPOINTER)&dateType,0,&cbDateType);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	rc = SQLBindCol(hStmt,6,SQL_C_CHAR, (SQLPOINTER)typename,sizeof(typename),&cbTypeName);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	rc = SQLBindCol(hStmt,7,SQL_C_ULONG, (SQLPOINTER)&columnSize,0,&cbColSize);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	rc = SQLBindCol(hStmt,8,SQL_C_ULONG, (SQLPOINTER)&buffLen,0,&cbBuffLen);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	rc = SQLBindCol(hStmt,14,SQL_C_SSHORT, (SQLPOINTER)&sqlDateType,0,&cbSqlDateType);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	rc = SQLBindCol(hStmt,16,SQL_C_ULONG, (SQLPOINTER)&octLen,0,&cbOctLen);
	if (!SQL_SUCCEEDED(rc))
		goto exit;
	printf("bengin sqlFetch:\n");
	rc = SQLFetch(hStmt);	
	while(rc != SQL_NO_DATA)
	{	
		printf("DATA_TYPE--------------------------: %d\n",dateType);
		printf("TYPE_NAME--------------------------: %s\n",typename);
		printf("COLUMN_SIZE(in characters)---------: %d\n",columnSize);
		printf("SQL_DATA_TYPE----------------------: %d\n",sqlDateType);
		printf("BUFFER_LENGTH(in bytes)------------: %d\n",buffLen);
		printf("CHAR_OCTET_LENGTH(in bytes)--------: %d\n",buffLen);
		rc = SQLFetch(hStmt);	
	};
	
	if (!SQL_SUCCEEDED(rc) && rc != SQL_NO_DATA)
		goto exit;
	 
	/*****************************************************************/
	/* Free statement handle														*/
	/*****************************************************************/
	rc = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	if (!SQL_SUCCEEDED(RETCODE))		 // An advertised API failed
		goto dberror;
	 
	/*****************************************************************/
	/* DISCONNECT from data source												*/
	/*****************************************************************/
	RETCODE = SQLDisconnect(hDbc);
	if (!SQL_SUCCEEDED(RETCODE))
		goto dberror;
	 
	/*****************************************************************/
	/* Deallocate connection handle											 */
	/*****************************************************************/
	RETCODE = SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
	if (!SQL_SUCCEEDED(RETCODE))
		goto dberror;
	 
	/*****************************************************************/
	/* Free environment handle													*/
	/*****************************************************************/
	RETCODE = SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
	if (!SQL_SUCCEEDED(RETCODE))
		goto exit;
	return 0;
dberror:
		RETCODE=12;
exit:
		(void) printf ("**** Exiting	CLIP06.\n\n");
		return(RETCODE);
}
