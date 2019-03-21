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
	SYSTEM_INFO SystemInfo;			//ϵͳ��Ϣ�ṹ��
	GetSystemInfo(&SystemInfo);		//��ȡϵͳ��Ϣ�ṹ��

	//����ϵͳ�Ͽ��õĴ������������������̡߳�Ϊÿ���������������������̡߳�
	for (int i = 0; i < SystemInfo.dwNumberOfProcessors * 2; i++)
	{
		//���������������̣߳�������ɶ˿ڴ��ݸ����̡߳�
		DWORD ThreadID;
		HANDLE ThreadHandle = CreateThread(
			NULL,					//lpThreadAttributes ��ʾ�߳��ں˶���İ�ȫ���ԣ�һ�㴫��NULL��ʾʹ��Ĭ�����á�
			0,						//dwStackSize ��ʾ�߳�ջ�ռ��С������0��ʾʹ��Ĭ�ϴ�С��1MB����
			ServerWorkerThread,		//lpStartAddress ��ʾ���߳���ִ�е��̺߳�����ַ������߳̿���ʹ��ͬһ��������ַ��
			this,					//lpParameter �Ǵ����̺߳����Ĳ�����
			0,						//dwCreationFlags ָ������ı�־�������̵߳Ĵ�����Ϊ0��ʾ�̴߳���֮�������Ϳ��Խ��е���
			&ThreadID);				//lpThreadId �������̵߳�ID�ţ�����NULL��ʾ����Ҫ���ظ��߳�ID�š�
		
		if (ThreadHandle == NULL)
		{
			printf("�����߳�ʧ�ܣ������룺 %d\n", GetLastError());
			return;
		}
		printf("�߳�ID��%d\n",ThreadID);

		//�ر��߳̾��
		CloseHandle(ThreadHandle);
	}

	printf("�����߳�����%d", SystemInfo.dwNumberOfProcessors * 2);

	_iocpServer->Accept();		//��ʼ�ȴ�����
}

void Workers::ThreadProc()
{

}
