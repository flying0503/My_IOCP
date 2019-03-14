#ifndef _IocpServer_h_
#define _IocpServer_h_
#include <WinSock2.h>
#include <Windows.h>
#include <io.h>
#include<thread>
#include<iostream>
#pragma comment(lib,"ws2_32.lib")

class IocpServer
{
public:
	IocpServer();
	~IocpServer();

public:
	int Init(const char* IP, unsigned short port,unsigned int nListen);//nListenָ�����ܵȴ���

	int WinSockInit();	//�汾��ʼ��

	int InitSocket();	//�׽��ֳ�ʼ��

	int Bind(const char* IP,unsigned short port);	//�󶨺���
	
	int Listen(unsigned int nListen);	//��������
	
	int Accept();		//���ܺ���

	void Mainloop();	//��ѭ��

	void Run(const char* IP, unsigned short port, unsigned int nListen);//����

public:
	bool _wsa_inited;			//��Ч��־λ
	HANDLE _completion_port;	//��ɶ˿ھ��
	SOCKET _socket;				//�׽��ֽṹ��
};

#endif