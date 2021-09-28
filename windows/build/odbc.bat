@echo off
setlocal

set WD=%__CD__%
set LIB_SECURITY_DIR=%WD%\..\..\..\third_party\platform\Huawei_Secure_C\Huawei_Secure_C_V100R001C01SPC010B002
set LIB_GAUSSDB_DIR=%WD%\..\..\..\server
set LIB_ODBC_DIR=%WD%\..\..

set MINGW_DIR=C:\buildtools\mingw-8.1.0\msys32\mingw32
set CMAKE_DIR=C:\buildtools\cmake
set OPENSSL_DIR=C:\buildtools\OpenSSL-Win32


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
%p7zip%\7z.exe a GaussDB-Kernel-V500R002C00-Windows-Odbc-X86.tar *
%p7zip%\7z.exe a -tgzip GaussDB-Kernel-V500R002C00-Windows-Odbc-X86.tar.gz *.tar
del *.tar

set OUTPUT_DIR=%LIB_ODBC_DIR%/output
mkdir "%OUTPUT_DIR%"
cp GaussDB-Kernel-V500R002C00-Windows-Odbc-X86.tar.gz %OUTPUT_DIR%

