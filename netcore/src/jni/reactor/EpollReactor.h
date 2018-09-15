#ifndef EPOLL_H
#define EPOLL_H

#include "IReactor.h"
#include <stdlib.h>
#include <map>
#include <list>
#include <sys/epoll.h>

typedef struct EventStub{
	int events;
	struct epoll_event* event;
	int fd;
	IReactHandler* listener;
	void* tag;

}EventStub;

using namespace std;

class EpollReactor:public IReactor,public IReactHandler{

public:
	EpollReactor();
	~EpollReactor();

	int addEvent(int fd,int events,IReactHandler* listener,void* tag=NULL);
	int delEvent(int fd,int events);
	int containEvent(int fd,int events);
	int start();
	int stop();

protected:
	void onReadable(int fd,IReactor*,void* tag);
	void onWritable(int fd,IReactor*,void* tag);
	void onException(int fd,IReactor*,int error,void* tag);
	void onPeriodicallyCheck(int fd,IReactor*, uint32_t interval, void* tag);

private:

	bool mWorking = false;
	void cleanDeletedEvents();
	void dispatchCheck();
	void dispatchEvent(struct epoll_event event[],int size);
	int convertEvent(int event);
	int writeCmd(uint8_t cmd);

	int mEpollFD = -1;
	std::map<int,EventStub*> mFd2Events;
	std::list<EventStub*> mDelEventStubs;
	int mCmdPipe[2];

	static const  int CHECK_PERIOD = 10;
	static const int MAX_EVENTS = 20;
	static const char* TAG;

};



#endif