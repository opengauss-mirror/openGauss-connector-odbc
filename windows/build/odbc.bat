REM Copyright Huawei Technologies Co., Ltd. 2010-2018. All rights reserved.
@echo off
setlocal

set WD=%__CD__%
set LIB_SECURITY_DIR=D:\windows_odbc\win32\open_source\Huawei_Secure_C_V100R001C01SPC010B002
set LIB_GAUSSDB_DIR=D:\windows_odbc\win32\open_source\openGauss-server
set LIB_ODBC_DIR=D:\windows_odbc\win32\open_source\openGauss-connector-odbc

set MINGW_DIR=D:\buildtools\mingw-8.1.0\msys64\mingw32
set CMAKE_DIR=D:\env\cmake-3.26
REM openss compiled direction
set OPENSSL_DIR=D:\windows_odbc\win32\open_source\output\openssl-win32

set p7zip=D:\install\7-Zip
set OUTPUT_DIR=%LIB_ODBC_DIR%/odbc_output

if not exist %OPENSSL_DIR%/libcrypto-1_1.dll (
    cd %WD%
    REM Build openssl
    call openssl.bat
)

cd %WD%
REM Build libsecurec.lib
cp win32/libpq/CMakeLists-huawei-securec.txt %LIB_SECURITY_DIR%/CMakeLists.txt 
cd %LIB_SECURITY_DIR% 
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="%MINGW_DIR%" -D"CMAKE_MAKE_PROGRAM:PATH=%MINGW_DIR%/bin/make.exe" -G "MinGW Makefiles" ..
make

cd %WD%
REM Build libpq.lib 
rm -rf %LIB_GAUSSDB_DIR%/libpq-win32
cp -r win32/libpq %LIB_GAUSSDB_DIR%/libpq-win32
cd %LIB_GAUSSDB_DIR%/libpq-win32 
cp -r %LIB_SECURITY_DIR%/output ./lib
bash %LIB_GAUSSDB_DIR%/libpq-win32/project.sh
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="%MINGW_DIR%" -DOPENSSL_DIR="%OPENSSL_DIR%" -D"CMAKE_MAKE_PROGRAM:PATH=%MINGW_DIR%/bin/make.exe" -G "MinGW Makefiles" ..
sed -i 's/LIB_CRYPTO-NOTFOUND/D:\/windows_odbc\/win32\/open_source\/output\/openssl-win32\/libcrypto-1_1.dll/g' CMakeCache.txt
sed -i 's/LIB_SSL-NOTFOUND/D:\/windows_odbc\/win32\/open_source\/output\/openssl-win32\/libssl-1_1.dll/g' CMakeCache.txt
make

cd %WD%
REM Build psqlodbc35w.lib 
cd %LIB_ODBC_DIR%
rm -rf libpq
cp -r %LIB_GAUSSDB_DIR%/libpq-win32/libpq-export ./libpq
cp -r %LIB_GAUSSDB_DIR%/libpq-win32/lib/* ./libpq/lib
cp -r %LIB_GAUSSDB_DIR%/libpq-win32/output/libpq.lib ./libpq/lib
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="%MINGW_DIR%" -DOPENSSL_DIR="%OPENSSL_DIR%" -D"CMAKE_MAKE_PROGRAM:PATH=%MINGW_DIR%/bin/make.exe" -G "MinGW Makefiles" ..
sed -i 's/LIB_CRYPTO-NOTFOUND/D:\/windows_odbc\/win32\/open_source\/output\/openssl-win32\/libcrypto-1_1.dll/g' CMakeCache.txt
sed -i 's/LIB_SSL-NOTFOUND/D:\/windows_odbc\/win32\/open_source\/output\/openssl-win32\/libssl-1_1.dll/g' CMakeCache.txt
make

cd %WD%
REM Build psqlodbc.exe
cd psqlodbc-installer
rm -rf win32_dll
mkdir win32_dll
cp %LIB_ODBC_DIR%/output/psqlodbc35w.dll ./win32_dll
cp "%OPENSSL_DIR%"/libssl-1_1.dll ./win32_dll
cp "%OPENSSL_DIR%"/libcrypto-1_1.dll ./win32_dll
makensis odbc-installer.nsi

cd %WD%
rm -rf odbc_output
mkdir odbc_output
cp psqlodbc-installer/psqlodbc.exe odbc_output
rm -rf psqlodbc-installer/psqlodbc.exe

cd odbc_output
%p7zip%\7z.exe a -tgzip openGauss-5.0.0-ODBC-windows.tar.gz ./*

rm -rf "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%"
cp -r openGauss-5.0.0-ODBC-windows.tar.gz %OUTPUT_DIR%