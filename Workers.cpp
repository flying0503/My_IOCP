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

void Workers::ThreadProc()
{

}
