/*------
 * Module:			connection.c
 *
 * Description:		This module contains routines related to
 *					connecting to and disconnecting from the Postgres DBMS.
 *
 * Classes:			ConnectionClass (Functions prefix: "CC_")
 *
 * API functions:	SQLAllocConnect, SQLConnect, SQLDisconnect, SQLFreeConnect,
 *					SQLBrowseConnect(NI)
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *-------
 */
/* Multibyte support	Eiji Tokuya 2001-03-15 */

/*	TryEnterCritiaclSection needs the following #define */
#ifndef	_WIN32_WINNT
#define	_WIN32_WINNT	0x0400
#endif /* _WIN32_WINNT */

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/* for htonl */
#ifdef WIN32
#include <Winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#endif

#ifndef WIN32
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif 
																  
#include <dlfcn.h>
#include <pwd.h>
#endif

#include "connection.h"
#include "misc.h"
#include "environ.h"
#include "statement.h"
#include "qresult.h"
#include "lobj.h"
#include "dlg_specific.h"
#include "loadlib.h"

#include "multibyte.h"

#include "pgapifunc.h"

#define	SAFE_STR(s)	(NULL != (s) ? (s) : "(null)")

/* how many statement holders to allocate
 * at a time
 */
#define STMT_INCREMENT 16
#define MAX_PARTS 16
#define EXTRA_ROOM 128     /*  Extra room for string additional splicing */

#define MAX_CN 128        /* the maximum number of CN is 128 */
#define EMPTY 0
#define CORRECT 1
#define WRONG 2
#define lenNameType 63

BOOL conn_inited = FALSE;
BOOL conn_precheck = FALSE;
pthread_rwlock_t init_lock = PTHREAD_RWLOCK_INITIALIZER;
int refresh_flag = 0;
unsigned long int pgxc_node_thread_id;

typedef struct CnEntry {
	char ip_list[MAX_CN][MEDIUM_REGISTRY_LEN]; /* a char array to store IPs */
	int ip_status[MAX_CN]; /* an integer array that indicates the status of IPs */
	char port_list[MAX_CN][SMALL_REGISTRY_LEN]; /* a char array that stores port */
	int port_status[MAX_CN]; /* an integer array that indicates the status of IPs */
	int ip_count; /* define integer to record the number of IPs stored in the IP_list */
	int port_count; /*
	                 * define integer to record the number of IPs stored in the IP_list
	                 * this should be equal to IP_count
	                 */
	int step[MAX_CN]; /* record the offset for roundRobin when autobalance is on */
	BOOL is_usable;
	pthread_rwlock_t ip_list_lock; /* define a lock to isolate read and write to ip_list */
	pthread_rwlock_t step_lock[MAX_CN]; /* define a lock to isolate read and write to step */
} CnEntry;

typedef struct dsn_time {
	char *DSN;
	int timeinterval;
} dsn_time;

CnEntry orig_entry;
CnEntry pgxc_entry;

static int LIBPQ_connect(ConnectionClass *self);
int split_host_or_port_with_limit(const char *str, char *result_array[MAX_PARTS], int* total_length);
char* generate_conninfo_URL_by_ConnInfo(ConnInfo* ci, int* host_number, int* port_number);
#ifdef WIN32
DWORD WINAPI read_pgxc_node(LPVOID arg)
#else
static void *read_pgxc_node(void *arg)
#endif
{
	dsn_time read_cn;
	read_cn = *(dsn_time *) arg;
	char *DSN = malloc(strlen(read_cn.DSN) + 1);
	if (DSN == NULL) {
		exit(1);
	}
	strncpy_null(DSN, read_cn.DSN, strlen(read_cn.DSN) + 1);
	read_cn.DSN = DSN;
	int time = read_cn.timeinterval;
#ifdef WIN32
	pgxc_node_thread_id = GetCurrentThreadId();
#else
	pgxc_node_thread_id = pthread_self();
#endif
	int refresh_count = 0;
	/* read CNs' IP from pgxc_node */
	for (;;) {
		MYLOG(0, "REFRESH starts\n");
		SQLHENV hEnv = SQL_NULL_HENV;
		SQLHDBC hDbc = SQL_NULL_HDBC;
		SQLHSTMT hStmt = SQL_NULL_HSTMT;
		SQLRETURN rc = SQL_SUCCESS;
		SQLINTEGER RETCODE = 0;
		char node_port[SMALL_REGISTRY_LEN];
		char node_host[lenNameType];
		SQLLEN lenPort=0, lenHost=0;
		RETCODE = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
		if (RETCODE != SQL_SUCCESS) {
			continue;
		}
		SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*) SQL_OV_ODBC3, 0);
		RETCODE = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
		if (RETCODE != SQL_SUCCESS) {
			SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
			continue;
		}
		RETCODE = SQLConnect(hDbc,        // Connect handle
		                    (SQLCHAR *)DSN, //DSN
		                     SQL_NTS,     // DSN is nul-terminated
		                     NULL,        // Null UID
		                     0   ,
		                     NULL,        // Null Auth string
		                     0);
		if (RETCODE != SQL_SUCCESS) {
			SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
			SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
			continue;
		}
		RETCODE = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
		if (RETCODE != SQL_SUCCESS) {
			SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
			SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
			continue;
		}
		RETCODE = SQLExecDirect(hStmt,"select node_port, node_host from pgxc_node where node_type = 'C' and nodeis_active order by node_name",SQL_NTS);
		if (RETCODE != SQL_SUCCESS) {
			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
			SQLDisconnect(hDbc);
			SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
			SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
			continue;
		}
		RETCODE = SQLBindCol(hStmt, 1, SQL_C_CHAR, node_port, SMALL_REGISTRY_LEN, &lenPort);
		RETCODE = SQLBindCol(hStmt, 2, SQL_C_CHAR, node_host, lenNameType + 1, &lenHost);
		if (RETCODE != SQL_SUCCESS) {
			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
			SQLDisconnect(hDbc);
			SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
			SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
			continue;
		}
		int count = 0;
		char IP_list_temp[MAX_CN][MEDIUM_REGISTRY_LEN];
		char port_list_temp[MAX_CN][SMALL_REGISTRY_LEN];
		char port_temp[SMALL_REGISTRY_LEN];
		while ((rc = SQLFetch(hStmt)) == SQL_SUCCESS) {
			refresh_flag = 1;
			STRCPY_FIXED(IP_list_temp[count], node_host);
			STRCPY_FIXED(port_list_temp[count++], node_port);
			pgxc_entry.ip_count = count;
		}
		refresh_count++;

		if (refresh_count > 10 && rc == SQL_ERROR) {
			refresh_flag = 1;
			MYLOG(0, "Refresh failed for ten times, change signal and unlock other threads.\n");
		}

		int i;
		if (count != 0 && pthread_rwlock_wrlock(&pgxc_entry.ip_list_lock) == 0) {
			for (i = 0; i < pgxc_entry.ip_count; i++) {
				STRCPY_FIXED(pgxc_entry.ip_list[i], IP_list_temp[i]);
				STRCPY_FIXED(pgxc_entry.port_list[i], port_list_temp[i]);
				MYLOG(0, "ip = %s, port = %s\n", pgxc_entry.ip_list[i], pgxc_entry.port_list[i]);
			}
			MYLOG(0, "CN list has been refreshed.\n");
			if (pthread_rwlock_unlock(&pgxc_entry.ip_list_lock) != 0) {
				SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
				SQLDisconnect(hDbc);
				SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
				SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
				MYLOG(0, "Unlock failed. Exit process.\n");
				exit(1);
			}
		}
		SQLDisconnect(hDbc);
		sleep(time);
	}
}

static BOOL check_IP_connection(ConnectionClass *conn, CnEntry *entry)
{
	int i;
	int pqret;
	BOOL ret = FALSE;
	ConnInfo *ci = &conn->connInfo;

	if (conn == NULL) {
		return FALSE;
	}
	MYLOG(0, "Start checking the connection for each pair of IP and PORT.\n");
	if (entry == &pgxc_entry) {
		pthread_rwlock_rdlock(&entry->ip_list_lock);
	}
	for (i = 0; i < entry->ip_count; i++) {
		STRCPY_FIXED(ci->server, entry->ip_list[i]);
		STRCPY_FIXED(ci->port, entry->port_list[i]);
		if ((pqret = LIBPQ_connect(conn)) <= 0) {
			/* connection failed, kick out the wrong IP from IP_list and write the wrong IP into log */
			MYLOG(0, "Cannot establish connection via IP: %s\n", entry->ip_list[i]);
			entry->ip_status[i] = WRONG;
		} else {
			/* connection successful, current IP remains in IP_list but disconnect */
			entry->ip_status[i] = CORRECT;
			PQfinish(conn->pqconn);
			ret = TRUE;
		}
	}
	MYLOG(0, "Check finished.\n");
	if (entry == &pgxc_entry) {
		pthread_rwlock_unlock(&entry->ip_list_lock);
	}
	return ret;
}

static void start_new_thread(dsn_time *read_cn)
{	
#ifdef WIN32
	CreateThread(NULL, 0, read_pgxc_node, (LPVOID)(read_cn), 0, NULL);
#else
	pthread_t ntid;
	pthread_create(&ntid, NULL, read_pgxc_node, read_cn);
#endif
}

static RETCODE init_conn(ConnectionClass *conn)
{
	RETCODE	ret = SQL_SUCCESS;
	ConnInfo *ci = &conn->connInfo;
	if (conn == NULL) {
		return SQL_ERROR;
	}

	/* initialize */
	memset(&pgxc_entry, 0, sizeof(CnEntry));
	memset(&orig_entry, 0, sizeof(CnEntry));
	pgxc_entry.is_usable = TRUE;
	orig_entry.is_usable = TRUE;

	int i;
	for (i = 0; i < MAX_CN; i++) {
		pthread_rwlock_init(&pgxc_entry.step_lock[i], NULL);
		pthread_rwlock_init(&orig_entry.step_lock[i], NULL);
	}
	pthread_rwlock_init(&pgxc_entry.ip_list_lock, NULL);
	pthread_rwlock_init(&orig_entry.ip_list_lock, NULL);

	/* make a copy of ci->server and ci->port to prevent changing the original conn when parsing it */
	char server[LARGE_REGISTRY_LEN];
	STRCPY_FIXED(server, ci->server);
	char port[LARGE_REGISTRY_LEN];
	STRCPY_FIXED(port, ci->port);

	/* parsing ci->server to seperate IPs and store them into IP_list */
	char *p = strtok(server, ",");
	while (p != NULL) {
		STRCPY_FIXED(orig_entry.ip_list[orig_entry.ip_count++], p);
		STRCPY_FIXED(pgxc_entry.ip_list[pgxc_entry.ip_count++], p);
		p = strtok(NULL, ",");
	}

	/* parsing ci->port to seperate PORTs and store them into port_list */
	p = strtok(port, ",");
	while (p != NULL) {
		STRCPY_FIXED(orig_entry.port_list[orig_entry.port_count++], p);
		STRCPY_FIXED(pgxc_entry.port_list[pgxc_entry.port_count++], p);
		p = strtok(NULL, ",");
	}

	/* if only one port was configured, then each CN has the same port by default */
	if (orig_entry.port_count == 1) {
		for (i = 1; i < orig_entry.ip_count; i++) {
			STRCPY_FIXED(orig_entry.port_list[orig_entry.port_count++], orig_entry.port_list[0]);
			STRCPY_FIXED(pgxc_entry.port_list[pgxc_entry.port_count++], pgxc_entry.port_list[0]);
		}
	}

	/* if serverl ports were configured, the number of ports has to be equal to the number of IPs */
	if (orig_entry.ip_count != orig_entry.port_count) {
		MYLOG(0, "The number of IP %d does not match the number of Port %d.\n", orig_entry.ip_count, orig_entry.port_count);
		return SQL_ERROR;
	}

	/* check the connection of each pair of IP and port and update the list and status */
	if (!check_IP_connection(conn, &orig_entry)) {
		return SQL_ERROR;
	}
	memcpy(pgxc_entry.ip_status, orig_entry.ip_status, sizeof(orig_entry.ip_status));

	/* start new thread to connect to datbase and select node_port from pgxc_node */
	dsn_time read_cn;
	read_cn.DSN = ci->dsn;
	if (ci->refreshcnlisttime == 0) {
		read_cn.timeinterval = 10;
	} else {
		read_cn.timeinterval = ci->refreshcnlisttime;
	}
	start_new_thread(&read_cn);
	return ret;
}

int get_location(BOOL *visited, CnEntry *entry, int *visited_count)
{
	if (visited == NULL || entry == NULL || visited_count == NULL) {
		return -1;
	}
	/* select random IP from ip_list */
	srand(pthread_self());
	unsigned int ind = rand() % entry->ip_count;

	/* record the offset of each IP for roundRobin */
	int *offset = &entry->step[ind];
	pthread_rwlock_t *offset_lock = &entry->step_lock[ind];

	/*
	 * if the selected IP can be connected and has not been visited, connect to this IP
	 * else enter the while loop to choose another IP for connect
	 * use visited_count to record the number of IPs that have been visited
	 * visited_count equals to ip_count means reaching the limit
	 */
	while ((entry->ip_status[ind] == WRONG || visited[ind] == TRUE) && (*visited_count) != entry->ip_count) {
		if (visited[ind] == FALSE) {
			visited[ind] = TRUE;
			(*visited_count)++;
		}

		pthread_rwlock_wrlock(offset_lock);
		while (visited[ind] == TRUE && (*visited_count) != entry->ip_count) {
			ind = ind + *offset;
			(*offset)++;
			ind = ind % entry->ip_count;
		}
		pthread_rwlock_unlock(offset_lock);
	}

	/*
	 * the selected IP after the while loop should not have been visited
	 * if it has been visited, return error
	 */
	if (visited[ind] == TRUE) {
		return -1;
	}

	visited[ind] = TRUE;
	(*visited_count)++;
	return ind;
}

static RETCODE connect_random_IP(ConnectionClass *conn, CnEntry *entry)
{
	RETCODE	ret = SQL_ERROR;
	ConnInfo *ci = &conn->connInfo;
	CSTR func = "PGAPI_Connect";
	char fchar;
	BOOL visited[MAX_CN] = {FALSE};
	int visited_count = 0;
	BOOL check_ret = check_IP_connection(conn, entry);

	/* only connection successful and all connection failed will break the while loop */
	while (ret == SQL_ERROR) {
		if (entry == &pgxc_entry && pthread_rwlock_rdlock(&entry->ip_list_lock) != 0) {
			return SQL_ERROR;
		}

		int ind = get_location(visited, entry, &visited_count);
		if (ind == -1) {
			pthread_rwlock_unlock(&entry->ip_list_lock);
			return SQL_ERROR;
		}

		STRCPY_FIXED(ci->server, entry->ip_list[ind]);
		STRCPY_FIXED(ci->port, entry->port_list[ind]);
		while (entry == &pgxc_entry && pthread_rwlock_unlock(&entry->ip_list_lock) != 0);
		if ((fchar = CC_connect(conn, NULL)) <= 0) {
			/* Error messages are filled in */
			CC_log_error(func, "Error on CC_connect", conn);
			ret = SQL_ERROR;
		} else {
			ret = SQL_SUCCESS;
		}
	}
	if (ret == SQL_SUCCESS && fchar == 2) {
		ret = SQL_SUCCESS_WITH_INFO;
	}
	MYLOG(0, "leaving..%d.\n", ret);
	/* Empty the password stored in memory to avoid password leak */
	if (NAME_IS_VALID(ci->password))
		memset(ci->password.name, 0, strlen(ci->password.name));
	return ret;
}

static RETCODE connect_IP(ConnectionClass *conn)
{
	if (conn->connInfo.priority == 1 && orig_entry.is_usable && connect_random_IP(conn, &orig_entry) != SQL_ERROR) {
		return SQL_SUCCESS;
	}

	return connect_random_IP(conn, &pgxc_entry);
}

static RETCODE check_and_init(ConnectionClass *conn)
{
	if (conn_precheck) {
		return SQL_SUCCESS;
	}
	if (pthread_rwlock_rdlock(&init_lock)) {
		return SQL_ERROR;
	}
	
	if (conn_inited) {
		pthread_rwlock_unlock(&init_lock);
		return SQL_SUCCESS;
	}

	pthread_rwlock_unlock(&init_lock);

	if (pthread_rwlock_wrlock(&init_lock)) {
		return SQL_ERROR;
	}

	if (conn_inited) {
		pthread_rwlock_unlock(&init_lock);
		return SQL_SUCCESS;
	}

	if (init_conn(conn) != SQL_SUCCESS) {
		pthread_rwlock_unlock(&init_lock);
		return SQL_ERROR;
	} else {
		conn_inited = TRUE;
	}

	pthread_rwlock_unlock(&init_lock);
	conn_precheck = TRUE;
	return SQL_SUCCESS;
}

static SQLRETURN CC_lookup_lo(ConnectionClass *self);
static int  CC_close_eof_cursors(ConnectionClass *self);

static void LIBPQ_update_transaction_status(ConnectionClass *self);

static void CC_set_error_if_not_set(ConnectionClass *self, int errornumber, const char *errormsg, const char *func)
{
	int	errornum = CC_get_errornumber(self);
	const char *errmsg = CC_get_errormsg(self);

	if (errornumber == 0)
		return;
	if (errornumber > 0)
	{
		if (errornum <= 0)
			CC_set_error(self, errornumber, errormsg, func);
		else if (!errmsg)
			CC_set_errormsg(self, errormsg);
	}
	else if (errornum == 0)
		CC_set_error(self, errornumber, errormsg, func);
	else if (errornum < 0 && !errmsg)
		CC_set_errormsg(self, errormsg);
}

RETCODE		SQL_API
PGAPI_AllocConnect(HENV henv,
				   HDBC * phdbc)
{
	EnvironmentClass *env = (EnvironmentClass *) henv;
	ConnectionClass *conn;
	CSTR func = "PGAPI_AllocConnect";

	MYLOG(0, "entering...\n");

	conn = CC_Constructor();
	MYLOG(0, "**** henv = %p, conn = %p\n", henv, conn);

	if (!conn)
	{
		env->errormsg = "Couldn't allocate memory for Connection object.";
		env->errornumber = ENV_ALLOC_ERROR;
		*phdbc = SQL_NULL_HDBC;
		EN_log_error(func, "", env);
		return SQL_ERROR;
	}

	if (!EN_add_connection(env, conn))
	{
		env->errormsg = "Maximum number of connections exceeded.";
		env->errornumber = ENV_ALLOC_ERROR;
		CC_Destructor(conn);
		*phdbc = SQL_NULL_HDBC;
		EN_log_error(func, "", env);
		return SQL_ERROR;
	}

	if (phdbc)
		*phdbc = (HDBC) conn;

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_Connect(HDBC hdbc,
			  const SQLCHAR * szDSN,
			  SQLSMALLINT cbDSN,
			  const SQLCHAR * szUID,
			  SQLSMALLINT cbUID,
			  const SQLCHAR * szAuthStr,
			  SQLSMALLINT cbAuthStr)
{
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	ConnInfo   *ci;
	CSTR func = "PGAPI_Connect";
	RETCODE	ret = SQL_SUCCESS;
	char	fchar, *tmpstr;

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	ci = &conn->connInfo;
	CC_conninfo_init(ci, INIT_GLOBALS);

	make_string(szDSN, cbDSN, ci->dsn, sizeof(ci->dsn));

	/* get the values for the DSN from the registry */
	getDSNinfo(ci, NULL);

	logs_on_off(1, ci->drivers.debug, ci->drivers.commlog);
	/* initialize pg_version from connInfo.protocol    */
	CC_initialize_pg_version(conn);

	MYLOG(0, "entering..cbDSN=%hi.\n", cbDSN);

	/*
	 * override values from DSN info with UID and authStr(pwd) This only
	 * occurs if the values are actually there.
	 */
	fchar = ci->username[0]; /* save the first byte */
	make_string(szUID, cbUID, ci->username, sizeof(ci->username));
	if ('\0' == ci->username[0]) /* an empty string is specified */
		ci->username[0] = fchar; /* restore the original username */
	tmpstr = make_string(szAuthStr, cbAuthStr, NULL, 0);
	if (tmpstr)
	{
		if (tmpstr[0]) /* non-empty string is specified */
			STR_TO_NAME(ci->password, tmpstr);
		free(tmpstr);
	}

	MYLOG(0, "conn = %p (DSN='%s', UID='%s', PWD='%s')\n", conn, ci->dsn, ci->username, NAME_IS_VALID(ci->password) ? "xxxxx" : "");

	if (ci->autobalance == 1) {
		if (check_and_init(conn) != SQL_SUCCESS) {
			return SQL_ERROR;
		}

#ifdef WIN32
		if (GetCurrentThreadId() != pgxc_node_thread_id)
#else
		if (pthread_self() != pgxc_node_thread_id)
#endif
		{
			while (refresh_flag != 1) {
#ifdef WIN32
				sleep(10);
#else
				usleep(10000);
#endif
			}
		}
		ret = connect_IP(conn);
	}
	else {
		if ((fchar = CC_connect(conn, NULL)) <= 0) {
			CC_log_error(func, "Error on CC_connect", conn);
			ret = SQL_ERROR;
		}
		if (SQL_SUCCESS == ret && 2 == fchar) {
			ret = SQL_SUCCESS_WITH_INFO;
		}
		MYLOG(0, "leaving..%d.\n", ret);
		if (NAME_IS_VALID(ci->password)) {
			memset(ci->password.name, 0, strlen(ci->password.name));
		}
	}
	return ret;
}


RETCODE		SQL_API
PGAPI_BrowseConnect(HDBC hdbc,
					const SQLCHAR * szConnStrIn,
					SQLSMALLINT cbConnStrIn,
					SQLCHAR * szConnStrOut,
					SQLSMALLINT cbConnStrOutMax,
					SQLSMALLINT * pcbConnStrOut)
{
	CSTR func = "PGAPI_BrowseConnect";
	ConnectionClass *conn = (ConnectionClass *) hdbc;

	MYLOG(0, "entering...\n");

	CC_set_error(conn, CONN_NOT_IMPLEMENTED_ERROR, "Function not implemented", func);
	return SQL_ERROR;
}


/* Drop any hstmts open on hdbc and disconnect from database */
RETCODE		SQL_API
PGAPI_Disconnect(HDBC hdbc)
{
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	CSTR func = "PGAPI_Disconnect";


	MYLOG(0, "entering...\n");

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	if (conn->status == CONN_EXECUTING)
	{
		CC_set_error(conn, CONN_IN_USE, "A transaction is currently being executed", func);
		return SQL_ERROR;
	}

	logs_on_off(-1, conn->connInfo.drivers.debug, conn->connInfo.drivers.commlog);
	MYLOG(0, "about to CC_cleanup\n");

	/* Close the connection and free statements */
	CC_cleanup(conn, FALSE);

	MYLOG(0, "done CC_cleanup\n");
	MYLOG(0, "leaving...\n");

	return SQL_SUCCESS;
}


RETCODE		SQL_API
PGAPI_FreeConnect(HDBC hdbc)
{
	ConnectionClass *conn = (ConnectionClass *) hdbc;
	CSTR func = "PGAPI_FreeConnect";
	EnvironmentClass *env;

	MYLOG(0, "entering...hdbc=%p\n", hdbc);

	if (!conn)
	{
		CC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	/* Remove the connection from the environment */
	if (NULL != (env = CC_get_env(conn)) &&
	    !EN_remove_connection(env, conn))
	{
		CC_set_error(conn, CONN_IN_USE, "A transaction is currently being executed", func);
		return SQL_ERROR;
	}

	CC_Destructor(conn);

	MYLOG(0, "leaving...\n");

	return SQL_SUCCESS;
}

/*
 *		IMPLEMENTATION CONNECTION CLASS
 */

static void
reset_current_schema(ConnectionClass *self)
{
	if (self->current_schema)
	{
		free(self->current_schema);
		self->current_schema = NULL;
	}
	self->current_schema_valid = FALSE;
}

static ConnectionClass *
CC_alloc(void)
{
	return (ConnectionClass *) calloc(sizeof(ConnectionClass), 1);
}

static void
CC_lockinit(ConnectionClass *self)
{
	INIT_CONNLOCK(self);
	INIT_CONN_CS(self);
}

static ConnectionClass *
CC_initialize(ConnectionClass *rv, BOOL lockinit)
{
	size_t		clear_size;

#if defined(WIN_MULTITHREAD_SUPPORT) || defined(POSIX_THREADMUTEX_SUPPORT)
	clear_size = (char *)&(rv->cs) - (char *)rv;
#else
	clear_size = sizeof(ConnectionClass);
#endif /* WIN_MULTITHREAD_SUPPORT */

	memset(rv, 0, clear_size);
	rv->status = CONN_NOT_CONNECTED;
	rv->transact_status = CONN_IN_AUTOCOMMIT;		/* autocommit by default */
	rv->unnamed_prepared_stmt = NULL;

	rv->stmts = (StatementClass **) malloc(sizeof(StatementClass *) * STMT_INCREMENT);
	if (!rv->stmts)
		goto cleanup;
	memset(rv->stmts, 0, sizeof(StatementClass *) * STMT_INCREMENT);

	rv->num_stmts = STMT_INCREMENT;
	rv->descs = (DescriptorClass **) malloc(sizeof(DescriptorClass *) * STMT_INCREMENT);
	if (!rv->descs)
		goto cleanup;
	memset(rv->descs, 0, sizeof(DescriptorClass *) * STMT_INCREMENT);

	rv->num_descs = STMT_INCREMENT;

	rv->lobj_type = PG_TYPE_LO_UNDEFINED;
	if (isMsAccess())
		rv->ms_jet = 1;
	rv->isolation = 0; // means initially unknown server's default isolation
	rv->mb_maxbyte_per_char = 1;
	rv->max_identifier_length = -1;
	rv->autocommit_public = SQL_AUTOCOMMIT_ON;

	/* Initialize statement options to defaults */
	/* Statements under this conn will inherit these options */

	InitializeStatementOptions(&rv->stmtOptions);
	InitializeARDFields(&rv->ardOptions);
	InitializeAPDFields(&rv->apdOptions);
#ifdef	_HANDLE_ENLIST_IN_DTC_
	rv->asdum = NULL;
	rv->gTranInfo = 0;
#endif /* _HANDLE_ENLIST_IN_DTC_ */
	if (lockinit)
		CC_lockinit(rv);

	return rv;

cleanup:
	CC_Destructor(rv);
	return NULL;
}

ConnectionClass *
CC_Constructor()
{
	ConnectionClass *rv, *retrv = NULL;

	if (rv = CC_alloc(), NULL != rv)
		retrv = CC_initialize(rv, TRUE);
	return retrv;
}

char
CC_Destructor(ConnectionClass *self)
{
	MYLOG(0, "entering self=%p\n", self);

	if (self->status == CONN_EXECUTING)
		return 0;

	CC_cleanup(self, FALSE);			/* cleanup socket and statements */

	MYLOG(0, "after CC_Cleanup\n");

	/* Free up statement holders */
	if (self->stmts)
	{
		free(self->stmts);
		self->stmts = NULL;
	}
	if (self->descs)
	{
		free(self->descs);
		self->descs = NULL;
	}
	MYLOG(0, "after free statement holders\n");

	NULL_THE_NAME(self->schemaIns);
	NULL_THE_NAME(self->tableIns);
	CC_conninfo_release(&self->connInfo);
	if (self->__error_message)
		free(self->__error_message);
	DELETE_CONN_CS(self);
	DELETE_CONNLOCK(self);
	free(self);

	MYLOG(0, "leaving\n");

	return 1;
}


/*	Return how many cursors are opened on this connection */
int
CC_cursor_count(ConnectionClass *self)
{
	StatementClass *stmt;
	int			i,
				count = 0;
	QResultClass		*res;

	MYLOG(0, "self=%p, num_stmts=%d\n", self, self->num_stmts);

	CONNLOCK_ACQUIRE(self);
	for (i = 0; i < self->num_stmts; i++)
	{
		stmt = self->stmts[i];
		if (stmt && (res = SC_get_Result(stmt)) && QR_get_cursor(res))
			count++;
	}
	CONNLOCK_RELEASE(self);

	MYLOG(0, "leaving %d\n", count);

	return count;
}


void
CC_clear_error(ConnectionClass *self)
{
	if (!self)	return;
	CONNLOCK_ACQUIRE(self);
	self->__error_number = 0;
	if (self->__error_message)
	{
		free(self->__error_message);
		self->__error_message = NULL;
	}
	self->sqlstate[0] = '\0';
	CONNLOCK_RELEASE(self);
}

void
CC_examine_global_transaction(ConnectionClass *self)
{
	if (!self)	return;
#ifdef	_HANDLE_ENLIST_IN_DTC_
	if (CC_is_in_global_trans(self))
		CALL_IsolateDtcConn(self, TRUE);
#endif /* _HANDLE_ENLIST_IN_DTC_ */
}


CSTR	bgncmd = "START TRANSACTION";
CSTR	cmtcmd = "COMMIT";
CSTR	rbkcmd = "ROLLBACK";
CSTR	svpcmd = "SAVEPOINT";
CSTR	per_query_svp = "_per_query_svp_";
CSTR	rlscmd = "RELEASE";

/*
 *	Used to begin a transaction.
 */
char
CC_begin(ConnectionClass *self)
{
	char	ret = TRUE;
	if (!CC_is_in_trans(self))
	{
		QResultClass *res = CC_send_query(self, bgncmd, NULL, 0, NULL);
		MYLOG(0, "  sending BEGIN!\n");

		ret = QR_command_maybe_successful(res);
		QR_Destructor(res);
	}

	return ret;
}

/*
 *	Used to commit a transaction.
 *	We are almost always in the middle of a transaction.
 */
char
CC_commit(ConnectionClass *self)
{
	char	ret = TRUE;
	if (CC_is_in_trans(self))
	{
		if (!CC_is_in_error_trans(self))
			CC_close_eof_cursors(self);
		if (CC_is_in_trans(self))
		{
			QResultClass *res = CC_send_query(self, cmtcmd, NULL, 0, NULL);
			MYLOG(0, "  sending COMMIT!\n");
			ret = QR_command_maybe_successful(res);
			QR_Destructor(res);
		}
	}

	return ret;
}

/*
 *	Used to cancel a transaction.
 *	We are almost always in the middle of a transaction.
 */
char
CC_abort(ConnectionClass *self)
{
	char	ret = TRUE;
	if (CC_is_in_trans(self))
	{
		QResultClass *res = CC_send_query(self, rbkcmd, NULL, 0, NULL);
		MYLOG(0, "  sending ABORT!\n");
		ret = QR_command_maybe_successful(res);
		QR_Destructor(res);
	}

	return ret;
}

/* This is called by SQLSetConnectOption etc also */
char
CC_set_autocommit(ConnectionClass *self, BOOL on)
{
	BOOL currsts = CC_is_in_autocommit(self);

	if ((on && currsts) ||
	    (!on && !currsts))
		return on;
	MYLOG(0, " %d->%d\n", currsts, on);
	if (CC_is_in_trans(self))
		CC_commit(self);
	if (on)
		self->transact_status |= CONN_IN_AUTOCOMMIT;
	else
		self->transact_status &= ~CONN_IN_AUTOCOMMIT;

	return on;
}

/* Clear cached table info */
static void
CC_clear_col_info(ConnectionClass *self, BOOL destroy)
{
	if (self->col_info)
	{
		int	i;
		COL_INFO	*coli;

		for (i = 0; i < self->ntables; i++)
		{
			if (coli = self->col_info[i], NULL != coli)
			{
				if (destroy || coli->refcnt == 0)
				{
					free_col_info_contents(coli);
					free(coli);
					self->col_info[i] = NULL;
				}
				else
					coli->acc_time = 0;
			}
		}
		self->ntables = 0;
		if (destroy)
		{
			free(self->col_info);
			self->col_info = NULL;
			self->coli_allocated = 0;
		}
	}
}

static void
CC_set_locale_encoding(ConnectionClass *self, const char * encoding)
{
	char	*currenc = self->locale_encoding;

	if (encoding)
		self->locale_encoding = strdup(encoding);
	else
		self->locale_encoding = NULL;
	if (currenc)
		free(currenc);
}

static void
CC_determine_locale_encoding(ConnectionClass *self)
{
	const char *dbencoding = PQparameterStatus(self->pqconn, "client_encoding");
	const char *encoding;

	QLOG(0, "PQparameterStatus(%p, \"client_encoding\")=%s\n", self->pqconn, SAFE_STR(dbencoding));
	if (self->locale_encoding) /* already set */
                return;
	encoding = derive_locale_encoding(dbencoding);
	if (!encoding)
		encoding = "SQL_ASCII";
	CC_set_locale_encoding(self, encoding);
}

static void
CC_set_client_encoding(ConnectionClass *self, const char * encoding)
{
	char	*currenc = self->original_client_encoding;

	if (encoding)
	{
		self->original_client_encoding = strdup(encoding);
		self->ccsc = pg_CS_code(encoding);
	}
	else
	{
		self->original_client_encoding = NULL;
		self->ccsc = SQL_ASCII;
	}
	self->mb_maxbyte_per_char = pg_mb_maxlen(self->ccsc);
	if (currenc)
		free(currenc);
}

int
CC_send_client_encoding(ConnectionClass *self, const char * encoding)
{
	const char *dbencoding = PQparameterStatus(self->pqconn, "client_encoding");

	if (encoding && (!dbencoding || stricmp(encoding, dbencoding)))
	{
                char	query[64];
		QResultClass	*res;
		BOOL	cmd_success;

		SPRINTF_FIXED(query, "set client_encoding to '%s'", encoding);
		res = CC_send_query(self, query, NULL, 0, NULL);
		cmd_success = QR_command_maybe_successful(res);
		QR_Destructor(res);

		if (!cmd_success)
			return SQL_ERROR;
	}
	CC_set_client_encoding(self, encoding);

	return SQL_SUCCESS;
}

/* This is called by SQLDisconnect also */
char
CC_cleanup(ConnectionClass *self, BOOL keepCommunication)
{
	int			i;
	StatementClass *stmt;
	DescriptorClass *desc;

	if (self->status == CONN_EXECUTING)
		return FALSE;

	MYLOG(0, "entering self=%p\n", self);

	ENTER_CONN_CS(self);
	/* Cancel an ongoing transaction */
	/* We are always in the middle of a transaction, */
	/* even if we are in auto commit. */
	if (self->pqconn)
	{
		QLOG(0, "PQfinish: %p\n", self->pqconn);
		PQfinish(self->pqconn);
		self->pqconn = NULL;
	}

	MYLOG(0, "after PQfinish\n");

	/* Free all the stmts on this connection */
	for (i = 0; i < self->num_stmts; i++)
	{
		stmt = self->stmts[i];
		if (stmt)
		{
			stmt->hdbc = NULL;	/* prevent any more dbase interactions */

			SC_Destructor(stmt);

			self->stmts[i] = NULL;
		}
	}
	/* Free all the descs on this connection */
	for (i = 0; i < self->num_descs; i++)
	{
		desc = self->descs[i];
		if (desc)
		{
			DC_get_conn(desc) = NULL;	/* prevent any more dbase interactions */
			DC_Destructor(desc);
			free(desc);
			self->descs[i] = NULL;
		}
	}

	/* Check for translation dll */
#ifdef WIN32
	if (!keepCommunication && self->translation_handle)
	{
		FreeLibrary(self->translation_handle);
		self->translation_handle = NULL;
	}
#endif

	if (!keepCommunication)
	{
		self->status = CONN_NOT_CONNECTED;
		self->transact_status = CONN_IN_AUTOCOMMIT;
		self->unnamed_prepared_stmt = NULL;
	}
	if (!keepCommunication)
	{
		CC_conninfo_init(&(self->connInfo), CLEANUP_FOR_REUSE);
		if (self->original_client_encoding)
		{
			free(self->original_client_encoding);
			self->original_client_encoding = NULL;
		}
		if (self->locale_encoding)
		{
			free(self->locale_encoding);
			self->locale_encoding = NULL;
		}
		if (self->server_encoding)
		{
			free(self->server_encoding);
			self->server_encoding = NULL;
		}
		reset_current_schema(self);
	}
	/* Free cached table info */
	CC_clear_col_info(self, TRUE);
	if (self->num_discardp > 0 && self->discardp)
	{
		for (i = 0; i < self->num_discardp; i++)
			free(self->discardp[i]);
		self->num_discardp = 0;
	}
	if (self->discardp)
	{
		free(self->discardp);
		self->discardp = NULL;
	}

	LEAVE_CONN_CS(self);
	MYLOG(0, "leaving\n");
	return TRUE;
}


int
CC_set_translation(ConnectionClass *self)
{

#ifdef WIN32
	CSTR	func = "CC_set_translation";

	if (self->translation_handle != NULL)
	{
		FreeLibrary(self->translation_handle);
		self->translation_handle = NULL;
	}

	if (self->connInfo.translation_dll[0] == 0)
		return TRUE;

	self->translation_option = atoi(self->connInfo.translation_option);
	self->translation_handle = LoadLibrary(self->connInfo.translation_dll);

	if (self->translation_handle == NULL)
	{
		CC_set_error(self, CONN_UNABLE_TO_LOAD_DLL, "Could not load the translation DLL.", func);
		return FALSE;
	}

	self->DataSourceToDriver
		= (DataSourceToDriverProc) GetProcAddress(self->translation_handle,
												"SQLDataSourceToDriver");

	self->DriverToDataSource
		= (DriverToDataSourceProc) GetProcAddress(self->translation_handle,
												"SQLDriverToDataSource");

	if (self->DataSourceToDriver == NULL || self->DriverToDataSource == NULL)
	{
		CC_set_error(self, CONN_UNABLE_TO_LOAD_DLL, "Could not find translation DLL functions.", func);
		return FALSE;
	}
#endif
	return TRUE;
}

#ifndef PG_DIAG_SEVERITY_NONLOCALIZED
#define PG_DIAG_SEVERITY_NONLOCALIZED 'V'
#endif

void
handle_pgres_error(ConnectionClass *self, const PGresult *pgres,
				   const char *comment,
				   QResultClass *res, BOOL error_not_a_notice)
{
	char	   *errseverity;
	char	   *errseverity_nonloc = NULL;
	char	   *errprimary = NULL;
	char	   *errmsg = NULL;
	size_t		errmsglen;
	char	*sqlstate = NULL;
	int	level = MIN_LOG_LEVEL;

	MYLOG(DETAIL_LOG_LEVEL, "entering\n");

	sqlstate = PQresultErrorField(pgres, PG_DIAG_SQLSTATE);
	if (res && pgres)
	{
		if (sqlstate)
			STRCPY_FIXED(res->sqlstate, sqlstate);
	}

	if (NULL == pgres &&
	    NULL == self->pqconn)
	{
		const char *errmsg = "The connection has been lost";

		MYLOG(0, "setting error message=%s\n", errmsg);
		QLOG(0, "\t%ssetting error message=%s\n", __FUNCTION__, errmsg);
		if (CC_get_errornumber(self) <= 0)
			CC_set_error(self, CONNECTION_COMMUNICATION_ERROR, errmsg, comment);
		if (res)
		{
			QR_set_rstatus(res, PORES_FATAL_ERROR);
			QR_set_message(res, errmsg);
		}
		goto cleanup;
	}
	/*
	 * The full message with details and context and everything could
	 * be obtained with PQresultErrorMessage(). I think that would be
	 * more user-friendly, but for now, construct a message with
	 * severity and primary message, which is backwards compatible.
	 */
	errseverity = PQresultErrorField(pgres, PG_DIAG_SEVERITY);
	if (PG_VERSION_GE(self, 9.6))
	{
		errseverity_nonloc = PQresultErrorField(pgres, PG_DIAG_SEVERITY_NONLOCALIZED);
		MYLOG(0, "PG_DIAG_SEVERITY_NONLOCALIZED=%s\n", SAFE_STR(errseverity_nonloc));
	}
	if (!error_not_a_notice)
	{
		if (errseverity_nonloc)
		{
			if (stricmp(errseverity_nonloc, "NOTICE") != 0)
				level = 1;
		}
		else if (errseverity)
		{
			if (stricmp(errseverity, "NOTICE") != 0)
				level = 1;
		}
	}
	errprimary = PQresultErrorField(pgres, PG_DIAG_MESSAGE_PRIMARY);
	if (errseverity_nonloc)
		QLOG(level, "\t%s(%s) %s '%s'\n", errseverity_nonloc, SAFE_STR(errseverity), SAFE_STR(sqlstate), SAFE_STR(errprimary));
	else
		QLOG(level, "\t(%s) %s '%s'\n", SAFE_STR(errseverity), SAFE_STR(sqlstate), SAFE_STR(errprimary));
	if (errprimary == NULL)
	{
		/* Hmm. got no primary message. Check if there's a connection error */
		if (self->pqconn)
			errprimary = PQerrorMessage(self->pqconn);

		if (errprimary == NULL)
			errprimary = "no error information";
	}
	if (errseverity && errprimary)
	{
		errmsglen = strlen(errseverity) + 2 + strlen(errprimary) + 1;
		errmsg = malloc(errmsglen);
		if (errmsg)
			snprintf(errmsg, errmsglen, "%s: %s", errseverity, errprimary);
	}
	if (errmsg == NULL)
		errmsg = errprimary;

	if (!error_not_a_notice) /* warning, notice, log etc */
	{
		MYLOG(0, "notice message %s\n", errmsg);
		if (res)
		{
			if (QR_command_successful(res))
				QR_set_rstatus(res, PORES_NONFATAL_ERROR); /* notice or warning */
			QR_add_notice(res, errmsg);  /* will dup this string */
		}
		goto cleanup;
	}

	MYLOG(0, "error message=%s(" FORMAT_SIZE_T ")\n", errmsg, strlen(errmsg));

	if (res)
	{
		QR_set_rstatus(res, PORES_FATAL_ERROR); /* error or fatal */
		if (errmsg[0])
			QR_set_message(res, errmsg);
		QR_set_aborted(res, TRUE);
	}

	/*
	 *	If the error is continuable after rollback?
	 */
	if (PQstatus(self->pqconn) == CONNECTION_BAD)
	{
		CC_set_errornumber(self, CONNECTION_COMMUNICATION_ERROR);
		CC_on_abort(self, CONN_DEAD); /* give up the connection */
	}
	else if ((errseverity_nonloc && strcmp(errseverity_nonloc, "FATAL") == 0) ||
		(NULL == errseverity_nonloc && errseverity && strcmp(errseverity, "FATAL") == 0)) /* no */ 
	{
		CC_set_errornumber(self, CONNECTION_SERVER_REPORTED_SEVERITY_FATAL);
		CC_on_abort(self, CONN_DEAD); /* give up the connection */
	}
	else /* yes */
	{
		CC_set_errornumber(self, CONNECTION_SERVER_REPORTED_SEVERITY_ERROR);
		if (CC_is_in_trans(self))
			CC_set_in_error_trans(self);
	}

	/* If any error/warning/notice happened, there should be a message in connection, 
	 * espacially while a connection is being created.
	 * While a connection is being created, postgresql never returns any error. But in
	 * our cluster, there maybe a warning or a notice when GTM is in trouble.
	 */
	CC_set_errormsg(self, errmsg);
	
cleanup:
	if (errmsg != errprimary)
		free(errmsg);
	LIBPQ_update_transaction_status(self);
}

/*
 * This is a libpq notice receiver callback, for handling incoming NOTICE
 * messages while processing a query.
 */
typedef struct
{
	ConnectionClass *conn;
	const char *comment;
	QResultClass *res;
} notice_receiver_arg;

void
receive_libpq_notice(void *arg, const PGresult *pgres)
{
	if (arg != NULL)
	{
		notice_receiver_arg *nrarg = (notice_receiver_arg *) arg;

		handle_pgres_error(nrarg->conn, pgres, nrarg->comment, nrarg->res, FALSE);
	}
}

static char CC_initial_log(ConnectionClass *self, const char *func)
{
	const ConnInfo	*ci = &self->connInfo;
	char	*encoding, vermsg[128];

	snprintf(vermsg, sizeof(vermsg), "Driver Version='%s,%s'"
#ifdef	WIN32
		" linking %d"
#ifdef	_MT
#ifdef	_DLL
		" dynamic"
#else
		" static"
#endif /* _DLL */
		" Multithread"
#else
		" Singlethread"
#endif /* _MT */
#ifdef	_DEBUG
		" Debug"
#endif /* DEBUG */
		" library"
#endif /* WIN32 */
		"\n", POSTGRESDRIVERVERSION, __DATE__
#ifdef	_MSC_VER
		, _MSC_VER
#endif /* _MSC_VER */
		);
	QLOG(0, "%s", vermsg);
	MYLOG(DETAIL_LOG_LEVEL, "Global Options: fetch=%d, unknown_sizes=%d, max_varchar_size=%d, max_longvarchar_size=%d\n",
		 ci->drivers.fetch_max,
		 ci->drivers.unknown_sizes,
		 ci->drivers.max_varchar_size,
		 ci->drivers.max_longvarchar_size);
	MYLOG(DETAIL_LOG_LEVEL, "                unique_index=%d, use_declarefetch=%d\n",
		 ci->drivers.unique_index,
		 ci->drivers.use_declarefetch);
	MYLOG(DETAIL_LOG_LEVEL, "                text_as_longvarchar=%d, unknowns_as_longvarchar=%d, bools_as_char=%d NAMEDATALEN=%d\n",
		 ci->drivers.text_as_longvarchar,
		 ci->drivers.unknowns_as_longvarchar,
		 ci->drivers.bools_as_char,
		 TABLE_NAME_STORAGE_LEN);

	if (NULL == self->locale_encoding)
	{
		encoding = check_client_encoding(ci->conn_settings);
		CC_set_locale_encoding(self, encoding);
		MYLOG(DETAIL_LOG_LEVEL, "                extra_systable_prefixes='%s', conn_settings='%s' conn_encoding='%s'\n",
			ci->drivers.extra_systable_prefixes,
			PRINT_NAME(ci->conn_settings),
			encoding ? encoding : "");
        if (NULL != encoding)
        {
            free(encoding);
        }
	}
	if (self->status == CONN_DOWN)
	{
		CC_set_error_if_not_set(self, CONN_OPENDB_ERROR, "Connection broken.", func);
		return 0;
	}
	else if (self->status != CONN_NOT_CONNECTED)
	{
		CC_set_error_if_not_set(self, CONN_OPENDB_ERROR, "Already connected.", func);
		return 0;
	}

	MYLOG(0, "DSN = '%s', server = '%s', port = '%s', database = '%s'\n", ci->dsn, ci->server, ci->port, ci->database);

	return 1;
}

static int handle_show_results(const QResultClass *res);
#define	TRANSACTION_ISOLATION "transaction_isolation"
#define	ISOLATION_SHOW_QUERY "show " TRANSACTION_ISOLATION

static char
LIBPQ_CC_connect(ConnectionClass *self, char *salt_para)
{
	int		ret;
	CSTR		func = "LIBPQ_CC_connect";
	QResultClass	*res;

	MYLOG(0, "entering...\n");

	if (0 == CC_initial_log(self, func))
		return 0;

	if (ret = LIBPQ_connect(self), ret <= 0)
		return ret;
	res = CC_send_query(self, "SET DateStyle = 'ISO';SET extra_float_digits = 2;" ISOLATION_SHOW_QUERY, NULL, READ_ONLY_QUERY, NULL);
	if (QR_command_maybe_successful(res))
	{
		handle_show_results(res);
		ret = 1;
	}
	else
		ret = 0;
	QR_Destructor(res);

	return ret;
}
RETCODE
CC_detect_batch_proto(ConnectionClass *self)
{
	const char	*value = NULL;
	const char 	*query = "select count(*) from pg_settings where name = 'support_batch_bind' and setting = 'on';";
	PGresult 	*res = NULL;
	ExecStatusType restype;
	int			cnt = 0;
	RETCODE		ret = SQL_SUCCESS;

	CSTR func = "CC_detect_batch_proto";
	mylog("%s: entering...\n", func);

	qlog("    [%s : conn = %p, query = select count(*) from pg_settings where name = 'support_batch_bind' and setting = 'on']\n",
		func, self);
	res = PQexec(self->pqconn, query);

	if (NULL == res)
	{
		self->connInfo.backend_support_batch_proto = 0;
		mylog("%s: NULL result detected, backend_support_batch_proto set to 0", func);
		qlog("    [%s : conn = %p, query result = NULL]\n",
				func, self);
		return SQL_ERROR;
	}
	restype = PQresultStatus(res);

	if (restype != PGRES_TUPLES_OK &&
		restype != PGRES_SINGLE_TUPLE)
	{
		self->connInfo.backend_support_batch_proto = 0;
		PQclear(res);
		mylog("%s: invalid result type %d detected, backend_support_batch_proto set to 0", func, restype);
		qlog("    [%s : conn = %p, query result type = %d]\n",
				func, self, restype);
		return SQL_ERROR;
	}

	if (PQntuples(res) < 1 ||
		PQnfields(res) < 1)
	{
		self->connInfo.backend_support_batch_proto = 0;
		mylog("%s: invalid result rowsCount(%d) or colsCount(%d) detected, backend_support_batch_proto set to 0", 
				func, PQntuples(res), PQnfields(res));
		qlog("    [%s : conn = %p, invalid query result : %d rows, %d columns]\n",
				func, self, PQntuples(res), PQnfields(res));
		PQclear(res);
		return SQL_ERROR;
	}

	value = PQgetvalue(res, 0, 0);
	cnt = (value ? atoi(value) : 0);

	PQclear(res);

	if (cnt < 1)
		self->connInfo.backend_support_batch_proto = 0;
	else
		self->connInfo.backend_support_batch_proto = 1;

	if (0 == self->connInfo.backend_support_batch_proto &&
		self->connInfo.use_batch_protocol)
	{
		CC_set_error(self, CONN_UNSUPPORTED_OPTION, 
			"Backend does not support batch bind protocol, \"" INI_USEBATCHPROTOCOL "\" disabled.", func);
		ret = SQL_SUCCESS_WITH_INFO;
	}

	mylog("%s: query result: %d, backend_support_batch_proto set to %d", 
				func, cnt, self->connInfo.backend_support_batch_proto);
	qlog("    [%s : conn = %p, query result : %d ]\n",
				func, self, cnt);
	mylog("%s: exiting\n", func);

	return ret;
}

char
CC_connect(ConnectionClass *self, char *salt_para)
{
	ConnInfo *ci = &(self->connInfo);
	CSTR	func = "CC_connect";
	char		ret, *saverr = NULL, retsend;
	const char	*errmsg = NULL;
	RETCODE		batchDetectRet = 0;

	MYLOG(0, "entering...sslmode=%s\n", self->connInfo.sslmode);

	ret = LIBPQ_CC_connect(self, salt_para);
	if (ret <= 0)
		return ret;

	CC_set_translation(self);

	/*
	 * Send any initial settings
	 */

	/*
	 * Since these functions allocate statements, and since the connection
	 * is not established yet, it would violate odbc state transition
	 * rules.  Therefore, these functions call the corresponding local
	 * function instead.
	 */

	/* Per Datasource settings */
	retsend = CC_send_settings(self, GET_NAME(self->connInfo.conn_settings));
	if (CONN_DOWN == self->status)
	{
		ret = 0;
		goto cleanup;
	}

	if (CC_get_errornumber(self) > 0 &&
	    NULL != (errmsg = CC_get_errormsg(self)))
		saverr = strdup(errmsg);
	CC_clear_error(self);			/* clear any error */

	if (!SQL_SUCCEEDED(CC_lookup_lo(self)))	/* a hack to get the oid of our large object oid type */
	{
		ret = 0;
		goto cleanup;
	}

	/*
	 *		Multibyte handling
	 *
	 *	Send 'UTF8' when required Unicode behavior, otherwise send
	 *	locale encodings.
	 */
	CC_clear_error(self);
	CC_determine_locale_encoding(self); /* determine the locale_encoding */
#ifdef UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(self))
	{
		if (!SQL_SUCCEEDED(CC_send_client_encoding(self, "UTF8")))
		{
			ret = 0;
			goto cleanup;
		}
	}
	else	/* for unicode drivers require ANSI behavior */
#endif /* UNICODE_SUPPORT */
	{
		if (!SQL_SUCCEEDED(CC_send_client_encoding(self, self->locale_encoding)))
		{
			ret = 0;
			goto cleanup;
		}
	}

	CC_clear_error(self);
	if (self->server_isolation != self->isolation)
		if (!CC_set_transact(self, self->isolation))
		{
			ret = 0;
			goto cleanup;
		}

	ci_updatable_cursors_set(ci);

	if (CC_get_errornumber(self) > 0)
		CC_clear_error(self);		/* clear any initial command errors */
	self->status = CONN_CONNECTED;
	if (CC_is_in_unicode_driver(self)
	    && (CC_is_in_ansi_app(self) || 0 < ci->bde_environment))
		self->unicode |= CONN_DISALLOW_WCHAR;
MYLOG(0, "conn->unicode=%d Client Encoding='%s' (Code %d)\n", self->unicode, self->original_client_encoding, self->ccsc);

	batchDetectRet = CC_detect_batch_proto(self);
	if (batchDetectRet == SQL_ERROR)
		goto cleanup;
	else if (batchDetectRet == SQL_SUCCESS_WITH_INFO)
	{
		/* Caller will recognize 2 as SQL_SUCCESS_WITH_INFO. */
		ret = 2;
		goto cleanup;
	}

	ret = 1;

cleanup:
	MYLOG(0, "leaving...%d\n", ret);
	if (NULL != saverr)
	{
		if (ret > 0 && CC_get_errornumber(self) <= 0)
			CC_set_error(self, -1, saverr, func);
		free(saverr);
	}
	if (1 == ret && FALSE == retsend)
		ret = 2;

	return ret;
}


char
CC_add_statement(ConnectionClass *self, StatementClass *stmt)
{
	int	i;
	char	ret = TRUE;

	MYLOG(0, "self=%p, stmt=%p\n", self, stmt);

	CONNLOCK_ACQUIRE(self);
	for (i = 0; i < self->num_stmts; i++)
	{
		if (!self->stmts[i])
		{
			stmt->hdbc = self;
			self->stmts[i] = stmt;
			break;
		}
	}

	if (i >= self->num_stmts) /* no more room -- allocate more memory */
	{
		StatementClass **newstmts;
		Int2 new_num_stmts;

		new_num_stmts = STMT_INCREMENT + self->num_stmts;

		if (new_num_stmts > 0)
			newstmts = (StatementClass **)
				realloc(self->stmts, sizeof(StatementClass *) * new_num_stmts);
		else
			newstmts = NULL; /* num_stmts overflowed */
		if (!newstmts)
			ret = FALSE;
		else
		{
			self->stmts = newstmts;
			memset(&self->stmts[self->num_stmts], 0, sizeof(StatementClass *) * STMT_INCREMENT);

			stmt->hdbc = self;
			self->stmts[self->num_stmts] = stmt;

			self->num_stmts = new_num_stmts;
		}
	}
	CONNLOCK_RELEASE(self);

	return ret;
}

static void
CC_set_error_statements(ConnectionClass *self)
{
	int	i;

	MYLOG(0, "entering self=%p\n", self);

	for (i = 0; i < self->num_stmts; i++)
	{
		if (NULL != self->stmts[i])
			SC_ref_CC_error(self->stmts[i]);
	}
}


char
CC_remove_statement(ConnectionClass *self, StatementClass *stmt)
{
	int	i;
	char	ret = FALSE;

	CONNLOCK_ACQUIRE(self);
	for (i = 0; i < self->num_stmts; i++)
	{
		if (self->stmts[i] == stmt && stmt->status != STMT_EXECUTING)
		{
			self->stmts[i] = NULL;
			ret = TRUE;
			break;
		}
	}
	CONNLOCK_RELEASE(self);

	return ret;
}

char CC_get_escape(const ConnectionClass *self)
{
	const char	   *scf;
	static const ConnectionClass *conn = NULL;

	scf = PQparameterStatus(self->pqconn, "standard_conforming_strings");
	if (self != conn)
	{
		QLOG(0, "PQparameterStatus(%p, \"standard_conforming_strings\")=%s\n", self->pqconn, SAFE_STR(scf));
		conn = self;
	}
	if (scf == NULL)
	{
		/* we're connected to a pre-8.1 server, and E'' is not supported */
		return '\0';
	}
	if (strcmp(scf, "on") != 0)
		return ESCAPE_IN_LITERAL;
	else
		return '\0';
}


int	CC_get_max_idlen(ConnectionClass *self)
{
	int	len = self->max_identifier_length;

	if  (len < 0)
	{
		QResultClass	*res;

		res = CC_send_query(self, "show max_identifier_length", NULL, READ_ONLY_QUERY, NULL);
		if (QR_command_maybe_successful(res))
			len = self->max_identifier_length = QR_get_value_backend_int(res, 0, 0, FALSE);
		QR_Destructor(res);
	}
MYLOG(0, "max_identifier_length=%d\n", len);
	return len < 0 ? 0 : len;
}

static SQLINTEGER
isolation_str_to_enum(const char *str_isolation)
{
	SQLINTEGER	isolation = 0;

	if (strnicmp(str_isolation, "seri", 4) == 0)
		isolation = SQL_TXN_SERIALIZABLE;
	else if (strnicmp(str_isolation, "repe", 4) == 0)
		isolation = SQL_TXN_REPEATABLE_READ;
	else if (strnicmp(str_isolation, "read com", 8) == 0)
		isolation = SQL_TXN_READ_COMMITTED;
	else if (strnicmp(str_isolation, "read unc", 8) == 0)
		isolation = SQL_TXN_READ_UNCOMMITTED;

	return isolation;
}

static int handle_show_results(const QResultClass *res)
{
	int			count = 0;
	const QResultClass	*qres;
	ConnectionClass		*conn = QR_get_conn(res);

	for (qres = res; qres; qres = qres->next)
	{
		if (!qres->command ||
		    stricmp(qres->command, "SHOW") != 0)
			continue;
		if (strcmp(QR_get_fieldname(qres, 0), TRANSACTION_ISOLATION) == 0)
		{
			conn->server_isolation = isolation_str_to_enum(QR_get_value_backend_text(qres, 0, 0));
			MYLOG(0, "isolation %d to be %d\n", conn->server_isolation, conn->isolation);
			if (0 == conn->isolation)
				conn->isolation = conn->server_isolation;
			if (0 == conn->default_isolation)
				conn->default_isolation = conn->server_isolation;
			count++;
		}
	}

	return count;
}
/*
 *	This function may not be called as long as ISOLATION_SHOW_QUERY is
 *	issued in LIBPQ_CC_connect.
 */
SQLUINTEGER	CC_get_isolation(ConnectionClass *self)
{
	SQLUINTEGER	isolation = 0;
	QResultClass	*res;

	res = CC_send_query(self, ISOLATION_SHOW_QUERY, NULL, READ_ONLY_QUERY, NULL);
	if (QR_command_maybe_successful(res))
	{
		handle_show_results(res);
		isolation = self->server_isolation;
	}
	QR_Destructor(res);
MYLOG(0, "isolation=%d\n", isolation);
	return isolation;
}

void
CC_set_error(ConnectionClass *self, int number, const char *message, const char *func)
{
	CONNLOCK_ACQUIRE(self);
	if (self->__error_message)
		free(self->__error_message);
	self->__error_number = number;
	self->__error_message = message ? strdup(message) : NULL;
	if (0 != number)
		CC_set_error_statements(self);
	if (func && number != 0)
		CC_log_error(func, "", self);
	CONNLOCK_RELEASE(self);
}


void
CC_set_errormsg(ConnectionClass *self, const char *message)
{
	CONNLOCK_ACQUIRE(self);
	if (self->__error_message)
		free(self->__error_message);
	self->__error_message = message ? strdup(message) : NULL;
	CONNLOCK_RELEASE(self);
}


char
CC_get_error(ConnectionClass *self, int *number, char **message)
{
	int			rv;

	MYLOG(0, "entering\n");

	CONNLOCK_ACQUIRE(self);

	if (CC_get_errornumber(self))
	{
		*number = CC_get_errornumber(self);
		*message = CC_get_errormsg(self);
	}
	rv = (CC_get_errornumber(self) != 0);

	CONNLOCK_RELEASE(self);

	MYLOG(0, "leaving\n");

	return rv;
}


static int CC_close_eof_cursors(ConnectionClass *self)
{
	int	i, ccount = 0;
	StatementClass	*stmt;
	QResultClass	*res;

	if (!self->ncursors)
		return ccount;
	CONNLOCK_ACQUIRE(self);
	for (i = 0; i < self->num_stmts; i++)
	{
		if (stmt = self->stmts[i], NULL == stmt)
			continue;
		if (res = SC_get_Result(stmt), NULL == res)
			continue;
		if (NULL != QR_get_cursor(res) &&
		    QR_is_withhold(res) &&
		    QR_once_reached_eof(res))
		{
			if (QR_get_num_cached_tuples(res) >= QR_get_num_total_tuples(res) ||
				SQL_CURSOR_FORWARD_ONLY == stmt->options.cursor_type)
			{
				QR_close(res);
				ccount++;
			}
		}
	}
	CONNLOCK_RELEASE(self);
	return ccount;
}

static void CC_clear_cursors(ConnectionClass *self, BOOL on_abort)
{
	int	i;
	StatementClass	*stmt;
	QResultClass	*res;

	if (!self->ncursors)
		return;
	CONNLOCK_ACQUIRE(self);
	for (i = 0; i < self->num_stmts; i++)
	{
		stmt = self->stmts[i];
		if (stmt && (res = SC_get_Result(stmt)) &&
			 (NULL != QR_get_cursor(res)))
		{
			/*
			 * non-holdable cursors are automatically closed
			 * at commit time.
			 * all non-permanent cursors are automatically closed
			 * at rollback time.
			 */
			if ((on_abort && !QR_is_permanent(res)) ||
				!QR_is_withhold(res))
			{
				QR_on_close_cursor(res);
			}
			else if (!QR_is_permanent(res))
			{
				QResultClass	*wres;
				char	cmd[64];

				if (QR_needs_survival_check(res))
				{
					SPRINTF_FIXED(cmd, "MOVE 0 in \"%s\"", QR_get_cursor(res));
					CONNLOCK_RELEASE(self);
					wres = CC_send_query(self, cmd, NULL, ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN | READ_ONLY_QUERY, NULL);
					QR_set_no_survival_check(res);
					if (QR_command_maybe_successful(wres) &&
					    CONN_ERROR_IGNORED != CC_get_errornumber(self))
						QR_set_permanent(res);
					else
						QR_set_cursor(res, NULL);
					QR_Destructor(wres);
					CONNLOCK_ACQUIRE(self);
MYLOG(DETAIL_LOG_LEVEL, "%p->permanent -> %d %p\n", res, QR_is_permanent(res), QR_get_cursor(res));
				}
				else
					QR_set_permanent(res);
			}
		}
	}
	CONNLOCK_RELEASE(self);
}

static void CC_mark_cursors_doubtful(ConnectionClass *self)
{
	int	i;
	StatementClass	*stmt;
	QResultClass	*res;

	if (!self->ncursors)
		return;
	CONNLOCK_ACQUIRE(self);
	for (i = 0; i < self->num_stmts; i++)
	{
		stmt = self->stmts[i];
		if (NULL != stmt &&
		    NULL != (res = SC_get_Result(stmt)) &&
		    NULL != QR_get_cursor(res) &&
		    !QR_is_permanent(res))
			QR_set_survival_check(res);
	}
	CONNLOCK_RELEASE(self);
}

void	CC_on_commit(ConnectionClass *conn)
{
	if (conn->on_commit_in_progress)
		return;
	conn->on_commit_in_progress = 1;
	CONNLOCK_ACQUIRE(conn);
	if (CC_is_in_trans(conn))
	{
		CC_set_no_trans(conn);
		CC_set_no_manual_trans(conn);
	}
	CC_svp_init(conn);
	CC_start_stmt(conn);
	CC_clear_cursors(conn, FALSE);
	CONNLOCK_RELEASE(conn);
	CC_discard_marked_objects(conn);
	CONNLOCK_ACQUIRE(conn);
	if (conn->result_uncommitted)
	{
		CONNLOCK_RELEASE(conn);
		ProcessRollback(conn, FALSE, FALSE);
		CONNLOCK_ACQUIRE(conn);
		conn->result_uncommitted = 0;
	}
	CONNLOCK_RELEASE(conn);
	conn->on_commit_in_progress = 0;
}
void	CC_on_abort(ConnectionClass *conn, unsigned int opt)
{
	BOOL	set_no_trans = FALSE;

MYLOG(0, "entering opt=%x\n", opt);
	CONNLOCK_ACQUIRE(conn);
	if (0 != (opt & CONN_DEAD)) /* CONN_DEAD implies NO_TRANS also */
		opt |= NO_TRANS;
	if (CC_is_in_trans(conn))
	{
		if (0 != (opt & NO_TRANS))
		{
			CC_set_no_trans(conn);
			CC_set_no_manual_trans(conn);
			set_no_trans = TRUE;
		}
	}
	CC_svp_init(conn);
	CC_start_stmt(conn);
	CC_clear_cursors(conn, TRUE);
	if (0 != (opt & CONN_DEAD))
	{
		conn->status = CONN_DOWN;
		if (conn->pqconn)
		{
			CONNLOCK_RELEASE(conn);
			QLOG(0, "PQfinish: %p\n", conn->pqconn);
			PQfinish(conn->pqconn);
			CONNLOCK_ACQUIRE(conn);
			conn->pqconn = NULL;
		}
	}
	else if (set_no_trans)
	{
		CONNLOCK_RELEASE(conn);
		CC_discard_marked_objects(conn);
		CONNLOCK_ACQUIRE(conn);
	}
	if (conn->result_uncommitted)
	{
		CONNLOCK_RELEASE(conn);
		ProcessRollback(conn, TRUE, FALSE);
		CONNLOCK_ACQUIRE(conn);
		conn->result_uncommitted = 0;
	}
	CONNLOCK_RELEASE(conn);
}

void	CC_on_abort_partial(ConnectionClass *conn)
{
MYLOG(0, "entering\n");
	CONNLOCK_ACQUIRE(conn);
	ProcessRollback(conn, TRUE, TRUE);
	CC_discard_marked_objects(conn);
	CONNLOCK_RELEASE(conn);
}

static BOOL
is_setting_search_path(const char *query)
{
	const char *q = query;
	if (strnicmp(q, "set", 3) != 0)
		return FALSE;
	q += 3;
	while (isspace(*q)) q++;
	for (; *q;)
	{
		if (IS_NOT_SPACE(*q))
		{
			if (strnicmp(q, "search_path", 11) == 0)
				return TRUE;
			q++;
			while (IS_NOT_SPACE(*q))
				q++;
		}
		else
			q++;
	}
	return FALSE;
}

static BOOL
CC_from_PGresult(QResultClass *res, StatementClass *stmt,
				 ConnectionClass *conn, const char *cursor, PGresult **pgres)
{
	BOOL	success = TRUE;

	if (!QR_from_PGresult(res, stmt, conn, cursor, pgres))
	{
		QLOG(0, "\tGetting result from PGresult failed\n");
		success = FALSE;
		if (0 >= CC_get_errornumber(conn))
		{
			switch (QR_get_rstatus(res))
			{
				case PORES_NO_MEMORY_ERROR:
					CC_set_error(conn, CONN_NO_MEMORY_ERROR, NULL, __FUNCTION__);
					break;
				case PORES_BAD_RESPONSE:
					CC_set_error(conn, CONNECTION_COMMUNICATION_ERROR, "communication error occured", __FUNCTION__);
					break;
				default:
					CC_set_error(conn, CONN_EXEC_ERROR, QR_get_message(res), __FUNCTION__);
					break;
			}
		}
	}
	return success;
}

int
CC_internal_rollback(ConnectionClass *self, int rollback_type, BOOL ignore_abort)
{
	int	ret = 0;
	char		cmd[128];
	PGresult   *pgres = NULL;

	if (!CC_is_in_error_trans(self))
		return 1;
	switch (rollback_type)
	{
		case PER_STATEMENT_ROLLBACK:
			GenerateSvpCommand(self, INTERNAL_ROLLBACK_OPERATION, cmd, sizeof(cmd));
			QLOG(0, "PQexec: %p '%s'\n", self->pqconn, cmd);
			pgres = PQexec(self->pqconn, cmd);
			switch (PQresultStatus(pgres))
			{
				case PGRES_COMMAND_OK:
					QLOG(0, "\tok: - 'C' - %s\n", PQcmdStatus(pgres));
				case PGRES_NONFATAL_ERROR:
					ret = 1;
					if (ignore_abort)
						CC_set_no_error_trans(self);
					LIBPQ_update_transaction_status(self);
					break;
				default:
					handle_pgres_error(self, pgres, __FUNCTION__, NULL, TRUE);
					break;
			}
			break;
		case PER_QUERY_ROLLBACK:
			SPRINTF_FIXED(cmd, "%s TO %s;%s %s"
				, rbkcmd, per_query_svp , rlscmd, per_query_svp);
			QLOG(0, "PQsendQuery: %p '%s'\n", self->pqconn, cmd);
			PQsendQuery(self->pqconn, cmd);
			ret = 0;
			while (self->pqconn && (pgres = PQgetResult(self->pqconn)) != NULL)
			{
				switch (PQresultStatus(pgres))
				{
					case PGRES_COMMAND_OK:
						QLOG(0, "\tok: - 'C' - %s\n", PQcmdTuples(pgres));
						ret = 1;
						break;
					case PGRES_NONFATAL_ERROR:
						ret = 1;
					default:
						handle_pgres_error(self, pgres, __FUNCTION__, NULL, !ret);
				}
			}
			if (!ret)
			{
				if (ignore_abort)
					CC_set_no_error_trans(self);
				else
					MYLOG(0, " return error\n");
			}
			LIBPQ_update_transaction_status(self);
			break;
	}
	if (pgres)
		PQclear(pgres);

	return ret;
}

/*
 *	The "result_in" is only used by QR_next_tuple() to fetch another group of rows into
 *	the same existing QResultClass (this occurs when the tuple cache is depleted and
 *	needs to be re-filled).
 *
 *	The "cursor" is used by SQLExecute to associate a statement handle as the cursor name
 *	(i.e., C3326857) for SQL select statements.  This cursor is then used in future
 *	'declare cursor C3326857 for ...' and 'fetch 100 in C3326857' statements.
 *
 * * If issue_begin, send "BEGIN"
 * * if needed, send "SAVEPOINT ..."
 * * Send "query", read result
 * * Send appendq, read result.
 *
 */
QResultClass *
CC_send_query_append(ConnectionClass *self, const char *query, QueryInfo *qi, UDWORD flag, StatementClass *stmt, const char *appendq)
{
	CSTR	func = "CC_send_query";
	QResultClass *cmdres = NULL,
			   *retres = NULL,
			   *res = NULL;
	BOOL	ignore_abort_on_conn = ((flag & IGNORE_ABORT_ON_CONN) != 0),
		create_keyset = ((flag & CREATE_KEYSET) != 0),
		issue_begin = ((flag & GO_INTO_TRANSACTION) != 0 && !CC_is_in_trans(self)),
		rollback_on_error, query_rollback, end_with_commit,
		read_only, prepend_savepoint = FALSE,
		ignore_roundtrip_time = ((self->connInfo.extra_opts & BIT_IGNORE_ROUND_TRIP_TIME) != 0);

	char		*ptr;
	BOOL		ReadyToReturn = FALSE,
				query_completed = FALSE,
				aborted = FALSE,
				used_passed_result_object = FALSE,
			discard_next_begin = FALSE,
			discard_next_savepoint = FALSE,
			discard_next_release = FALSE,
			consider_rollback;
	BOOL	discardTheRest = FALSE;
	int		func_cs_count = 0;
	PQExpBufferData		query_buf = {0};
	size_t		query_len;

	/* QR_set_command() dups this string so doesn't need static */
	char	   *cmdbuffer;
	PGresult   *pgres = NULL;
	notice_receiver_arg nrarg;

	if (appendq)
	{
		MYLOG(0, "conn=%p, query='%s'+'%s'\n", self, query, appendq);
	}
	else
	{
		MYLOG(0, "conn=%p, query='%s'\n", self, query);
	}

	if (!self->pqconn)
	{
		PQExpBufferData	pbuf = {0};
		initPQExpBuffer(&pbuf);
		appendPQExpBuffer(&pbuf, "The connection is down\nFailed to send '%s'", query);
		CC_set_error(self, CONNECTION_COULD_NOT_SEND, pbuf.data, func);
		termPQExpBuffer(&pbuf);
		return NULL;
	}

	ENTER_INNER_CONN_CS(self, func_cs_count);
/* Indicate that we are sending a query to the backend */
	if ((NULL == query) || (query[0] == '\0'))
	{
		CLEANUP_FUNC_CONN_CS(func_cs_count, self);
		return NULL;
	}

	/*
	 *	In case the round trip time can be ignored, the query
	 *	and the appeneded query would be issued separately.
	 *	Otherwise a multiple command query would be issued.
	 */
	if (appendq && ignore_roundtrip_time)
	{
		res = CC_send_query_append(self, query, qi, flag, stmt, NULL);
		if (QR_command_maybe_successful(res))
		{
			cmdres = CC_send_query_append(self, appendq, qi, flag & (~(GO_INTO_TRANSACTION)), stmt, NULL);
			if (QR_command_maybe_successful(cmdres))
				res->next = cmdres;
			else
			{
				QR_Destructor(res);
				res = cmdres;
			}
		}
		CLEANUP_FUNC_CONN_CS(func_cs_count, self);
		return res;
	}

	rollback_on_error = (flag & ROLLBACK_ON_ERROR) != 0;
	end_with_commit = (flag & END_WITH_COMMIT) != 0;
	read_only = (flag & READ_ONLY_QUERY) != 0;
#define	return DONT_CALL_RETURN_FROM_HERE???
	consider_rollback = (issue_begin || (CC_is_in_trans(self) && !CC_is_in_error_trans(self)) || strnicmp(query, "begin", 5) == 0);
	if (rollback_on_error)
		rollback_on_error = consider_rollback;
	query_rollback = (rollback_on_error && !end_with_commit && PG_VERSION_GE(self, 8.0));
	if (!query_rollback && consider_rollback && !end_with_commit)
	{
		if (stmt)
		{
			StatementClass	*astmt = SC_get_ancestor(stmt);
			unsigned int svpopt = 0;

			if (read_only)
				svpopt |= SVPOPT_RDONLY;
			if (!ignore_roundtrip_time)
				svpopt |= SVPOPT_REDUCE_ROUNDTRIP;
			if (!CC_started_rbpoint(self))
			{
				if (SQL_ERROR == SetStatementSvp(astmt, svpopt))
				{
					SC_set_error(stmt, STMT_INTERNAL_ERROR, "internal savepoint error", func);
					goto cleanup;
				}
			}
		}
	}

	/* prepend internal savepoint command ? */
	if (PREPEND_IN_PROGRESS == self->internal_op)
		prepend_savepoint = TRUE;

	/* append all these together, to avoid round-trips */
	query_len = strlen(query);
	MYLOG(0, "query_len=" FORMAT_SIZE_T "\n", query_len);

	initPQExpBuffer(&query_buf);
	/* issue_begin, query_rollback and prepend_savepoint are exclusive */
	if (issue_begin)
	{
		appendPQExpBuffer(&query_buf, "%s;", bgncmd);
		discard_next_begin = TRUE;
	}
	else if (query_rollback && !self->connInfo.drivers.for_extension_connector)
	{
		appendPQExpBuffer(&query_buf, "%s %s;", svpcmd, per_query_svp);
		discard_next_savepoint = TRUE;
	}
	else if (prepend_savepoint)
	{
		char   	prepend_cmd[128];

		GenerateSvpCommand(self, INTERNAL_SAVEPOINT_OPERATION, prepend_cmd, sizeof(prepend_cmd));
		appendPQExpBuffer(&query_buf, "%s;", prepend_cmd);
		self->internal_op = SAVEPOINT_IN_PROGRESS;
	}
	appendPQExpBufferStr(&query_buf, query);
	if (appendq)
	{
		appendPQExpBuffer(&query_buf, ";%s", appendq);
	}
	if (query_rollback && !self->connInfo.drivers.for_extension_connector)
	{
		appendPQExpBuffer(&query_buf, ";%s %s", rlscmd, per_query_svp);
	}
	if (PQExpBufferDataBroken(query_buf))
	{
		CC_set_error(self, CONN_NO_MEMORY_ERROR, "Couldn't alloc buffer for query.", "");
		goto cleanup;
	}

	/* Set up notice receiver */
	nrarg.conn = self;
	nrarg.comment = func;
	nrarg.res = NULL;
	PQsetNoticeReceiver(self->pqconn, receive_libpq_notice, &nrarg);

	QLOG(0, "PQsendQuery: %p '%s'\n", self->pqconn, query_buf.data);
	if (!PQsendQuery(self->pqconn, query_buf.data))
	{
		char *errmsg = PQerrorMessage(self->pqconn);
		QLOG(0, "\nCommunication Error: %s\n", SAFE_STR(errmsg));
		CC_set_error(self, CONNECTION_COMMUNICATION_ERROR, errmsg, func);
		goto cleanup;
	}
	PQsetSingleRowMode(self->pqconn);

	cmdres = qi ? qi->result_in : NULL;
	if (cmdres)
		used_passed_result_object = TRUE;
	else
	{
		cmdres = QR_Constructor();
		if (!cmdres)
		{
			CC_set_error(self, CONNECTION_COULD_NOT_RECEIVE, "Could not create result info in send_query.", func);
			goto cleanup;
		}
	}
	res = cmdres;
	if (qi)
	{
		res->cmd_fetch_size = qi->fetch_size;
		res->cache_size = qi->row_size;
	}
	nrarg.res = res;

	while (self->pqconn && (pgres = PQgetResult(self->pqconn)) != NULL)
	{
		int status = PQresultStatus(pgres);

		if (discardTheRest)
			continue;
		switch (status)
		{
			case PGRES_COMMAND_OK:
				/* portal query command, no tuples returned */
				/* read in the return message from the backend */
				cmdbuffer = PQcmdStatus(pgres);
				QLOG(0, "\tok: - 'C' - %s\n", cmdbuffer);

				if (query_completed)	/* allow for "show" style notices */
				{
					res->next = QR_Constructor();
					if (!res->next)
					{
						CC_set_error(self, CONNECTION_COULD_NOT_RECEIVE, "Could not create result info in send_query.", func);
						ReadyToReturn = TRUE;
						retres = NULL;
						break;
					}
					res = res->next;
					nrarg.res = res;
				}

				MYLOG(0, " setting cmdbuffer = '%s'\n", cmdbuffer);

				my_trim(cmdbuffer); /* get rid of trailing space */
				if (strnicmp(cmdbuffer, bgncmd, strlen(bgncmd)) == 0)
				{
					CC_set_in_trans(self);
					CC_set_in_manual_trans(self);
					if (discard_next_begin) /* discard the automatically issued BEGIN */
					{
						discard_next_begin = FALSE;
						break; /* discard the result */
					}
				}
				/*
				 * There are 2 risks to RELEASE an internal savepoint.
				 * One is to RELEASE the savepoint invalitated
				 * due to manually issued ROLLBACK or RELEASE.
				 * Another is to invalitate manual SAVEPOINTs unexpectedly
				 * by RELEASing the internal savepoint.
				 */
				else if (strnicmp(cmdbuffer, svpcmd, strlen(svpcmd)) == 0)
				{
					if (discard_next_savepoint)
					{
						discard_next_savepoint = FALSE;
						discard_next_release = TRUE;
MYLOG(DETAIL_LOG_LEVEL, "Discarded a SAVEPOINT result\n");
						break; /* discard the result */
					}
					if (SAVEPOINT_IN_PROGRESS == self->internal_op)
					{
						CC_start_rbpoint(self);
						self->internal_op = 0;
						break; /* discard the result */
					}
					/* Don't release the internal savepoint */
					self->internal_svp = 0;
				}
				else if (strnicmp(cmdbuffer, rbkcmd, strlen(rbkcmd)) == 0)
				{
					CC_mark_cursors_doubtful(self);
					CC_set_in_error_trans(self); /* mark the transaction error in case of manual rollback */
					self->internal_svp = 0; /* possibly an internal savepoint is invalid */
					self->opt_previous = 0; /* unknown */
					CC_init_opt_in_progress(self);
				}
				else if (strnicmp(cmdbuffer, rlscmd, strlen(rlscmd)) == 0)
				{
					if (discard_next_release)
					{
MYLOG(DETAIL_LOG_LEVEL, "Discarded a RELEASE result\n");
						discard_next_release = FALSE;
						break; /* discard the result */
					}
					self->internal_svp = 0;
					if (SAVEPOINT_IN_PROGRESS == self->internal_op)
						break; /* discard the result */
				}
				/*
				 *	DROP TABLE or ALTER TABLE may change
				 *	the table definition. So clear the
				 *	col_info cache though it may be too simple.
				 */
				else if (strnicmp(cmdbuffer, "DROP TABLE", 10) == 0 ||
						 strnicmp(cmdbuffer, "ALTER TABLE", 11) == 0)
					CC_clear_col_info(self, FALSE);
				else
				{
					ptr = strrchr(cmdbuffer, ' ');
					if (ptr)
						res->recent_processed_row_count = atoi(ptr + 1);
					else
						res->recent_processed_row_count = -1;
					if (self->current_schema_valid &&
						strnicmp(cmdbuffer, "SET", 3) == 0)
					{
						if (is_setting_search_path(query))
							reset_current_schema(self);
					}
				}

				if (QR_command_successful(res))
					QR_set_rstatus(res, PORES_COMMAND_OK);
				QR_set_command(res, cmdbuffer);
				query_completed = TRUE;
				MYLOG(0, " returning res = %p\n", res);
				break;

			case PGRES_EMPTY_QUERY:
				/* We return the empty query */
				QR_set_rstatus(res, PORES_EMPTY_QUERY);
				break;
			case PGRES_NONFATAL_ERROR:
				handle_pgres_error(self, pgres, "send_query", res, FALSE);
				break;

			case PGRES_BAD_RESPONSE:
			case PGRES_FATAL_ERROR:
				handle_pgres_error(self, pgres, "send_query", res, TRUE);

				/* We should report that an error occured. Zoltan */
				aborted = TRUE;

				query_completed = TRUE;
				break;
			case PGRES_TUPLES_OK:
				QLOG(0, "\tok: - 'T' - %s\n", PQcmdStatus(pgres));
			case PGRES_SINGLE_TUPLE:
				if (query_completed)
				{
					res->next = QR_Constructor();
					if (!res->next)
					{
						CC_set_error(self, CONNECTION_COULD_NOT_RECEIVE, "Could not create result info in send_query.", func);
						ReadyToReturn = TRUE;
						retres = NULL;
						break;
					}
					if (create_keyset)
					{
						QR_set_haskeyset(res->next);
						if (stmt)
							res->next->num_key_fields = stmt->num_key_fields;
					}
					MYLOG(0, " 'T' no result_in: res = %p\n", res->next);
					res = res->next;
					nrarg.res = res;

					if (qi)
					{
						QR_set_cache_size(res, qi->row_size);
						res->cmd_fetch_size = qi->fetch_size;
					}
				}
				if (!used_passed_result_object)
				{
					const char *cursor = qi ? qi->cursor : NULL;
					if (create_keyset)
					{
						QR_set_haskeyset(res);
						if (stmt)
							res->num_key_fields = stmt->num_key_fields;
						if (cursor && cursor[0])
							QR_set_synchronize_keys(res);
					}
					if (CC_from_PGresult(res, stmt, self, cursor, &pgres))
						query_completed = TRUE;
					else
					{
						aborted = TRUE;
						if (QR_command_maybe_successful(res))
							retres = NULL;
						else
							retres = cmdres;
					}
				}
				else
				{				/* next fetch, so reuse an existing result */
					const char *cursor = res->cursor_name;

					/*
					 * called from QR_next_tuple and must return
					 * immediately.
					 */
					if (!CC_from_PGresult(res, stmt, NULL, cursor, &pgres))
					{
						retres = NULL;
						break;
					}
					retres = cmdres;
				}
				if (res->rstatus == PORES_TUPLES_OK && res->notice)
				{
					QR_set_rstatus(res, PORES_NONFATAL_ERROR);
				}
				else if (PORES_NO_MEMORY_ERROR == QR_get_rstatus(res))
				{
					PGcancel *cancel = PQgetCancel(self->pqconn);
					char	dummy[8];

					discardTheRest = TRUE;
					if (cancel != NULL)
					{
						PQcancel(cancel, dummy, sizeof(dummy));
						PQfreeCancel(cancel);
					}
					else
						goto cleanup;
				}
				break;
			case PGRES_COPY_OUT:
				/* XXX: We used to read from stdin here. Does that make any sense? */
			case PGRES_COPY_IN:
				if (query_completed)
				{
					res->next = QR_Constructor();
					if (!res->next)
					{
						CC_set_error(self, CONNECTION_COULD_NOT_RECEIVE, "Could not create result info in send_query.", func);
						ReadyToReturn = TRUE;
						retres = NULL;
						break;
					}
					res = res->next;
					nrarg.res = res;
				}
				QR_set_rstatus(res, PORES_COPY_IN);
				ReadyToReturn = TRUE;
				retres = cmdres;
				break;
			case PGRES_COPY_BOTH:
			default:
				/* skip the unexpected response if possible */
				CC_set_error(self, CONNECTION_BACKEND_CRAZY, "Unexpected result status (send_query)", func);
				handle_pgres_error(self, pgres, "send_query", res, TRUE);
				CC_on_abort(self, CONN_DEAD);

				MYLOG(0, " error - %s\n", CC_get_errormsg(self));
				ReadyToReturn = TRUE;
				retres = NULL;
				break;
		}

		if (pgres)
		{
			PQclear(pgres);
			pgres = NULL;
		}
	}

cleanup:
	if (self->pqconn)
		PQsetNoticeReceiver(self->pqconn, receive_libpq_notice, NULL);
	if (pgres != NULL)
	{
		PQclear(pgres);
		pgres = NULL;
	}
MYLOG(DETAIL_LOG_LEVEL, " rollback_on_error=%d CC_is_in_trans=%d discard_next_savepoint=%d query_rollback=%d\n", rollback_on_error, CC_is_in_trans(self), discard_next_savepoint, query_rollback);
	if (rollback_on_error && CC_is_in_trans(self) && !discard_next_savepoint)
	{
		if (query_rollback)
		{
			if (!CC_internal_rollback(self, PER_QUERY_ROLLBACK, ignore_abort_on_conn))
				ignore_abort_on_conn = FALSE;
		}
		else if (CC_is_in_error_trans(self))
		{
			QLOG(0, "PQexec: %p '%s'\n", self->pqconn, rbkcmd);
			pgres = PQexec(self->pqconn, rbkcmd);
		}
		/*
		 * XXX: we don't check the result here. Should we? We're rolling back,
		 * so it's not clear what else we can do on error. Giving an error
		 * message to the application would be nice though.
		 */
		if (pgres != NULL)
		{
			PQclear(pgres);
			pgres = NULL;
		}
	}

	CLEANUP_FUNC_CONN_CS(func_cs_count, self);
#undef	return
	/*
	 * Break before being ready to return.
	 */
	if (!ReadyToReturn)
		retres = cmdres;

	if (!PQExpBufferDataBroken(query_buf))
		termPQExpBuffer(&query_buf);

	/*
	 * Cleanup garbage results before returning.
	 */
	if (cmdres && retres != cmdres && !used_passed_result_object)
		QR_Destructor(cmdres);
	/*
	 * Cleanup the aborted result if specified
	 */
	if (retres)
	{
		if (aborted)
		{
			/** if (ignore_abort_on_conn)
			{
				if (!used_passed_result_object)
				{
					QR_Destructor(retres);
					retres = NULL;
				}
			} **/
			if (retres)
			{
				/*
				 *	discard results other than errors.
				 */
				QResultClass	*qres;
				for (qres = retres; qres->next; qres = retres)
				{
					if (QR_get_aborted(qres))
						break;
					retres = qres->next;
					qres->next = NULL;
					QR_Destructor(qres);
				}
				/*
				 *	If error message isn't set
				 */
				if (ignore_abort_on_conn)
				{
					CC_set_errornumber(self, CONN_ERROR_IGNORED);
					if (retres)
						QR_set_rstatus(retres, PORES_NONFATAL_ERROR);
MYLOG(DETAIL_LOG_LEVEL, " ignored abort_on_conn\n");
				}
				else if (retres)
				{
					if (NULL == CC_get_errormsg(self) ||
					    !CC_get_errormsg(self)[0])
						CC_set_errormsg(self, QR_get_message(retres));
					if (!self->sqlstate[0])
						STRCPY_FIXED(self->sqlstate, retres->sqlstate);
				}
			}
		}
	}

	/*
	 * Update our copy of the transaction status.
	 *
	 * XXX: Once we stop using the socket directly, and do everything with
	 * libpq, we can get rid of the transaction_status field altogether
	 * and always ask libpq for it.
	 */
	LIBPQ_update_transaction_status(self);

	if (retres)
		QR_set_conn(retres, self);
	return retres;
}

#define MAX_SEND_FUNC_ARGS	3
static const char *func_param_str[MAX_SEND_FUNC_ARGS + 1] =
{
	"()",
	"($1)",
	"($1, $2)",
	"($1, $2, $3)"
};

static
Int8 odbc_hton64(Int8 h64)
{
	union {
		Int8	n64;
		UInt4	i32[2];
	} u;

	u.i32[0] = htonl((UInt4) (h64 >> 32));
	u.i32[1] = htonl((UInt4) h64);

	return u.n64;
}

static
Int8 odbc_ntoh64(Int8 n64)
{
	union {
		Int8	h64;
		UInt4	i32[2];
	} u;
	Int8 result;

	u.h64 = n64;
	result = ntohl(u.i32[0]);
	result <<= 32;
	result |= ntohl(u.i32[1]);

	return result;
}


int
CC_send_function(ConnectionClass *self, const char *fn_name, void *result_buf, int *actual_result_len, int result_is_int, LO_ARG *args, int nargs)
{
	int			i;
	int			ret = FALSE;
	int			func_cs_count = 0;
	char		sqlbuffer[1000];
	PGresult   *pgres = NULL;
	Oid			paramTypes[MAX_SEND_FUNC_ARGS];
	char	   *paramValues[MAX_SEND_FUNC_ARGS];
	int			paramLengths[MAX_SEND_FUNC_ARGS];
	int			paramFormats[MAX_SEND_FUNC_ARGS];
	Int4		intParamBufs[MAX_SEND_FUNC_ARGS];
	Int8		int8ParamBufs[MAX_SEND_FUNC_ARGS];

	MYLOG(0, "conn=%p, fn_name=%s, result_is_int=%d, nargs=%d\n", self, fn_name, result_is_int, nargs);

	/* Finish the pending extended query first */
#define	return DONT_CALL_RETURN_FROM_HERE???
	ENTER_INNER_CONN_CS(self, func_cs_count);

	SPRINTF_FIXED(sqlbuffer, "SELECT pg_catalog.%s%s", fn_name,
			 func_param_str[nargs]);
	for (i = 0; i < nargs; ++i)
	{
		MYLOG(0, "  arg[%d]: len = %d, isint = %d, integer = " FORMATI64 ", ptr = %p\n", i, args[i].len, args[i].isint, args[i].isint == 2 ? args[i].u.integer64 : args[i].u.integer, args[i].u.ptr);
		/* integers are sent as binary, others as text */
		if (args[i].isint == 2)
		{
			paramTypes[i] = PG_TYPE_INT8;
			int8ParamBufs[i] = odbc_hton64(args[i].u.integer64);
			paramValues[i] = (char *) &int8ParamBufs[i];
			paramLengths[i] = 8;
			paramFormats[i] = 1;
		}
		else if (args[i].isint)
		{
			paramTypes[i] = PG_TYPE_INT4;
			intParamBufs[i] = htonl(args[i].u.integer);
			paramValues[i] = (char *) &intParamBufs[i];
			paramLengths[i] = 4;
			paramFormats[i] = 1;
		}
		else
		{
			paramTypes[i] = 0;
			paramValues[i] = args[i].u.ptr;
			paramLengths[i] = args[i].len;
			paramFormats[i] = 1;
		}
	}

	QLOG(0, "PQexecParams: %p '%s' nargs=%d\n", self->pqconn, sqlbuffer, nargs);
	pgres = PQexecParams(self->pqconn, sqlbuffer, nargs,
						 paramTypes, (const char * const *) paramValues,
						 paramLengths, paramFormats, 1);

	MYLOG(0, "done sending function\n");

	if (PQresultStatus(pgres) == PGRES_TUPLES_OK)
		QLOG(0, "\tok: - 'T' - %s\n", PQcmdStatus(pgres));
	else
	{
		handle_pgres_error(self, pgres, "send_query", NULL, TRUE);
		goto cleanup;
	}

	if (PQnfields(pgres) != 1 || PQntuples(pgres) != 1)
	{
		CC_set_errormsg(self, "unexpected result set from large_object function");
		goto cleanup;
	}

	*actual_result_len = PQgetlength(pgres, 0, 0);

	QLOG(0, "\tgot result with length: %d\n", *actual_result_len);

	if (*actual_result_len > 0)
	{
		char *value = PQgetvalue(pgres, 0, 0);
		if (result_is_int == 2)
		{
			Int8 int8val;
			memcpy(&int8val, value, sizeof(Int8));
			int8val = odbc_ntoh64(int8val);
			memcpy(result_buf, &int8val, sizeof(Int8));
MYLOG(0, "int8 result=" FORMATI64 "\n", int8val);
		}
		else if (result_is_int)
		{
			Int4 int4val;
			memcpy(&int4val, value, sizeof(Int4));
			int4val = ntohl(int4val);
			memcpy(result_buf, &int4val, sizeof(Int4));
		}
		else
			memcpy(result_buf, value, *actual_result_len);
	}

	ret = TRUE;

cleanup:
#undef	return
	CLEANUP_FUNC_CONN_CS(func_cs_count, self);
	if (pgres)
		PQclear(pgres);
	return ret;
}


char
CC_send_settings(ConnectionClass *self, const char *set_query)
{
	HSTMT		hstmt;
	RETCODE		result;
	char		status = TRUE;
	char	   *cs,
			   *ptr;
#ifdef	HAVE_STRTOK_R
	char	*last;
#endif /* HAVE_STRTOK_R */
	CSTR func = "CC_send_settings";


	MYLOG(0, "entering...\n");

	if (set_query == NULL) return TRUE;

/*
 *	This function must use the local odbc API functions since the odbc state
 *	has not transitioned to "connected" yet.
 */

	result = PGAPI_AllocStmt(self, &hstmt, 0);
	if (!SQL_SUCCEEDED(result))
		return FALSE;

	/* non-external handle ensures no BEGIN/COMMIT/ABORT stuff */

	cs = strdup(set_query);
	if (cs == NULL)
	{
		CC_set_error(self, CONN_NO_MEMORY_ERROR, "Couldn't alloc buffer for query.", func);
		return FALSE;
	}

#ifdef	HAVE_STRTOK_R
	ptr = strtok_r(cs, ";", &last);
#else
	ptr = strtok(cs, ";");
#endif /* HAVE_STRTOK_R */
	while (ptr)
	{
		result = PGAPI_ExecDirect(hstmt, (SQLCHAR *) ptr, SQL_NTS, 0);
		if (!SQL_SUCCEEDED(result))
			status = FALSE;

		MYLOG(0, "result %d, status %d from '%s'\n", result, status, ptr);

#ifdef	HAVE_STRTOK_R
		ptr = strtok_r(NULL, ";", &last);
#else
		ptr = strtok(NULL, ";");
#endif /* HAVE_STRTOK_R */
	}
	free(cs);

	PGAPI_FreeStmt(hstmt, SQL_DROP);

	return status;
}


/*
 *	This function is just a hack to get the oid of our Large Object oid type.
 *	If a real Large Object oid type is made part of Postgres, this function
 *	will go away and the define 'PG_TYPE_LO' will be updated.
 */
static SQLRETURN
CC_lookup_lo(ConnectionClass *self)
{
	SQLRETURN	ret = SQL_SUCCESS;
	QResultClass	*res;

	MYLOG(0, "entering...\n");

	res = CC_send_query(self, "select oid, typbasetype from pg_type where typname = '"  PG_TYPE_LO_NAME "'",
		NULL, READ_ONLY_QUERY, NULL);

	if (!QR_command_maybe_successful(res))
		ret = SQL_ERROR;
	else if (QR_command_maybe_successful(res) && QR_get_num_cached_tuples(res) > 0)
	{
		OID	basetype;

		self->lobj_type = QR_get_value_backend_int(res, 0, 0, NULL);
		basetype = QR_get_value_backend_int(res, 0, 1, NULL);
		if (PG_TYPE_OID == basetype)
			self->lo_is_domain = 1;
		else if (0 != basetype)
			self->lobj_type = 0;
	}
	QR_Destructor(res);
	MYLOG(0, "Got the large object oid: %d\n", self->lobj_type);
	return ret;
}


/*
 *	This function initializes the version of PostgreSQL from
 *	connInfo.protocol that we're connected to.
 *	h-inoue 01-2-2001
 */
void
CC_initialize_pg_version(ConnectionClass *self)
{
	STRCPY_FIXED(self->pg_version, "7.4");
	self->pg_version_major = 7;
	self->pg_version_minor = 4;
}


void
CC_log_error(const char *func, const char *desc, const ConnectionClass *self)
{
#define NULLCHECK(a) (a ? a : "(NULL)")

	if (self)
	{
		MYLOG(0, "CONN ERROR: func=%s, desc='%s', errnum=%d, errmsg='%s'\n", func, desc, self->__error_number, NULLCHECK(self->__error_message));
		MYLOG(DETAIL_LOG_LEVEL, "            ------------------------------------------------------------\n");
		MYLOG(DETAIL_LOG_LEVEL, "            henv=%p, conn=%p, status=%u, num_stmts=%d\n", self->henv, self, self->status, self->num_stmts);
		MYLOG(DETAIL_LOG_LEVEL, "            pqconn=%p, stmts=%p, lobj_type=%d\n", self->pqconn, self->stmts, self->lobj_type);
	}
	else
	{
		MYLOG(0, "INVALID CONNECTION HANDLE ERROR: func=%s, desc='%s'\n", func, desc);
	}
}

/*
 *	This doesn't really return the CURRENT SCHEMA
 *	but there's no alternative.
 */
const char *
CC_get_current_schema(ConnectionClass *conn)
{
	if (!conn->current_schema_valid)
	{
		QResultClass	*res;

		if (res = CC_send_query(conn, "select current_schema()", NULL, READ_ONLY_QUERY, NULL), QR_command_maybe_successful(res))
		{
			if (QR_get_num_total_tuples(res) == 1)
			{
				char *curschema = QR_get_value_backend_text(res, 0, 0);
				if (curschema)
					conn->current_schema = strdup(curschema);
			}
			if (conn->current_schema)
				conn->current_schema_valid = TRUE;
		}
		QR_Destructor(res);
	}
	return (const char *) conn->current_schema;
}

int	CC_mark_a_object_to_discard(ConnectionClass *conn, int type, const char *plan)
{
	int	cnt = conn->num_discardp + 1, plansize;
	char	*pname;

	CC_REALLOC_return_with_error(conn->discardp, char *,
		(cnt * sizeof(char *)), conn, "Couldn't alloc discardp.", -1);
	plansize = strlen(plan) + 2;
	CC_MALLOC_return_with_error(pname, char, plansize,
		conn, "Couldn't alloc discardp mem.", -1);
	pname[0] = (char) type;	/* 's':prepared statement 'p':cursor */
	strncpy_null(pname + 1, plan, plansize - 1);
	conn->discardp[conn->num_discardp++] = pname;

	return 1;
}

int	CC_discard_marked_objects(ConnectionClass *conn)
{
	int	i, cnt;
	QResultClass *res;
	char	*pname, cmd[64];

	if ((cnt = conn->num_discardp) <= 0)
		return 0;
	for (i = cnt - 1; i >= 0; i--)
	{
		pname = conn->discardp[i];
		if ('s' == pname[0])
			SPRINTF_FIXED(cmd, "DEALLOCATE \"%s\"", pname + 1);
		else
			SPRINTF_FIXED(cmd, "CLOSE \"%s\"", pname + 1);
		res = CC_send_query(conn, cmd, NULL, ROLLBACK_ON_ERROR | IGNORE_ABORT_ON_CONN | READ_ONLY_QUERY, NULL);
		QR_Destructor(res);
		free(conn->discardp[i]);
		/* CodeDEX with CID=12044 */
		conn->discardp[i] = NULL;
		conn->num_discardp--;
	}

	return 1;
}

static void
LIBPQ_update_transaction_status(ConnectionClass *self)
{
	if (!self->pqconn)
		return;

	switch (PQtransactionStatus(self->pqconn))
	{
		case PQTRANS_IDLE:
			if (CC_is_in_trans(self))
			{
				if (CC_is_in_error_trans(self))
					CC_on_abort(self, NO_TRANS);
				else
					CC_on_commit(self);
			}
			break;

		case PQTRANS_INTRANS:
			CC_set_in_trans(self);
			if (CC_is_in_error_trans(self))
			{
				CC_set_no_error_trans(self);
				CC_on_abort_partial(self);
			}
			break;

		case PQTRANS_INERROR:
			CC_set_in_trans(self);
			CC_set_in_error_trans(self);
			break;

		case PQTRANS_ACTIVE:
			/*
			 * A query is still executing. It might have already aborted,
			 * but all we know for sure is that we're in a transaction.
			 */
			CC_set_in_trans(self);
			break;

		default: 			/* unknown status */
			break;
	}
}

static void CC_getLibpath(char *libpath, int libpathLen)
{
	const char *fname = NULL;
	int fnameIndex = 0;
	int libpathIndex = 0;

#ifndef WIN32
	Dl_info dl_info;
	int ret = 0;
    if (libpath == NULL || libpathLen <= 0)
    {
        return ;
    }

    ret = dladdr((void*)CC_getLibpath, &dl_info);

	if (ret != 0)
	{
		fname = dl_info.dli_fname;
	}
#else
	MEMORY_BASIC_INFORMATION mbi;
	char fpath[4096] = {'\0'};
    if (libpath == NULL || libpathLen <= 0)
    {
        return ;
    }

	if ((VirtualQuery(CC_getLibpath, &mbi, sizeof(mbi)) != 0) &&
		GetModuleFileName((HMODULE)mbi.AllocationBase, fpath, sizeof(fpath)))
	{
		fname = fpath;
	}	
#endif

	while ((fname != NULL) && (fname[fnameIndex] != '\0') && (libpathIndex < libpathLen - 1))
	{
		if (fname[fnameIndex] == '\'')
		{
			libpath[libpathIndex++] = '\'';
		}
		else if ((fname[fnameIndex] == '"') || (fname[fnameIndex] == '\\'))
		{
			libpath[libpathIndex++] = '\\';
		}
		libpath[libpathIndex++] = fname[fnameIndex++];
	}
	libpath[libpathIndex] = '\0';
}

static void CC_getOSUser(char *username, int usernameLen)
{
#ifndef WIN32
	struct passwd *pw = NULL;

	if (username == NULL || usernameLen <= 0)
	{
		return ;
	}
    
	pw = getpwuid(geteuid());
	if (pw == NULL)
	{
		username[0] = '\0';
	}
	else
	{
		strncpy(username, pw->pw_name, strlen(pw->pw_name));
	}
#else
	DWORD len = usernameLen;
    
	if (username == NULL || usernameLen <= 0)
	{
		return ;
    }
    
	if(!GetUserName(username, &len))
	{
		username[0] = '\0';
		return ;
	}
#endif
}

#define        PROTOCOL3_OPTS_MAX      30

int split_host_or_port_with_limit(const char *str, char *result_array[MAX_PARTS], int* total_length)
{
    int count = 0;
    char *lasts = NULL;
    char *dup_str = strdup(str);
    
    char *token = strtok_r(dup_str, ",", &lasts);
    while (token && count < MAX_PARTS) {
        result_array[count] = strdup(token);
        if (!result_array[count]) {
            for (int i = 0; i < count; i++) {
                free(result_array[i]);
            }
            free(dup_str);
            return -1;
        }
        *total_length += strlen(result_array[count]);
        count++;
        token = strtok_r(NULL, ",", &lasts);
    }

    free(dup_str);
    for (int i = count;i < MAX_PARTS;i++) {
        result_array[i] = NULL;
    }
    return count;
}

char* generate_conninfo_URL_by_ConnInfo(ConnInfo* ci, int* host_number, int* port_number)
{
    char *host_array[MAX_PARTS] = {0};
    char *port_array[MAX_PARTS] = {0};
    int host_length = 0;
    int port_length = 0;
    if (!ci->server || !ci->port) {
        return NULL;
    }
    *host_number = split_host_or_port_with_limit(ci->server, host_array, &host_length);
    *port_number = split_host_or_port_with_limit(ci->port, port_array, &port_length);

    if ((-1 == *host_number || -1 == *port_number) ||
        ((*host_number != *port_number) && *host_number != 1 && *port_number != 1)) {
        for (int i = 0; i < MAX_PARTS; i++) {
            if (host_array[i] != NULL) {
                free(host_array[i]);
            }
            if (port_array[i] != NULL) {
                free(port_array[i]);
            }
        }
        return NULL;
    }

    size_t total_length = host_length + port_length + *host_number * 2 + *port_number * 2 +
                          (ci->username ? strlen(ci->username) : 0) +
                          (ci->password.name ? strlen(ci->password.name) : 0) +
                          (ci->database ? strlen(ci->database) : 0) + EXTRA_ROOM;
    char* temp_URL = (char*)malloc(total_length);
    if (!temp_URL) {
        return NULL;
    }
    memset(temp_URL, 0, total_length);
    int valid_count = *host_number > *port_number ? *host_number : *port_number;

    (void)snprintf(temp_URL, -1, "postgres://%s@", ci->username);
    for (int i = 0; i < valid_count; i++) {
        strcat(temp_URL, (*host_number == 1) ? host_array[0] : host_array[i]);
        strcat(temp_URL, ":");
        strcat(temp_URL, (*port_number == 1) ? port_array[0] : port_array[i]);
        if (i != valid_count - 1) {
            strcat(temp_URL, ",");
        }
    }
    strcat(temp_URL, "/");
    strcat(temp_URL, ci->database);
    char target_session_attrs[MEDIUM_REGISTRY_LEN] = "?target_session_attrs=";
    if (*host_number == 1 && *port_number == 1) {
        strcat(target_session_attrs, "any");
    } else if ('\0' == ci->target_session_attrs[0]) {
        strcat(target_session_attrs, "read-write");
    } else {
        strcat(target_session_attrs, ci->target_session_attrs);
    }
    strcat(temp_URL, target_session_attrs);
    strcat(temp_URL, "&password=");
    strcat(temp_URL, ci->password.name);
    if ('\0' != ci->sslmode) {
        strcat(temp_URL, "&sslmode=");
        strcat(temp_URL, ci->sslmode);
    }
    if ('\0' != ci->connect_timeout[0]) {
        char connect_timeout[MEDIUM_REGISTRY_LEN] = "&connect_timeout=";
        strcat(connect_timeout, ci->connect_timeout);
        strcat(temp_URL, connect_timeout);
    }
    if ('\0' != ci->rw_timeout[0]) {
        char rw_timeout[MEDIUM_REGISTRY_LEN] = "&rw_timeout=";
        strcat(rw_timeout, ci->rw_timeout);
        strcat(temp_URL, rw_timeout);
    }
    return temp_URL;
}

static int
LIBPQ_connect(ConnectionClass *self)
{
	CSTR		func = "LIBPQ_connect";
	ConnInfo	*ci = &(self->connInfo);
	char		ret = 0;
	void	   *pqconn = NULL;
	int			pqret;
	int			pversion;
	const	char	*opts[PROTOCOL3_OPTS_MAX], *vals[PROTOCOL3_OPTS_MAX];
	PQconninfoOption	*conninfoOption = NULL, *pqopt;
	int			i, cnt;
	char		login_timeout_str[20];
	char		keepalive_idle_str[20];
	char		keepalive_interval_str[20];
	char		*errmsg = NULL;
	char		local_conninfo[8192];
    int         host_number = 1;
    int         port_number = 1;
    char*       URL = NULL;
	
	MYLOG(0, "connecting to the database using %s as the server and pqopt={%s}\n", self->connInfo.server, SAFE_NAME(ci->pqopt));

	if (NULL == (conninfoOption = PQconninfoParse(SAFE_NAME(ci->pqopt), &errmsg)))
	{
		char emsg[200];

		if (errmsg != NULL)
			SPRINTF_FIXED(emsg, "libpq connection parameter error:%s", errmsg);
		else
			STRCPY_FIXED(emsg, "memory error? in PQconninfoParse");
		CC_set_error(self, CONN_OPENDB_ERROR, emsg, func);
		goto cleanup;
	}
    /* multiple_hostip or multiple_port from DSN */
    URL = generate_conninfo_URL_by_ConnInfo(ci, &host_number, &port_number);
    if (!URL) {
        if (!ci->server || !ci->port) {
            CC_set_error(self, CONN_INVALID_ARGUMENT_NO, "The server or port should not be empty.", func);
        } else if (-1 == host_number || -1 == port_number) {
            CC_set_error(self, CONN_NO_MEMORY_ERROR, "Memory allocation failure when resolving address.", func);
        } else if ((host_number != port_number) && host_number != 1 && port_number != 1) {
            CC_set_error(self, CONN_VALUE_OUT_OF_RANGE,
                         "The number of hosts should be the same as the number of ports when both are multiple.", func);
        } else {
            CC_set_error(self, CONN_NO_MEMORY_ERROR,
                         "Memory allocation failure when Trying to splice strings.", func);
        }
    }
    if (host_number > 1 || port_number > 1) {
        MYLOG(0, "connecting to the database using URL: %s\n", URL);
        pqconn = PQconnectdb(URL);
    } else {
        /* Build arrays of keywords & values, for PQconnectDBParams */
        cnt = 0;
        if (ci->server[0]) {
            opts[cnt] = "host";
            vals[cnt++] = ci->server;
        }
        if (ci->port[0]) {
            opts[cnt] = "port";
            vals[cnt++] = ci->port;
        }
        if (ci->database[0]) {
            opts[cnt] = "dbname";
            vals[cnt++] = ci->database;
        }
        if (ci->username[0]) {
            opts[cnt] = "user";
            vals[cnt++] = ci->username;
        }
        switch (ci->sslmode[0]) {
            case '\0':
                break;
            case SSLLBYTE_VERIFY:
                opts[cnt] = "sslmode";
                switch (ci->sslmode[1]) {
                    case 'f':
                        vals[cnt++] = SSLMODE_VERIFY_FULL;
                        break;
                    case 'c':
                        vals[cnt++] = SSLMODE_VERIFY_CA;
                        break;
                    default:
                        vals[cnt++] = ci->sslmode;
                }
                break;
            default:
                opts[cnt] = "sslmode";
                vals[cnt++] = ci->sslmode;
        }
        if (NAME_IS_VALID(ci->password)) {
            opts[cnt] = "password";
            vals[cnt++] = SAFE_NAME(ci->password);
        }
        if (ci->disable_keepalive) {
            opts[cnt] = "keepalives";
                vals[cnt++] = "0";
        }
        if (ci->connect_timeout[0]) {
            opts[cnt] = "connect_timeout";
            vals[cnt++] = ci->connect_timeout;
        }
        if (ci->rw_timeout[0]) {
            opts[cnt] = "rw_timeout";
            vals[cnt++] = ci->rw_timeout;
        }
        if (self->connInfo.keepalive_idle > 0) {
            ITOA_FIXED(keepalive_idle_str, self->connInfo.keepalive_idle);
            opts[cnt] = "keepalives_idle";
            vals[cnt++] = keepalive_idle_str;
        }
        if (self->connInfo.keepalive_interval > 0) {
            ITOA_FIXED(keepalive_interval_str, self->connInfo.keepalive_interval);
            opts[cnt] = "keepalives_interval";
            vals[cnt++] = keepalive_interval_str;
        }
        if ((odbcVersionString != NULL) && (odbcVersionString[0] != '\0')) {
            if (self->connInfo.connection_extra_info > 0) {
                char libpath[4096] = {'\0'};
                char username[128] = {'\0'};

                (void)CC_getLibpath(libpath, sizeof(libpath));
                (void)CC_getOSUser(username, sizeof(username));

                snprintf(local_conninfo, sizeof(local_conninfo),
                "{\"driver_name\":\"ODBC\", \"driver_version\":\"%s\", \"driver_path\":\"%s\", \"os_user\":\"%s\"}",
                odbcVersionString, libpath, username);
            } else {
                snprintf(local_conninfo, sizeof(local_conninfo),
                        "{\"driver_name\":\"ODBC\",\"driver_version\":\"%s\"}",
                        odbcVersionString);
            }
            opts[cnt] = "connection_info";
            vals[cnt++] = local_conninfo;
        }
        if (conninfoOption != NULL) {
            const char *keyword, *val;
            int j;

            for (i = 0, pqopt = conninfoOption; (keyword = pqopt->keyword) != NULL; i++, pqopt++) {
                if ((val = pqopt->val) != NULL) {
                    for (j = 0; j < cnt; j++) {
                        if (stricmp(opts[j], keyword) == 0) {
                            char emsg[100];
                            if (vals[j] != NULL && strcmp(vals[j], val) == 0) {
                                continue;
                            }
                            SPRINTF_FIXED(emsg,
                                          "%s parameter in pqopt option conflicts with other ordinary option", keyword);
                            CC_set_error(self, CONN_OPENDB_ERROR, emsg, func);
                            goto cleanup;
                        }
                    }
                    if (j >= cnt && cnt < PROTOCOL3_OPTS_MAX - 1) {
                        opts[cnt] = keyword;
                        vals[cnt++] = val;
                    }
                }
            }
        }
        opts[cnt] = vals[cnt] = NULL;
        /* Ok, we're all set to connect */

        if (get_qlog() > 0 || get_mylog() > 0) {
            const char **popt, **pval;
            QLOG(0, "PQconnectdbParams:");
            for (popt = opts, pval = vals; *popt; popt++, pval++) {
                QPRINTF(0, " %s='%s'", *popt, *pval);
            }
            QPRINTF(0, "\n");
        }
        pqconn = PQconnectdbParams(opts, vals, FALSE);
    }
    free(URL);

	if (!pqconn)
	{
		CC_set_error(self, CONN_OPENDB_ERROR, "PQconnectdb error", func);
		goto cleanup;
	}
	self->pqconn = pqconn;

	pqret = PQstatus(pqconn);
	if (pqret == CONNECTION_BAD && PQconnectionNeedsPassword(pqconn))
	{
		const char	*errmsg;

		MYLOG(0, "password retry\n");
		errmsg = PQerrorMessage(pqconn);
		CC_set_error(self, CONNECTION_SERVER_NOT_REACHED, errmsg, func);
		QLOG(0, "PQfinish: %p\n", pqconn);
		PQfinish(pqconn);
		self->pqconn = NULL;
		self->connInfo.password_required = TRUE;
		ret = -1;
		goto cleanup;
	}

	if (CONNECTION_OK != pqret)
	{
		const char	*errmsg;
MYLOG(DETAIL_LOG_LEVEL, "status=%d\n", pqret);
		errmsg = PQerrorMessage(pqconn);
		CC_set_error(self, CONNECTION_SERVER_NOT_REACHED, errmsg, func);
		MYLOG(0, "Could not establish connection to the database; LIBPQ returned -> %s\n", errmsg);
		goto cleanup;
	}

	if (PQpass(pqconn) && strlen(PQpass(pqconn)))
	{
		char *pwd = PQpass(pqconn);
		memset(pwd, 0, strlen(pwd));
	}

	MYLOG(0, "libpq connection to the database established.(IP: %s)\n", PQhost(pqconn));
	pversion = PQprotocolVersion(pqconn);
	if (pversion < 3)
	{
		MYLOG(0, "Protocol version %d is not supported\n", pversion);
		goto cleanup;
	}
	MYLOG(0, "protocol=%d\n", pversion);

	pversion = PQserverVersion(pqconn);
	self->pg_version_major = pversion / 10000;
	self->pg_version_minor = (pversion % 10000) / 100;
	SPRINTF_FIXED(self->pg_version, "%d.%d.%d",  self->pg_version_major, self->pg_version_minor, pversion % 100);

	MYLOG(0, "Server version=%s\n", self->pg_version);

	if (!CC_get_username(self)[0])
	{
		MYLOG(0, "PQuser=%s\n", PQuser(pqconn));
		STRCPY_FIXED(self->connInfo.username, PQuser(pqconn));
	}

	ret = 1;

cleanup:
	if (errmsg != NULL)
		free(errmsg);
	PQconninfoFree(conninfoOption);
	if (ret != 1)
	{
		if (self->pqconn)
		{
			QLOG(0, "PQfinish: %p\n", self->pqconn);
			PQfinish(self->pqconn);
		}
		self->pqconn = NULL;
	}

	MYLOG(0, "leaving %d\n", ret);
	return ret;
}

int
CC_send_cancel_request(const ConnectionClass *conn)
{
	int	ret = 0;
	char	errbuf[256];
	void	*cancel;

	/* Check we have an open connection */
	if (!conn || !conn->pqconn)
		return FALSE;

	cancel = PQgetCancel(conn->pqconn);
	if (!cancel)
		return FALSE;
	ret = PQcancel(cancel, errbuf, sizeof(errbuf));
	PQfreeCancel(cancel);
	if (1 == ret)
		return TRUE;
	else
		return FALSE;
}

const char *CurrCat(const ConnectionClass *conn)
{
	/*
	 * Returning the database name causes problems in MS Query. It
	 * generates query like: "SELECT DISTINCT a FROM byronnbad3
	 * bad3"
	 */
	if (isMsQuery())	/* MS Query */
		return NULL;
	return conn->connInfo.database;
}

const char *CurrCatString(const ConnectionClass *conn)
{
	const char *cat = CurrCat(conn);

	if (!cat)
		cat = NULL_STRING;
	return cat;
}

/*------
 *	Create a null terminated lower-case string if the
 *	original string contains upper-case characters.
 *	The SQL_NTS length is considered.
 *------
 */
SQLCHAR *
make_lstring_ifneeded(ConnectionClass *conn, const SQLCHAR *s, ssize_t len, BOOL ifallupper)
{
	ssize_t	length = len;
	char	   *str = NULL;
	const char *ccs = (const char *) s;

	if (s && (len > 0 || (len == SQL_NTS && (length = strlen(ccs)) > 0)))
	{
		int	i;
		UCHAR tchar;
		encoded_str encstr;

		make_encoded_str(&encstr, conn, ccs);
		for (i = 0; i < length; i++)
		{
			tchar = encoded_nextchar(&encstr);
			if (MBCS_NON_ASCII(encstr))
				continue;
			if (ifallupper && islower(tchar))
			{
				if (str)
				{
					free(str);
					str = NULL;
				}
				break;
			}
			if (tolower(tchar) != tchar)
			{
				if (!str)
				{
					str = malloc(length + 1);
					if (!str) return NULL;
					memcpy(str, s, length);
					str[length] = '\0';
				}
				str[i] = tolower(tchar);
			}
		}
	}

	return (SQLCHAR *) str;
}

/*
 *	Concatenate a single formatted argument to a given buffer handling the SQL_NTS thing.
 *	"fmt" must contain somewhere in it the single form '%.*s'.
 *	This is heavily used in creating queries for info routines (SQLTables, SQLColumns).
 *	This routine could be modified to use vsprintf() to handle multiple arguments.
 */
static int
my_str(char *buf, int buflen, const char *fmt, const char *s, ssize_t len)
{
	if (s && (len > 0 || (len == SQL_NTS && *s != 0)))
	{
		size_t	length = (len > 0) ? len : strlen(s);

		return snprintf(buf, buflen, fmt, length, s);
	}
	buf[0] = '\0';
	return 0;
}

int
schema_str(char *buf, int buflen, const SQLCHAR *s, SQLLEN len, BOOL table_is_valid, ConnectionClass *conn)
{
	CSTR	fmt = "%.*s";

	buf[0] = '\0';
	if (!s || 0 == len)
	{
		/*
		 * Note that this driver assumes the implicit schema is
		 * the CURRENT_SCHEMA() though it doesn't worth the
		 * naming.
		 */
		if (table_is_valid)
			return my_str(buf, buflen, fmt, CC_get_current_schema(conn), SQL_NTS);
		return 0;
	}
	return my_str(buf, buflen, fmt, (char *) s, len);
}

static void
my_appendPQExpBuffer(PQExpBufferData *buf, const char *fmt, const char *s, ssize_t len)
{
	if (s && (len > 0 || (len == SQL_NTS && *s != 0)))
	{
		size_t	length = (len > 0) ? len : strlen(s);

		appendPQExpBuffer(buf, fmt, length, s);
	}
}

/*
 *	my_appendPQExpBuffer1 is a extension of my_appendPQExpBuffer.
 *	It can have 1 more parameter than my_aapendPQExpBuffer.
 */
static void
my_appendPQExpBuffer1(PQExpBufferData *buf, const char *fmt, const char *s1, const char *s)
{
	if (s && s[0] != '\0')
	{
		ssize_t	length = strlen(s);

		if (s1)
			appendPQExpBuffer(buf, fmt, s1, length, s);
		else
			appendPQExpBuffer(buf, fmt, length, s);
	}
}

void
schema_appendPQExpBuffer(PQExpBufferData *buf, const char *fmt, const SQLCHAR *s, SQLLEN len, BOOL table_is_valid, ConnectionClass *conn)
{
	if (!s || 0 == len)
	{
		/*
		 * Note that this driver assumes the implicit schema is
		 * the CURRENT_SCHEMA() though it doesn't worth the
		 * naming.
		 */
		if (table_is_valid)
			my_appendPQExpBuffer(buf, fmt, CC_get_current_schema(conn), SQL_NTS);
		return;
	}
	my_appendPQExpBuffer(buf, fmt, (char *) s, len);
}

void
schema_appendPQExpBuffer1(PQExpBufferData *buf, const char *fmt, const char *s1, const char *s, BOOL table_is_valid, ConnectionClass *conn)
{
	if (!s || s[0] == '\0')
	{
		if (table_is_valid)
			my_appendPQExpBuffer1(buf, fmt, s1, CC_get_current_schema(conn));
		return;
	}
	my_appendPQExpBuffer1(buf, fmt, s1, s);
}

#ifdef	_HANDLE_ENLIST_IN_DTC_
	/*
	 *	Export the following functions so that the pgenlist dll
	 *	can handle ConnectionClass objects as opaque ones.
	 */

#define	_PGDTC_FUNCS_IMPLEMENT_
#include "connexp.h"

#define	SYNC_AUTOCOMMIT(conn)								\
	(SQL_AUTOCOMMIT_OFF != conn->autocommit_public ?		\
	 (conn->transact_status |= CONN_IN_AUTOCOMMIT) :		\
	 (conn->transact_status &= ~CONN_IN_AUTOCOMMIT))

DLL_DECLARE void PgDtc_create_connect_string(void *self, char *connstr, int strsize)
{
	ConnectionClass	*conn = (ConnectionClass *) self;
	ConnInfo *ci = &(conn->connInfo);
	const char *drivername = ci->drivername;
	char	xaOptStr[32];

#if defined(_WIN32) && !defined(_WIN64)
	/*
	 * If this is an x86 driver running on an x64 host then the driver name
	 * passed to MSDTC must be the (x64) driver but the client app will be
	 * using the 32-bit driver name. So MSDTC.exe will fail to find the driver
	 * and we'll fail to recover XA transactions.
	 *
	 * IsWow64Process(...) would be the ideal function for this, but is only
	 * available on Windows 6+ (Vista/2k8). We'd use GetNativeSystemInfo, which
	 * is supported on XP and 2k3, instead, but that won't link with older
	 * SDKs.
	 *
	 * It's impler to just test the PROCESSOR_ARCHITEW6432 environment
	 * variable.
	 *
	 * See http://www.postgresql.org/message-id/53A45B59.70303@2ndquadrant.com
	 * for details on this issue.
	 */
	const char * const procenv = getenv("PROCESSOR_ARCHITEW6432");
	if (procenv != NULL && strcmp(procenv, "AMD64") == 0)
	{
		/*
		 * We're a 32-bit binary running under SysWow64 on a 64-bit host and need
		 * to pass a different driver name.
		 *
		 * To avoid playing memory management games, just return a different
		 * string constant depending on the unicode-ness of the driver.
		 *
		 * It probably doesn't matter whether we use the Unicode or ANSI driver
		 * for the XA transaction manager, but pick the same as the client driver
		 * to keep things as similar as possible.
		 */
		if (strcmp(drivername, DBMS_NAME) == 0)
#ifdef UNICODE_SUPPORT
		drivername = DBMS_NAME_UNICODE"(x64)";
#else
		drivername = DBMS_NAME_ANSI"(x64)";
#endif
	}
#endif // _WIN32 &&  !_WIN64

	if (0 >= ci->xa_opt)	return;
	switch (ci->xa_opt)
	{
		case DTC_CHECK_LINK_ONLY:
		case DTC_CHECK_BEFORE_LINK:
			SPRINTF_FIXED(xaOptStr, KEYWORD_DTC_CHECK "=0;");
			break;
		case DTC_CHECK_RM_CONNECTION:
			SPRINTF_FIXED(xaOptStr, KEYWORD_DTC_CHECK "=1;");
			break;
		default:
			*xaOptStr = '\0';
			break;
	}
	snprintf(connstr, strsize,
			 "DRIVER={%s};%s"
			 "SERVER=%s;PORT=%s;DATABASE=%s;"
			 "UID=%s;PWD=%s;" ABBR_SSLMODE "=%s",
			 drivername, xaOptStr,
			 ci->server, ci->port, ci->database, ci->username,
			 SAFE_NAME(ci->password), ci->sslmode
		);
	return;
}

#define SECURITY_WIN32
#include <security.h>
DLL_DECLARE int PgDtc_is_recovery_available(void *self, char *reason, int rsize)
{
	ConnectionClass	*conn = (ConnectionClass *) self;
	ConnInfo *ci = &(conn->connInfo);
	int	ret = -1;	// inknown
	LONG	nameSize;
	char	loginUser[256];
	BOOL	outReason = FALSE;
	BOOL	doubtRootCert = TRUE, doubtCert = TRUE;
	const char *delim;

	/*
	 *	Root certificate is used?
	 */
	if (NULL != reason &&
	    rsize > 0)
		outReason = TRUE;
	/*
	 *	Root certificate is used?
	 */
	doubtRootCert = FALSE;
	if (0 == stricmp(ci->sslmode, SSLMODE_VERIFY_CA) ||
	    0 == stricmp(ci->sslmode, SSLMODE_VERIFY_FULL))
	{
		if (outReason)
			strncpy_null(reason, "sslmode verify-[ca|full]", rsize);
		return 0;
	}

	/*
	 * Did we use SSL client certificate, SSPI, Kerberos or similar
	 * authentication methods?
	 * There seems no way to check it directly.
	 */
	doubtCert = FALSE;
	if (PQgetssl(conn->pqconn) != NULL)
		doubtCert = TRUE;

	nameSize = sizeof(loginUser);
	if (GetUserNameEx(NameUserPrincipal, loginUser, &nameSize))
	{
		MYLOG(0, "loginUser=%s\n", loginUser);
	}
	else
	{
		int err = GetLastError();
		switch (err)
		{
			case ERROR_NONE_MAPPED:
				MYLOG(0, "The user name is unavailable in the specified format\n");
				break;
			case ERROR_NO_SUCH_DOMAIN:
				MYLOG(0, "The domain controller is unavailable to perform the lookup\n");
				break;
			case ERROR_MORE_DATA:
				MYLOG(0, "The buffer is too small\n");
				break;
			default:
				MYLOG(0, "GetUserNameEx error=%d\n", err);
				break;
		}
	}

	ret = 1;
	if (outReason)
		*reason = '\0';
	delim = "";
	if (doubtRootCert)
	{
		if (outReason)
			snprintf(reason, rsize, "%s%ssslmode verify-[ca|full]", reason, delim);
		delim = ", ";
		ret = -1;
	}
	if (doubtCert)
	{
		if (outReason)
			snprintf(reason, rsize, "%s%scertificate", reason, delim);
		delim = ", ";
		ret = -1;
	}
	return ret;
}

DLL_DECLARE void PgDtc_set_async(void *self, void *async)
{
	ConnectionClass	*conn = (ConnectionClass *) self;

	if (!conn)	return;
	CONNLOCK_ACQUIRE(conn);
	if (NULL != async)
		CC_set_autocommit(conn, FALSE);
	else
		SYNC_AUTOCOMMIT(conn);
	conn->asdum = async;
	CONNLOCK_RELEASE(conn);
}

DLL_DECLARE void	*PgDtc_get_async(void *self)
{
	ConnectionClass *conn = (ConnectionClass *) self;

	return conn->asdum;
}

DLL_DECLARE void PgDtc_set_property(void *self, int property, void *value)
{
	ConnectionClass *conn = (ConnectionClass *) self;

	CONNLOCK_ACQUIRE(conn);
	switch (property)
	{
		case inprogress:
			if (NULL != value)
				CC_set_dtc_executing(conn);
			else
				CC_no_dtc_executing(conn);
			break;
		case enlisted:
			if (NULL != value)
				CC_set_dtc_enlisted(conn);
			else
				CC_no_dtc_enlisted(conn);
			break;
		case prepareRequested:
			if (NULL != value)
				CC_set_dtc_prepareRequested(conn);
			else
				CC_no_dtc_prepareRequested(conn);
			break;
	}
	CONNLOCK_RELEASE(conn);
}

DLL_DECLARE void PgDtc_set_error(void *self, const char *message, const char *func)
{
	ConnectionClass *conn = (ConnectionClass *) self;

	CC_set_error(conn, CONN_UNSUPPORTED_OPTION, message, func);
}

DLL_DECLARE int PgDtc_get_property(void *self, int property)
{
	ConnectionClass *conn = (ConnectionClass *) self;
	int	ret;

	CONNLOCK_ACQUIRE(conn);
	switch (property)
	{
		case inprogress:
			ret = CC_is_dtc_executing(conn);
			break;
		case enlisted:
			ret = CC_is_dtc_enlisted(conn);
			break;
		case inTrans:
			ret = CC_is_in_trans(conn);
			break;
		case errorNumber:
			ret = CC_get_errornumber(conn);
			break;
		case idleInGlobalTransaction:
			ret = CC_is_idle_in_global_transaction(conn);
			break;
		case connected:
			ret = (CONN_CONNECTED == conn->status);
			break;
		case prepareRequested:
			ret = CC_is_dtc_prepareRequested(conn);
			break;
	}
	CONNLOCK_RELEASE(conn);
	return ret;
}

DLL_DECLARE BOOL PgDtc_connect(void *self)
{
	CSTR	func = "PgDtc_connect";
	ConnectionClass *conn = (ConnectionClass *) self;

	if (CONN_CONNECTED == conn->status)
		return TRUE;
	if (CC_connect(conn, NULL) <= 0)
	{
		/* Error messages are filled in */
		CC_log_error(func, "Error on CC_connect", conn);
		return FALSE;
	}
	return TRUE;
}

DLL_DECLARE void PgDtc_free_connect(void *self)
{
	ConnectionClass *conn = (ConnectionClass *) self;

	PGAPI_FreeConnect(conn);
}

DLL_DECLARE BOOL PgDtc_one_phase_operation(void *self, int operation)
{
	ConnectionClass *conn = (ConnectionClass *) self;
	BOOL	ret, is_in_progress = CC_is_dtc_executing(conn);

	if (!is_in_progress)
		CC_set_dtc_executing(conn);
	switch (operation)
	{
		case ONE_PHASE_COMMIT:
			ret = CC_commit(conn);
			break;
		default:
			ret = CC_abort(conn);
			break;
	}

	if (!is_in_progress)
		CC_no_dtc_executing(conn);

	return ret;
}

DLL_DECLARE BOOL
PgDtc_two_phase_operation(void *self, int operation, const char *gxid)
{
	ConnectionClass *conn = (ConnectionClass *) self;
	QResultClass	*qres;
	BOOL	ret = TRUE;
	char		cmd[512];

	switch (operation)
	{
		case PREPARE_TRANSACTION:
			SPRINTF_FIXED(cmd, "PREPARE TRANSACTION '%s'", gxid);
			break;
		case COMMIT_PREPARED:
			SPRINTF_FIXED(cmd, "COMMIT PREPARED '%s'", gxid);
			break;
		case ROLLBACK_PREPARED:
			SPRINTF_FIXED(cmd, "ROLLBACK PREPARED '%s'", gxid);
			break;
	}

	qres = CC_send_query(conn, cmd, NULL, 0, NULL);
	if (!QR_command_maybe_successful(qres))
		ret = FALSE;
	QR_Destructor(qres);
	return ret;
}

DLL_DECLARE BOOL
PgDtc_lock_cntrl(void *self, BOOL acquire, BOOL bTrial)
{
	ConnectionClass *conn = (ConnectionClass *) self;
	BOOL	ret = TRUE;

	if (acquire)
		if (bTrial)
			ret = TRY_ENTER_CONN_CS(conn);
		else
			ENTER_CONN_CS(conn);
	else
		LEAVE_CONN_CS(conn);

	return ret;
}

static ConnectionClass *
CC_Copy(const ConnectionClass *conn)
{
	ConnectionClass	*newconn = CC_alloc();

	if (newconn)
	{
		memcpy(newconn, conn, sizeof(ConnectionClass));
		CC_lockinit(newconn);
	}
	return newconn;
}

DLL_DECLARE void *
PgDtc_isolate(void *self, DWORD option)
{
	BOOL	disposingConn = (0 != (disposingConnection & option));
	ConnectionClass *sconn = (ConnectionClass *) self, *newconn = NULL;

	if (0 == (useAnotherRoom & option))
	{
		HENV	henv = sconn->henv;

		CC_cleanup(sconn, TRUE);
		if (newconn = CC_Copy(sconn), NULL == newconn)
			return newconn;
		MYLOG(0, "newconn=%p from %p\n", newconn, sconn);
		CC_initialize(sconn, FALSE);
		if (!disposingConn)
			CC_copy_conninfo(&sconn->connInfo, &newconn->connInfo);
		CC_initialize_pg_version(sconn);
		sconn->henv = henv;
		newconn->henv = NULL;
		SYNC_AUTOCOMMIT(sconn);
		return newconn;
	}
	newconn = CC_Constructor();
	if (!newconn) return NULL;
	CC_copy_conninfo(&newconn->connInfo, &sconn->connInfo);
	CC_initialize_pg_version(newconn);
	newconn->asdum = sconn->asdum;
	newconn->gTranInfo = sconn->gTranInfo;
	CC_set_dtc_isolated(newconn);
	sconn->asdum = NULL;
	SYNC_AUTOCOMMIT(sconn);
	CC_set_dtc_clear(sconn);
	MYLOG(0, "generated connection=%p with %p\n", newconn, newconn->asdum);

	return newconn;
}

#endif /* _HANDLE_ENLIST_IN_DTC_ */

BOOL
CC_set_transact(ConnectionClass *self, UInt4 isolation)
{
	char *query;
	QResultClass *res;
	BOOL	bShow = FALSE;

	if (PG_VERSION_LT(self, 8.0) &&
		(isolation == SQL_TXN_READ_UNCOMMITTED ||
		 isolation == SQL_TXN_REPEATABLE_READ))
	{
		CC_set_error(self, CONN_NOT_IMPLEMENTED_ERROR, "READ_UNCOMMITTED or REPEATABLE_READ is not supported by the server", __FUNCTION__);
		return FALSE;
	}

	switch (isolation)
	{
		case SQL_TXN_SERIALIZABLE:
			query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE";
						break;
		case SQL_TXN_REPEATABLE_READ:
			query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL REPEATABLE READ";
			break;
		case SQL_TXN_READ_UNCOMMITTED:
			query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
			break;
		default:
			query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ COMMITTED";
			break;
	}
	if (self->default_isolation == 0)
		bShow = TRUE;
	if (bShow)
		res = CC_send_query_append(self, ISOLATION_SHOW_QUERY, NULL, READ_ONLY_QUERY, NULL, query);
	else
		res = CC_send_query(self, query, NULL, READ_ONLY_QUERY, NULL);
	if (!QR_command_maybe_successful(res))
	{
		CC_set_error(self, CONN_EXEC_ERROR, "ISOLATION change request to the server error", __FUNCTION__);
		QR_Destructor(res);
		return FALSE;
	}
	if (bShow)
		handle_show_results(res);
	QR_Destructor(res);
	self->server_isolation = isolation;

	return TRUE;
}

