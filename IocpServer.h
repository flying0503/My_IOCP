#ifndef _IocpServer_h_
#define _IocpServer_h_
#include <WinSock2.h>
#include <Windows.h>
#include <io.h>
#include <thread>
#include <iostream>
#include <MSWSock.h>
#pragma comment(lib,"ws2_32.lib")

class IocpServer
{
public:
	IocpServer();
	~IocpServer();

public:
	int Init(const char* IP, unsigned short port,unsigned int nListen);//nListen指最大接受等待数

	int WinSockInit();	//版本初始化

	int InitSocket();	//套接字初始化

	int Bind(const char* IP,unsigned short port);	//绑定函数
	
	int Listen(unsigned int nListen);	//监听函数
	
	int Accept();		//接受函数

	void Mainloop();	//主循环

	void Run(const char* IP, unsigned short port, unsigned int nListen);//启动

public:
	bool _wsa_inited;				//无效标志位
	HANDLE _completion_port;		//完成端口句柄
	SOCKET _socket;					//套接字结构体
	LPFN_ACCEPTEX _acceptex_func;	//函数地址
};

#endif