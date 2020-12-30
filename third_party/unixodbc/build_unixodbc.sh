#!/bin/bash
#######################################################################
# Copyright (c): 2020-2025, Huawei Tech. Co., Ltd.
# descript: Compile and pack MPPDB
#           Return 0 means OK.
#           Return 1 means failed.
# version:  2.0
# date:     2020-08-09
#######################################################################

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
CONFIG_FILE_NAME=config.ini
BUILD_OPTION=release 
TAR_FILE_NAME=unixODBC-2.3.9.tar.gz
SOURCE_CODE_PATH=unixODBC-2.3.9
LOG_FILE=${LOCAL_DIR}/build_unixODBC.log
BUILD_FAILED=1

#######################################################################
## print help information
#######################################################################
function print_help()
{
        echo "Usage: $0 [OPTION]
        -h|--help               show help information
        -m|--build_option       provode type of operation, values of paramenter is build, shrink, dist or clean
	"
}

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

ls ${LOCAL_DIR}/${CONFIG_FILE_NAME} >/dev/null 2>&1
if [ $? -ne 0 ]; then
	die "[Error] the file ${CONFIG_FILE_NAME} not exist."
fi

COMPLIE_TYPE_LIST=$(cat ${LOCAL_DIR}/${CONFIG_FILE_NAME} | grep -v '#' | grep -v '^$' | awk -F '=' '{print $2}' | sed  's/|/ /g')
COMPONENT_NAME=$(cat ${LOCAL_DIR}/${CONFIG_FILE_NAME} | grep -v '#' | grep -v '^$' |awk -F '=' '{print $1}'| awk -F '@' '{print $2}')
COMPONENT_TYPE=$(cat ${LOCAL_DIR}/${CONFIG_FILE_NAME} | grep -v '#' | grep -v '^$' | awk -F '@' '{print $1}')

if [ "${COMPONENT_NAME}"X = ""X ]
then
	die "[Error] get component name failed!"
fi

if [ "${COMPONENT_TYPE}"X = ""X ]
then
	die "[Error] get component type failed!"
fi

ROOT_DIR="${LOCAL_DIR}/../../"
INSTALL_COMPOENT_PATH_NAME="${ROOT_DIR}/${COMPONENT_TYPE}/${COMPONENT_NAME}"

#######################################################################
# build and install component
#######################################################################
function build_component()
{
        cd ${LOCAL_DIR}
	rm -rf ${TAR_FILE_NAME%.tar.gz}
	rm -rf ${TAR_FILE_NAME}
	cp ${TAR_FILE_NAME%.tar.gz}.file ${TAR_FILE_NAME}
        tar -xvf ${TAR_FILE_NAME}
	
	cd ${LOCAL_DIR}/${SOURCE_CODE_PATH}
	if [ $? -ne 0 ]; then
                die "[Error] change dir to $SRC_DIR failed."
        fi

	log "[Notice] start autoreconf."
	autoreconf -fi
	if [ $? -ne 0 ]; then
		die "[Error] autoreconf failed, please install libtool and libtool-ltdl-devel."
	fi
	chmod +x configure       
	for COMPILE_TYPE in ${COMPLIE_TYPE_LIST}
	do
		case "${COMPILE_TYPE}" in
                	release)
				die "[Error] unixODBC not supported build type."
				;;
                	debug)
				die "[Error] unixODBC not supported build type."
				;;
                	comm)	
				mkdir -p ${LOCAL_DIR}/install_comm/unixODBC-2.3.9
                                log "[Notice] unixODBC configure string: ./configure CFLAGS='-fstack-protector-all -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fPIC -fPIE -pie' --enable-gui=no --prefix=${LOCAL_DIR}/install_comm"
                                ./configure CFLAGS='-fstack-protector-all -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fPIC -fPIE -pie' --enable-gui=no --prefix=${LOCAL_DIR}/install_comm/unixODBC-2.3.9
                        	;;
			release_llt)
				die "[Error] unixODBC not supported build type."
				;;
			debug_llt)
				die "[Error] unixODBC not supported build type."
				;;
              		llt)
				die "[Error] unixODBC not supported build type."
		       		;;
               		 *)
                        	log "Internal Error: option processing error: $1"   
                        	log "please write right paramenter in ${CONFIG_FILE_NAME}"
				exit 1
   		esac

                if [ $? -ne 0 ]; then
                        die "[Error] unixODBC configure failed."
                fi
                log "[Notice] unixODBC End configure"
		
                log "[Notice] disable rpath"

	        sed -i 's/runpath_var=LD_RUN_PATH/runpath_var=""/g' ./libtool
        	sed -i 's/hardcode_libdir_flag_spec="\\\${wl}-rpath \\\${wl}\\\$libdir"/hardcode_libdir_flag_spec=""/g' ./libtool

        	PLAT_FORM_STR=`uname -p`

        	if [ "${PLAT_FORM_STR}"X = "aarch64"X ]
       		then
			sed -i "250c runpath_var=" libtool
                	sed -i "385c hardcode_libdir_flag_spec=" libtool
        	fi
 
		log "[Notice] unixODBC using \"${COMPILE_TYPE}\" Begin make"
                make -sj
                if [ $? -ne 0 ]; then
                        die "unixODBC make failed."
                 fi
                log "[Notice] unixODBC End make"

                log "[Notice] unixODBC using \"${COMPILE_TYPE}\" Begin make install"
                make install -sj
                if [ $? -ne 0 ]; then
                        die "[Error] unixODBC make install failed."
                fi
                log "[Notice] unixODBC using \"${COMPILE_TYPE}\" End make install"

                make clean
                log "[Notice] unixODBC build using \"${COMPILE_TYPE}\" has been finished"



	done	
}

#######################################################################
# choose the real files 
#######################################################################
function shrink_component()
{
	for COMPILE_TYPE in ${COMPLIE_TYPE_LIST}
	do
        	case "${COMPILE_TYPE}" in
                	comm)
                                mkdir ${LOCAL_DIR}/install_comm_dist
                                cp -r ${LOCAL_DIR}/install_comm/* ${LOCAL_DIR}/install_comm_dist
                                if [ $? -ne 0 ]; then
                                        die "[Error] \"cp -r ${LOCAL_DIR}/install_comm/* ${LOCAL_DIR}/install_comm_dist\" failed."
                                fi
				;;
                        release)
                                ;;
                        debug)
                                ;;
                	llt)
                       		;;
			release_llt)
				;;
			debug_llt)
				;;
                	*)
        	esac
		log "[Notice] unixODBC shrink using \"${COMPILE_TYPE}\" has been finished!"
	done
}

##############################################################################################################
# dist the real files to the matched path
#	we could makesure that $INSTALL_COMPOENT_PATH_NAME is not null, '.' or '/'
##############################################################################################################
function dist_component()
{
	for COMPILE_TYPE in ${COMPLIE_TYPE_LIST}
	do
        	case "${COMPILE_TYPE}" in
                	comm)
                        	rm -rf ${INSTALL_COMPOENT_PATH_NAME}/comm/*
                       		cp -r ${LOCAL_DIR}/install_comm_dist/* ${INSTALL_COMPOENT_PATH_NAME}/comm
                                if [ $? -ne 0 ]; then
                                        die "[Error] \"cp -r ${LOCAL_DIR}/install_comm_dist/* ${INSTALL_COMPOENT_PATH_NAME}/comm\" failed."
                                fi 
	                	;;
			release)
				;;
			debug)
				;;
                	llt)
                       		;;
			release_llt)
				;;
			debug_llt)
				;;
                	*)
        	esac
		log "[Notice] unixODBC dist using \"${COMPILE_TYPE}\" has been finished!"
	done
}

#######################################################################
# clean component 
#######################################################################
function clean_component()
{
	cd ${LOCAL_DIR}/${SOURCE_CODE_PATH}
        if [ $? -ne 0 ]; then
	        die "[Error] cd ${LOCAL_DIR}/${SOURCE_CODE_PATH} failed."
        fi
	
	cd ${LOCAL_DIR}
	if [ $? -ne 0 ]; then
                die "[Error] cd ${LOCAL_DIR} failed."
        fi
	[ -n "${SOURCE_CODE_PATH}" ] && rm -rf ${SOURCE_CODE_PATH}
	rm -rf install_*

	log "[Notice] unixODBC clean has been finished!"
}
#######################################################################
#######################################################################
#######################################################################
# main
#######################################################################
#######################################################################
#######################################################################
function main()
{
        case "${BUILD_OPTION}" in
                build)
			build_component
			;;
		shrink)
			shrink_component
			;;
		dist)
			dist_component			
			;;
		clean)
			clean_component	
                        ;;
                all)
			build_component
			shrink_component
			dist_component
			clean_component
                        ;;
                *)
                        log "Internal Error: option processing error: $2"   
                        log "please input right paramenter values build, shrink, dist or clean "
        esac
}


########################################################################
if [ $# = 0 ] ; then
        log "missing option"
        print_help
        exit 1
fi

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
                        die "no given version number values"
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

###########################################################################
main
