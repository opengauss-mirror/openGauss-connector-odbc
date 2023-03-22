REM Copyright Huawei Technologies Co., Ltd. 2010-2018. All rights reserved.
@echo off
setlocal

set WD=%__CD__%
set LIB_SECURITY_DIR=D:\windows_odbc\win32\open_source\Huawei_Secure_C_V100R001C01SPC010B002
set LIB_GAUSSDB_DIR=D:\windows_odbc\win32\open_source\openGauss-server
set LIB_ODBC_DIR=D:\windows_odbc\win32\open_source\openGauss-connector-odbc

set MINGW_DIR=D:\buildtools\mingw-8.1.0\msys64\mingw32
set CMAKE_DIR=D:\env\cmake-3.26
set OPENSSL_DIR=D:\windows_odbc\win32\open_source\output\openssl-win32

set OPENSSL_DIR=D:\windows_odbc\win32\open_source\openssl-OpenSSL_1_1_1n\openssl-OpenSSL_1_1_1n
set MSYS_SHELL=D:\buildtools\mingw-8.1.0\msys64\msys2_shell.cmd

cd %WD%
REM build openssl
cd %OPENSSL_DIR%
REM openssl config
%MSYS_SHELL% -defterm -mingw32 -no-start -full-path -here -c './Configure ^
--prefix=$PWD/openssl-win32 ^
shared mingw no-tests; make -j20; make install -j20; ^
cp $PWD/openssl-win32/bin/libssl-1_1.dll $PWD/openssl-win32; ^
cp $PWD/openssl-win32/bin/libcrypto-1_1.dll $PWD/openssl-win32; ^
rm -rf $PWD/../../output; ^
mkdir -p $PWD/../../output; mv $PWD/openssl-win32 $PWD/../../output/.' ^

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
bash -l %LIB_GAUSSDB_DIR%/libpq-win32/project.sh
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="%MINGW_DIR%" -DOPENSSL_DIR="%OPENSSL_DIR%" -D"CMAKE_MAKE_PROGRAM:PATH=%MINGW_DIR%/bin/make.exe" -G "MinGW Makefiles" ..
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
%p7zip%\7z.exe a windows-odbc-x86.tar *
%p7zip%\7z.exe a -tgzip windows-odbc-x86.tar.gz *.tar
del *.tar

set OUTPUT_DIR=%LIB_ODBC_DIR%/output
mkdir "%OUTPUT_DIR%"
cp windows-odbc-x86.tar.gz %OUTPUT_DIR%

