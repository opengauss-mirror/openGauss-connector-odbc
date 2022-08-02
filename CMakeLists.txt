CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
PROJECT(PSQLODBC)

SET(CMAKE_VERBOSE_MAKEFILEON ON)

MESSAGE(STATUS "Building PSQLODBC Library.")

IF(NOT DEFINED OPENSSL_DIR)
	set(OPENSSL_DIR "D:\\Program_Files\\OpenSSL-Win32")
ENDIF(NOT DEFINED OPENSSL_DIR)
IF(NOT DEFINED MINGW_DIR)
	set(MINGW_DIR "D:\\buildtools\\mingw-8.1.0\\msys32\\mingw32")
ENDIF(NOT DEFINED MINGW_DIR)

SET(PWD_DIR ${CMAKE_CURRENT_SOURCE_DIR})
SET(LIBPQ_DIR ${PWD_DIR}/libpq)
SET(MINGW32_DIR ${MINGW_DIR}/"i686-w64-mingw32")
SET(LIBRARY_OUTPUT_PATH ${PWD_DIR}/output)

SET(DEF_FILE ${PWD_DIR}/psqlodbc.def)
SET(RC_FILE ${PWD_DIR}/psqlodbc.rc)

SET(CMAKE_CXX_COMPILER "${MINGW_DIR}/bin/i686-w64-mingw32-gcc")
SET(CMAKE_SHARED_LINKER_FLAGS "-static -Wl,--enable-stdcall-fixup") #-static-libgcc -static-libstdc++

IF(NOT DEFINED RELEASE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=gnu++11 -DWIN32 -D_MINGW32 -DUNICODE_SUPPORT -DWIN_MULTITHREAD_SUPPORT -DDRIVER_CURSOR_IMPLEMENT -fpermissive -fPIC -m32 -Wno-dev -w") 
ELSE(NOT DEFINED RELEASE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -std=gnu++11 -DWIN32 -D_MINGW32 -DUNICODE_SUPPORT -DWIN_MULTITHREAD_SUPPORT -DDRIVER_CURSOR_IMPLEMENT -fpermissive -fPIC -m32 -Wno-dev -w") 
ENDIF(NOT DEFINED RELEASE)

SET(CMAKE_RC_COMPILER "${MINGW_DIR}/bin/windres")
SET(CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILER} -Jrc -I${CMAKE_CURRENT_SOURCE_DIR} <SOURCE> <OBJECT>")

INCLUDE_DIRECTORIES(${PWD_DIR}
                    ${LIBPQ_DIR}/include
                    ${LIBPQ_DIR}/include/libpq
		    ${MINGW_DIR}/include
		    ${MINGW32_DIR}/include)

SET(SRC_LIST ${PWD_DIR}/bind.c
	     ${PWD_DIR}/columninfo.c
	     ${PWD_DIR}/connection.c
	     ${PWD_DIR}/convert.c
	     ${PWD_DIR}/descriptor.c
	     ${PWD_DIR}/dlg_specific.c
	     ${PWD_DIR}/dlg_wingui.c
	     ${PWD_DIR}/drvconn.c
	     ${PWD_DIR}/environ.c
	     ${PWD_DIR}/execute.c
	     ${PWD_DIR}/info.c
	     ${PWD_DIR}/inouealc.c
	     ${PWD_DIR}/loadlib.c
	     ${PWD_DIR}/lobj.c
	     ${PWD_DIR}/misc.c
	     ${PWD_DIR}/multibyte.c
     	     ${PWD_DIR}/mylog.c
	     ${PWD_DIR}/odbcapi.c
	     ${PWD_DIR}/odbcapi30.c
     	     ${PWD_DIR}/odbcapi30w.c
	     ${PWD_DIR}/odbcapiw.c
	     ${PWD_DIR}/options.c
	     ${PWD_DIR}/parse.c
	     ${PWD_DIR}/pgapi30.c
	     ${PWD_DIR}/pgtypes.c
	     ${PWD_DIR}/psqlodbc.c
	     ${PWD_DIR}/qresult.c
	     ${PWD_DIR}/results.c
	     ${PWD_DIR}/setup.c
	     ${PWD_DIR}/statement.c
	     ${PWD_DIR}/tuple.c
	     ${PWD_DIR}/win_unicode.c
	     ${RC_FILE})

LINK_DIRECTORIES(${MINGW_DIR}/bin
		 ${MINGW_DIR}/lib
		 ${MINGW32_DIR}/lib)

ADD_LIBRARY(OBJ_PSQLODBC OBJECT ${SRC_LIST}) 
SET_PROPERTY(TARGET OBJ_PSQLODBC PROPERTY POSITION_INDEPENDENT_CODE 1)

ADD_LIBRARY(LIB_PSQLODBC STATIC $<TARGET_OBJECTS:OBJ_PSQLODBC>)
SET_TARGET_PROPERTIES(LIB_PSQLODBC PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(LIB_PSQLODBC PROPERTIES OUTPUT_NAME "psqlodbc35w")
SET_TARGET_PROPERTIES(LIB_PSQLODBC PROPERTIES SUFFIX ".lib")

ADD_LIBRARY(DLL_PSQLODBC SHARED $<TARGET_OBJECTS:OBJ_PSQLODBC> ${DEF_FILE})
SET_TARGET_PROPERTIES(DLL_PSQLODBC PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(DLL_PSQLODBC PROPERTIES OUTPUT_NAME "psqlodbc35w")

FIND_LIBRARY(LIB_CRYPTO  NAMES crypto  PATHS ${OPENSSL_DIR}/lib)
FIND_LIBRARY(LIB_SSL 	 NAMES ssl     PATHS ${OPENSSL_DIR}/lib)
FIND_LIBRARY(LIB_SECUREC NAMES securec PATHS ${LIBPQ_DIR}/lib)
FIND_LIBRARY(LIB_LIBPQ   NAMES pq      PATHS ${LIBPQ_DIR}/lib)

TARGET_LINK_LIBRARIES(DLL_PSQLODBC
		      ${LIB_LIBPQ}
		      -lodbc32
		      -lodbccp32
		      -lwsock32
		      -lws2_32
		      -lsecur32
		      -lwinmm
		      -lsecurity
		      -lgdi32
		      ${LIB_CRYPTO} 
		      ${LIB_SSL}
		      ${LIB_SECUREC})
