/*
测试odbc支持nvarchar2类型
test: SQLPrimaryKeys()函数,字段类型为nvarchar2的列作为主键的时候，能查询出来
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
   SQLCHAR schema[200] = "PUBLIC";
   SQLCHAR table[200] = "ODBC_NVARCHAR2_04";   
   SQLCHAR typename[200] = {0};
   SQLLEN cbtypename = 0;

    
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
     {
	    printf("connetct failed!");
	    goto dberror;
     }
  /*****************************************************************/
  /* Allocate statement handle                                     */
  /*****************************************************************/
   rc = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
	
  /*****************************************************************/
  /*                                                               */
  /* Retrieve native SQL types from DSN ------------>              */
  /*                                                               */
  /*  The result set consists of 15 columns. We only bind          */
  /*  serveral we modified for tinyint. Note: Need   			   */
  /*  not bind all columns of result set -- only those required.   */
  /*                                                               */
  /*****************************************************************/
   rc = SQLExecDirect(hStmt,"drop table IF EXISTS  odbc_nvarchar2_04",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     {
     	goto exit;
     }
   rc = SQLExecDirect(hStmt,"create table odbc_nvarchar2_04(name nvarchar2(20) PRIMARY KEY) ",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))  
     {
     	goto exit;
     }
   rc = SQLExecDirect(hStmt,"insert into odbc_nvarchar2_04 values('nimei')",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))
     {
     	goto exit;
     } 		
   rc = SQLPrimaryKeys(hStmt, NULL, 0, schema, sizeof(schema),table,sizeof(table));
   if (!SQL_SUCCEEDED(rc))
    {
    	printf("SQLPrimaryKeys error!\n");
		goto exit;  
	}
   rc = SQLBindCol(hStmt,4,SQL_C_CHAR, (SQLPOINTER)typename,sizeof(typename),&cbtypename);
   rc = SQLFetch(hStmt);  
   while(rc != SQL_NO_DATA)
   {   
    printf("The primary key column is        ----: %s\n",typename);

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
