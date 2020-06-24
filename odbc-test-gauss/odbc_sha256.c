/*
测试sha256链接
test: 远程链接配置里面默认配的都是sha256方式
*/
#include <stdio.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <time.h>
SQLHENV       V_OD_Env;    // Handle ODBC environment
SQLHENV       V_OD_hstmt; 
SQLRETURN       rc;
long       V_OD_erg,V_OD_erg1;    // result of functions
SQLHDBC       V_OD_hdbc;                      // Handle connection
char       V_OD_stat[10];    // Status SQL
SQLINTEGER     V_OD_err,V_OD_rowanz1,V_OD_id,V_ID;
SQLLEN	V_OD_rowanz;
SQLSMALLINT     V_OD_mlen,V_OD_mlen1,V_OD_colanz,V_OD_colanz1;
char             V_OD_msg[200],V_OD_buffer[200],schema[200],table[200],type[200],remark[200],V_OD_buffer1[200];

SQLINTEGER maxlv,minlv;
SQLINTEGER m_min,m_max;

char *buf = "Mike";
char *buf1 = "haha";
int value = 3;

int main(int argc,char *argv[])
{

  /**
	ODBC handle
	1) SQL_HANDLE_ENV 
	2) SQL_HANDLE_DBC
	3) SQL_HANDLE_STMT
  */
	
  // 1. allocate Environment handle and register version 
  V_OD_erg = SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&V_OD_Env);
  if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error AllocHandle\n");
    return -1;
  }
  V_OD_erg = SQLSetEnvAttr(V_OD_Env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0); 
  if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error SetEnv\n");
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }
  
  // 2. allocate connection handle, set timeout
  V_OD_erg = SQLAllocHandle(SQL_HANDLE_DBC, V_OD_Env, &V_OD_hdbc); 
  if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error AllocHDB %d\n",V_OD_erg);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }
  
  // 3. Connect to the datasource "web" 
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
  printf("Connected successfuly!\n");
  
  //4. allocate statement handle.
  V_OD_erg=SQLAllocHandle(SQL_HANDLE_STMT, V_OD_hdbc, &V_OD_hstmt);
  if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Fehler im AllocStatement %d\n",V_OD_erg);
    SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }
  
	//SQLExecDirect
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"drop table IF EXISTS odbc_sha256",SQL_NTS);  
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
    {
       printf("Error in drop %d\n",V_OD_erg);
       SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
       printf("%s (%d)\n",V_OD_msg,V_OD_err);
       SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
       SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
       SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
       return -1;
    }
	
	
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"create table odbc_sha256(result varchar(20))",SQL_NTS);  
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
    {
       printf("Error in create %d\n",V_OD_erg);
       SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
       printf("%s (%d)\n",V_OD_msg,V_OD_err);
       SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
       SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
       SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
       return -1;
    }
		
	SQLExecDirect(V_OD_hstmt,"insert into odbc_sha256 values('connected success')",SQL_NTS);	

	V_OD_erg = SQLExecDirect(V_OD_hstmt,"select *  from odbc_sha256",SQL_NTS);  
	
    if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
    {
       printf("Error in Select %d\n",V_OD_erg);
       SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
       printf("%s (%d)\n",V_OD_msg,V_OD_err);
       SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
       SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
       SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
       return -1;
    }
	
	//SQLNumResultCols返回结果集中的列数
    V_OD_erg=SQLNumResultCols(V_OD_hstmt,&V_OD_colanz);
    if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
    {
        SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
        SQLDisconnect(V_OD_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
       return -1;
    }
    printf("SQLNumResultCols ---- Number of Columns %d\n",V_OD_colanz);
	
	//SQLRowCount 返回结果集中的行数
    V_OD_erg=SQLRowCount(V_OD_hstmt,&V_OD_rowanz);
    if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
    {
      printf("Number of RowCount %d\n",V_OD_erg);
      SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
      SQLDisconnect(V_OD_hdbc);
      SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
      SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
      return -1;
    }
    printf("SQLRowCount ----- Number of Rows %d\n",V_OD_rowanz);
	
    SQLBindCol(V_OD_hstmt,1,SQL_C_CHAR, &V_OD_buffer,150,&V_OD_rowanz);	
	
	//SQLFetch
    V_OD_erg=SQLFetch(V_OD_hstmt);  
    while(V_OD_erg != SQL_NO_DATA)
    {
     printf("SQLBindCol ----- Result: %s\n",V_OD_buffer);
     V_OD_erg=SQLFetch(V_OD_hstmt);  
    };

	
    SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
    SQLDisconnect(V_OD_hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return(0);
}

