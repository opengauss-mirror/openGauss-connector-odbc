REM Copyright Huawei Technologies Co., Ltd. 2010-2018. All rights reserved.
@echo off
setlocal

set WD=%__CD__%
REM openssl source direction
set OPENSSL_SOURCE_DIR=C:\windows_odbc\win32\open_source\openssl-openssl-3.0.9
set MSYS_SHELL=C:\buildtools\msys64\msys2_shell.cmd

cd %WD%
REM build openssl
cd %OPENSSL_SOURCE_DIR%
REM openssl config
rm -rf openssl-win32
%MSYS_SHELL% -defterm -mingw32 -no-start -full-path -here -c './Configure ^
--prefix=$PWD/openssl-win32 ^
shared mingw no-tests; make -j20; make install -j20; ^
cp $PWD/openssl-win32/bin/libssl-3.dll $PWD/openssl-win32; ^
cp $PWD/openssl-win32/bin/libcrypto-3.dll $PWD/openssl-win32; ^
rm -rf $PWD/../../output; ^
mkdir -p $PWD/../../output; mv $PWD/openssl-win32 $PWD/../../output/.' ^