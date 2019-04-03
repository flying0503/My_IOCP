#include "IocpServer.h"
#include <WinSock2.h>
#include <mswsock.h>
#include <WS2tcpip.h>
#include "overlapped.h"
#include <memory>
#include "SocketExFnsHunter.h"
#include "Workers.h"
#pragma warning(disable:4996)

inline void log_sockaddr_in(const char* tag, sockaddr_in* addr)
{
	char ip_addr[30];
	inet_ntop(AF_INET, &addr->sin_addr, ip_addr, sizeof(ip_addr));
	fprintf(stdout, "%s ---- %s:%d\n", tag, ip_addr, ntohs(addr->sin_port));
}

#pragma pack(push) //保存对齐状态
#pragma pack(1)//设定为4字节对齐

typedef struct acceptex_sockaddr_in {
	CHAR b_zero[10];
	sockaddr_in addr_in;
	CHAR e_zero[2];
}acceptex_sockaddr_in;
#pragma pack(pop)//恢复对齐状态

IocpServer::IocpServer()
{
	_wsa_inited = false;
	_socket = INVALID_SOCKET;
}


IocpServer::~IocpServer()
{
}

int IocpServer::Init(const char* ip, unsigned short port,unsigned int nListen)
{
	int ret = 0;
	do
	{
		ret = WinSockInit();								//初始化Socket库
		if (ret == -1)
		{
			fprintf(stderr, "初始化WinSockInit失败\n");
			break;
		}
		_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);//创建完成端口
		if (!_completion_port)
		{
			fprintf(stderr, "创建完成端口失败!\n");
			ret = -1;
			break;
		}

		if ((ret = InitSocket()) == -1)				//初始化套接字
			break;

		if ((ret = Bind(ip, port)) == -1)			//绑定
			break;

		if ((ret = Listen(nListen)) == -1)			//监听Socket
			break;

		//if (Accept() == -1)							//接受客户端联入
		//break;
		
		//获取acceptex函数地址，只需要获取一次就可以了
		SocketExFnsHunter _socketExFnsHunter;
		_acceptex_func = _socketExFnsHunter.AcceptEx;

		Workers *_workers = new Workers(this);
		_workers->Start();

	} while (0);
	return ret;

}

int IocpServer::WinSockInit()
{
	int ret = -1;
	do
	{
		WORD version = MAKEWORD(2, 2);
		WSADATA wsaData;
		_wsa_inited = !WSAStartup(version, &wsaData);
		if (!_wsa_inited)
			break;
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)	//高低版本一致，否则报错
		{
			WSACleanup();
			_wsa_inited = false;
			break;
		}
		ret = 0;
	} while (0);
	return ret;
}

int IocpServer::InitSocket()
{
	int ret = 0;
	do
	{
		//创建服务器套接字，这里要注意的是最后一个参数必须为：WSA_FLAG_OVERLAPPED重叠模式
		_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "创建 WSASocket( listenSocket )失败\n");
			ret = -1;
			break;
		}
		if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(_socket), _completion_port, 0, 0))
		{
			fprintf(stderr, "将listen socket关联到完成端口失败\n");
			ret = -1;
		}
	} while (0);
	return ret;
}

int IocpServer::Bind(const char* ip, unsigned short port)
{
	SOCKADDR_IN addr;							//Socket信息
	addr.sin_addr.S_un.S_addr = inet_addr(ip);	//设置IP地址
	addr.sin_family = AF_INET;					//IPV4通信协议		
	addr.sin_port = htons(port);				//端口号
	if (bind(_socket, (SOCKADDR*)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		std::cout << "绑定Socket失败，错误码：" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

int IocpServer::Listen(unsigned int nListen)
{
	if (listen(_socket, nListen) == SOCKET_ERROR)
	{
		std::cout << "监听失败，错误码：" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

void IocpServer::Run(const char* ip, unsigned short port, unsigned int nListen = 5)
{
	if (Init(ip, port, nListen) == -1)
	{
		fprintf(stderr, "服务器启动失败\n");
		return;
	}
	std::cout << "服务器已经启动\n";
	Mainloop();
}

void IocpServer::Mainloop()
{
	//子线程启动，主线程空闲
	while (true)
	{

	}
}

int IocpServer::Accept()
{

}


void IocpServer::AsyncRead()
{
	
}

void IocpServer::AsyncWrite()
{
	
}