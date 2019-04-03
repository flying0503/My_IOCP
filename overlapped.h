#include<WinSock2.h>
#define ReadBufferSize 1024
struct Overlapped
{
	enum Type
	{
		Connect_type,
		Accept_type,
		Read_type,
		Write_type,
	};
	WSAOVERLAPPED overlapped;

	Type type;

	WSABUF wsa_buf;

	SOCKET _accepted_socket;
};