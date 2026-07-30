#!/usr/bin/env bash
set -e

WD=/c/Users/luodongxu/work/openGauss-connector-odbc/windows/build
LIB_SECURITY_DIR=/c/windows_odbc/win32/open_source/Huawei_Secure_C_V100R001C01SPC010B002
LIB_GAUSSDB_DIR=/c/windows_odbc/win32/open_source/openGauss-server
LIB_ODBC_DIR=/c/Users/luodongxu/work/openGauss-connector-odbc
MINGW_DIR=/c/buildtools/msys64/mingw32
CMAKE_DIR=/c/env/cmake-3.26
OPENSSL_DIR=/c/windows_odbc/win32/open_source/output/openssl-win32
OUTPUT_DIR=$LIB_ODBC_DIR/odbc_output

export PATH=/c/buildtools/msys64/usr/bin:$MINGW_DIR/bin:$CMAKE_DIR/bin:/c/install/NSIS:/c/install/7-Zip:$PATH

# Build libsecurec.lib
cd "$WD"
cp win32/libpq/CMakeLists-huawei-securec.txt "$LIB_SECURITY_DIR/CMakeLists.txt"
cd "$LIB_SECURITY_DIR"
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="$MINGW_DIR" -D"CMAKE_MAKE_PROGRAM:PATH=$MINGW_DIR/bin/make.exe" -G "MinGW Makefiles" ..
make

# Build libpq.lib
cd "$WD"
rm -rf "$LIB_GAUSSDB_DIR/libpq-win32"
cp -r win32/libpq "$LIB_GAUSSDB_DIR/libpq-win32"
cd "$LIB_GAUSSDB_DIR/libpq-win32"
cp -r "$LIB_SECURITY_DIR/output" ./lib
bash "$LIB_GAUSSDB_DIR/libpq-win32/project.sh"
# Fix missing cpu_set_t and bool on Windows
sed -i 's|#include <windows.h>|#include <windows.h>\n#ifndef __cplusplus\n#include <stdbool.h>\n#endif\ntypedef int cpu_set_t;|' "$LIB_GAUSSDB_DIR/libpq-win32/include/gs_thread.h"
sed -i 's|#include <windows.h>|#include <windows.h>\n#ifndef __cplusplus\n#include <stdbool.h>\n#endif\ntypedef int cpu_set_t;|' "$LIB_GAUSSDB_DIR/libpq-win32/libpq-export/include/gs_thread.h"
# Fix missing setenv on Windows
python "$WD/patch_gs_env_r.py" "$LIB_GAUSSDB_DIR/libpq-win32/src/common/port/gs_env_r.cpp"
python "$WD/patch_getaddrinfo.py" "$LIB_GAUSSDB_DIR/libpq-win32/src/common/port/getaddrinfo.cpp"
# Fix read/write macros conflicting with C++ iostream
python "$WD/patch_palloc.py" "$LIB_GAUSSDB_DIR/libpq-win32/include/utils/palloc.h"
# Fix missing inet_ntop/inet_pton on Windows
python "$WD/patch_ip.py" "$LIB_GAUSSDB_DIR/libpq-win32/src/common/backend/libpq/ip.cpp"
# Set Windows target to Vista for inet_ntop/inet_pton support
sed -i 's|#define _WIN32_WINNT 0x0501|#define _WIN32_WINNT 0x0600|' "$LIB_GAUSSDB_DIR/libpq-win32/include/pg_config_os.h"
# Copy missing openssl client header
cp -r "$LIB_GAUSSDB_DIR/src/include/ssl" "$LIB_GAUSSDB_DIR/libpq-win32/include/"
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="$MINGW_DIR" -DOPENSSL_DIR="$OPENSSL_DIR" -DCMAKE_FIND_LIBRARY_SUFFIXES=".a;.dll.a;.lib" -D"CMAKE_MAKE_PROGRAM:PATH=$MINGW_DIR/bin/make.exe" -G "MinGW Makefiles" .. || true
sed -i "s|LIB_CRYPTO-NOTFOUND|${OPENSSL_DIR}/lib/libcrypto.a|g" CMakeCache.txt
sed -i "s|LIB_SSL-NOTFOUND|${OPENSSL_DIR}/lib/libssl.a|g" CMakeCache.txt
make

# Build psqlodbc35w.lib
cd "$LIB_ODBC_DIR"
rm -rf libpq
cp -r "$LIB_GAUSSDB_DIR/libpq-win32/libpq-export" ./libpq
cp -r "$LIB_GAUSSDB_DIR/libpq-win32/lib/"* ./libpq/lib
cp -r "$LIB_GAUSSDB_DIR/libpq-win32/output/libpq.lib" ./libpq/lib
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="$MINGW_DIR" -DOPENSSL_DIR="$OPENSSL_DIR" -DCMAKE_FIND_LIBRARY_SUFFIXES=".a;.dll.a;.lib" -D"CMAKE_MAKE_PROGRAM:PATH=$MINGW_DIR/bin/make.exe" -G "MinGW Makefiles" ..
sed -i "s|LIB_CRYPTO-NOTFOUND|${OPENSSL_DIR}/lib/libcrypto.a|g" CMakeCache.txt
sed -i "s|LIB_SSL-NOTFOUND|${OPENSSL_DIR}/lib/libssl.a|g" CMakeCache.txt
make

# Build psqlodbc.exe
cd "$WD"
cd psqlodbc-installer
rm -rf win32_dll
mkdir win32_dll
cp "$LIB_ODBC_DIR/output/psqlodbc35w.dll" ./win32_dll
cp "$OPENSSL_DIR/libssl-3.dll" ./win32_dll
cp "$OPENSSL_DIR/libcrypto-3.dll" ./win32_dll
makensis odbc-installer.nsi

# Package
cd "$WD"
rm -rf odbc_output
mkdir odbc_output
cp psqlodbc-installer/psqlodbc.exe odbc_output
rm -rf psqlodbc-installer/psqlodbc.exe

cd odbc_output
7z.exe a -tgzip openGauss-5.0.0-ODBC-windows.tar.gz ./*

rm -rf "$OUTPUT_DIR"
mkdir "$OUTPUT_DIR"
cp -r openGauss-5.0.0-ODBC-windows.tar.gz "$OUTPUT_DIR"

echo 'ODBC build completed'
