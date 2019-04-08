#ifndef _IocpServer_h_
#define _IocpServer_h_
#include <WinSock2.h>
#include <Windows.h>
#include <io.h>
#include <thread>
#include <iostream>
#include "connection.h"
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

	int Bind(const char* ip,unsigned short port);

	int Listen(unsigned int nListen);

	void Run(const char* ip, unsigned short port, unsigned int nListen);

	void Mainloop();

	void AsyncRead(const Connection* conn);

	char *DoRead(Overlapped *overlapped, DWORD &bytes_transferred);

	void AsyncWrite(const Connection* conn, void* data, std::size_t size);

	bool DoWrite(Overlapped * overlapped, DWORD &bytes_transferred);

	int PostAccept();

	int DoAccept(Overlapped *overlapped);

public:
	bool _wsa_inited;
	HANDLE _completion_port;
	SOCKET _socket;
	LPFN_ACCEPTEX _acceptex_func;	//用于保存AcceptEx()的指针
};

#endif