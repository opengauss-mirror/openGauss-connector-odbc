#include <stdio.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <time.h>
SQLHENV       V_OD_Env;    // Handle ODBC environment
SQLHENV       V_OD_hstmt; 
SQLRETURN       rc;
long       V_OD_erg;    // result of functions
SQLHDBC       V_OD_hdbc;                      // Handle connection
char       V_OD_stat[10];    // Status SQL
SQLINTEGER     V_OD_err,V_OD_rowanz,V_OD_id,V_ID;
SQLSMALLINT     V_OD_mlen,V_OD_colanz;
char             V_OD_msg[200],V_OD_buffer[200],schema[200],table[200],type[200],remark[200],V_OD_buffer1[200];

SQLINTEGER maxlv,minlv,curschema;
SQLINTEGER m_min,m_max;

char *buf = "Mike";
int value = 3;

int main(int argc,char *argv[])
{

  /**
	ODBC handle
	1) SQL_HANDLE_ENV 
	2) SQL_HANDLE_DBC
	3) SQL_HANDLE_STMT
  */
	
  // 1. test SQLAllocEnv
  V_OD_erg = SQLAllocEnv(&V_OD_Env);
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
  V_OD_erg = SQLAllocConnect (V_OD_Env, &V_OD_hdbc); 
  if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error AllocHDB %d\n",V_OD_erg);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }
  // //SQLSetConnectAttr(V_OD_hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER *)5, 0);
  //SET SQL_ATTR_AUTOCOMMIT OFF
  SQLSetConnectAttr(V_OD_hdbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, 0);
  
  // 3. Connect to the datasource "gaussdb" 
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
  printf("Connected Successfuly!\n");

 // SQLGetFunctions parameters
	SQLUSMALLINT     fExists  = SQL_TRUE;
	SQLUSMALLINT    *pfExists = &fExists;
	SQLUSMALLINT TablesExists, ColumnsExists, StatisticsExists,retcodeTables, retcodeColumns, retcodeStatistics;
	SQLUSMALLINT SQLDescribeParam,SQLDescribeParamexists,SQLSETDESCREC,SQLSETDESCRECexists,SQLGETDESCREC,SQLGETDESCRECexists;

	retcodeTables = SQLGetFunctions(V_OD_hdbc, SQL_API_SQLTABLES, &TablesExists);
	if ((retcodeTables != SQL_SUCCESS) && (retcodeTables != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error SQLGetFunctions %d\n",retcodeTables);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		return -1;
	} 
	
	SQLDescribeParam = SQLGetFunctions(V_OD_hdbc, SQL_API_SQLDESCRIBEPARAM, &SQLDescribeParamexists);
	if ((SQLDescribeParam != SQL_SUCCESS) && (SQLDescribeParam != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error SQLGetFunctions %d\n",SQLDescribeParam);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		return -1;
	} 
	
	SQLSETDESCREC = SQLGetFunctions(V_OD_hdbc, SQL_API_SQLSETDESCREC, &SQLSETDESCRECexists);
	if ((SQLSETDESCREC != SQL_SUCCESS) && (SQLSETDESCREC != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error SQLGetFunctions %d\n",SQLSETDESCREC);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		return -1;
	} 	
	
	SQLGETDESCREC = SQLGetFunctions(V_OD_hdbc, SQL_API_SQLGETDESCREC, &SQLGETDESCRECexists);
	if ((SQLGETDESCREC != SQL_SUCCESS) && (SQLGETDESCREC != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error SQLGetFunctions %d\n",SQLGETDESCREC);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		return -1;
	} 

	retcodeColumns = SQLGetFunctions(V_OD_hdbc, SQL_API_SQLCOLUMNS, &ColumnsExists);
	retcodeStatistics = SQLGetFunctions(V_OD_hdbc, SQL_API_SQLSTATISTICS, &StatisticsExists);

// SQLGetFunctions is completed successfully and SQLTables, SQLColumns, and SQLStatistics are supported by the driver.
	if (retcodeTables == SQL_SUCCESS && TablesExists == SQL_TRUE && 
		retcodeColumns == SQL_SUCCESS && ColumnsExists == SQL_TRUE && 
		retcodeStatistics == SQL_SUCCESS && StatisticsExists == SQL_TRUE) 
	{

		printf("SQL_API_SQLTABLES SQL_API_SQLCOLUMNS SQL_API_SQLSTATISTICS is true\n");
   // Continue with application

	}
	if (SQLDescribeParam == SQL_SUCCESS && SQLDescribeParamexists == SQL_TRUE) 
	{

		printf("  SQLDescribeParam is true\n");
   // Continue with application

	}
	else
	    printf(" SQLDescribeParam  is false\n");
	if (SQLGETDESCREC == SQL_SUCCESS && SQLDescribeParamexists == SQL_TRUE) 
	{

		printf("  SQLGETDESCREC is true\n");
   // Continue with application

	}
	else
	    printf(" SQLGETDESCREC  is false\n");
	if (SQLSETDESCREC == SQL_SUCCESS && SQLDescribeParamexists == SQL_TRUE) 
	{

		printf("  SQLSETDESCREC is true\n");
   // Continue with application

	}
	else
	    printf(" SQLSETDESCREC  is false\n");		



    SQLDisconnect(V_OD_hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return(0);
}



