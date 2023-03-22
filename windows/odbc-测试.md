# odbc-测试

## 1、通过客户端odbct32w测试

### 1.1下载软件并安装

| 软件名字   | 地址                                                         |
| ---------- | ------------------------------------------------------------ |
| odbct32w   | https://download.microsoft.com/download/9/a/1/9a1256c9-d301-4fdc-93b9-370c5b2f9827/mdac28sdk.msi |
| odbc安装包 | https://opengauss.obs.cn-south-1.myhuaweicloud.com/tools/odbc/openGauss-5.0.0-ODBC-windows.tar.gz |



### 1.2启动opengauss并配置白名单

参考opengauss官网，[odbc相关资料](https://docs.opengauss.org/zh/docs/latest/docs/BriefTutorial/ODBC.html)



### 1.3配置数据源

打开windows自带的数据管理，选择odbc data source 32

![image-20230322211202304](odbc-%E6%B5%8B%E8%AF%95.assets/image-20230322211202304.png)



配置提前启动的opengauss数据库

![image-20230322211421933](odbc-%E6%B5%8B%E8%AF%95.assets/image-20230322211421933.png)

### 1.3通过odbct32w测试工具测试

启动odbct32w，在C:\Program Files (x86)\Microsoft Data Access SDK 2.8\Tools\x86，选择对应的架构类型，打开odbct32w.exe

![image-20230322211526968](odbc-%E6%B5%8B%E8%AF%95.assets/image-20230322211526968.png)

选择配置好的数据源

![image-20230322211744562](odbc-%E6%B5%8B%E8%AF%95.assets/image-20230322211744562.png)

执行简单的sql，分别点击第一个按钮，会显示是否执行成功，第二个按钮，将结果输出

![image-20230322211914017](odbc-%E6%B5%8B%E8%AF%95.assets/image-20230322211914017.png)