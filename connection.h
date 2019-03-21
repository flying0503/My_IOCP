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
	//func() const ���η���ֵΪconst����

	SOCKET& GetSocket();                                        //��ȡ�׽���

	void* GetReadBuffer();                                      //��ȡ��������

	std::size_t GetReadBufferSize();                            //��ȡ�Ȼ�������С

	void* GetWriteBuffer();                                     //��ȡд������

	std::size_t GetWriteBufferSize() const;                     //��ȡд��������С

	void ResizeWriteBuffer(std::size_t new_size);               //����д������

	std::size_t GetSentBytes() const;                           //��ȡ�����ֽ�

	void SetSentBytes(std::size_t value);                       //���÷����ֽ�

	std::size_t GetTotalBytes() const;                          //��ȡȫ���ֽ�

	void SetTotalBytes(std::size_t value);                      //����ȫ���ֽ�

	Overlapped* GetConnectOverlapped() const;                   //�����ص��ṹ

	Overlapped* GetAcceptOverlapped() const;                    //�����ص��ṹ

	Overlapped* GetReadOverlapped() const;                      //���ص��ṹ

	Overlapped* GetWriteOverlapped() const;                     //д�ص��ṹ

private:
	class  Implementation;										//ʵ����
	std::unique_ptr<Implementation> _impl;			
};

class Connection::Implementation								//ʵ���ඨ�壬Ƕ�׹�ϵ
{
public:
	Implementation(const SOCKET& socket, Connection* owner);
	~Implementation();

public:
	SOCKET _socket;

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