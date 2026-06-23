#!/usr/bin/env bash
set -e
cd /c/windows_odbc/win32/open_source/openssl-openssl-3.0.9
rm -rf openssl-win32
./Configure --prefix=$PWD/openssl-win32 shared mingw no-tests
make -j$(nproc)
make install -j$(nproc)
cp $PWD/openssl-win32/bin/libssl-3.dll $PWD/openssl-win32/
cp $PWD/openssl-win32/bin/libcrypto-3.dll $PWD/openssl-win32/
rm -rf $PWD/../../output
mkdir -p $PWD/../../output
mv $PWD/openssl-win32 $PWD/../../output/
echo 'OpenSSL build completed'
