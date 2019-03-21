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

void Workers::ThreadProc()		//ҵ����
{
#if 1//Workers�̴߳���ҵ����ѭ������
	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	DWORD Flags = 0;
	Overlapped *overlapped = nullptr;

	while (1)
	{
		//�Ӷ����л�ȡ���󣬼����ɶ˿�״̬���ڽ�����������ĵ���
		bool bRet = GetQueuedCompletionStatus(
			_iocpServer->_completion_port,					//��ɶ˿ھ�������Ա
			&bytes_transferred,								//������ɺ󷵻��ֽ���
			&completion_key,								//������ɺ��ŵ�key
			reinterpret_cast<LPOVERLAPPED*>(&overlapped),	//�ص��ṹ
			INFINITE);										//�ȴ�ʱ�䣬��Ϊ��

		if (bRet == false)
		{
			//�ͷ���ֱ���˳���û�е���closesocket�����ر�����
			if (GetLastError() == WAIT_TIMEOUT || GetLastError() == ERROR_NETNAME_DELETED)
			{
				//�ͻ��˶Ͽ�
				fprintf(stderr, "client:%d �Ͽ�\n", overlapped->connection->GetSocket());
				delete overlapped->connection;
				overlapped = nullptr;
				continue;
			}

		}

		if (overlapped->type == Overlapped::Accept_type)
		{
			//acceptex����˲������������ǻ�Ҫ�����������ɶ˿ڡ�
			//�����Ȳ����죬�Ⱥ������ǻ�����Ż�����
			//����Ҳ������Ӷ��accept����ɶ˿�
			_iocpServer->Accept();
			//�¿ͻ�������
			fprintf(stderr, "�¿ͻ��˼���\n");
			fprintf(stderr, "client:%d\n", overlapped->connection->GetSocket());
			_iocpServer->AsyncRead(overlapped->connection);
			//AsyncRead(overlapped->connection);
			continue;
		}

		if (bytes_transferred == 0)
		{
			//�ͻ��˶Ͽ�
			fprintf(stderr, "client:%d �Ͽ�\n", overlapped->connection->GetSocket());
			delete overlapped->connection;
			overlapped = nullptr;
			continue;
		}

		if (overlapped->type == Overlapped::Type::Read_type)
		{
			// �첽�����
			char* value = reinterpret_cast<char*>(overlapped->connection->GetReadBuffer());
			value[bytes_transferred] = '\0';	//�γɱ�׼�ַ���
			fprintf(stderr, "client:%d , msg:%s\n", overlapped->connection->GetSocket(), value);
			//�ط����ܣ����ͻ��˷��ͻ�ȥ
			_iocpServer->AsyncWrite(overlapped->connection, value, bytes_transferred);
			//AsyncWrite(overlapped->connection, value, bytes_transferred);
			continue;
		}

		if (overlapped->type == Overlapped::Type::Write_type)
		{
			Connection *conn = overlapped->connection;
			conn->SetSentBytes(conn->GetSentBytes() + bytes_transferred);

			//�ж��Ƿ�ֻ������һ����
			if (conn->GetSentBytes() < conn->GetTotalBytes())
			{
				//��ʣ�ಿ���ٷ���
				overlapped->wsa_buf.len = conn->GetTotalBytes() - conn->GetSentBytes();
				overlapped->wsa_buf.buf = reinterpret_cast<CHAR*>(conn->GetWriteBuffer()) + conn->GetSentBytes();

				int send_result = WSASend(
					conn->GetSocket(),								//�ͻ������ӵ��׽���
					&overlapped->wsa_buf,							//wsa������
					1,												//wsa��������Ŀ
					&bytes_transferred,								//�����ֽ�
					0,												//��־λ����Ϊ��
					reinterpret_cast<LPWSAOVERLAPPED>(overlapped),	//�ص��ṹ
					NULL);											//������ɺ�ĵ��ú���ָ�룬��Ϊ��

				if (!(send_result == NULL || (send_result == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING)))
					fprintf(stderr, "��������ʧ��\n");
			}
			else
			{
				//������ɣ��ȴ���ȡ
				_iocpServer->AsyncRead(overlapped->connection);
				//AsyncRead(overlapped->connection);
			}
		}
	}
#endif //Workers�̴߳���ҵ����ѭ������
}
