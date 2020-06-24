 /*
测试odbc支持smalldatetime类型
test: SQLGetData()函数,可以取出smalldatetime类型的数据
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
   
   TIMESTAMP_STRUCT sTime ;
   SQLLEN cbID = 0;
   SQLCHAR result[200]={0};
     
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
  /* Retrieve native SQL types from DSN 					       */
  /*****************************************************************/
   rc = SQLExecDirect(hStmt,"drop table IF EXISTS odbc_smalldatetime_05_SQLGetData",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
	goto exit;
	
   rc = SQLExecDirect(hStmt,"create table odbc_smalldatetime_05_SQLGetData(tm smalldatetime,name varchar(20))",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

	/* smalldatetime [0,255] */
   rc = SQLExecDirect(hStmt,"insert into odbc_smalldatetime_05_SQLGetData values('2012-12-14 12:12','Joe')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

   rc = SQLExecDirect(hStmt,"insert into odbc_smalldatetime_05_SQLGetData values('2013-09-01 12:12:12','Mike')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
   rc = SQLExecDirect(hStmt,"insert into odbc_smalldatetime_05_SQLGetData values('2013-09-01 12:12:36','Mary')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	  
   rc = SQLExecDirect(hStmt,"select tm,name from odbc_smalldatetime_05_SQLGetData order by tm;",SQL_NTS);
   
   if (!SQL_SUCCEEDED(rc))
     goto exit;
   
   rc = SQLFetch(hStmt);  
   while(rc != SQL_NO_DATA)
   {	 
	 /* test SQLGetData */
	 SQLGetData(hStmt,1,SQL_C_TYPE_TIMESTAMP,(SQLPOINTER)&sTime,sizeof(sTime),&cbID);
	 sprintf(result, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", sTime.year, sTime.month, sTime.day,sTime.hour,sTime.minute,sTime.second);
	 
	 printf("TIME = %s\n\n",result);
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
