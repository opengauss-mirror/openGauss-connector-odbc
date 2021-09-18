#################################################################
## Makefile for building ODBC library on Windows using MinGW   ##
#################################################################

ifndef $(OPENSSL_DIR)
    OPENSSL_DIR:=/d/Program_Files/OpenSSL-Win32
endif

ifndef $(MINGW_DIR)
    MINGW_DIR:=/d/buildtools/mingw-8.1.0/msys32/mingw32
endif

PWD_DIR:=$(shell pwd)
LIBPQ_DIR:=$(PWD_DIR)/libpq
MINGW32_DIR:=$(MINGW_DIR)/i686-w64-mingw32

OUT_DIR:=$(PWD_DIR)/output
OBJ_DIR:=$(PWD_DIR)/obj

DLL_TARGET:=$(OUT_DIR)/psqlodbc35w.dll
LIB_TARGET:=$(OUT_DIR)/psqlodbc35w.lib

CC:=$(MINGW_DIR)/bin/i686-w64-mingw32-gcc
AR:=$(MINGW_DIR)/bin/i686-w64-mingw32-gcc-ar
RC_CC:=$(MINGW_DIR)/bin/windres

MD:=mkdir -p
RM:=rm -rf

DEFFILE:=$(PWD_DIR)/psqlodbc.def 

CCFLAG:=-std=gnu++11 \
	-DWIN32 \
	-D_MINGW32 \
	-DUNICODE_SUPPORT \
	-DWIN_MULTITHREAD_SUPPORT \
	-DDRIVER_CURSOR_IMPLEMENT \
	-fpermissive \
	-fPIC \
	-w \
	-m32

ifdef RELEASE
	CCFLAG:=$(CCFLAG) -O2 
else
	CCFLAG:=$(CCFLAG) -g 
endif

LFLAG:=-L$(OPENSSL_DIR)/lib \
       -L$(LIBPQ_DIR)/lib \
       -L$(MINGW_DIR)/bin \
       -L$(MINGW32_DIR)/lib \
       -llibpq \
       -lodbc32 \
       -lodbccp32 \
       -lwinmm \
       -lws2_32 \
       -llibsecurec \
       -lsecurity \
       -llibcrypto \
       -llibssl \
       -lgdi32 \
       -Wl,--enable-stdcall-fixup,--out-implib,$(DLL_TARGET)

RC_SRC:=$(PWD_DIR)/psqlodbc.rc
RC_OBJ:=$(OBJ_DIR)/psqlodbc_rc.o

INC:=-I$(PWD_DIR) \
     -I$(LIBPQ_DIR)/include \
     -I$(LIBPQ_DIR)/include/libpq \
     -I$(MINGW_DIR)/include \
     -I$(MINGW32_DIR)/include

SRC:=$(PWD_DIR)/bind.c \
     $(PWD_DIR)/columninfo.c \
     $(PWD_DIR)/connection.c \
     $(PWD_DIR)/convert.c \
     $(PWD_DIR)/descriptor.c \
     $(PWD_DIR)/dlg_specific.c \
     $(PWD_DIR)/dlg_wingui.c \
     $(PWD_DIR)/drvconn.c \
     $(PWD_DIR)/environ.c \
     $(PWD_DIR)/execute.c \
     $(PWD_DIR)/info.c \
     $(PWD_DIR)/inouealc.c \
     $(PWD_DIR)/loadlib.c \
     $(PWD_DIR)/lobj.c \
     $(PWD_DIR)/misc.c \
     $(PWD_DIR)/multibyte.c \
     $(PWD_DIR)/mylog.c \
     $(PWD_DIR)/odbcapi.c \
     $(PWD_DIR)/odbcapi30.c \
     $(PWD_DIR)/odbcapi30w.c \
     $(PWD_DIR)/odbcapiw.c \
     $(PWD_DIR)/options.c \
     $(PWD_DIR)/parse.c \
     $(PWD_DIR)/pgapi30.c \
     $(PWD_DIR)/pgtypes.c \
     $(PWD_DIR)/psqlodbc.c \
     $(PWD_DIR)/qresult.c \
     $(PWD_DIR)/results.c \
     $(PWD_DIR)/setup.c \
     $(PWD_DIR)/statement.c \
     $(PWD_DIR)/tuple.c \
     $(PWD_DIR)/win_unicode.c

OBJ:=$(OBJ_DIR)/bind.o \
     $(OBJ_DIR)/columninfo.o \
     $(OBJ_DIR)/connection.o \
     $(OBJ_DIR)/convert.o \
     $(OBJ_DIR)/descriptor.o \
     $(OBJ_DIR)/dlg_specific.o \
     $(OBJ_DIR)/dlg_wingui.o \
     $(OBJ_DIR)/drvconn.o \
     $(OBJ_DIR)/environ.o \
     $(OBJ_DIR)/execute.o \
     $(OBJ_DIR)/info.o \
     $(OBJ_DIR)/inouealc.o \
     $(OBJ_DIR)/loadlib.o \
     $(OBJ_DIR)/lobj.o \
     $(OBJ_DIR)/misc.o \
     $(OBJ_DIR)/multibyte.o \
     $(OBJ_DIR)/mylog.o \
     $(OBJ_DIR)/odbcapi.o \
     $(OBJ_DIR)/odbcapi30.o \
     $(OBJ_DIR)/odbcapi30w.o \
     $(OBJ_DIR)/odbcapiw.o \
     $(OBJ_DIR)/options.o \
     $(OBJ_DIR)/parse.o \
     $(OBJ_DIR)/pgapi30.o \
     $(OBJ_DIR)/pgtypes.o \
     $(OBJ_DIR)/psqlodbc.o \
     $(OBJ_DIR)/qresult.o \
     $(OBJ_DIR)/results.o \
     $(OBJ_DIR)/setup.o \
     $(OBJ_DIR)/statement.o \
     $(OBJ_DIR)/tuple.o \
     $(OBJ_DIR)/win_unicode.o

SRC_TO_OBJ=$(OBJ_DIR)/$(patsubst %.c,%.o, $(notdir $(1)))

define BUILD_OBJ
$(call SRC_TO_OBJ,$(1)):$(1)
	$(CC) $(CCFLAG) $(INC) -c $$^ -o $$@ 
endef

#$(RC_OBJ):$(RC_SRC)
#	$(RC_CC) -Jrc -I$(PWD_DIR) $(RC_SRC) -o $(RC_OBJ)

all:dirs $(OBJ)
	$(RC_CC) -Jrc -I$(PWD_DIR) $(RC_SRC) -o $(RC_OBJ)
	$(CC) $(OBJ) $(RC_OBJ) $(DEFFILE) -static -shared -o $(DLL_TARGET) $(LFLAG)
	$(AR) rcs $(LIB_TARGET) $(OBJ) $(RC_OBJ)
dirs:
	$(MD) $(OBJ_DIR)
	$(MD) $(OUT_DIR)

clean:
	$(RM) $(OBJ_DIR)
	$(RM) $(OUT_DIR)

$(foreach c,$(SRC),$(eval $(call BUILD_OBJ,$(c))))

.PHONY: all clean dirs 
#################################################################
## THE END OF PSQLODBC MINGW32 Makefile                        ##
#################################################################
