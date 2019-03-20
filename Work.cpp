#include "Work.h"
#include "overlapped.h"
#include "SocketExFnsHunter.h"
#include <WinSock2.h>
#include <stdio.h>
#include <assert.h>
#include <WS2tcpip.h>

DWORD WINAPI ServerWorkerThread(LPVOID p)
{
	assert(p);						//维护指针p
	auto workers = (Work*)p;
	workers->ThreadProc();
	return 0;
}

Work::Work(IocpServer *server)
{
	_iocpServer = (IocpServer*)(server);
}


Work::~Work()
{
}

#pragma pack(push) //保存对齐状态
#pragma pack(1)//设定为4字节对齐

typedef struct acceptex_sockaddr_in {
	CHAR b_zero[10];
	sockaddr_in addr_in;
	CHAR e_zero[2];
}acceptex_sockaddr_in;
#pragma pack(pop)//恢复对齐状态

inline void log_sockaddr_in(const char* tag, sockaddr_in* addr)//地址转换封装
{
	char ip_addr[30];
	inet_ntop(AF_INET, &addr->sin_addr, ip_addr, sizeof(ip_addr));	 //将点分十进制的ip地址转化为用于网络传输的数值格式
	fprintf(stdout, "%s ---- %s:%d\n", tag, ip_addr, ntohs(addr->sin_port));
}

void Work::Start()
{
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);

	//根据系统上可用的处理器数量创建工作线程。为每个处理器创建两个工作线程。

	for (size_t i = 0; i < SystemInfo.dwNumberOfProcessors*2; i++)
	{
		HANDLE ThreadHandle;
		//创建一个服务器工作线程，并将完成端口传递给该线程。
		DWORD ThreadID;
		if ((ThreadHandle = CreateThread(NULL, 0, ServerWorkerThread, this, 0, &ThreadID)) == NULL)
		{
			std::cout << "创建线程错误，错误码是：" << GetLastError() << std::endl;
			return ;
		}
		//关闭线程句柄
		CloseHandle(ThreadHandle);
	}
	_iocpServer->Accept();
}

void Work::ThreadProc()
{
	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	DWORD Flags = 0;
	Overlapped* overlapped = nullptr;

	while (1)
	{
		//获取连接队列
		BOOL bRet = GetQueuedCompletionStatus(_iocpServer->_completion_port,
			&bytes_transferred,
			&completion_key,
			reinterpret_cast<LPOVERLAPPED*>(&overlapped),					//新特性强制类型转换
			INFINITE);
		if (bRet)
		{
			if (overlapped->type == Overlapped::Accept_type)
			{
				auto locale_addr = &((acceptex_sockaddr_in*)(overlapped->_read_buffer))->addr_in;
				auto remote_addr = &((acceptex_sockaddr_in*)(overlapped->_read_buffer + sizeof(acceptex_sockaddr_in)))->addr_in;


				log_sockaddr_in("locale: ", locale_addr);
				log_sockaddr_in("remote: ", remote_addr);

				//acceptex完成了操作，所以我们还要将其关联到完成端口。
				//这里先不改造，等后面我们会进行优化改造
				//我们也可以添加多个accept到完成端口

				_iocpServer->Accept();

				//新客户端连接
				fprintf(stderr, "新客户端加入\n");
				continue;
			}
		}
	}
}