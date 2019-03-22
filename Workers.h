#ifndef _WORKERS_
#define _WORKERS_

#include <mswsock.h>
#include "IocpServer.h"

class Workers
{
public:
	Workers(IocpServer* server);
	void Start();
    void ThreadProc();
private:
	IocpServer* _iocpServer;
};

#endif // !_WORKERS_


