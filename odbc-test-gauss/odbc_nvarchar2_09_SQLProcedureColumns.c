/*
测试odbc支持nvarchar2类型
test: SQLProcedureColumns()函数,测试通过odbc取出存储过程入参FIRSTNAME为nvarchar2的列
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
   char            *pDSN = "STLEC1";
   char            procedure_name [20];
   char            parameter_name [20];
   char            ptype          [20];
   SQLSMALLINT     parameter_type = 0;
   SQLSMALLINT     data_type = 0;
   char            type_name      [20];
   SWORD           cbCursor;
   SQLLEN          cbValue3;
   SQLLEN          cbValue4;
   SQLLEN          cbValue5;
   SQLLEN          cbValue6;
   SQLLEN          cbValue7; 
   char            ProcCatalog [20] = {0};
   char            ProcSchema  [20] = {"PUBLIC"};
   char            ProcName    [20] = {"ODBC_NVARCHAR2_09"};
   char            ColumnName  [20] = {"*NAME"};
   SQLSMALLINT     cbProcCatalog = 0;
   SQLSMALLINT     cbProcSchema  = 0;
   SQLSMALLINT     cbProcName    = strlen(ProcName);
   SQLSMALLINT     cbColumnName  = strlen(ColumnName);
   
   (void) printf ("**** Entering CLIP06.\n\n");
  /*****************************************************************/
  /* Allocate environment handle                                   */
  /*****************************************************************/
   RETCODE = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
   if (!SQL_SUCCEEDED(RETCODE))
     {
	    printf("SQLAllocHandle ENV error!\n");	
	    goto dberror;
	 }
   SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0); 
   
  /*****************************************************************/
  /* Allocate connection handle to DSN                             */
  /*****************************************************************/
   RETCODE = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
   if( !SQL_SUCCEEDED(RETCODE) )      // Could not get a Connect Handle
     {
	    printf("SQLAllocHandle dbc error!\n");	
	    goto dberror;
	 }
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
     {
	    printf("SQLAllocHandle error!\n");	
	    goto exit;
	 }
	 
  /*****************************************************************/
  /*                                                               */
  /* Retrieve native SQL types from DSN ------------>              */
  /*                                                               */
  /*  The result set consists of 15 columns. We only bind          */
  /*  serveral we modified for tinyint. Note: Need   			   */
  /*  not bind all columns of result set -- only those required.   */
  /*                                                               */
  /*****************************************************************/
   rc = SQLExecDirect(hStmt," create or replace function odbc_nvarchar2_09(in firstname nvarchar2(20)) returns nvarchar2(20) as $$ BEGIN return substring(firstname from '^....');END;$$ LANGUAGE plpgsql;",SQL_NTS);
   if (!SQL_SUCCEEDED(rc))  
     {
    	printf("create function error!\n");	 
     	goto exit;
     }
	 
   rc = SQLProcedureColumns(hStmt,
                            (SQLCHAR *) NULL,
                            0          ,
                            (SQLCHAR *) ProcSchema ,
                            cbProcSchema           ,
                            (SQLCHAR *) ProcName   ,
                            cbProcName             ,
                            (SQLCHAR *) NULL ,
                            sizeof(ColumnName));


   if (!SQL_SUCCEEDED(rc))
    {
    	printf("SQLProcedureColumns error!\n");	
		goto exit;  
	}
  rc = SQLBindCol (hStmt,           // bind procedure_name
                   3,
                   SQL_C_CHAR,
                   procedure_name,
                   sizeof(procedure_name),
                   &cbValue3);
  if (!SQL_SUCCEEDED(rc))
  {
    (void) printf ("**** Bind of procedure_name Failed.\n");
    goto dberror;
  }
  rc = SQLBindCol (hStmt,           // bind parameter_name
                   4,
                   SQL_C_CHAR,
                   parameter_name,
                   sizeof(parameter_name),
                   &cbValue4);
  if (!SQL_SUCCEEDED(rc))
  {
    (void) printf ("**** Bind of parameter_name Failed.\n");
    goto dberror;
  }
  rc = SQLBindCol (hStmt,           // bind parameter_type
                   5,
                   SQL_C_SHORT,
                   &parameter_type,
                   0,
                   &cbValue5);
  if (!SQL_SUCCEEDED(rc))
  {
    (void) printf ("**** Bind of parameter_type Failed.\n");
    goto dberror;
  }
  rc = SQLBindCol (hStmt,           // bind SQL data type
                   6,
                   SQL_C_SHORT,
                   &data_type,
                   0,
                   &cbValue6);
  if (!SQL_SUCCEEDED(rc))
  {
    (void) printf ("**** Bind of data_type Failed.\n");
    goto dberror;
  }
  rc = SQLBindCol (hStmt,           // bind type_name
                   7,
                   SQL_C_CHAR,
                   type_name,
                   sizeof(type_name),
                   &cbValue7);
  if (!SQL_SUCCEEDED(rc))
  {
    (void) printf ("**** Bind of type_name Failed.\n");
    goto dberror;
  }
  /*****************************************************************/
  /* Answer set is available - Fetch rows and print parameters for */
  /* all procedures.                                               */
  /*****************************************************************/
  printf ("**** begin fetch  variable.\n");

  while (SQL_SUCCEEDED(rc = SQLFetch (hStmt)))
  {
    (void) printf ("**** Procedure Name = %s\n Parameter %s",
                   procedure_name,
                   parameter_name);
    switch (parameter_type)
    {
      case SQL_PARAM_INPUT        :
        (void) strcpy (ptype, "INPUT");
        break;
      case SQL_PARAM_OUTPUT       :
        (void) strcpy (ptype, "OUTPUT");
        break;
      case SQL_PARAM_INPUT_OUTPUT :
        (void) strcpy (ptype, "INPUT/OUTPUT");
        break;
      default                     :
        (void) strcpy (ptype, "UNKNOWN");
        break;
    }
    (void) printf (" is %s. Data Type is %d\n Type Name is %s.\n",
                   ptype     ,
                   data_type ,
                   type_name);
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
   if (!SQL_SUCCEEDED(RETCODE))
     goto exit;
   
   RETCODE = 0;
   goto exit;
dberror:
     RETCODE=12;
exit:
     (void) printf ("**** Exiting  CLIP06.\n\n");
     return(RETCODE);
}
