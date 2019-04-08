#ifndef _connection_h_
#define _connection_h_
#include<memory>
#include<WinSock2.h>
#define ReadBufferSize 1024
struct Overlapped;

class Connection
{
public:
	Connection(const SOCKET& socket);
	~Connection();

public:
	//func() const 修饰返回值为const类型

	SOCKET& GetSocket();                                        //获取套接字

	void* GetReadBuffer();                                      //获取读缓冲区

	std::size_t GetReadBufferSize();                            //获取度缓冲区大小

	void* GetWriteBuffer();                                     //获取写缓冲区

	std::size_t GetWriteBufferSize() const;                     //获取写缓冲区大小

	void ResizeWriteBuffer(std::size_t new_size);               //调整写缓冲区

	std::size_t GetSentBytes() const;                           //获取发送字节

	void SetSentBytes(std::size_t value);                       //设置发送字节

	std::size_t GetTotalBytes() const;                          //获取全部字节

	void SetTotalBytes(std::size_t value);                      //设置全部字节

	Overlapped* GetConnectOverlapped() const;                   //链接重叠结构

	Overlapped* GetAcceptOverlapped() const;                    //接受重叠结构

	Overlapped* GetReadOverlapped() const;                      //读重叠结构

	Overlapped* GetWriteOverlapped() const;                     //写重叠结构

private:
	class  Implementation;										//实现类
	std::unique_ptr<Implementation> _impl;			
};

class Connection::Implementation								//实现类定义，嵌套关系
{
public:
	Implementation(const SOCKET& socket, Connection* owner);
	~Implementation();

public:
	SOCKET client_socket;

	char _read_buffer[ReadBufferSize];

	std::size_t _write_buffer_size;

	std::unique_ptr<char> _write_buffer;

	std::size_t _sent_bytes;

	std::size_t _total_bytes;

	std::unique_ptr<Overlapped> _connect_overlapped;

	std::unique_ptr<Overlapped> _accept_overlapped;

	std::unique_ptr<Overlapped> _read_overlapped;

	std::unique_ptr<Overlapped> _write_overlapped;
};
#endif