# Copyright Huawei Technologies Co., Ltd. 2010-2018. All rights reserved.
#!/bin/bash
MD="mkdir -p"
RM="rm -rf"
CP="cp -r"
ROOT_DIR=$LIB_GAUSSDB_DIR 
#ROOT_DIR=`pwd`/..
SRC_DIR=$ROOT_DIR/../open_source/opengauss/src
LIBPQ_WIN32_DIR=$ROOT_DIR/libpq-win32
PREPARED_DIR=$LIBPQ_WIN32_DIR
EXPORT_DIR=$LIBPQ_WIN32_DIR/libpq-export

### function ###
function copy_file()
{
file=$1 #file name
src=$2  #source directory
dst=$3  #target directory
if [ ! -f "$src"/"$file" ];then
    echo "Error: $src/$file doesn't exist, exit."
    exit -1
fi
if [ ! -d $dst ];then
    $MD $dst
fi
#echo "Copy $src/$file to $dst/$file"
$CP $src/$file $dst/$file
}

### function ###
function init_win_project()
{
   if [ ! -f "$PREPARED_DIR"/include/pg_config.h ]; then
      echo "Error: Header file pg_config.h for Windows lost, exit."
      exit -1
   fi
   if [ ! -f "$PREPARED_DIR"/include/pg_config_os.h ]; then
      echo "Error: Header file pg_config_os.h for Windows lost, exit."
      exit -1
   fi
   if [ ! -f "$PREPARED_DIR"/include/pg_config_paths.h ]; then
      echo "Error: Header file pg_config_paths.h for Windows lost, exit."
      exit -1
   fi
   if [ ! -f "$PREPARED_DIR"/include/errcodes.h ]; then
      echo "Error: Header file errcodes.h for Windows lost, exit."
      exit -1
   fi
   if [ ! -f "$PREPARED_DIR"/include/libpqdll.def ]; then
      echo "Error: Interface definition file libpqdll.def for Windows lost, exit."
      exit -1
   fi
   if [ ! -f "$PREPARED_DIR"/Makefile ];then
      echo "Error: Makefile for MinGW complier lost, exit."
      exit -1
   fi
   if [ ! -f "$PREPARED_DIR"/CMakeLists.txt ];then
      echo "Error: CMakeLists.txt for CMake && MinGW complier lost, exit."
      exit -1
   fi
#   $RM $LIBPQ_WIN32_DIR/include
   $RM $LIBPQ_WIN32_DIR/src
   if [ ! -d "$LIBPQ_WIN32_DIR"/lib ];then
       $MD $LIBPQ_WIN32_DIR/lib
   fi
}

### function ###
function copy_headers()
{ # copy header files
# copy form prepared directory
copy_file securec.h         $LIB_SECURITY_DIR/include $LIBPQ_WIN32_DIR/include
copy_file securectype.h     $LIB_SECURITY_DIR/include $LIBPQ_WIN32_DIR/include
#copy_file pg_config.h       $PREPARED_DIR $LIBPQ_WIN32_DIR/include
#copy_file pg_config_os.h    $PREPARED_DIR $LIBPQ_WIN32_DIR/include
# include
copy_file c.h	              $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file cipher.h 	      $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file datatypes.h 	      $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file getaddrinfo.h       $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file gs_thread.h 	      $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file gs_threadlocal.h    $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file pg_config_manual.h  $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file port.h              $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file postgres.h          $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file postgres_ext.h      $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file postgres_fe.h       $SRC_DIR/include $LIBPQ_WIN32_DIR/include
copy_file securec_check.h     $SRC_DIR/include $LIBPQ_WIN32_DIR/include
# include/access
copy_file attnum.h  $SRC_DIR/include/access $LIBPQ_WIN32_DIR/include/access
copy_file tupdesc.h $SRC_DIR/include/access $LIBPQ_WIN32_DIR/include/access
# include/catlog
copy_file genbki.h        $SRC_DIR/include/catalog $LIBPQ_WIN32_DIR/include/catalog
copy_file pg_attribute.h  $SRC_DIR/include/catalog $LIBPQ_WIN32_DIR/include/catalog
# include/client_logic
copy_file cache.h              $SRC_DIR/include/client_logic $LIBPQ_WIN32_DIR/include/client_logic
copy_file client_logic.h       $SRC_DIR/include/client_logic $LIBPQ_WIN32_DIR/include/client_logic
copy_file client_logic_enums.h $SRC_DIR/include/client_logic $LIBPQ_WIN32_DIR/include/client_logic
copy_file cstrings_map.h       $SRC_DIR/include/client_logic $LIBPQ_WIN32_DIR/include/client_logic
# include/gstrace 
copy_file gstrace_infra.h     $SRC_DIR/include/gstrace $LIBPQ_WIN32_DIR/include/gstrace
copy_file gstrace_infra_int.h $SRC_DIR/include/gstrace $LIBPQ_WIN32_DIR/include/gstrace
copy_file gstrace_tool.h      $SRC_DIR/include/gstrace $LIBPQ_WIN32_DIR/include/gstrace
# include/libcomm
#copy_file libcomm.h $SRC_DIR/include/libcomm $LIBPQ_WIN32_DIR/include/libcomm
# include/libpq
copy_file auth.h         $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file be-fsstubs.h   $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file cl_state.h     $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file crypt.h        $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file guc_vars.h     $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file hba.h          $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file ip.h           $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file libpq.h        $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file libpq-be.h     $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file libpq-events.h $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file libpq-fe.h     $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file libpq-fs.h     $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file libpq-int.h    $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file md5.h          $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file pqcomm.h       $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file pqexpbuffer.h  $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file pqformat.h     $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file pqsignal.h     $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
copy_file sha2.h         $SRC_DIR/include/libpq $LIBPQ_WIN32_DIR/include/libpq
# include/mb
copy_file pg_wchar.h $SRC_DIR/include/mb $LIBPQ_WIN32_DIR/include/mb
# include/nodes
copy_file nodes.h             $SRC_DIR/include/nodes $LIBPQ_WIN32_DIR/include/nodes
copy_file params.h            $SRC_DIR/include/nodes $LIBPQ_WIN32_DIR/include/nodes
copy_file parsenodes_common.h $SRC_DIR/include/nodes $LIBPQ_WIN32_DIR/include/nodes
copy_file pg_list.h           $SRC_DIR/include/nodes $LIBPQ_WIN32_DIR/include/nodes
copy_file primnodes.h         $SRC_DIR/include/nodes $LIBPQ_WIN32_DIR/include/nodes
copy_file value.h             $SRC_DIR/include/nodes $LIBPQ_WIN32_DIR/include/nodes
# include/parser
copy_file backslash_quotes.h $SRC_DIR/include/parser $LIBPQ_WIN32_DIR/include/parser
# include/port/win32
copy_file dlfcn.h  $SRC_DIR/include/port/win32 $LIBPQ_WIN32_DIR/include/port/win32
copy_file grp.h    $SRC_DIR/include/port/win32 $LIBPQ_WIN32_DIR/include/port/win32
copy_file netdb.h  $SRC_DIR/include/port/win32 $LIBPQ_WIN32_DIR/include/port/win32
copy_file pwd.h    $SRC_DIR/include/port/win32 $LIBPQ_WIN32_DIR/include/port/win32
# include/port/win32/arpa
copy_file inet.h   $SRC_DIR/include/port/win32/arpa $LIBPQ_WIN32_DIR/include/port/win32/arpa
# include/port/win32/netinet
copy_file in.h     $SRC_DIR/include/port/win32/netinet $LIBPQ_WIN32_DIR/include/port/win32/netinet
# include/port/win32/sys
copy_file socket.h $SRC_DIR/include/port/win32/sys $LIBPQ_WIN32_DIR/include/port/win32/sys
copy_file wait.h   $SRC_DIR/include/port/win32/sys $LIBPQ_WIN32_DIR/include/port/win32/sys
# include/port/win32_msvc
copy_file dirent.h $SRC_DIR/include/port/win32_msvc $LIBPQ_WIN32_DIR/include/port/win32_msvc
copy_file unistd.h $SRC_DIR/include/port/win32_msvc $LIBPQ_WIN32_DIR/include/port/win32_msvc
copy_file utime.h  $SRC_DIR/include/port/win32_msvc $LIBPQ_WIN32_DIR/include/port/win32_msvc
# include/port/win32_msvc/sys
copy_file file.h   $SRC_DIR/include/port/win32_msvc/sys $LIBPQ_WIN32_DIR/include/port/win32_msvc/sys
copy_file param.h  $SRC_DIR/include/port/win32_msvc/sys $LIBPQ_WIN32_DIR/include/port/win32_msvc/sys
copy_file time.h   $SRC_DIR/include/port/win32_msvc/sys $LIBPQ_WIN32_DIR/include/port/win32_msvc/sys
# include/storage 
copy_file spin.h   $SRC_DIR/include/storage $LIBPQ_WIN32_DIR/include/storage
# include/storage/lock
copy_file pg_sema.h   $SRC_DIR/include/storage/lock $LIBPQ_WIN32_DIR/include/storage/lock
copy_file s_lock.h    $SRC_DIR/include/storage/lock $LIBPQ_WIN32_DIR/include/storage/lock
# include/utils
copy_file be_module.h     $SRC_DIR/include/utils $LIBPQ_WIN32_DIR/include/utils
copy_file elog.h          $SRC_DIR/include/utils $LIBPQ_WIN32_DIR/include/utils
copy_file palloc.h        $SRC_DIR/include/utils $LIBPQ_WIN32_DIR/include/utils
copy_file pg_crc.h        $SRC_DIR/include/utils $LIBPQ_WIN32_DIR/include/utils
copy_file pg_crc_tables.h $SRC_DIR/include/utils $LIBPQ_WIN32_DIR/include/utils
copy_file syscall_lock.h  $SRC_DIR/include/utils $LIBPQ_WIN32_DIR/include/utils
# copy from prepared directory
copy_file errcodes.h      $PREPARED_DIR/include $LIBPQ_WIN32_DIR/include/utils # !!!! special source directory
}

function copy_codes()
{
# src/common/backend/libpq
copy_file ip.cpp   $SRC_DIR/common/backend/libpq $LIBPQ_WIN32_DIR/src/common/backend/libpq
copy_file md5.cpp  $SRC_DIR/common/backend/libpq $LIBPQ_WIN32_DIR/src/common/backend/libpq
copy_file sha2.cpp $SRC_DIR/common/backend/libpq $LIBPQ_WIN32_DIR/src/common/backend/libpq
# src/common/backend/utils/mb
copy_file encnames.cpp $SRC_DIR/common/backend/utils/mb $LIBPQ_WIN32_DIR/src/common/backend/utils/mb
copy_file wchar.cpp    $SRC_DIR/common/backend/utils/mb $LIBPQ_WIN32_DIR/src/common/backend/utils/mb
# src/common/interfaces/libpq
copy_file fe-auth.cpp       $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-auth.h         $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-connect.cpp    $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-exec.cpp       $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-lobj.cpp       $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-misc.cpp       $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-print.cpp      $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-protocol2.cpp  $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-protocol3.cpp  $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file fe-secure.cpp     $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file libpq-events.cpp  $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file pqexpbuffer.cpp   $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file pqsignal.cpp      $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file pqsignal.h        $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file pthread-win32.cpp $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file win32.cpp         $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
copy_file win32.h           $SRC_DIR/common/interfaces/libpq $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
# copy interface definition file from prepared directory
copy_file libpqdll.def      $PREPARED_DIR/include $LIBPQ_WIN32_DIR/src/common/interfaces/libpq
# src/common/interfaces/libpq/client_logic_cache
copy_file cache_loader.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cache_loader.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cached_column_manager.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cached_global_setting.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file column_settings_list.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file global_settings_list.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file icached_columns.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file types_to_oid.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cache_refresh_type.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cached_column_setting.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cached_setting.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file columns_list.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file icached_column.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file schemas_list.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cached_column.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file cached_columns.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file column_hook_executors_list.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file dataTypes.def \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file icached_column_manager.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
copy_file search_path_list.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_cache \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_cache
# src/common/interfaces/libpq/client_logic_common
copy_file client_logic_utils.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_common \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_common
copy_file col_full_name.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_common \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_common
copy_file cstring_oid_map.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_common \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_common
copy_file pg_client_logic_params.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_common \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_common
copy_file statement_data.h  \
	  $SRC_DIR/common/interfaces/libpq/client_logic_common \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_common
copy_file table_full_name.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_common \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_common
# src/common/interfaces/libpq/client_logic_hooks
copy_file abstract_hook_executor.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_hooks \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_hooks
copy_file column_hook_executor.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_hooks \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_hooks
copy_file global_hook_executor.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_hooks \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_hooks
copy_file hook_resource.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_hooks \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_hooks
copy_file hooks_factory.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_hooks \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_hooks
copy_file hooks_manager.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_hooks \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_hooks
# src/common/interfaces/libpq/client_logic_processor
copy_file create_stmt_processor.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file prepared_statement.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file processor_utils.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file raw_values_cont.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file stmt_processor.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file where_clause_processor.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file encryption_pre_process.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file prepared_statements_list.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file raw_value.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file raw_values_list.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
copy_file values_processor.h \
	  $SRC_DIR/common/interfaces/libpq/client_logic_processor \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/client_logic_processor
# src/common/interfaces/libpq/frontend_parser
copy_file datatypes.h \
	  $SRC_DIR/common/interfaces/libpq/frontend_parser \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/frontend_parser
copy_file Parser.h \
	  $SRC_DIR/common/interfaces/libpq/frontend_parser \
  	  $LIBPQ_WIN32_DIR/src/common/interfaces/libpq/frontend_parser
# src/common/port
copy_file chklocale.cpp         $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file dirent.cpp            $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file getaddrinfo.cpp       $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file gs_readdir.cpp        $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file gs_syscall_lock.cpp   $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file inet_net_ntop.cpp     $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file path.cpp              $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file pgsleep.cpp           $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file pthread-win32.h       $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file thread.cpp            $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file win32error.cpp        $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file cipher.cpp            $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file dirmod.cpp            $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file gs_env_r.cpp          $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file gs_strerror.cpp       $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file inet_aton.cpp         $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file noblock.cpp           $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file pgstrcasecmp.cpp      $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file strlcpy.cpp           $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file win32env.cpp          $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
copy_file win32setlocale.cpp    $SRC_DIR/common/port $LIBPQ_WIN32_DIR/src/common/port
# copy from prepared directory
copy_file pg_config_paths.h     $PREPARED_DIR/include $LIBPQ_WIN32_DIR/src/common/port
}

function copy_makefiles()
{
copy_file Makefile       $PREPARED_DIR $LIBPQ_WIN32_DIR
copy_file CMakeLists.txt $PREPARED_DIR $LIBPQ_WIN32_DIR
}

function generate_libpq_export_files()
{
copy_file auth.h         $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file be-fsstubs.h   $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file guc_vars.h     $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file libpq-be.h     $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file libpq-int.h    $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file pqformat.h     $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file be-fsstubs.h   $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file hba.h          $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file libpq-events.h $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file md5.h          $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file pqsignal.h     $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file cl_state.h     $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file ip.h           $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file libpq-fe.h     $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file pqcomm.h       $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file sha2.h         $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file crypt.h        $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file libpq.h        $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file libpq-fs.h     $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file pqexpbuffer.h    $LIBPQ_WIN32_DIR/include/libpq $EXPORT_DIR/include/libpq
copy_file gs_thread.h      $LIBPQ_WIN32_DIR/include $EXPORT_DIR/include
copy_file postgres_ext.h   $LIBPQ_WIN32_DIR/include $EXPORT_DIR/include
copy_file gs_threadlocal.h $LIBPQ_WIN32_DIR/include $EXPORT_DIR/include

if [ ! -d "$EXPORT_DIR"/lib ];then 
   $MD $EXPORT_DIR/lib
fi
}

### function ###
function __main__()
{
echo "Start."
echo -n "Step[1] - initialize libpq project directory for Windows ... "
init_win_project
echo "done."
echo -n "Step[2] - copy libpq header files ... "
copy_headers 
echo "done."
echo -n "Step[3] - copy libpq source code files ... "
copy_codes
echo "done."
echo -n "Step[4] - prepare Makefile && CMakeLists.txt files for compilation ... "
copy_makefiles
echo "done."
echo -n "Step[5] - generate export directory and files ... "
generate_libpq_export_files
echo "done."
echo "Done."
}

function __clean__()
{
$RM $LIBPQ_WIN32_DIR/include
$RM $LIBPQ_WIN32_DIR/lib
$RM $LIBPQ_WIN32_DIR/libpq-export
$RM $LIBPQ_WIN32_DIR/src
$RM $LIBPQ_WIN32_DIR/CMakeLists.txt
$RM $LIBPQ_WIN32_DIR/Makefile
}
### main ###
if [ $# -eq 0 ];then 
__main__
exit 0
fi
if [ $# -eq 1 -a "x$1" = "xclean" ];then 
__clean__
exit 0
fi

echo "Usage: \"sh $0\" for initialization or \"sh $0 clean\" for cleaning."
