kbengine
========

##演示截图(点击图片看视频):
[![kbengine](https://sourceforge.net/p/kbengine/screenshot/323758.jpg)](http://v.youku.com/v_show/id_XMTc2MDcxMDUy.html)

##控制台截图:
[![kbengine](https://sourceforge.net/p/kbengine/screenshot/guiconsole_debug.jpg)](http://v.youku.com/v_show/id_XMTc2MDcxMDUy.html)
[![kbengine](https://sourceforge.net/p/kbengine/screenshot/guiconsole_log.jpg)](http://v.youku.com/v_show/id_XMTc2MDcxMDUy.html)

##什么是KBEngine?

kbengine仿照bigworld技术努力成为一款开源mmog引擎。
bigworld引擎的特点是理论支持无限大世界，多进程分布式负载。
开发者无需过多接触c++底层，无需重复性的实现网络模块，内存管理，
线程管理等底层通用性技术，把精力集中到游戏开发层面上来，
使用python就能够简单高效的打造一款游戏。

简单的介绍一下引擎的各个主要组件部分:

	· dbmgr:
	实现数据的存取，默认使用mysql作为数据库。另外还包括整个游戏的entityID分配、
	共享数据(globaldata, baseAppData, cellAppData)、与第三方运营接口对接框架。


	· baseappmgr:
	主要负责协调所有的baseapp的工作（base部分的负载平衡）等。


	· baseapp:
	客户端与服务端的通讯只能通过baseapp来完成, 一个帐号登陆到baseapp之后就不会再改变，这个baseapp会一直维护这个帐号，
	直到与他断开连接, 并不像cellapp一样会迁移entity。另外它还会定义备份entity的cell数据、定时写数据到数据库、
	定时备份相关数据到其他baseapp、数据包加密解密、逻辑层还可以利用base不迁移的特性在此实现类似全局管理器之类的系统。
	baseapp部署的数量可自由配置。


	· cellappmgr:
	主要负责协调所有cellapp的工作，包括负载均衡处理等。


	· cellapp:
	一个cellapp负责处理一个(尽量)或者多个space，每个space包含一个cell, cell可以维护整个space区域或者部分区域，当一个space上消耗过大时
	cellappmgr将会寻找空闲的cellapp来均衡负载， 整个游戏的实时处理逻辑部分也在cellapp，包括aoi, 逻辑层的ai, entity移动导航等等。 
	cellapp部署的数量可自由配置。

	· loginapp:
	它只处理client的登录接入, 账号由dbmgr检查通过就会从baseappmgr得到一个baseapp的地址发给客户端，
	然后就与客户端断开连接， 客户端需要主动连接获得到的baseapp，可在多台机器部署多个loginapp进程。 


	· client:
	客户端我们将提供基础框架，这个框架不包括渲染部分和输入输出部分的具体实现, 
	我们将提供一个lib文件和一套API接口，开发者可以选择使用自己比较适合的图形渲染引擎与输入输出控制部分， 
	当然我们也会封装一套默认的相关模块，目前我们准备采用开源的渲染引擎ogre来实现图形表现部分。 


	· kbmachine:
	抽象出一个kbe硬件节点(一台硬件服务器只能存在一个这样的进程)。主要用途是接受远程指令处理本机上的组件启动与关闭, 
	提供本机上运行组件的接入口以及搜集当前机器上的一些信息， 
	如：CPU， 内存等。 这些信息会提供给一些对此比较感兴趣的组件。 


	· guiconsole: 
	这是一个控制台工具， 可以实时的观察服务端运行状态， 实时观测不同space中entity的动态，
	并支持动态调试服务端python逻辑层以及查看各个组件的日志，启动服务端与关闭等。 


	· messagelog: 
	收集和备份各个组件的运行日志，可使用guiconsole来查看和搜索相关信息。
##GO!


**设置环境变量:**

kbe会读取KBE_ROOT，KBE_RES_PATH， KBE_HYBRID_PATH系统环境变量来做一些事情。


linux:

	[kbe@localhost ~]# vim ~/.bashrc

	ulimit -c unlimited

	export KBE_ROOT=/home/kbe/kbengine/

	export KBE_RES_PATH=$KBE_ROOT/kbe/res/:$KBE_ROOT/demo/:$KBE_ROOT/demo/res/

	export KBE_HYBRID_PATH=$KBE_ROOT/kbe/bin/Hybrid64/

	[root@localhost ~]# vim /etc/passwd
	
	修改kbe的uid必须唯一, uid用来区分不同的服务端组， 如果二台硬件维护一组服务端那么二台硬件上的uid必须一致, 值需大于0。
windows:

	鼠标右键"我的电脑"->"高级"->"环境变量" 设置对应的值就好了。
	(注意:windows下需要在环境变量中添加UID, 值需大于0)



KBE_ROOT:

	kbe根目录路径。


KBE_RES_PATH:

	相关资源路径用':'或者';'分隔, 第一个res必须是kbe引擎的res, 第二个res必须是用户脚本根目录， 其他无限制。


KBE_HYBRID_PATH:

	kbe二进制文件所在目录路径。



##编译:

linux:

	测试系统(x32&x64): centos >= 5.x, debian >= 5.x
	编译器 gcc: >= 4.4.x


	[root@localhost ~]# cd $KBE_ROOT/kbe/src

	[root@localhost /src]# make


windows:

	安装好vc2008sp1版本直接编译完即可

	KBE_ROOT\kbengine\kbe\src\kbengine_vs90.sln

注意: 

	1: 如使用其他版本编译器最好将python、openssl、log4cxx(kbe\src\libs\*.a)也重新编译。

	2: 某些平台上的mysql路径可能不是/usr/lib64/mysql/mysql_config

	修改kbe\src\build\common.mak中的MYSQL_CONFIG_PATH=/usr/lib64/mysql/mysql_config

	3: 在linux上编译完成如因为python无法初始化无法正常运行的情况(这是个bug http://bugs.python.org/issue11320):

	cd src\lib\python

	./configure

	make

	make install

	再启动服务端。


##配置数据库:
	1: 安装好mysql
		如果是windows系统则my.ini中加入如下代码使mysql大小写敏感
		[mysqld]
		lower_case_table_names=2

	2: 创建一个数据库账户， 假设用户名密码都为"kbe"
		grant all privileges on mydb.* to kbe@'%' identified by 'kbe';
		grant select,insert,update,delete,create,drop on mydb.* to kbe@'%' identified by 'kbe';
		FLUSH PRIVILEGES;

	3: 新建一个数据库， 假设数据库名为"kbe"
		create database kbe;

	4: 在res\server\kbengine_defs.xml的dbmgr节修改databaseName参数(推荐在demo\res\server\kbengine.xml进行重载修改)。



##启动服务端:


linux:

	[root@localhost ~]# cd $KBE_HYBRID_PATH

	sh start.sh

	sh kill.sh

	(注意: 如有开放防火墙请对外开放这些端口tcp:loginapp登录端口、 baseapp登录端口具体请看kbengine_defs.xml。 
	并且允许udp广播端口20086-20088)

windows:

	cd KBE_HYBRID_PATH

	!.bat

	~.bat

	或者使用tools\server\guiconsole\guiconsole.exe来启动和关闭服务端。
	(注意: windows版本仅用于测试， 由于使用select目前不支持大量连接。)

##日志:

	KBE_HYBRID_PATH目录下会产生各组件运行的日志信息， 也可以使用guiconsole来查看。
