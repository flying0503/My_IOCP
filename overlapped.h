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

	//! Static read buffer
	char _read_buffer[ReadBufferSize];
};
