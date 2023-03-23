# odbc-win32编译指导

## 1、下载并配置环境变量

odbc编译windows32位软件包，需要下面的依赖

### 1.1下载下面的软件包

| 下载的软件包 | 官方地址                                                     |
| :----------- | :----------------------------------------------------------- |
| msys2        | https://www.msys2.org/                                       |
| mingw32      | https://udomain.dl.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/8.1.0/threads-posix/sjlj/i686-8.1.0-release-posix-sjlj-rt_v6-rev0.7z |
| cmake        | https://cmake.org/download/                                  |
| nsis         | https://nsis.sourceforge.io/Download                         |
| 7zip         | https://7-zip.org/download.html                              |



### 1.2配置环境变量

<img src="odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230321200733264.png" alt="image-20230321200733264"  />

**注意：**

- 安装完msys2和mingw32，将mingw32目录覆盖msys2目录下的mingw32目录，并将覆盖后的mingw32/bin目录下mingw32-make.exe重命名make.exe
- 修改msys2的镜像源，可以参考[华为镜像源](https://mirrors.huaweicloud.com/home)

<img src="odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230321195408892.png" alt="image-20230321195408892" style="zoom:80%;" />

- 配置完镜像源，打开msys2控制台，下载一些必要的环境依赖

```shell
# 更新软件包数据
pacman -Syu
# 下载一些必要的软件
pacman -S --needed base-devel
```



### 1.3验证环境变量

```shell
验证perl -v，make -v, cmake -version, gcc -v, g++ -v， sed是否存在
```



## 2、准备源代码

odbc编译windows32需要的代码有，openssl，安全函数，openGauss-connector-odbc，openGauss-server；其中openssl和安全函数都在社区的三方库中

| 代码名称                 | 代码仓库                                                     |
| ------------------------ | ------------------------------------------------------------ |
| openssl                  | https://gitee.com/opengauss/openGauss-third_party/blob/master/dependency/openssl/openssl-OpenSSL_1_1_1n.tar.gz |
| 安全函数                 | https://gitee.com/opengauss/openGauss-third_party/raw/master/platform/Huawei_Secure_C/Huawei_Secure_C_V100R001C01SPC010B002.zip |
| openGauss-connector-odbc | https://gitee.com/opengauss/openGauss-connector-odbc.git     |
| openGauss-server         | https://gitee.com/opengauss/openGauss-server.git             |



## 3、编译odbc

首先先将上面的源代码，统一放到一个文件夹下，这样是为了方便规划，比如我这边是将上面4个源代码，放到window_odbc/win32/open_source路径下

![image-20230322095314026](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322095314026.png)

**脚本介绍**

在openGauss-connector-odbc\windows\build路径下有两个脚本，一个是openssl.bat编译openssl，一个是odbc.bat编译odbc

odbc.bat脚本的最开始，是一些环境变量和代码的路径，需要根据自己实际路径配置下

```shell
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
```



### 3.1编译openssl

在odbc.bat脚本中调用openssl.bat编译openssl

```shell
if not exist %OPENSSL_DIR%/libcrypto-1_1.dll (
    cd %WD%
    REM Build openssl
    call openssl.bat
)
```

openssl.bat脚本的内容

```shell
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
```



**编译结果**

![image-20230322115658122](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322115658122.png)

同时在window_odbc/win32/open_source/output路径中也会生成openssl-win32的文件夹

![image-20230322115736573](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322115736573.png)

### 3.2编译libsecurec.lib

```shell
cd %WD%
REM Build libsecurec.lib
cp win32/libpq/CMakeLists-huawei-securec.txt %LIB_SECURITY_DIR%/CMakeLists.txt 
cd %LIB_SECURITY_DIR% 
rm -rf build
mkdir build
cd build
cmake -DMINGW_DIR="%MINGW_DIR%" -D"CMAKE_MAKE_PROGRAM:PATH=%MINGW_DIR%/bin/make.exe" -G "MinGW Makefiles" ..
make
```



**编译结果**

![image-20230322153805967](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322153805967.png)



安全函数编译出的libsecurec.lib会在安全函数的output目录下

![image-20230322153855140](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322153855140.png)

### 3.3编译libpq.lib

```shell
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
make
```



注意：

- 在编译libpq.lib的时候，要确保project.sh执行成功，sh可以在git控制台执行

![image-20230322155042421](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322155042421.png)

- 修改dirmod.cpp

```c
第45行注释掉 // #include "storage/file/fio_device.h"
    
在87行增加
bool is_file_delete(int err)
{
    return (err == ENOENT);
}
```

- 修改gs_readdir.cpp

```c
第31行注释掉 // #include "storage/file/fio_device.h"
```



**编译结果**

如果遇到下面的问题，说明是openssl没有找到

![image-20230322161142228](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322161142228.png)

解决方法：手动修改D:\windows_odbc\win32\open_source\openGauss-server\libpq-win32\build目录下的CMakeCache.txt文件

![image-20230322161255741](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322161255741.png)

修改LIB_CRYPTO和LIB_SSL的环境依赖

![image-20230322161333409](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322161333409.png)

重新make

![image-20230322161444780](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322161444780.png)



![image-20230322161457883](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322161457883.png)

### 3.4编译psqlodbc35w.lib

```shell
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
```



**编译结果**

如果遇到下面的问题，这说明还是环境依赖找不到的原因，和编译libpq.lib问题一样的，只需要修改CMakeCache.txt文件对应的环境依赖，参考libpq.lib的修改

![image-20230322161634102](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322161634102.png)



输出正常结果

![image-20230322162117798](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322162117798.png)

![image-20230322162151602](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322162151602.png)



### 3.5制作odbc安装包

```shell
cd %WD%
REM Build psqlodbc.exe
cd psqlodbc-installer
rm -rf win32_dll
mkdir win32_dll
cp %LIB_ODBC_DIR%/output/psqlodbc35w.dll ./win32_dll
cp "%OPENSSL_DIR%"/libssl-1_1.dll ./win32_dll
cp "%OPENSSL_DIR%"/libcrypto-1_1.dll ./win32_dll
makensis odbc-installer.nsi
```



**编译结果**

![image-20230322162318583](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322162318583.png)



### 3.6将odbc打包成tar.gz

```shell
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
```



**编译结果**

![image-20230322164651923](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322164651923.png)

![image-20230322164631029](odbc-win32%E7%BC%96%E8%AF%91%E6%8C%87%E5%AF%BC.assets/image-20230322164631029.png)