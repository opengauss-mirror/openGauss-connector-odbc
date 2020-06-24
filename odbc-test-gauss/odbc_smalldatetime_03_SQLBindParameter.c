/*
测试odbc支持smalldatetime类型
test: SQLBindParameter()函数 查询出来的结果跟数据库中的结果不一致！
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

int main( )
{
   SQLHENV         hEnv    = SQL_NULL_HENV;
   SQLHDBC         hDbc    = SQL_NULL_HDBC;
   SQLHSTMT        hStmt   = SQL_NULL_HSTMT;
   SQLRETURN       rc      = SQL_SUCCESS;
   SQLINTEGER      RETCODE = 0;
   
   TIMESTAMP_STRUCT *sTime = NULL;
   TIMESTAMP_STRUCT result;
   sTime = (TIMESTAMP_STRUCT *)malloc(sizeof(TIMESTAMP_STRUCT));
   if (NULL == sTime)
	goto exit;
	
   sTime->year = 2012;
   sTime->month = 12;
   sTime->day = 12;
   sTime->hour = 9;
   sTime->minute = 10;
   sTime->second = 31;
   
   SQLLEN cbID = 0;
   SQLINTEGER bID = 0;
   SQLINTEGER errInfo = 0;
 
   SQLSMALLINT errCb = 0;
   
   SQLCHAR time[200];
   SQLCHAR errMsg[200];
   
   (void) printf ("**** Entering CLIP06.\n\n");
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
	 
  /*****************************************************************/
  /* Allocate statement handle                                     */
  /*****************************************************************/
   rc = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

  /*****************************************************************/
  /* Retrieve SQL data types from DSN                              */
  /*****************************************************************/
   rc = SQLExecDirect(hStmt,"drop table IF EXISTS odbc_smalldatetime_03_SQLBindParamater",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
	goto exit;
	
   rc = SQLExecDirect(hStmt,"create table odbc_smalldatetime_03_SQLBindParamater(tm smalldatetime)",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

  SQLPrepare(hStmt,"insert into odbc_smalldatetime_03_SQLBindParamater values(?)",SQL_NTS);
   
  SQLBindParameter(hStmt,1,SQL_PARAM_INPUT,SQL_C_TYPE_TIMESTAMP,SQL_TYPE_TIMESTAMP,100,0,sTime,sizeof(TIMESTAMP_STRUCT),&cbID);
  rc = SQLExecute(hStmt);
  if ((!SQL_SUCCEEDED(rc)))
   goto exit;
 
  
   rc = SQLExecDirect(hStmt,"select tm from odbc_smalldatetime_03_SQLBindParamater",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
   rc = SQLBindCol(hStmt,1,SQL_C_TYPE_TIMESTAMP, (SQLPOINTER)&result,sizeof(result),&cbID);
   
   if (!SQL_SUCCEEDED(rc))
     goto exit;
   
   rc = SQLFetch(hStmt);  
   while(rc != SQL_NO_DATA)
   {
      sprintf(time, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", result.year, result.month, result.day,result.hour,result.minute,result.second);
	   printf("TIME = %s\n\n",time);
     rc = SQLFetch(hStmt);  
   };
	
   if (!SQL_SUCCEEDED(rc) && rc != SQL_NO_DATA)
     goto exit;
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
   if (!SQL_SUCCEEDED(RETCODE))
     goto exit;
   return 0;

     dberror:
     RETCODE=12;
     exit:
     (void) printf ("**** Exiting  CLIP06.\n\n");
     return(RETCODE);
}
