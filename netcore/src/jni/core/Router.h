/**
* not thread safe do not use this in multi-thread.
**/

#ifndef Router_H
#define Router_H

#include "IPAdpt.h"
#include "Error.h"
#include <pthread.h>
#include <stdlib.h>
#include "IReactor.h"
#include "Session.h"
#include "Defines.h"
#include "EventDispatcher.h"
#include "BufferQueue.h"

class Router:public IReactHandler,public EventHandler<Event,Result>{

public:
	

   	Router(int tunFd);
   	~Router();

   	int init();
   	int start();
   	int stop();

protected:
	void onReadable(int fd,IReactor*,void* tag);
	void onWritable(int fd,IReactor*,void* tag);
	void onException(int fd,IReactor*,int error,void* tag);
	void onPeriodicallyCheck(int fd, IReactor*, uint32_t interval, void* tag);
	int onSyncEvent(Event& event,Result* result);
	int onEvent(Event& event);

private:
	static const char* TAG;
	const int MTU = 65535;
	uint8_t* mIPData = NULL;

	Error route(std::list<Session*>& sessions,IPHeader& ip);
	int releaseSessions(std::list<Session*>& sessions);
	int cleanDeadSessions();
	int cleanDeadSessions(std::list<Session*>& sessions);
	void releaseData();

	std::list<Session*> mUDPSessions;
	std::list<Session*> mTCPSessions;

	IReactor* mReactor;

	int mTunFd;
	BufferQueue<> mData;
	int t = 0;
	char mTemporary[128];
	
};

#endif