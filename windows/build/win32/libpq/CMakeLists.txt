CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT(LIBPQ)

SET(CMAKE_VERBOSE_MAKEFILEON ON)

MESSAGE(STATUS "Building LIBPQ Library.")

IF(NOT DEFINED OPENSSL_DIR)
	set(OPENSSL_DIR "D:\\windows_odbc\\win32\\open_source\\output\\openssl-win32")
ENDIF(NOT DEFINED OPENSSL_DIR)
IF(NOT DEFINED MINGW_DIR)
	set(MINGW_DIR "D:\\buildtools\\mingw-8.1.0\\msys32\\mingw32")
ENDIF(NOT DEFINED MINGW_DIR)

SET(MINGW32_DIR ${MINGW_DIR}/"i686-w64-mingw32")
SET(PWD_DIR ${CMAKE_CURRENT_SOURCE_DIR})
SET(LIBRARY_OUTPUT_PATH ${PWD_DIR}/output)

SET(CMAKE_CXX_COMPILER "${MINGW_DIR}/bin/i686-w64-mingw32-c++")
SET(CMAKE_SHARED_LINKER_FLAGS "-static -fstack-protector-all") #-static-libgcc -static-libstdc++"

IF(NOT DEFINED RELEASE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DWIN32 -DFRONTEND -D_MINGW32 -fPIC -fPIE -pie -fpermissive -no-pthread -m32 -w")
ELSE(NOT DEFINED RELEASE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DWIN32 -DFRONTEND -D_MINGW32 -fPIC -fPIE -pie -fpermissive -no-pthread -m32 -w")
ENDIF(NOT DEFINED RELEASE)

SET(DEF_FILE ${PWD_DIR}/src/common/interfaces/libpq/libpqdll.def)

INCLUDE_DIRECTORIES(${PWD_DIR}/include
	            ${PWD_DIR}/include/port
		    ${PWD_DIR}/include/port/win32
		    ${PWD_DIR}/src
		    ${PWD_DIR}/src/common/interfaces/libpq/client_logic_processor
		    ${PWD_DIR}/src/common/interfaces/libpq/client_logic_hooks
		    ${PWD_DIR}/src/common/interfaces/libpq/client_logic_common
		    ${PWD_DIR}/src/common/interfaces/libpq/client_logic_cache
		    ${PWD_DIR}/src/common/port
		    ${PWD_DIR}/src/common/interfaces/libpq
		    ${PWD_DIR}/src/common/interfaces/libpq/frontend_parser
		    ${OPENSSL_DIR}/include
		    ${MINGW_DIR}/include
		    ${MINGW32_DIR}/include)

SET(SRC_LIST ${PWD_DIR}/src/common/port/win32setlocale.cpp
	     ${PWD_DIR}/src/common/port/win32env.cpp
	     ${PWD_DIR}/src/common/port/strlcpy.cpp
	     ${PWD_DIR}/src/common/port/pgstrcasecmp.cpp
	     ${PWD_DIR}/src/common/port/noblock.cpp
	     ${PWD_DIR}/src/common/port/inet_aton.cpp
	     ${PWD_DIR}/src/common/port/gs_strerror.cpp
	     ${PWD_DIR}/src/common/port/gs_env_r.cpp
	     ${PWD_DIR}/src/common/port/dirmod.cpp
	     ${PWD_DIR}/src/common/port/cipher.cpp
	     ${PWD_DIR}/src/common/port/win32error.cpp
	     ${PWD_DIR}/src/common/port/thread.cpp
	     ${PWD_DIR}/src/common/port/pgsleep.cpp
	     ${PWD_DIR}/src/common/port/path.cpp
	     ${PWD_DIR}/src/common/port/inet_net_ntop.cpp
	     ${PWD_DIR}/src/common/port/gs_syscall_lock.cpp
	     ${PWD_DIR}/src/common/port/gs_readdir.cpp
	     ${PWD_DIR}/src/common/port/getaddrinfo.cpp
	     ${PWD_DIR}/src/common/port/dirent.cpp
	     ${PWD_DIR}/src/common/port/chklocale.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/pthread-win32.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/pqexpbuffer.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-protocol3.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-misc.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-connect.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/win32.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-protocol2.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-lobj.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/pqsignal.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/libpq-events.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-secure.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-print.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-exec.cpp
	     ${PWD_DIR}/src/common/interfaces/libpq/fe-auth.cpp
	     ${PWD_DIR}/src/common/backend/utils/mb/encnames.cpp
	     ${PWD_DIR}/src/common/backend/utils/mb/wchar.cpp
	     ${PWD_DIR}/src/common/backend/libpq/sha2.cpp
	     ${PWD_DIR}/src/common/backend/libpq/md5.cpp
	     ${PWD_DIR}/src/common/backend/libpq/ip.cpp)

LINK_DIRECTORIES(${MINGW_DIR}/bin
		 ${MINGW_DIR}/lib
		 ${MINGW32_DIR}/lib)

ADD_LIBRARY(OBJ_LIBPQ OBJECT ${SRC_LIST}) 
SET_PROPERTY(TARGET OBJ_LIBPQ PROPERTY POSITION_INDEPENDENT_CODE 1)

ADD_LIBRARY(LIB_LIBPQ STATIC $<TARGET_OBJECTS:OBJ_LIBPQ>)
SET_TARGET_PROPERTIES(LIB_LIBPQ PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(LIB_LIBPQ PROPERTIES OUTPUT_NAME "libpq")
SET_TARGET_PROPERTIES(LIB_LIBPQ PROPERTIES SUFFIX ".lib")

ADD_LIBRARY(DLL_LIBPQ SHARED $<TARGET_OBJECTS:OBJ_LIBPQ> ${DEF_FILE})
SET_TARGET_PROPERTIES(DLL_LIBPQ PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(DLL_LIBPQ PROPERTIES OUTPUT_NAME "libpq")

FIND_LIBRARY(LIB_CRYPTO  NAMES crypto  PATHS ${OPENSSL_DIR}/lib)
FIND_LIBRARY(LIB_SSL 	 NAMES ssl     PATHS ${OPENSSL_DIR}/lib)
FIND_LIBRARY(LIB_SECUREC NAMES securec PATHS ${PWD_DIR}/lib)

TARGET_LINK_LIBRARIES(DLL_LIBPQ
		      -lwsock32
		      -lws2_32
		      -lsecur32
		      -lsecurity
		      -lpthread
		      ${LIB_CRYPTO} ${LIB_SSL}
		      ${LIB_SECUREC})

