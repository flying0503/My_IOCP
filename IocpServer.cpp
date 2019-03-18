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

#pragma pack(push) //�������״̬
#pragma pack(1)//�趨Ϊ4�ֽڶ���

#if 0
typedef struct acceptex_sockaddr_in {
    CHAR b_zero[10];
    sockaddr_in addr_in;
    CHAR e_zero[2];
}acceptex_sockaddr_in;
#pragma pack(pop)//�ָ�����״̬

inline void log_sockaddr_in(const char* tag,sockaddr_in* addr)//��ַת����װ
{
    char ip_addr[30];
    inet_ntop(AF_INET, &addr->sin_addr, ip_addr, sizeof(ip_addr));	 //�����ʮ���Ƶ�ip��ַת��Ϊ�������紫�����ֵ��ʽ
    fprintf(stdout, "%s ---- %s:%d\n", tag, ip_addr, ntohs(addr->sin_port));
}
#endif // �Ѿ��ƶ���Work�߳���

int IocpServer::WinSockInit()
{
	int ret = -1;
	do
	{
		WORD version = MAKEWORD(2, 2);		//�汾
		WSADATA wsaData;
		_wsa_inited = !WSAStartup(version, &wsaData);		//�汾����
		if (!_wsa_inited)
			break;
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)	//�߰汾���Ͱ汾
		{
			WSACleanup();					//������
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
		//WSASocketר������windows��̣�����ʹ���ص�ģʽ
		_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "���� WSASocket( listenSocket )ʧ��\n");
			ret = -1;
			break;
		}
		//�˴�CreateIoCompletionPort���ڹ���
		if (!CreateIoCompletionPort((HANDLE)_socket, _completion_port, 0, 0))
		{
			fprintf(stderr, "��listen socket��������ɶ˿�ʧ��\n");
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
	if (bind(_socket, (LPSOCKADDR)&addr, sizeof(SOCKADDR)) == SOCKET_ERROR)	//��
	{
		std::cout << " WSASocket( bind ) ʧ��. ������:" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

int IocpServer::Listen(unsigned int nListen)
{
	if (listen(_socket, nListen) == SOCKET_ERROR)
	{
		std::cout << " WSASocket( listen ) ʧ��. ������:" << GetLastError() << std::endl;
		return -1;
	}
	return 0;
}

int IocpServer::Accept()
{
	int ret = -1;
	do{
#if 0
		//�������ǲ���WSAAccept��
		//��Ϊ�������ڲ�ѯ��ɶ˿ڵ�ʱ���жϳ�����accept��read��connect��write����


		LPFN_ACCEPTEX _acceptex_func;			//���AcceptEx������ָ��
		GUID acceptex_guid = WSAID_ACCEPTEX;	//ȫ�ֱ�ʶ��
		DWORD bytes_returned;
		ret = WSAIoctl							//WSAIoctl��ȡacceptex�ĺ�����ַ�����ÿ��Ʋ���
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
#endif // ������ַ��Work�߳�ֻ��ȡһ��

		//�����׽��֣�ipv4������tcpЭ��
		SOCKET accepted_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	
		if (accepted_socket == INVALID_SOCKET)
		{
			fprintf(stderr, "��ʼ��accept socketʧ��\n");
			ret = -1;
			break;
		}

		//��������Overlapped���ͣ���ʱ��ѯ��ɶ˿ڵ�ʱ�������������
		std::unique_ptr<Overlapped> accept_overlapped(new Overlapped);//unique_ptr����ָ��
		memset(accept_overlapped.get(), 0, sizeof(Overlapped));		  //���ṹ��ָ��λ�ó�ʼ��
		accept_overlapped->type = Overlapped::Accept_type;			  //����λ��������

		DWORD bytes = 0;

		//����acceptex���������������accept_ex_result��
		const int accept_ex_result = _acceptex_func
			(
			_socket,												//�����׽���
			accepted_socket,										//�ͻ����׽���
			accept_overlapped->_read_buffer,						//���ڱ��汾����ַ���ͻ��˵�ַ�������յ�����Ϣ
			0,														//���ڴ�����ݵĿռ��С������˲���=0����Acceptʱ����������ݵ�������ֱ�ӷ�������ͨ����Accept������ʱ���ò������Ϊ��sizeof(lpOutputBuffer)(ʵ�ε�ʵ�ʿռ��С) - 2 * (sizeof sockaddr_in + 16)��
			sizeof(sockaddr_in) + 16,								//��ű���ַ��ַ��Ϣ�Ŀռ��С
			sizeof(sockaddr_in) + 16,								//��ű�Զ�˵�ַ��Ϣ�Ŀռ��С
			&bytes,													//���ڴ�Ž��յ������ݳ��ȡ��ò���ֻ����ͬ��IO��ʱ�����Ч���أ�������첽���ص�IO��������֪ͨ��Ϣ����õ���(���MSDN)
			(LPOVERLAPPED)accept_overlapped.get()					//��ʶ�첽����ʱ���ص�IO�ṹ��Ϣ
			);
		//???
		if (!(accept_ex_result == TRUE || WSAGetLastError() == WSA_IO_PENDING))
		{
			ret = -1;
			fprintf(stderr, "����acceptex ����ʧ��\n");
			break;
		}

		// ��accept_socket��������ɶ˿�
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accepted_socket), _completion_port, 0, 0);

		accept_overlapped.release();	//�ͷŶ˿�
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
			fprintf(stderr, "��ʼ��WinSockInitʧ��\n");
			break;
		}
		_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);//������ɶ˿�
		if (!_completion_port)
		{
			fprintf(stderr, "������ɶ˿�ʧ��!\n");
			ret = -1;
			break;
		}

		if ((ret = InitSocket()) == -1)						//socket��ʼ��
			break;

		if ((ret = Bind(IP, port)) == -1)					//�󶨶˿�
			break;

		if ((ret = Listen(nListen)) == -1)					//��������
			break;

		//��ȡacceptex������ַ��ֻ�û�ȡһ��
		SocketExFnsHunter _socketExFnsHunter;
		_acceptex_func = _socketExFnsHunter.AcceptEx;

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
            
            //acceptex����˲������������ǻ�Ҫ�����������ɶ˿ڡ�
			//�����Ȳ����죬�Ⱥ������ǻ�����Ż�����
			//����Ҳ������Ӷ��accept����ɶ˿�
			Accept();

			//�¿ͻ�������
			fprintf(stderr, "�¿ͻ��˼���\n");
			continue;
		}

	}
	#endif//ԭ���߳�

	//���߳̿��С�
	std::cout << "���߳̿���" << std::endl;
	while (1)
	{}
}

void IocpServer::Run(const char* ip, unsigned short port, unsigned int nListen = 5)
{
	if (Init(ip, port, nListen) == -1)
	{
		fprintf(stderr, "����������ʧ��\n");
		return;
	}
	std::cout << "�������Ѿ�����\n";
	Mainloop();
}

