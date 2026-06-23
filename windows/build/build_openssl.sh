#!/usr/bin/env bash
set -e
MAKE=/c/buildtools/msys64/mingw32/bin/make.exe
PERL=/c/buildtools/msys64/usr/bin/perl
export PATH=/c/buildtools/msys64/mingw32/bin:/c/buildtools/msys64/usr/bin:$PATH
hash -r
cd /c/windows_odbc/win32/open_source/openssl-openssl-3.0.9
$MAKE distclean || true
rm -rf openssl-win32
$PERL ./Configure --prefix=$PWD/openssl-win32 shared mingw no-tests
sed -i 's|^PERL=/usr/bin/perl|PERL=/c/buildtools/msys64/usr/bin/perl|' Makefile
sed -i 's|"PERL" => "/usr/bin/perl"|"PERL" => "/c/buildtools/msys64/usr/bin/perl"|' configdata.pm
sed -i 's|"perl_cmd" => "/usr/bin/perl"|"perl_cmd" => "/c/buildtools/msys64/usr/bin/perl"|' configdata.pm
$MAKE -j$(nproc)
$MAKE install_sw -j$(nproc)
cp $PWD/openssl-win32/bin/libssl-3.dll $PWD/openssl-win32/
cp $PWD/openssl-win32/bin/libcrypto-3.dll $PWD/openssl-win32/
rm -rf $PWD/../../output
mkdir -p $PWD/../../output
mv $PWD/openssl-win32 $PWD/../../output/
echo 'OpenSSL build completed'
