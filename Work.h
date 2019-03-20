#pragma once


#ifndef _WORK_
#define _WORK_


#include "IocpServer.h"
#include "SocketExFnsHunter.h"

class Work
{
public:
	Work(IocpServer *server);
	~Work();

	void Start();
	void ThreadProc();

private:
	IocpServer *_iocpServer;

};


#endif // _WORK_


