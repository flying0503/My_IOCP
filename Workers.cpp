#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "Workers.h"
#include "overlapped.h"
#include "SocketExFnsHunter.h"

DWORD WINAPI ServerWorkerThread(LPVOID p)
{
	assert(p);
	Workers *workers = (Workers*)p;
	workers->ThreadProc();
	return 0;
}

Workers::Workers(IocpServer * server)
{
	_iocpServer = const_cast<IocpServer*>(server);
}

void Workers::Start()
{
	SYSTEM_INFO SystemInfo;			//系统信息结构体
	GetSystemInfo(&SystemInfo);		//获取系统信息结构体

	//根据系统上可用的处理器数量创建工作线程。为每个处理器创建两个工作线程。
	for (int i = 0; i < SystemInfo.dwNumberOfProcessors * 2; i++)
	{
		//创建服务器工作线程，并将完成端口传递给该线程。
		DWORD ThreadID;
		HANDLE ThreadHandle = CreateThread(
			NULL,					//lpThreadAttributes 表示线程内核对象的安全属性，一般传入NULL表示使用默认设置。
			0,						//dwStackSize 表示线程栈空间大小。传入0表示使用默认大小（1MB）。
			ServerWorkerThread,		//lpStartAddress 表示新线程所执行的线程函数地址，多个线程可以使用同一个函数地址。
			this,					//lpParameter 是传给线程函数的参数。
			0,						//dwCreationFlags 指定额外的标志来控制线程的创建，为0表示线程创建之后立即就可以进行调度
			&ThreadID);				//lpThreadId 将返回线程的ID号，传入NULL表示不需要返回该线程ID号。
		
		if (ThreadHandle == NULL)
		{
			printf("创建线程失败，错误码： %d\n", GetLastError());
			return;
		}
		printf("线程ID：%d\n",ThreadID);

		//关闭线程句柄
		CloseHandle(ThreadHandle);
	}

	printf("启动线程数：%d", SystemInfo.dwNumberOfProcessors * 2);

	_iocpServer->Accept();		//开始等待链接
}

void Workers::ThreadProc()		//业务函数
{
#if 1//Workers线程处理业务，主循环空闲
	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	DWORD Flags = 0;
	Overlapped *overlapped = nullptr;

	while (1)
	{
		//从队列中获取请求，检查完成端口状态用于接受网络操作的到达
		bool bRet = GetQueuedCompletionStatus(
			_iocpServer->_completion_port,					//完成端口句柄，类成员
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
			_iocpServer->Accept();
			//新客户端连接
			fprintf(stderr, "新客户端加入\n");
			fprintf(stderr, "client:%d\n", overlapped->connection->GetSocket());
			_iocpServer->AsyncRead(overlapped->connection);
			//AsyncRead(overlapped->connection);
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
			fprintf(stderr, "client:%d , msg:%s\n", overlapped->connection->GetSocket(), value);
			//回发功能，给客户端发送回去
			_iocpServer->AsyncWrite(overlapped->connection, value, bytes_transferred);
			//AsyncWrite(overlapped->connection, value, bytes_transferred);
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
				_iocpServer->AsyncRead(overlapped->connection);
				//AsyncRead(overlapped->connection);
			}
		}
	}
#endif //Workers线程处理业务，主循环空闲
}
