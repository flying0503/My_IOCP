#include <iostream>
#include "IocpServer.h"

int main(int argc,char *argv[])
{
	IocpServer server;
	if (argv[1] == NULL||argv[2] == NULL||argv[3])
	{
		std::cout << "ʹ�÷�����My_IOCP.exe IP��ַ �˿ں� �ȴ�������\n";
		std::cout << "���磺My_IOCP.exe 127.0.0.1 8888 5 (Ĭ��������ʽ)\n";
		server.Run("127.0.0.1", 8888,5);
	}
	else
	{
		unsigned short port = atoi(argv[2]);
		unsigned int nListen = atoi(argv[3]);
		server.Run(argv[1], port, nListen);
	}
	
	getchar();
	return 0;
}