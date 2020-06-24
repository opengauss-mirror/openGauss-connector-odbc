#!/bin/bash
# description: the script that make install PSQLODBC

######################################################################
# Parameter setting
######################################################################
LOCAL_PATH=${0}
FIRST_CHAR=$(expr substr "$LOCAL_PATH" 1 1)
if [ "$FIRST_CHAR" = "/" ]; then
    LOCAL_PATH=${0}
else
    LOCAL_PATH="$(pwd)/${LOCAL_PATH}"
fi

LOCAL_DIR=$(dirname "${LOCAL_PATH}")
BUILD_OPTION=
TAR_FILE_NAME=unixODBC-2.3.6.tar.gz
SOURCE_CODE_PATH=unixODBC-2.3.6
LOG_FILE=${LOCAL_DIR}/build_PSQLODBC.log
BUILD_FAILED=1

#######################################################################
## print help information
#######################################################################
function print_help()
{
        echo "Usage: $0 [OPTION]
        -h|--help              show help information
        -m|--build_option      provode the path of GAUSSHOME
        "
}


if [ $# = 0 ] ; then
        log "missing option"
        print_help
        exit 1
fi

#######################################################################
#  Print log.
#######################################################################
log()
{
    echo "[Build unixODBC] $(date +%y-%m-%d' '%T): $@"
    echo "[Build unixODBC] $(date +%y-%m-%d' '%T): $@" >> "$LOG_FILE" 2>&1
}

#######################################################################
#  print log and exit.
#######################################################################
die()
{
    log "$@"
    echo "$@"
    exit $BUILD_FAILED
}

#######################################################################
# build and install component
#######################################################################
function build_component()
{
    cd ${LOCAL_DIR}/third_party/unixodbc/
    sh ./build_unixodbc.sh -m build  >> "$LOG_FILE" 2>&1
    cd ${LOCAL_DIR}

    export GAUSSHOME=$BUILD_OPTION
    export LD_LIBRARY_PATH=$BUILD_OPTION/lib:$LD_LIBRARY_PATH

    log "[Notice] PSQLODBC configure string: ./configure CFLAGS='-fstack-protector-all -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fPIC' --prefix=`pwd`/install --with-libpq=$BUILD_OPTION --with-unixodbc=./third_party/unixodbc/install_comm/unixODBC-2.3.6/"
    ./configure CFLAGS='-fstack-protector-all -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fPIC' --prefix=`pwd`/install --with-libpq=$BUILD_OPTION --with-unixodbc=./third_party/unixodbc/install_comm/unixODBC-2.3.6/  >> "$LOG_FILE" 2>&1

    log "[Notice] PSQLODBC Begin make"
    make -sj  >> "$LOG_FILE" 2>&1
    if [ $? -ne 0 ]; then
        die "PSQLODBC make failed."
    fi

    log "[Notice] PSQLODBC Begin make install"
    make install -sj  >> "$LOG_FILE" 2>&1
    if [ $? -ne 0 ]; then
        die "[Error] PSQLODBC make install failed."
    fi

    make clean  >> "$LOG_FILE" 2>&1

    log "[Notice] PSQLODBC Begin make package"
    mv ${LOCAL_DIR}/install ${LOCAL_DIR}/odbc
    rm -rf ${LOCAL_DIR}/lib
    mkdir ${LOCAL_DIR}/lib

    #copy libraries into lib
    cp $BUILD_OPTION/lib/libpq* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libssl* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libcrypto* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libgssapi_krb5_gauss* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libgssrpc_gauss* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libkrb5_gauss* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libkrb5support_gauss* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libk5crypto_gauss* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libconfig* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libpgport_tool* ${LOCAL_DIR}/lib
    cp $BUILD_OPTION/lib/libcom_err_gauss* ${LOCAL_DIR}/lib

    cp ./third_party/unixodbc/install_comm/unixODBC-2.3.6/lib/libodbcinst* ${LOCAL_DIR}/lib

    tar -czvf openGauss-1.0.0-ODBC.tar.gz ./lib ./odbc
    
    rm -rf ${LOCAL_DIR}/lib

    log "[Notice] PSQLODBC has been finished"
}

##########################################################################
#read command line paramenters
##########################################################################
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
                print_help
                exit 1
                ;;
        -m|--build_option)
                if [ "$2"X = X ];then
                        die "no given path of GAUSSHOME"
                fi
                BUILD_OPTION=$2
                shift 2
                ;;
        *)
                log "Internal Error: option processing error: $1" 1>&2
                log "please input right paramtenter, the following command may help you"
                log "./build.sh --help or ./build.sh -h"
                exit 1
    esac
done

build_component


