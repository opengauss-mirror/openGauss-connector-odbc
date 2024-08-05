#!/bin/bash
#######################################################################
# Copyright (c): 2020-2025, Huawei Tech. Co., Ltd.
# descript: Compile and pack MPPDB
#           Return 0 means OK.
#           Return 1 means failed.
# version:  2.0
# date:     2020-08-09
#######################################################################
declare install_package_format='tar'
declare serverlib_dir='None'

#detect platform information.
PLATFORM=32
bit=$(getconf LONG_BIT)
if [ "$bit" -eq 64 ]; then
    PLATFORM=64
fi

#get OS distributed version.
kernel=""
version=""
if [ -f "/etc/euleros-release" ]; then
    kernel=$(cat /etc/euleros-release | awk -F ' ' '{print $1}' | tr A-Z a-z)
    version=$(cat /etc/euleros-release | awk -F '(' '{print $2}'| awk -F ')' '{print $1}' | tr A-Z a-z)
elif [ -f "/etc/openEuler-release" ]; then
    kernel=$(cat /etc/openEuler-release | awk -F ' ' '{print $1}' | tr A-Z a-z)
    version=$(cat /etc/openEuler-release | awk -F '(' '{print $2}'| awk -F ')' '{print $1}' | tr A-Z a-z)
elif [ -f "/etc/centos-release" ]; then
    kernel=$(cat /etc/centos-release | awk -F ' ' '{print $1}' | tr A-Z a-z)
    version=$(cat /etc/centos-release | awk -F '(' '{print $2}'| awk -F ')' '{print $1}' | tr A-Z a-z)
elif [ -f "/etc/kylin-release" ]; then
    kernel=$(cat /etc/kylin-release | awk -F ' ' '{print $1}' | tr A-Z a-z)
    version=$(cat /etc/kylin-release | awk '{print $6}' | tr A-Z a-z)
else
    kernel=$(lsb_release -d | awk -F ' ' '{print $2}'| tr A-Z a-z)
    version=$(lsb_release -r | awk -F ' ' '{print $2}')
fi

if [ X"$kernel" == X"euleros" ]; then
    dist_version="EULER"
elif [ X"$kernel" == X"centos" ]; then 
    dist_version="CENTOS"
elif [ X"$kernel" == X"openeuler" ]; then 
    dist_version="OPENEULER"
elif [ X"$kernel" == X"kylin" ]; then
    dist_version="KYLIN"
elif [ X"$kernel" == X"suse" ]; then
	dist_version="SUSE"
elif [ X"$kernel" = X"redflag" ]; then
    dist_version="Asianux"
elif [ X"$kernel" = X"asianux" ]; then
    dist_version="Asianux"
else
    echo "We only support EulerOS, OPENEULER(aarch64) SUSE, CentOS and Asianux platform."
    echo "Kernel is $kernel"
    exit 1
fi

##default install version storage path
declare mppdb_name='openGauss-ODBC'
declare version_number='6.0.0'
#######################################################################
## print help information
#######################################################################
function print_help()
{
    echo "Usage: $0 [OPTION]
    -h|--help              show help information.
    -bd|--serverlib_dir    the directory of sever binarylibs.
"
}

if [ $# = 0 ] ; then 
    echo "missing option"
    print_help 
    exit 1
fi

LOCAL_PATH=${0}
FIRST_CHAR=$(expr substr "$LOCAL_PATH" 1 1)
if [ "$FIRST_CHAR" = "/" ]; then
    LOCAL_PATH=${0}
else
    LOCAL_PATH="$(pwd)/${LOCAL_PATH}"
fi

LOCAL_DIR=$(dirname "${LOCAL_PATH}")
#########################################################################
##read command line paramenters
#######################################################################
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
            print_help
            exit 1
            ;;
        -bd|--serverlib_dir)
            if [ "$2"X = X ]; then
                echo "no given binarylib directory values"
                exit 1
            fi
            serverlib_dir=$2
            shift 2
            ;;
	    -ud|--unixodbc_dir)
            if [ "$2"X = X ]; then
                echo "no given unixodbc directory values"
                exit 1
            fi
            UNIX_ODBC=$2
            shift 2
            ;;

         *)
            echo "Internal Error: option processing error: $1" 1>&2  
            echo "please input right paramtenter, the following command may help you"
            echo "./mpp_package.sh --help or ./mpp_package.sh -h"
            exit 1
    esac
done

#######################################################################
## declare all package name
#######################################################################

kernel=""
dist_version=""
arch=$(uname -p)
if [ -f "/etc/euleros-release" ]; then
    kernel=$(cat /etc/euleros-release | awk -F ' ' '{print $1}' | tr A-Z a-z)
elif [ -f "/etc/openEuler-release" ]; then
    kernel=$(cat /etc/openEuler-release | awk -F ' ' '{print $1}' | tr A-Z a-z)
elif [ -f "/etc/centos-release" ]; then
    kernel=$(cat /etc/centos-release | awk -F ' ' '{print $1}' | tr A-Z a-z)
else
    kernel=$(lsb_release -d | awk -F ' ' '{print $2}'| tr A-Z a-z)
fi

if [ X"$kernel" == X"euleros" ]; then
    dist_version="EulerOS"
elif [ X"$kernel" == X"centos" ]; then
    dist_version="CentOS"
elif [ X"$kernel" == X"openeuler" ]; then
    dist_version="openEuler"
elif [ X"$kernel" == X"kylin" ]; then
    dist_version="kylin"
else
    dist_version="Linux"
fi

os_version=$(cat /etc/os-release | grep -w VERSION_ID | awk -F '"' '{print $2}')

declare version_string="${mppdb_name}-${version_number}"
declare odbc_package_name="${version_string}-${dist_version}${os_version}-${arch}.tar.gz"
declare windows_odbc_package_name="${version_string}-Windows-Odbc.tar.gz"

echo "[makeodbc] $(date +%y-%m-%d' '%T): script dir : ${LOCAL_DIR}"
declare LOG_FILE="${LOCAL_DIR}/build.log"
declare BUILD_DIR="${LOCAL_DIR}/tmp_odbc"
declare ODBC_INSTALL_DIR="${BUILD_DIR}/odbc"
declare ERR_MKGS_FAILED=1
declare MKGS_OK=0

if [ "$UNIX_ODBC"X = X ]; then
    UNIX_ODBC="${LOCAL_DIR}/third_party/unixodbc/install_comm/unixODBC-2.3.9"
fi

SERVERLIBS_PATH="${serverlib_dir}"

###################################
# build parameter about enable-llt 
##################################
COMPLIE_TYPE="comm"
echo "[makeodbc] $(date +%y-%m-%d' '%T): Work root dir : ${LOCAL_DIR}"
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
    exit $ERR_MKGS_FAILED
}

function clean_environment()
{
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi

    if [ -f "$LOG_FILE" ]; then
        rm -rf "$LOG_FILE"
    fi

    echo "clean completely"
}

#######################################################################
##install odbc
#######################################################################
function install_odbc()
{
    cd ${LOCAL_DIR}

    cd ${LOCAL_DIR}/third_party/unixodbc/
    sh ./build_unixodbc.sh -m build  >> "$LOG_FILE" 2>&1
    cd ${LOCAL_DIR}

    export GAUSSHOME=$SERVERLIBS_PATH
    export LD_LIBRARY_PATH=$SERVERLIBS_PATH/lib:$LD_LIBRARY_PATH

    if [ "$version_mode"x == "memcheck"x ]; then
        export LIBS="-lrt -ldl -lm -lpthread -lasan"
    fi

    ./configure CFLAGS='-fstack-protector-all -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fPIC' --prefix=${ODBC_INSTALL_DIR} --with-libpq=${SERVERLIBS_PATH} --with-unixodbc=${UNIX_ODBC} >> "$LOG_FILE" 2>&1
    if [ $? -ne 0 ]; then
        die "configure odbc failed."
    fi

    make -sj  >> "$LOG_FILE" 2>&1
    if [ $? -ne 0 ]; then
        die "make odbc failed."
    fi

    make install -sj  >> "$LOG_FILE" 2>&1
    if [ $? -ne 0 ]; then
        die "make install odbc failed."
    fi

    echo "End make odbc" >> "$LOG_FILE" 2>&1
}

declare package_command
#######################################################################
##select package command accroding to install_package_format
#######################################################################
function select_package_command()
{
    case "$install_package_format" in
        tar)
            tar='tar'
            option=' -zcvf'
            package_command="$tar$option"
            ;;
        rpm)
            rpm='rpm'
            option=' -i'
            package_command="$rpm$option"
            ;;
    esac
}

###############################################################
##  copy the target to set path
###############################################################
function target_file_copy()
{
    rm -rf ${BUILD_DIR}/lib
    mkdir ${BUILD_DIR}/lib

    #copy libraries into lib
    cp $SERVERLIBS_PATH/lib/libpq* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libssl* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libcrypto* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libgssapi_krb5_gauss* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libgssrpc_gauss* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libkrb5_gauss* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libkrb5support_gauss* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libk5crypto_gauss* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libconfig* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libpgport_tool* ${BUILD_DIR}/lib
    cp $SERVERLIBS_PATH/lib/libcom_err_gauss* ${BUILD_DIR}/lib

    cp $UNIX_ODBC/lib/libodb* ${BUILD_DIR}/lib
}

#######################################################################
##function make_package have tree actions
##1.copy target file into a newly created temporary directory temp
##2.package all file in the temp directory and renome to destination package_path
#######################################################################
function make_package()
{
    cd ${BUILD_DIR}

    target_file_copy
    select_package_command

    echo "packaging odbc..."
    $package_command "${odbc_package_name}" ./lib ./odbc >>"$LOG_FILE" 2>&1
    if [ $? -ne 0 ]; then
        die "$package_command ${odbc_package_name} failed"
    fi

    mv ${odbc_package_name} ${BUILD_DIR}/

    echo "install odbc tools is ${odbc_package_name} of ${BUILD_DIR} directory " >> "$LOG_FILE" 2>&1
    echo "success!"
}

#############################################################
# main function
#############################################################

# 1. build odbc
install_odbc

# 2. make odbc package
make_package

# 3. cp odbc package to output
mkdir ${LOCAL_DIR}/output
mv ${BUILD_DIR}/*.tar.gz ${LOCAL_DIR}/output/

# 4. clean environment 
echo "clean enviroment"
echo "[makemppdb] $(date +%y-%m-%d' '%T): remove ${BUILD_DIR}" >>"$LOG_FILE" 2>&1
clean_environment

echo "now, odbc package has finished!"

exit 0
