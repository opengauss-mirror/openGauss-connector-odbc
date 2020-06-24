/*
测试odbc支持tinyint类型
test: SQLBindParameter()函数,且绑定的参数值超过范围[0,255]时，会自动截取为上下限的值
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
   
   SQLCHAR insertID1 = 125;
   SQLCHAR insertID2 = 0;
   SQLCHAR insertID3 = 255;
   SQLINTEGER insertID4 = -1;
   SQLINTEGER insertID5 = 256;

   SQLLEN cdInsertID1 = 0;
   SQLLEN cdInsertID2 = 0;
   SQLLEN cdInsertID3 = 0;
   SQLLEN cdInsertID4 = 0;
   SQLLEN cdInsertID5 = 0;
   
   SQLLEN cbID = 0;
   SQLINTEGER bID = 0;
   SQLINTEGER errInfo = 0;
 
   SQLSMALLINT errCb = 0;
   
   SQLCHAR errStat[200];
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
   rc = SQLExecDirect(hStmt,"drop table IF EXISTS odbc_tinyint_03_SQLBindParamater",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
	goto exit;
	
   rc = SQLExecDirect(hStmt,"create table odbc_tinyint_03_SQLBindParamater(id tinyint)",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

  SQLPrepare(hStmt,"insert into odbc_tinyint_03_SQLBindParamater values(?)",SQL_NTS);
   
  SQLBindParameter(hStmt,1,SQL_PARAM_INPUT,SQL_C_UTINYINT,SQL_TINYINT,0,0,&insertID1,sizeof(insertID1),&cdInsertID1);
  rc = SQLExecute(hStmt);
  if ((!SQL_SUCCEEDED(rc)))
   goto exit;
  
  SQLBindParameter(hStmt,1,SQL_PARAM_INPUT,SQL_C_UTINYINT,SQL_TINYINT,0,0,&insertID2,sizeof(insertID2),&cdInsertID2);
  rc = SQLExecute(hStmt);
  if ((!SQL_SUCCEEDED(rc)))
   goto exit;
  
  SQLBindParameter(hStmt,1,SQL_PARAM_INPUT,SQL_C_UTINYINT,SQL_TINYINT,0,0,&insertID3,sizeof(insertID3),&cdInsertID3);
  rc = SQLExecute(hStmt);
  if ((!SQL_SUCCEEDED(rc)))
   goto exit;
  
  /*-1 will be trucated*/
  SQLBindParameter(hStmt,1,SQL_PARAM_INPUT,SQL_C_UTINYINT,SQL_TINYINT,0,0,&insertID4,sizeof(insertID4),&cdInsertID4);
  rc = SQLExecute(hStmt);
  if ((!SQL_SUCCEEDED(rc)))
   goto exit;
   
   /*256 will be trucated*/
  SQLBindParameter(hStmt,1,SQL_PARAM_INPUT,SQL_C_UTINYINT,SQL_TINYINT,0,0,&insertID5,sizeof(insertID5),&cdInsertID5);
  rc = SQLExecute(hStmt);
  if ((!SQL_SUCCEEDED(rc)))
   goto exit;
  
   rc = SQLExecDirect(hStmt,"select id from odbc_tinyint_03_SQLBindParamater order by id;",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
   rc = SQLBindCol(hStmt,1,SQL_C_UTINYINT, (SQLPOINTER)&bID,0,&cbID);
   
   if (!SQL_SUCCEEDED(rc))
     goto exit;
   
   rc = SQLFetch(hStmt);  
   while(rc != SQL_NO_DATA)
   {
     printf("ID = %d\n",bID);
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
