 /*
test: SQLExtendedFetch()º¯Êý
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
   
   SQLCHAR sID = 0;
   SQLLEN cbID = 0;
     
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
   rc = SQLExecDirect(hStmt,"drop table IF EXISTS FVT_INTERFACE.SQLExtendedFetch",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
	goto exit;
	
   rc = SQLExecDirect(hStmt,"create table FVT_INTERFACE.SQLExtendedFetch(id tinyint,name varchar(20))",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

	/* tinyint [0,255] */
   rc = SQLExecDirect(hStmt,"insert into FVT_INTERFACE.SQLExtendedFetch values(1,'Joe')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;

   rc = SQLExecDirect(hStmt,"insert into FVT_INTERFACE.SQLExtendedFetch values(0,'Mike')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
   rc = SQLExecDirect(hStmt,"insert into FVT_INTERFACE.SQLExtendedFetch values(255,'Mary')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	  
   rc = SQLExecDirect(hStmt,"select id,name from FVT_INTERFACE.SQLExtendedFetch order by id",SQL_NTS);
   
   if (!SQL_SUCCEEDED(rc))
     goto exit;
   SQLULEN pcrow;
   SQLUSMALLINT    Row_Stat;
   
   while((rc=SQLExtendedFetch(hStmt, SQL_FETCH_NEXT, 0, &pcrow, &Row_Stat)) == SQL_SUCCESS)
   {	 
	 /* test SQLGetData */
	 SQLGetData(hStmt,1,SQL_C_UTINYINT,(SQLPOINTER)&sID,0,&cbID);
	 printf("ID = %d\n\n",sID);
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
