/*
test: 测试接口SQLStatistics，该接口is a deprecated function and is replaced by SQLAllocHandle()
*/
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
SQLHSTMT        hstmt   = SQL_NULL_HSTMT,hStmt;
char       V_OD_stat[10];    // Status SQL
SQLINTEGER     V_OD_err,V_OD_rowanz,V_OD_id,V_ID;
SQLSMALLINT     V_OD_mlen,V_OD_colanz;
char             V_OD_msg[200],V_OD_buffer[200],remark[200],V_OD_buffer1[200];
SQLLEN non_unique_ind,type_ind,index_name_ind,column_name_ind,card_ind,pages_ind;
SQLINTEGER maxlv,minlv,non_unique,type,index_name,column_name,cardinality,pages;
SQLINTEGER m_min,m_max;
SQLCHAR schema[200] = "FVT_INTERFACE";
SQLCHAR table[200] = "odbc_SQLStatistics";

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

/* ... */
  V_OD_erg = SQLAllocHandle(SQL_HANDLE_STMT, V_OD_hdbc, &hstmt);
  if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error allcoc hstmt %d\n",V_OD_erg);
    SQLGetDiagRec(SQL_HANDLE_STMT, hstmt,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }
  V_OD_erg = SQLExecDirect(hstmt,"drop table  IF EXISTS FVT_INTERFACE.odbc_SQLStatistics",SQL_NTS);
    if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error SQLExecDirect %d\n",V_OD_erg);
    SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }
  V_OD_erg = SQLExecDirect(hstmt,"create table  FVT_INTERFACE.odbc_SQLStatistics(id int,name varchar(20),age float,addr char)",SQL_NTS);
    if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error SQLExecDirect %d\n",V_OD_erg);
    SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }  
  V_OD_erg = SQLExecDirect(hstmt,"create UNIQUE index  odbc_SQLStatistics_index on FVT_INTERFACE.odbc_SQLStatistics(id)",SQL_NTS);
    if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error SQLExecDirect %d\n",V_OD_erg);
    SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }  


/* ... */
    V_OD_erg = SQLStatistics(hstmt, NULL, 0, schema, SQL_NTS,
                    table, SQL_NTS, SQL_INDEX_ALL , SQL_QUICK);
    if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
  {
    printf("Error SQLStatistics %d\n",V_OD_erg);
    SQLGetDiagRec(SQL_HANDLE_ENV, V_OD_Env,1, 
                  V_OD_stat, &V_OD_err,V_OD_msg,100,&V_OD_mlen);
    printf("%s (%d)\n",V_OD_msg,V_OD_err);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
    return -1;
  }  					
    V_OD_erg = SQLBindCol(hstmt, 4, SQL_C_SHORT,
                          &non_unique, 2, &non_unique_ind);
    V_OD_erg = SQLBindCol(hstmt, 6, SQL_C_CHAR,
                          &index_name, 129, &index_name_ind);
    V_OD_erg = SQLBindCol(hstmt, 7, SQL_C_SHORT,
                          &type, 2, &type_ind);
    V_OD_erg = SQLBindCol(hstmt, 9, SQL_C_CHAR,
                          &column_name, 129, &column_name_ind);
    V_OD_erg = SQLBindCol(hstmt, 11, SQL_C_LONG,
                          &cardinality, 4, &card_ind);
    V_OD_erg = SQLBindCol(hstmt, 12, SQL_C_LONG,
                          &pages, 4, &pages_ind);
    printf("Statistics for table\n");
    while ((V_OD_erg = SQLFetch(hstmt)) == SQL_SUCCESS)
    {  if (type != SQL_TABLE_STAT)
       {   printf("  Column: %-18s Index Name: %-18s\n",
                  column_name, index_name);
       }
       else
       {   printf("  Table Statistics:\n");
       }
       if (card_ind != SQL_NULL_DATA)
          printf("    Cardinality = ",cardinality);
       else
          printf("    Cardinality = (Unavailable)");
       if (pages_ind != SQL_NULL_DATA)
          printf(" Pages = ",pages);
       else
          printf(" Pages = (Unavailable)\n");
    }
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    SQLDisconnect(V_OD_hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
	
    return(0);
}

