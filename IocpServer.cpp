#include "IocpServer.h"
#include <WinSock2.h>
#include <mswsock.h>
#include "overlapped.h"
#include <memory>
#include "SocketExFnsHunter.h"
#pragma warning(disable:4996)

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
		ret = WinSockInit();
		if (ret == -1)
		{
			fprintf(stderr, "��ʼ��WinSockInitʧ��\n");
			break;
		}
		_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (!_completion_port)
		{
			fprintf(stderr, "������ɶ˿�ʧ��!\n");
			ret = -1;
			break;
		}

		if ((ret = InitSocket()) == -1)
			break;

		if ((ret = Bind(ip, port)) == -1)
			break;

		if ((ret = Listen(nListen)) == -1)
			break;

		if (Accept() == -1)
			break;
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
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
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
		//�����������׽��֣�����Ҫע��������һ����������Ϊ��WSA_FLAG_OVERLAPPED�ص�ģʽ
		_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "���� WSASocket( listenSocket )ʧ��\n");
			ret = -1;
			break;
		}
		if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(_socket), _completion_port, 0, 0))
		{
			fprintf(stderr, "��listen socket��������ɶ˿�ʧ��\n");
			ret = -1;
		}
	} while (0);
	return ret;
}

int IocpServer::Bind(const char* ip, unsigned short port)
{
	SOCKADDR_IN addr;
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(_socket, (SOCKADDR*)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		std::cout << " WSASocket( bind ) failed. Error:" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

int IocpServer::Listen(unsigned int nListen)
{
	if (listen(_socket, nListen) == SOCKET_ERROR)
	{
		std::cout << " WSASocket( listen ) failed. Error:" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

int IocpServer::Accept()
{
	int ret = -1;
	do{

		//�������ǲ���WSAAccept��
		//��Ϊ�������ڲ�ѯ��ɶ˿ڵ�ʱ���жϳ�����accept��read��connect��write����

		//WSAIoctl��ȡacceptex�ĺ�����ַ
		//���AcceptEx������ָ��
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
			fprintf(stderr, "��ȡAcceptEx ������ַʧ��\n");
			break;
		}

		SOCKET accepted_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (accepted_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "��ʼ��accept socketʧ��\n");
			ret = -1;
			break;
		}

		//����ʹ��Connection�����Overlapped����
		std::unique_ptr<Connection> new_connection(new Connection(accepted_socket));

		DWORD bytes = 0;
		const int accept_ex_result = _acceptex_func
			(
			_socket,																//
			accepted_socket,
			new_connection->GetReadBuffer(),
			0,
			sizeof(sockaddr_in) + 16, 
			sizeof(sockaddr_in) + 16,
			&bytes, 
			reinterpret_cast<LPOVERLAPPED>(new_connection->GetAcceptOverlapped())
			);

		if (!(accept_ex_result == TRUE || WSAGetLastError() == WSA_IO_PENDING))
		{
			ret = -1;
			fprintf(stderr, "����acceptex ����ʧ��\n");
			break;
		}

		// ��accept_socket��������ɶ˿�
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accepted_socket), _completion_port, 0, 0);

		new_connection.release();
	} while (0);
	return ret;
}

void IocpServer::Run(const char* ip, unsigned short port, unsigned int nListen = 5)
{
	if (Init(ip, port, nListen) == -1)
	{
		fprintf(stderr, "����������ʧ��\n");
		return;
	}
	Mainloop();
}

void IocpServer::Mainloop()
{
	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	DWORD Flags = 0;
	Overlapped* overlapped = nullptr;

	while (1)
	{
		//�Ӷ����л�ȡ���󣬼����ɶ˿�״̬���ڽ�����������ĵ���
		bool bRet = GetQueuedCompletionStatus(
			_completion_port,								//��ɶ˿ھ�������Ա
			&bytes_transferred,								
			&completion_key,
			reinterpret_cast<LPOVERLAPPED*>(&overlapped),
			INFINITE);

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
			Accept();
			//�¿ͻ�������
			fprintf(stderr, "�¿ͻ��˼���\n");

			AsyncRead(overlapped->connection);
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
			value[bytes_transferred] = '\0';
			fprintf(stderr, "client:%d , msg:%s\n",overlapped->connection->GetSocket(), value);
			//�ط����ܣ����ͻ��˷��ͻ�ȥ
			AsyncWrite(overlapped->connection, value, bytes_transferred);
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

				int send_result = WSASend(conn->GetSocket(),
					&overlapped->wsa_buf, 1, &bytes_transferred,
					0, reinterpret_cast<LPWSAOVERLAPPED>(overlapped),
					NULL);
				
				if (!(send_result == NULL || (send_result == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING)))
					fprintf(stderr, "��������ʧ��\n");
			}
			else
			{
				//������ɣ��ȴ���ȡ
				AsyncRead(overlapped->connection);
			}
		}
	}

}

void IocpServer::AsyncRead(const Connection* conn)
{
	Overlapped *overlapped = conn->GetReadOverlapped();
	overlapped->wsa_buf.len = overlapped->connection->GetReadBufferSize();
	overlapped->wsa_buf.buf = reinterpret_cast<CHAR*>(overlapped->connection->GetReadBuffer());

	DWORD flags = 0;
	DWORD bytes_transferred = 0;
	
	//�첽������Ͷ��
	int recv_result = WSARecv(overlapped->connection->GetSocket(),
		&overlapped->wsa_buf, 1, &bytes_transferred, &flags,
		reinterpret_cast<LPWSAOVERLAPPED>(overlapped), NULL);
	
	if (!(recv_result == 0 || (recv_result == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING)))
		fprintf(stderr, "��������ʧ��<AsyncRead>ʧ��\n");
}

void IocpServer::AsyncWrite(const Connection* conn, void* data, std::size_t size)
{
	Connection *mutable_conn = const_cast<Connection*>(conn);				//��ͬ������

	if (mutable_conn->GetWriteBufferSize() < size)
		mutable_conn->ResizeWriteBuffer(size);

	memcpy_s(mutable_conn->GetWriteBuffer(), mutable_conn->GetWriteBufferSize(), data, size);

	mutable_conn->SetSentBytes(0);
	mutable_conn->SetTotalBytes(size);

	Overlapped *overlapped = mutable_conn->GetWriteOverlapped();
	overlapped->wsa_buf.len = size;
	overlapped->wsa_buf.buf = reinterpret_cast<CHAR*>(mutable_conn->GetWriteBuffer());

	DWORD bytes;
	//�첽д����Ͷ��
	int send_result = WSASend(mutable_conn->GetSocket(),
		&overlapped->wsa_buf, 1,
		&bytes, 0,
		reinterpret_cast<LPWSAOVERLAPPED>(overlapped),
		NULL);

	if (!(send_result == 0 || (send_result == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING)))
		fprintf(stderr, "��������ʧ��\n");
}