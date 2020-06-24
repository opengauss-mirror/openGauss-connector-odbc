 /*
测试标识符大小的适配
test: SQLGetTypeInfo()函数,获取int数据类型的详细属性其typename应该显示为大写的INT4
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

 SQLCHAR typeName[200],prefix[200],suffix[200],localName[200];
   
 SQLINTEGER dataType = 0;
 SQLINTEGER columnSize = 0;
 SQLINTEGER minScale = 0;
 SQLINTEGER maxScale = 0;
 SQLINTEGER radix = 0;
  
 SQLLEN cbTypeName = 0;
 SQLLEN cbDataType = 0;
 SQLLEN cbColumnSize = 0;
 SQLLEN cbPrefix = 0;
 SQLLEN cbSuffix = 0;
 SQLLEN cbMinScale = 0;
 SQLLEN cbMaxScale = 0;
 SQLLEN cbRadix = 0;
 SQLLEN cbLocalName = 0;
 
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
	 
   if( !SQL_SUCCEEDED(RETCODE) && !SQL_SUCCEEDED(RETCODE))      // Connect failed
     goto dberror;
  /*****************************************************************/
  /* Retrieve SQL data types from DSN                              */
  /*****************************************************************/
  // local variables to Bind to retrieve TYPE_NAME, DATA_TYPE,
  // COLUMN_SIZE and NULLABLE
 
  /*****************************************************************/
  /* Allocate statement handle                                     */
  /*****************************************************************/
   rc = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
	 
	
  /*****************************************************************/
  /* Retrieve native SQL types from DSN          				   */
  /*****************************************************************/
   rc = SQLGetTypeInfo (hStmt,SQL_INTEGER);
   if (!SQL_SUCCEEDED(rc))
     goto exit;
   
   rc = SQLBindCol(hStmt,1,SQL_C_CHAR, (SQLPOINTER)typeName,sizeof(typeName),&cbTypeName);
   rc = SQLBindCol(hStmt,2,SQL_C_ULONG, (SQLPOINTER)&dataType,0,&cbDataType);
   rc = SQLBindCol(hStmt,3,SQL_C_ULONG, (SQLPOINTER)&columnSize,0,&cbColumnSize);
   rc = SQLBindCol(hStmt,4,SQL_C_CHAR, (SQLPOINTER)prefix,sizeof(prefix),&cbPrefix);
   rc = SQLBindCol(hStmt,5,SQL_C_CHAR, (SQLPOINTER)suffix,sizeof(suffix),&cbSuffix);
   rc = SQLBindCol(hStmt,13,SQL_C_CHAR, (SQLPOINTER)localName,sizeof(localName),&cbLocalName);
   rc = SQLBindCol(hStmt,14,SQL_C_SSHORT, (SQLPOINTER)&minScale,0,&cbMinScale);
   rc = SQLBindCol(hStmt,15,SQL_C_SSHORT, (SQLPOINTER)&maxScale,0,&cbMaxScale);
   rc = SQLBindCol(hStmt,18,SQL_C_ULONG, (SQLPOINTER)&radix,sizeof(radix),&cbRadix);
   

   if (!SQL_SUCCEEDED(rc))
     goto exit;
   
   rc = SQLFetch(hStmt);  
   while(rc != SQL_NO_DATA)
   {
	 printf("TYPENAME              = %s \n",typeName);
	 printf("DATA_TYPE             = %d SQL_TINYINT=%d \n",dataType,SQL_TINYINT);
	 printf("COLUMN_SIZE           = %d \n",columnSize);
	 printf("MINIMUM_SCALE         = %d \n",minScale);
	 printf("MAXIMUM_SCALE         = %d \n",maxScale);
	 printf("NUM_PREC_RADIX        = %d \n",radix);
	 printf("LOCAL_TYPE_NAME       = %s \n",localName);
	 if(suffix)
	 {
		printf("LITERAL_PREFIX        = %s \n",prefix);
	 }
	 if(prefix)
	 {
		printf("LITERAL_SUFFIX        = %s \n",suffix);
	 }
     rc = SQLFetch(hStmt);  
   };
	
   if (!SQL_SUCCEEDED(rc))
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
   if (SQL_SUCCEEDED(RETCODE))
     goto exit;
   return 0;
dberror:
     RETCODE=12;
exit:
     (void) printf ("**** Exiting  CLIP06.\n\n");
     return(RETCODE);
}
