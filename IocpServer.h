#ifndef _IocpServer_h_
#define _IocpServer_h_
#include <WinSock2.h>
#include <Windows.h>
#include <io.h>
#include <thread>
#include <iostream>
#include "SocketExFnsHunter.h"
#pragma comment(lib,"ws2_32.lib")

class IocpServer
{
public:
	IocpServer();
	~IocpServer();

public:
	int Init(const char* ip, unsigned short port,unsigned int nListen);

	int WinSockInit();

	int InitSocket();
	
	int Accept();

	int Bind(const char* ip,unsigned short port);

	int Listen(unsigned int nListen);

	void Run(const char* ip, unsigned short port, unsigned int nListen);

	void Mainloop();

	void AsyncRead();

	void AsyncWrite();


public:
	bool _wsa_inited;				//_wsa初始化标志位
	HANDLE _completion_port;		//完成端口key
	SOCKET _socket;					//服务器套接字
	LPFN_ACCEPTEX _acceptex_func;	//用于保存AcceptEx()的指针
};

#endif