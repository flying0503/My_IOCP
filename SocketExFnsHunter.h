#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>

typedef BOOL(WINAPI *AcceptExPtr)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL(WINAPI *ConnectExPtr)(SOCKET, const struct sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef void (WINAPI *GetAcceptExSockaddrsPtr)(PVOID, DWORD, DWORD, DWORD, LPSOCKADDR *, LPINT, LPSOCKADDR *, LPINT);

class SocketExFnsHunter
{
public:
    SocketExFnsHunter(){ Hunt(); }
    AcceptExPtr AcceptEx;							//函数指针
    ConnectExPtr ConnectEx;							//函数指针
    GetAcceptExSockaddrsPtr GetAcceptExSockaddrs;	//函数指针

private:

    void Hunt()
    {
        SOCKET Socket = socket(AF_INET, SOCK_STREAM, 0);
        if (Socket == INVALID_SOCKET)
            return;

		
        const GUID acceptex = WSAID_ACCEPTEX;			//AcceptEx函数指针
        AcceptEx = (AcceptExPtr)get_extension_function(Socket, &acceptex);
        const GUID connectex = WSAID_CONNECTEX;			// GUID，这个是识别AcceptEx函数必须的全局变量
        ConnectEx = (ConnectExPtr)get_extension_function(Socket, &connectex);
        const GUID getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        GetAcceptExSockaddrs = (GetAcceptExSockaddrsPtr)get_extension_function(Socket, &getacceptexsockaddrs);
        
        closesocket(Socket);
    }

    void * get_extension_function(SOCKET s, const GUID *which_fn)
    {
        void *ptr = NULL;
        DWORD bytes = 0;
        WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,			//控制端口模式
            (GUID*)which_fn, sizeof(*which_fn),
            &ptr, sizeof(ptr),
            &bytes, NULL, NULL);

        return ptr;
    }
};