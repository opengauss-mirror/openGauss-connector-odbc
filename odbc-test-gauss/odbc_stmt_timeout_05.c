/*
测试odbc支持语句执行超时连接设置
test: SQLSetStmtAttr()函数设定5秒超时,三条SQLExecDirect执行，第二个超时被终止，第一个未超时的语句执行生效,第三个执行语法错误语句，报错信息正确。
*/
#include <stdlib.h>
#include <stdio.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <time.h>
SQLHENV       V_OD_Env;    // Handle ODBC environment
SQLHENV       V_OD_hstmt;  // Handle statement
SQLHDBC       V_OD_hdbc;   // Handle connection
SQLRETURN     rc;
SQLLEN	      V_OD_rowanz;
SQLINTEGER    V_OD_err,V_OD_id,ID,IDLEN,typelen;
SQLSMALLINT   V_OD_mlen,V_OD_colanz,col_type,col_num;
long          V_OD_erg;
char          V_OD_stat[10];    // Status SQL
char          V_OD_msg[200],V_OD_buffer[200],type[200],name[200],
 	      typename[200],col_name[200], numattr[200];
int i;
char *buf = "zhuyunlong";
int value = 25;

int main(int argc,char *argv[])
{
	// 1. allocate Environment handle and register version 
	V_OD_erg = SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&V_OD_Env);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error AllocHandle\n");
		return -1;
	}
	SQLSetEnvAttr(V_OD_Env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0); 
	// 2. allocate connection handle, set timeout
	V_OD_erg = SQLAllocHandle(SQL_HANDLE_DBC, V_OD_Env, &V_OD_hdbc); 
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error AllocHDB %d\n",V_OD_erg);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		return -1;
	}
	//SET SQL_ATTR_AUTOCOMMIT OFF
	SQLSetConnectAttr(V_OD_hdbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, 0);
	V_OD_erg = SQLConnect(V_OD_hdbc, (SQLCHAR*) "gaussdb", SQL_NTS,
		 (SQLCHAR*) "", SQL_NTS,
		 (SQLCHAR*) "", SQL_NTS);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error SQLConnect %d\n",V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
		V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		return -1;
	}
	printf("Connected !\n");
	SQLBindCol(V_OD_hstmt,1,SQL_C_CHAR, (SQLPOINTER)&V_OD_buffer,150,(SQLLEN * )&V_OD_err);	

	V_OD_erg=SQLAllocHandle(SQL_HANDLE_STMT, V_OD_hdbc, &V_OD_hstmt);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Fehler im AllocStatement %d\n",V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		return -1;
	}
	// set the wairt time is 5 seconds.	
	SQLSetStmtAttr(V_OD_hstmt,SQL_ATTR_QUERY_TIMEOUT,(SQLPOINTER *)5,0);            //set the max timeout 5!
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"drop table IF EXISTS odbc_timeout_05_01;",SQL_NTS);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("drop error");
		SQLGetDiagRec(SQL_HANDLE_STMT, V_OD_hstmt,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		return -1;
	}
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"create table odbc_timeout_05_01(tm int);",SQL_NTS);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("create error");
		SQLGetDiagRec(SQL_HANDLE_STMT, V_OD_hstmt,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		return -1;
	}
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"insert into odbc_timeout_05_01 values(10);select pg_sleep(10);",SQL_NTS);         //create function  
    if (V_OD_erg != SQL_ERROR)
    {
        printf("Expected: SQL_ERROR with timeout, but not(%d).", V_OD_erg);
        exit(-1);
    }
    
    SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
	
	//query the insert whether success
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"select * from odbc_timeout_05_01(tm int);",SQL_NTS); //the synx erro statement
    if (V_OD_erg != SQL_ERROR)
    {
        printf("Expected: SQL_ERROR with syntax error, but not(%d).\n", V_OD_erg);
        exit(-1);
    }
    
    SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
	
	V_OD_erg=SQLRowCount(V_OD_hstmt,&V_OD_rowanz);
	if (V_OD_erg != SQL_ERROR)
    {
        printf("Expected: SQL_ERROR with previous error, but not(%d).", V_OD_erg);
        exit(-1);
    }
    
    SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);

	SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
	SQLDisconnect(V_OD_hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);

	printf("GO ON!\n");                                                             
	return(0);
}
