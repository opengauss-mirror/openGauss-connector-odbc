/*
Author:o00231432
测试odbc支持nvarchar2类型
test: SQLBindCol SQLColAttribute函数，是否能正确获取字段类型为varchar2的列的属性
*/
#include <stdlib.h>
#include <stdio.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <time.h>
SQLHENV		  V_OD_Env;		// Handle ODBC environment
SQLHENV		  V_OD_hstmt;  // Handle statement
SQLHDBC		  V_OD_hdbc;	// Handle connection
SQLRETURN	  rc;
SQLLEN		  V_OD_rowanz;
SQLINTEGER	  V_OD_err,V_OD_id,ID,IDLEN,typelen;
SQLSMALLINT	  V_OD_mlen,V_OD_colanz,col_type,col_num;
long		  V_OD_erg;
char		  V_OD_stat[10];	// Status SQL
char		  V_OD_msg[200],V_OD_buffer[200],type[200],name[200],
				typename[200],col_name[200], numattr[200];
char *buf = "zhuyunlong";
int value = 25;

int main(int argc,char *argv[])
{
	 // 1. allocate Environment handle and register version 
	V_OD_erg = SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&V_OD_Env);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error AllocHandle\n");
		exit(-1);
	}
	SQLSetEnvAttr(V_OD_Env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0); 
	SQLSetEnvAttr(V_OD_Env, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER *)0, 0); 
	// 2. allocate connection handle, set timeout
	V_OD_erg = SQLAllocHandle(SQL_HANDLE_DBC, V_OD_Env, &V_OD_hdbc); 
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error AllocHDB %d\n",V_OD_erg);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
  
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
		exit(-1);
	}
	printf("Connected !\n");
	V_OD_erg=SQLAllocHandle(SQL_HANDLE_STMT, V_OD_hdbc, &V_OD_hstmt);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Fehler im AllocStatement %d\n",V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"drop table IF EXISTS odbc_nvarchar2_02",SQL_NTS);	
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error in drop %d\n",V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}		
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"create table odbc_nvarchar2_02(id int,name nvarchar2(20) not null)",SQL_NTS);	
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error in create %d\n",V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	SQLExecDirect(V_OD_hstmt,"insert into odbc_nvarchar2_02 values(25,'朱云龙')",SQL_NTS);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Error in execute %d\n",V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	V_OD_erg = SQLBindCol(V_OD_hstmt,1,SQL_C_USHORT, &V_OD_id,150,NULL);
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	V_OD_erg = SQLBindCol(V_OD_hstmt,2,SQL_C_CHAR, &V_OD_buffer,150,NULL);
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	V_OD_erg = SQLExecDirect(V_OD_hstmt,"select id,name from odbc_nvarchar2_02",SQL_NTS);				  
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}

	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_DESC_LOCAL_TYPE_NAME,typename,100,NULL,NULL);					  //get the typename of column
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("Typename---------nvarchar2: %s\n",typename);	  
	
	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_COLUMN_CASE_SENSITIVE,typename,100,NULL,(SQLPOINTER)numattr);	  //whether case senstive
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("CaseSensitive----nvarchar2: %d\n",*numattr);
	
	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_COLUMN_SEARCHABLE,typename,100,NULL,(SQLPOINTER)numattr);		  //support link search
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("Searchable-------nvarchar2: %d\n",*numattr);

	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_COLUMN_LENGTH,typename,100,NULL,(SQLPOINTER)numattr);			  //bytes of the data length
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("Columnlen--------nvarchar2: %d\n",*numattr);

	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_COLUMN_DISPLAY_SIZE,typename,100,NULL,(SQLPOINTER)numattr);	  //bytes needed to display the data in character form
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("Maxdpsize--------nvarchar2: %d\n",*numattr);			
	
	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_DESC_TYPE,typename,100,NULL,(SQLPOINTER)numattr);				  //The SQL data type of the column
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("Desctype---------nvarchar2: %d\n",*numattr);

	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_COLUMN_TYPE,typename,100,NULL,(SQLPOINTER)numattr);			  //The SQL data type of the column(for next test)
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("Columntype-------nvarchar2: %d\n",*numattr);

	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_DESC_OCTET_LENGTH,typename,100,NULL,(SQLPOINTER)numattr);		  //bytes of the data length(test with front one)
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("DescOclen--------nvarchar2: %d\n",*numattr);
	
	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_DESC_TYPE_NAME,typename,100,NULL,NULL);		
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("DescTypeName-----nvarchar2: %s\n",typename);			
	
	V_OD_erg = SQLColAttribute(V_OD_hstmt,2,SQL_DESC_UNSIGNED,typename,100,NULL,(SQLPOINTER)numattr);		  //bytes of the data length(test with front one)
	if (!SQL_SUCCEEDED(V_OD_erg))
	{
		printf("Error in line %d, errcode: %d\n",__LINE__, V_OD_erg);
		SQLGetDiagRec(SQL_HANDLE_DBC, V_OD_hdbc,1, V_OD_stat,&V_OD_err,V_OD_msg,100,&V_OD_mlen);
		printf("%s (%d)\n",V_OD_msg,V_OD_err);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("UNSIGNED---------nvarchar2: %d\n",*numattr);			

	V_OD_erg=SQLNumResultCols(V_OD_hstmt,&V_OD_colanz);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLDisconnect(V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("SQLNumResultCols----------: %d\n",V_OD_colanz);
	V_OD_erg=SQLRowCount(V_OD_hstmt,&V_OD_rowanz);
	if ((V_OD_erg != SQL_SUCCESS) && (V_OD_erg != SQL_SUCCESS_WITH_INFO))
	{
		printf("Number of RowCount %d\n",V_OD_erg);
		SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
		SQLDisconnect(V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
		exit(-1);
	}
	printf("SQLRowCount---------------: %d\n",V_OD_rowanz);

	 SQLFreeHandle(SQL_HANDLE_STMT,V_OD_hstmt);
	 SQLDisconnect(V_OD_hdbc);
	 SQLFreeHandle(SQL_HANDLE_DBC,V_OD_hdbc);
	 SQLFreeHandle(SQL_HANDLE_ENV, V_OD_Env);
	 return(0);
}




