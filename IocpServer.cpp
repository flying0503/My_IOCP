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

int IocpServer::Accept()
{
	int ret = -1;
	do{
#if 0  //不需要获取AcceptEx()函数指针，由SocketExFnsHunter完成
		//这里我们不用WSAAccept，
		//因为我们需在查询完成端口的时候判断出，是accept、read、connect和write类型

		//WSAIoctl获取acceptex的函数地址
		//存放AcceptEx函数的指针
		LPFN_ACCEPTEX _acceptex_func;
		GUID acceptex_guid = WSAID_ACCEPTEX;
		DWORD bytes_returned;
		ret = WSAIoctl
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
#endif // 0  //不需要获取AcceptEx()函数指针，由SocketExFnsHunter完成

		

		SOCKET accepted_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);			//客户端套接字
		if (accepted_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "初始化accept socket失败\n");
			ret = -1;
			break;
		}

		//这里使用Connection类替代Overlapped结构体
		std::unique_ptr<Connection> new_connection(new Connection(accepted_socket));

		DWORD bytes = 0;
		const int accept_ex_result = _acceptex_func
			(
			_socket,																//监听套接字
			accepted_socket,														//客户端套接字
			new_connection->GetReadBuffer(),										//三用途缓冲区
			0,																		//三用途缓冲区大小
			sizeof(sockaddr_in) + 16,												//服务器地址信息空间大小
			sizeof(sockaddr_in) + 16,												//客户端地址信息空间大小
			&bytes,																	//out参数，用于存放接收到的数据长度。该参数只是在同步IO的时候会有效返回，如果是异步的重叠IO，需从完成通知信息里面得到。(详见MSDN)
			reinterpret_cast<LPOVERLAPPED>(new_connection->GetAcceptOverlapped())	//重叠结构
			);

		if (!(accept_ex_result == TRUE || WSAGetLastError() == WSA_IO_PENDING))
		{
			ret = -1;
			fprintf(stderr, "调用acceptex 函数失败\n");
			break;
		}

		// 将accept_socket关联到完成端口
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accepted_socket), _completion_port, 0, 0);

		new_connection.release();
	} while (0);
	return ret;
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
#if 0
	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	DWORD Flags = 0;
	Overlapped* overlapped = nullptr;

	while (1)
	{
		//从队列中获取请求，检查完成端口状态用于接受网络操作的到达
		bool bRet = GetQueuedCompletionStatus(
			_completion_port,								//完成端口句柄，类成员
			&bytes_transferred,								//操作完成后返回字节数
			&completion_key,								//操作完成后存放的key
			reinterpret_cast<LPOVERLAPPED*>(&overlapped),	//重叠结构
			INFINITE);										//等待时间，设为无

		if (bRet == false)
		{
			//客服端直接退出，没有调用closesocket正常关闭连接
			if (GetLastError() == WAIT_TIMEOUT || GetLastError() == ERROR_NETNAME_DELETED)
			{
				//客户端断开
				fprintf(stderr, "client:%d 断开\n", overlapped->connection->GetSocket());
				delete overlapped->connection;
				overlapped = nullptr;
				continue;
			}

		}
		
		if (overlapped->type == Overlapped::Accept_type)
		{
			//acceptex完成了操作，所以我们还要将其关联到完成端口。
			//这里先不改造，等后面我们会进行优化改造
			//我们也可以添加多个accept到完成端口
			Accept();
			//新客户端连接
			fprintf(stderr, "新客户端加入\n");
			fprintf(stderr, "client:%d\n", overlapped->connection->GetSocket());
			AsyncRead(overlapped->connection);
			continue;
		}

		if (bytes_transferred == 0)
		{
			//客户端断开
			fprintf(stderr, "client:%d 断开\n", overlapped->connection->GetSocket());
			delete overlapped->connection;
			overlapped = nullptr;
			continue;
		}

		if (overlapped->type == Overlapped::Type::Read_type)
		{
			// 异步读完成
			char* value = reinterpret_cast<char*>(overlapped->connection->GetReadBuffer());
			value[bytes_transferred] = '\0';	//形成标准字符串
			fprintf(stderr, "client:%d , msg:%s\n",overlapped->connection->GetSocket(), value);
			//回发功能，给客户端发送回去
			AsyncWrite(overlapped->connection, value, bytes_transferred);
			continue;
		}

		if (overlapped->type == Overlapped::Type::Write_type)
		{
			Connection *conn = overlapped->connection;
			conn->SetSentBytes(conn->GetSentBytes() + bytes_transferred);
			
			//判断是否只发送了一部分
			if (conn->GetSentBytes() < conn->GetTotalBytes())
			{
				//将剩余部分再发送
				overlapped->wsa_buf.len = conn->GetTotalBytes() - conn->GetSentBytes();
				overlapped->wsa_buf.buf = reinterpret_cast<CHAR*>(conn->GetWriteBuffer()) + conn->GetSentBytes();

				int send_result = WSASend(
					conn->GetSocket(),								//客户端链接的套接字
					&overlapped->wsa_buf,							//wsa缓冲区
					1,												//wsa缓冲区数目
					&bytes_transferred,								//传送字节
					0,												//标志位，设为零
					reinterpret_cast<LPWSAOVERLAPPED>(overlapped),	//重叠结构
					NULL);											//操作完成后的调用函数指针，设为空
				
				if (!(send_result == NULL || (send_result == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING)))
					fprintf(stderr, "发送数据失败\n");
			}
			else
			{
				//发送完成，等待读取
				AsyncRead(overlapped->connection);
			}
		}
	}
#endif //现有Workers线程去处理业务，主循环空闲

	while (true)
	{

	}
}

void IocpServer::AsyncRead(const Connection* conn)
{
	Overlapped *overlapped = conn->GetReadOverlapped();
	overlapped->wsa_buf.len = overlapped->connection->GetReadBufferSize();
	overlapped->wsa_buf.buf = reinterpret_cast<CHAR*>(overlapped->connection->GetReadBuffer());

	DWORD flags = 0;
	DWORD bytes_transferred = 0;
	
	//异步读请求投递
	int recv_result = WSARecv(
		overlapped->connection->GetSocket(),			//客户端链接的套接字
		&overlapped->wsa_buf,							//wsa缓冲区
		1,												//wsa缓冲区数目
		&bytes_transferred,								//传送字节
		&flags,											//标志位，设为零
		reinterpret_cast<LPWSAOVERLAPPED>(overlapped),	//重叠结构
		NULL);											//操作完成后的调用函数指针，设为空
	
	if (!(recv_result == 0 || (recv_result == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING)))
		fprintf(stderr, "接收数据失败<AsyncRead>失败\n");
}

void IocpServer::AsyncWrite(const Connection* conn, void* data, std::size_t size)
{
	Connection *mutable_conn = const_cast<Connection*>(conn);				//不同的链接

	if (mutable_conn->GetWriteBufferSize() < size)
		mutable_conn->ResizeWriteBuffer(size);

	memcpy_s(mutable_conn->GetWriteBuffer(), mutable_conn->GetWriteBufferSize(), data, size);

	mutable_conn->SetSentBytes(0);
	mutable_conn->SetTotalBytes(size);

	Overlapped *overlapped = mutable_conn->GetWriteOverlapped();
	overlapped->wsa_buf.len = size;
	overlapped->wsa_buf.buf = reinterpret_cast<CHAR*>(mutable_conn->GetWriteBuffer());

	DWORD bytes;
	//异步写请求投递
	int send_result = WSASend(mutable_conn->GetSocket(),
		&overlapped->wsa_buf, 1,
		&bytes, 0,
		reinterpret_cast<LPWSAOVERLAPPED>(overlapped),
		NULL);

	if (!(send_result == 0 || (send_result == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING)))
		fprintf(stderr, "发送数据失败\n");
}