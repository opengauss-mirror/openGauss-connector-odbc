/*
测试odbc支持tinyint类型
test: SQLBindCol()函数,可成功绑定数据类型为tinyint的列 
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

 /*global*/
 SQLLEN cbSID = 0;
 
 SQLINTEGER sID = 0;
 SQLINTEGER errInfo = 0;
 
 SQLSMALLINT errCb = 0;
 
 SQLCHAR errStat[200];
 SQLCHAR errMsg[200];
   
int main( )
{
   SQLHENV         hEnv    = SQL_NULL_HENV;
   SQLHDBC         hDbc    = SQL_NULL_HDBC;
   SQLHSTMT        hStmt   = SQL_NULL_HSTMT;
   SQLRETURN       rc      = SQL_SUCCESS;
   SQLINTEGER      RETCODE = 0;
   
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
  /* Retrieve native SQL types from DSN            				   */
  /*****************************************************************/
   rc = SQLExecDirect(hStmt,"drop table  IF EXISTS odbc_tinyint_01_SQLBindCol",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
	goto exit;
	 
   rc = SQLExecDirect(hStmt,"create table odbc_tinyint_01_SQLBindCol(id tinyint,name varchar(20))",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	
	/* tinyint [0,255] */
   rc = SQLExecDirect(hStmt,"insert into odbc_tinyint_01_SQLBindCol values(1,'Joe')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
   rc = SQLExecDirect(hStmt,"insert into odbc_tinyint_01_SQLBindCol values(0,'Mike')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
   rc = SQLExecDirect(hStmt,"insert into odbc_tinyint_01_SQLBindCol values(255,'Mary')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

   rc = SQLExecDirect(hStmt,"insert into odbc_tinyint_01_SQLBindCol values(-1,'Mary')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc) )
   {
	   SQLGetDiagRec(SQL_HANDLE_STMT,hStmt,1, errStat,&errInfo,errMsg,100,&errCb);
       printf("%s \n",errMsg);
   }
	 
   rc = SQLExecDirect(hStmt,"insert into odbc_tinyint_01_SQLBindCol values(256,'Mary')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
   {
	   SQLGetDiagRec(SQL_HANDLE_STMT, hStmt,1, errStat,&errInfo,errMsg,100,&errCb);
       printf("%s \n",errMsg);
   }
	 
   rc = SQLExecDirect(hStmt,"select id,name from odbc_tinyint_01_SQLBindCol order by 1",SQL_NTS);
   
   if (!SQL_SUCCEEDED(rc))
     goto exit;
   
   //test SQLBindCol
   rc = SQLBindCol(hStmt,1,SQL_C_UTINYINT, (SQLPOINTER)&sID,0,&cbSID);
   
   if (!SQL_SUCCEEDED(rc))
     goto exit;
   
   rc = SQLFetch(hStmt);  
   while(rc != SQL_NO_DATA)
   {
     printf("ID = %d\n",sID);
     rc = SQLFetch(hStmt);  
   };
	
   if (!SQL_SUCCEEDED(rc) && rc != SQL_NO_DATA)
     goto exit;
  /*****************************************************************/
  /* Free statement handle                                         */
  /*****************************************************************/
   rc = SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
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
