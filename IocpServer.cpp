#include "IocpServer.h"
#include <WinSock2.h>
#include <mswsock.h>
#include "overlapped.h"
#include <memory>
#include "Work.h"

#pragma warning(disable:4996)

IocpServer::IocpServer()
{
	_wsa_inited = false;
	_socket = INVALID_SOCKET;
}


IocpServer::~IocpServer()
{
}

#pragma pack(push) //保存对齐状态
#pragma pack(1)//设定为4字节对齐

#if 0
typedef struct acceptex_sockaddr_in {
    CHAR b_zero[10];
    sockaddr_in addr_in;
    CHAR e_zero[2];
}acceptex_sockaddr_in;
#pragma pack(pop)//恢复对齐状态

inline void log_sockaddr_in(const char* tag,sockaddr_in* addr)//地址转换封装
{
    char ip_addr[30];
    inet_ntop(AF_INET, &addr->sin_addr, ip_addr, sizeof(ip_addr));	 //将点分十进制的ip地址转化为用于网络传输的数值格式
    fprintf(stdout, "%s ---- %s:%d\n", tag, ip_addr, ntohs(addr->sin_port));
}
#endif // 已经移动到Work线程中

int IocpServer::WinSockInit()
{
	int ret = -1;
	do
	{
		WORD version = MAKEWORD(2, 2);		//版本
		WSADATA wsaData;
		_wsa_inited = !WSAStartup(version, &wsaData);		//版本启动
		if (!_wsa_inited)
			break;
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)	//高版本、低版本
		{
			WSACleanup();					//清理函数
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
		//WSASocket专门用于windows编程，可以使用重叠模式
		_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "创建 WSASocket( listenSocket )失败\n");
			ret = -1;
			break;
		}
		//此处CreateIoCompletionPort用于关联
		if (!CreateIoCompletionPort((HANDLE)_socket, _completion_port, 0, 0))
		{
			fprintf(stderr, "将listen socket关联到完成端口失败\n");
			ret = -1;
		}
	} while (0);
	return ret;
}

int IocpServer::Bind(const char* IP, unsigned short port)
{
	SOCKADDR_IN addr;
	addr.sin_addr.S_un.S_addr = inet_addr(IP);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(_socket, (LPSOCKADDR)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR)	//绑定
	{
		std::cout << " WSASocket( bind ) 失败. 错误码:" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

int IocpServer::Listen(unsigned int nListen)
{
	if (listen(_socket, nListen) == SOCKET_ERROR)
	{
		std::cout << " WSASocket( listen ) 失败. 错误码:" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

int IocpServer::Accept()
{
	int ret = -1;
	do{
#if 0
		//这里我们不用WSAAccept，
		//因为我们需在查询完成端口的时候判断出，是accept、read、connect和write类型


		LPFN_ACCEPTEX _acceptex_func;			//存放AcceptEx函数的指针
		GUID acceptex_guid = WSAID_ACCEPTEX;	//全局标识符
		DWORD bytes_returned;
		ret = WSAIoctl							//WSAIoctl获取acceptex的函数地址，设置控制参数
		(
			_socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&acceptex_guid, sizeof(acceptex_guid),
			&_acceptex_func, sizeof(_acceptex_func),
			&bytes_returned, NULL, NULL
		);
		if (ret != 0)
		{
			ret = -1;
			fprintf(stderr, "获取AcceptEx 函数地址失败\n");
			break;
		}
#endif // 函数地址在Work线程只获取一次

		//接受套接字，ipv4，流，tcp协议
		SOCKET accepted_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	
		if (accepted_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "初始化accept socket失败\n");
			ret = -1;
			break;
		}

		//这里设置Overlapped类型，到时查询完成端口的时候可以区别类型
		//Overlapped *accept_overlapped(new Overlapped);
		std::unique_ptr<Overlapped> accept_overlapped(new Overlapped);//unique_ptr智能指针
		memset(accept_overlapped.get(), 0, sizeof(Overlapped));				//将结构体指针位置初始化
		accept_overlapped->type = Overlapped::Accept_type;			  //设置位接受类型

		DWORD bytes = 0; 

		//调用acceptex函数，结果保存在accept_ex_result中
		const int accept_ex_result = _acceptex_func
			(
			_socket,												//本机套接字
			accepted_socket,										//客户端套接字
			accept_overlapped->_read_buffer,						//用于保存本机地址、客户端地址、可能收到的信息
			0,														//用于存放数据的空间大小。如果此参数=0，则Accept时将不会待数据到来，而直接返回所以通常当Accept有数据时，该参数设成为：sizeof(lpOutputBuffer)(实参的实际空间大小) - 2 * (sizeof sockaddr_in + 16)。
			sizeof(sockaddr_in) + 16,								//存放本地址地址信息的空间大小
			sizeof(sockaddr_in) + 16,								//存放本远端地址信息的空间大小
			&bytes,													//用于存放接收到的数据长度。该参数只是在同步IO的时候会有效返回，如果是异步的重叠IO，需从完成通知信息里面得到。(详见MSDN)
			(LPOVERLAPPED)accept_overlapped.get()					//标识异步操作时的重叠IO结构信息
			);
		//???
		if (!(accept_ex_result == TRUE || WSAGetLastError() == WSA_IO_PENDING))
		{
			ret = -1;
			fprintf(stderr, "调用acceptex 函数失败\n");
			break;
		}

		// 将accept_socket关联到完成端口
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accepted_socket), _completion_port, 0, 0);

		accept_overlapped.release();	//释放端口
	} while (0);
	return ret;
}

int IocpServer::Init(const char* IP, unsigned short port, unsigned int nListen)
{
	int ret = 0;
	do
	{
		ret = WinSockInit();
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

		if ((ret = InitSocket()) == -1)						//socket初始化
			break;

		if ((ret = Bind(IP, port)) == -1)					//绑定端口
			break;

		if ((ret = Listen(nListen)) == -1)					//监听启动
			break;

		//获取acceptex函数地址，只用获取一次
		SocketExFnsHunter _socketExFnsHunter;
		_acceptex_func = _socketExFnsHunter.AcceptEx;

		//建立工作者线程WorkThread
		Work *_workers = new Work(this);
		_workers->Start();

	} while (0);
	return ret;

}

void IocpServer::Mainloop()
{
	#if 0
	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	DWORD Flags = 0;
	Overlapped* overlapped = nullptr;

	while (GetQueuedCompletionStatus(_completion_port, &bytes_transferred, &completion_key, reinterpret_cast<LPOVERLAPPED*>(&overlapped), INFINITE))
	{
		if (!overlapped)
			continue;

		if (overlapped->type == Overlapped::Accept_type)
		{
            auto locale_addr = &((acceptex_sockaddr_in*)(overlapped->_read_buffer))->addr_in;
            auto remote_addr = &((acceptex_sockaddr_in*)(overlapped->_read_buffer+sizeof(acceptex_sockaddr_in)))->addr_in;
			

            log_sockaddr_in("locale: ", locale_addr);
            log_sockaddr_in("remote: ", remote_addr);
            
            //acceptex完成了操作，所以我们还要将其关联到完成端口。
			//这里先不改造，等后面我们会进行优化改造
			//我们也可以添加多个accept到完成端口
			Accept();

			//新客户端连接
			fprintf(stderr, "新客户端加入\n");
			continue;
		}

	}
	#endif//原主线程

	//主线程空闲。
	std::cout << "主线程空闲" << std::endl;
	while (1)
	{}
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

