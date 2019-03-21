#include <iostream>
#include "IocpServer.h"

using namespace std;

int main(int argc,char *argv[])
{ 
	IocpServer server;
	if (argv[1] == NULL||argv[2] == NULL||argv[3] == NULL)
	{
		cout << "使用方法：My_IOCP.exe IP地址 端口号 等待队列数\n";
		cout << "例如：My_IOCP.exe 127.0.0.1 8888 5 (默认启动方式)\n";
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