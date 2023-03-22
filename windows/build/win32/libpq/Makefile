#################################################################
## Makefile for building libpq library on Windows using MinGW  ##
#################################################################

ifndef $(OPENSSL_DIR)
OPENSSL_DIR:=/d/windows_odbc/win32/open_source/output/openssl-win32
endif

ifndef $(MINGW_DIR)
MINGW_DIR:=/d/msys32/mingw32
endif

PWD_DIR:=$(shell pwd)
MINGW32_DIR:=$(MINGW_DIR)/i686-w64-mingw32
OUT_DIR:=$(PWD_DIR)/output
OBJ_DIR:=$(PWD_DIR)/obj

DLL_TARGET:=$(OUT_DIR)/libpq.dll
LIB_TARGET:=$(OUT_DIR)/libpq.lib

CC:=$(MINGW_DIR)/bin/i686-w64-mingw32-c++
AR:=$(MINGW_DIR)/bin/i686-w64-mingw32-gcc-ar

MD:=mkdir -p
RM:=rm -rf

DEFFILE:=$(PWD_DIR)/src/common/interfaces/libpq/libpqdll.def

CCFLAG:=-DWIN32 \
	-DFRONTEND \
	-D_MINGW32 \
	-fpermissive \
	-no-pthread \
	-m32 \
	-w \
	-fstack-protector-all \
	--dynamicbase

ifdef $(RELEASE) 
	CCFLAG:=$(CCFLAG) -O2 
else
	CCFLAG:=$(CCFLAG) -g 
endif

LFLAG:=-L$(OPENSSL_DIR)/lib \
       -L$(PWD_DIR)/lib \
       -L$(MINGW_DIR)/bin \
       -L$(MINGW32_DIR)/lib \
       -llibcrypto \
       -llibssl \
       -lwsock32 \
       -lws2_32 \
       -lsecur32 \
       -lsecurity \
       -lopenssl \
       -llibsecurec

INCLUDE:=-I$(PWD_DIR)/include \
	 -I$(PWD_DIR)/include/port \
	 -I$(PWD_DIR)/include/port/win32 \
	 -I$(PWD_DIR)/src \
	 -I$(PWD_DIR)/src/common/interfaces/libpq/client_logic_processor \
	 -I$(PWD_DIR)/src/common/interfaces/libpq/client_logic_hooks \
	 -I$(PWD_DIR)/src/common/interfaces/libpq/client_logic_common \
	 -I$(PWD_DIR)/src/common/interfaces/libpq/client_logic_cache \
	 -I$(PWD_DIR)/src/common/port \
	 -I$(PWD_DIR)/src/common/interfaces/libpq \
	 -I$(PWD_DIR)/src/common/interfaces/libpq/frontend_parser \
	 -I$(OPENSSL_DIR)/include \
	 -I$(MINGW_DIR)/include \
	 -I$(MINGW32_DIR)/include

SRC:=$(PWD_DIR)/src/common/port/win32setlocale.cpp \
     $(PWD_DIR)/src/common/port/win32env.cpp \
     $(PWD_DIR)/src/common/port/strlcpy.cpp \
     $(PWD_DIR)/src/common/port/pgstrcasecmp.cpp \
     $(PWD_DIR)/src/common/port/noblock.cpp \
     $(PWD_DIR)/src/common/port/inet_aton.cpp \
     $(PWD_DIR)/src/common/port/gs_strerror.cpp \
     $(PWD_DIR)/src/common/port/gs_env_r.cpp \
     $(PWD_DIR)/src/common/port/dirmod.cpp \
     $(PWD_DIR)/src/common/port/cipher.cpp \
     $(PWD_DIR)/src/common/port/win32error.cpp \
     $(PWD_DIR)/src/common/port/thread.cpp \
     $(PWD_DIR)/src/common/port/pgsleep.cpp \
     $(PWD_DIR)/src/common/port/path.cpp \
     $(PWD_DIR)/src/common/port/inet_net_ntop.cpp \
     $(PWD_DIR)/src/common/port/gs_syscall_lock.cpp \
     $(PWD_DIR)/src/common/port/gs_readdir.cpp \
     $(PWD_DIR)/src/common/port/getaddrinfo.cpp \
     $(PWD_DIR)/src/common/port/dirent.cpp \
     $(PWD_DIR)/src/common/port/chklocale.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/pthread-win32.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/pqexpbuffer.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-protocol3.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-misc.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-connect.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/win32.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-protocol2.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-lobj.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/pqsignal.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/libpq-events.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-secure.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-print.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-exec.cpp \
     $(PWD_DIR)/src/common/interfaces/libpq/fe-auth.cpp \
     $(PWD_DIR)/src/common/backend/utils/mb/encnames.cpp \
     $(PWD_DIR)/src/common/backend/utils/mb/wchar.cpp \
     $(PWD_DIR)/src/common/backend/libpq/sha2.cpp \
     $(PWD_DIR)/src/common/backend/libpq/md5.cpp \
     $(PWD_DIR)/src/common/backend/libpq/ip.cpp 

OBJ:=$(OBJ_DIR)/win32setlocale.o \
     $(OBJ_DIR)/win32env.o \
     $(OBJ_DIR)/strlcpy.o \
     $(OBJ_DIR)/pgstrcasecmp.o\
     $(OBJ_DIR)/noblock.o \
     $(OBJ_DIR)/inet_aton.o \
     $(OBJ_DIR)/gs_strerror.o \
     $(OBJ_DIR)/gs_env_r.o \
     $(OBJ_DIR)/dirmod.o \
     $(OBJ_DIR)/cipher.o \
     $(OBJ_DIR)/win32error.o \
     $(OBJ_DIR)/thread.o \
     $(OBJ_DIR)/pgsleep.o \
     $(OBJ_DIR)/path.o \
     $(OBJ_DIR)/inet_net_ntop.o \
     $(OBJ_DIR)/gs_syscall_lock.o \
     $(OBJ_DIR)/gs_readdir.o \
     $(OBJ_DIR)/getaddrinfo.o \
     $(OBJ_DIR)/dirent.o \
     $(OBJ_DIR)/chklocale.o \
     $(OBJ_DIR)/pthread-win32.o \
     $(OBJ_DIR)/pqexpbuffer.o \
     $(OBJ_DIR)/fe-protocol3.o \
     $(OBJ_DIR)/fe-misc.o \
     $(OBJ_DIR)/fe-connect.o \
     $(OBJ_DIR)/win32.o \
     $(OBJ_DIR)/fe-protocol2.o \
     $(OBJ_DIR)/fe-lobj.o \
     $(OBJ_DIR)/pqsignal.o \
     $(OBJ_DIR)/libpq-events.o \
     $(OBJ_DIR)/fe-secure.o \
     $(OBJ_DIR)/fe-print.o \
     $(OBJ_DIR)/fe-exec.o \
     $(OBJ_DIR)/fe-auth.o \
     $(OBJ_DIR)/encnames.o \
     $(OBJ_DIR)/wchar.o \
     $(OBJ_DIR)/sha2.o \
     $(OBJ_DIR)/md5.o \
     $(OBJ_DIR)/ip.o

SRC_TO_OBJ=$(OBJ_DIR)/$(patsubst %.cpp,%.o, $(notdir $(1)))

define BUILD_OBJ
$(call SRC_TO_OBJ,$(1)):$(1)
	$(CC) $(CCFLAG) $(INCLUDE) -c $$^ -o $$@ 
endef

all:dirs $(OBJ)
	$(CC) $(OBJ) $(DEFFILE) -static -shared -o $(DLL_TARGET) $(LFLAG)
	$(AR) rcs $(LIB_TARGET) $(OBJ)

dirs:
	$(MD) $(OBJ_DIR)
	$(MD) $(OUT_DIR)

clean:
	$(RM) $(OBJ_DIR)
	$(RM) $(OUT_DIR)

$(foreach c,$(SRC),$(eval $(call BUILD_OBJ,$(c))))

.PHONY: all clean dirs 
#################################################################
## THE END OF LIBPQ MINGW32 Makefile                           ##
#################################################################
