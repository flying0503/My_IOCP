#include<WinSock2.h>
#include"connection.h"
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

	Connection* connection;
};

inline Overlapped* CreateOverlapped(Overlapped::Type type)
{
	Overlapped* overlapped = new Overlapped;
	memset(overlapped, 0, sizeof(Overlapped));
	overlapped->type = type;
	return overlapped;
}