#include "connection.h"
#include "overlapped.h"



Connection::Implementation::Implementation(const SOCKET& socket, Connection* owner)
	: _socket(socket)
	, _connect_overlapped(CreateOverlapped(Overlapped::Connect_type))
	, _accept_overlapped(CreateOverlapped(Overlapped::Accept_type))
	, _read_overlapped(CreateOverlapped(Overlapped::Read_type))
	, _write_overlapped(CreateOverlapped(Overlapped::Write_type))
	, _sent_bytes()
	, _total_bytes()
	, _write_buffer(nullptr)
	, _write_buffer_size()
{
	_connect_overlapped->connection = owner;
	_accept_overlapped->connection = owner;
	_read_overlapped->connection = owner;
	_write_overlapped->connection = owner;
}

Connection::Implementation::~Implementation()
{
	if (_socket)
		closesocket(_socket);
}

Connection::Connection(const SOCKET& socket)
	: _impl(new Implementation(socket, this))
{
}

Connection::~Connection()
{
}

Overlapped* Connection::GetWriteOverlapped() const
{
	return _impl->_write_overlapped.get();
}

Overlapped* Connection::GetReadOverlapped() const
{
	return _impl->_read_overlapped.get();
}

Overlapped* Connection::GetAcceptOverlapped() const
{
	return _impl->_accept_overlapped.get();
}

Overlapped* Connection::GetConnectOverlapped() const
{
	return _impl->_connect_overlapped.get();
}

void Connection::SetTotalBytes(std::size_t value)
{
	_impl->_total_bytes = value;
}

std::size_t Connection::GetTotalBytes() const
{
	return _impl->_total_bytes;
}

void Connection::SetSentBytes(std::size_t value)
{
	_impl->_sent_bytes = value;
}

std::size_t Connection::GetSentBytes() const
{
	return _impl->_sent_bytes;
}

void Connection::ResizeWriteBuffer(std::size_t new_size)
{
	_impl->_write_buffer.reset(new char[new_size]);
	_impl->_write_buffer_size = new_size;
}

std::size_t Connection::GetWriteBufferSize() const
{
	return _impl->_write_buffer_size;
}

void* Connection::GetWriteBuffer()
{
	return _impl->_write_buffer.get();
}

void* Connection::GetReadBuffer()
{
	return &_impl->_read_buffer;
}

std::size_t Connection::GetReadBufferSize()
{
	return ReadBufferSize;
}
SOCKET& Connection::GetSocket()
{
	return _impl->_socket;
}