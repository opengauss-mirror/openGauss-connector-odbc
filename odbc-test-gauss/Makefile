CC=gcc
deps:=common.o
ifndef cases
	cases:=  arraybinding-test \
		boolsaschar-test \
		odbc_capital_01 \
		odbc_capital_02 \
		odbc_capital_03 \
		odbc_capital_04 \
		odbc_capital_05 \
		odbc_capital_06 \
		odbc_capital_07 \
		odbc_null_adapt_01 \
		odbc_null_adapt_02 \
		odbc_null_adapt_03 \
		odbc_null_adapt_04 \
		odbc_null_adapt_05 \
		odbc_null_adapt_06 \
		odbc_nvarchar2_01_SQLGetData \
		odbc_nvarchar2_02_SQLColAttribute \
		odbc_nvarchar2_03_SQLColumns \
		odbc_nvarchar2_04_SQLPrimaryKeys \
		odbc_nvarchar2_07_SQLDescribeCol \
		odbc_nvarchar2_09_SQLProcedureColumns \
		odbc_nvarchar2_10_manycol \
		odbc_nvarchar2_11_out \
		odbc_sha256 \
		odbc_sm3 \
		odbc_smalldatetime_01_SQLPrimaryKeys \
		odbc_smalldatetime_02_SQLColumn \
		odbc_smalldatetime_03_SQLBindParameter \
		odbc_smalldatetime_04_SQLColAttribute \
		odbc_smalldatetime_05_SQLGetData \
		odbc_smalldatetime_06_SQLDescribeCol \
		odbc_smalldatetime_07_SQLProcedureColumns \
		odbc_SQLAllocConnect \
		odbc_SQLAllocEnv \
		odbc_SQLCancel \
		odbc_SQLDescribeParam \
		odbc_SQLExtendedFetch \
		odbc_SQLGetConnectAttr \
		odbc_SQLGetConnectOption \
		odbc_SQLGetDescField \
		odbc_SQLGetDiagField \
		odbc_SQLGetEnvAttr \
		odbc_SQLGetFunction \
		odbc_SQLGetInfo \
		odbc_SQLGetStmtAttr \
		odbc_SQLNumParams \
		odbc_SQLSetDescField \
		odbc_SQLStatistics \
		odbc_SQLTables \
		odbc_stmt_timeout_01_pg_sleep \
		odbc_stmt_timeout_02_select \
		odbc_stmt_timeout_03_procedure \
		odbc_stmt_timeout_04 \
		odbc_stmt_timeout_05 \
		odbc_stmt_timeout_06 \
		odbc_tinyint_01_SQLBindCol \
		odbc_tinyint_02_SQLColumn \
		odbc_tinyint_03_SQLBindParameter \
		odbc_tinyint_04_SQLColAttribute \
		odbc_tinyint_05_SQLGetData \
		odbc_tinyint_06_SQLGetTypeInfo \
		odbc_tinyint_07_SQLPrimaryKeys \
		odbc_tinyint_08_SQLDescribeCol \
		odbc_tinyint_09_SQLProcedureColumns \
		odbc_tinyint_10_manycol \
		odbc_transaction_01 \
		odbc_transaction_02 \
		odbc_transaction_03 \
		notice-test \
		dataatexecution-test \
		odbc_getresult-test \
		params-test \
		prepare-test \
		insertreturning-test \
		select-test \
		connect-test \
		cvtnulldate-test \
		stmthandles-test \
		odbc_batch_insert \
		odbc_DTS2019070108437 \
		odbc_transaction_for_extension_connector \
		odbc_DTS2019071815351
endif


all:$(deps) $(cases)

%: %.c
	$(CC) -l odbc -O0 -g -o $@ $< $(deps)

$(deps):common.c common.h
	$(CC) -O0 -g -c -o common.o common.c 

clean:
	rm -f $(cases) *.o *.diff && rm -rf log 

test: all
	time -p bash ./run.sh $(cases) 
