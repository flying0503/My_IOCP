#include "Work.h"
#include "overlapped.h"
#include "SocketExFnsHunter.h"
#include <WinSock2.h>
#include <stdio.h>
#include <assert.h>
#include <WS2tcpip.h>

DWORD WINAPI ServerWorkerThread(LPVOID p)
{
	assert(p);						//ά��ָ��p
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

#pragma pack(push) //�������״̬
#pragma pack(1)//�趨Ϊ4�ֽڶ���

typedef struct acceptex_sockaddr_in {
	CHAR b_zero[10];
	sockaddr_in addr_in;
	CHAR e_zero[2];
}acceptex_sockaddr_in;
#pragma pack(pop)//�ָ�����״̬

inline void log_sockaddr_in(const char* tag, sockaddr_in* addr)//��ַת����װ
{
	char ip_addr[30];
	inet_ntop(AF_INET, &addr->sin_addr, ip_addr, sizeof(ip_addr));	 //�����ʮ���Ƶ�ip��ַת��Ϊ�������紫�����ֵ��ʽ
	fprintf(stdout, "%s ---- %s:%d\n", tag, ip_addr, ntohs(addr->sin_port));
}

void Work::Start()
{
	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);

	//����ϵͳ�Ͽ��õĴ������������������̡߳�Ϊÿ���������������������̡߳�

	for (size_t i = 0; i < SystemInfo.dwNumberOfProcessors*2; i++)
	{
		HANDLE ThreadHandle;
		//����һ�������������̣߳�������ɶ˿ڴ��ݸ����̡߳�
		DWORD ThreadID;
		if ((ThreadHandle = CreateThread(NULL, 0, ServerWorkerThread, this, 0, &ThreadID)) == NULL)
		{
			std::cout << "�����̴߳��󣬴������ǣ�" << GetLastError() << std::endl;
			return ;
		}
		//�ر��߳̾��
		CloseHandle(ThreadHandle);
	}
}

void Work::ThreadProc()
{
	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	DWORD Flags = 0;
	Overlapped* overlapped = nullptr;

	//��ȡ���Ӷ���
	BOOL bRet = GetQueuedCompletionStatus(_iocpServer->_completion_port,		
		&bytes_transferred,
		&completion_key,
		reinterpret_cast<LPOVERLAPPED*>(&overlapped),					//������ǿ������ת��
		INFINITE);

	while (1)
	{
		if (bRet)
		{
			if (overlapped->type == Overlapped::Accept_type)
			{
				auto locale_addr = &((acceptex_sockaddr_in*)(overlapped->_read_buffer))->addr_in;
				auto remote_addr = &((acceptex_sockaddr_in*)(overlapped->_read_buffer + sizeof(acceptex_sockaddr_in)))->addr_in;


				log_sockaddr_in("locale: ", locale_addr);
				log_sockaddr_in("remote: ", remote_addr);

				//acceptex����˲������������ǻ�Ҫ�����������ɶ˿ڡ�
				//�����Ȳ����죬�Ⱥ������ǻ�����Ż�����
				//����Ҳ������Ӷ��accept����ɶ˿�

				_iocpServer->Accept();

				//�¿ͻ�������
				fprintf(stderr, "�¿ͻ��˼���\n");
				continue;
			}
		}
	}
}