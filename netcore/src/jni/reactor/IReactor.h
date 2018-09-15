#ifndef IREACTOR_H
#define IREACTOR_H

#include <stdint.h>

class IReactHandler;

class IReactor{

public:

	virtual ~IReactor(){};

	virtual int addEvent(int fd,int events,IReactHandler* listener,void* tag=0) = 0;
	virtual int delEvent(int fd,int events) = 0;
	virtual int containEvent(int fd,int events) = 0;
	virtual int start() = 0;
	virtual int stop() = 0;


	class EVENT{
	public:
		static const int READ = 1<<0;
		static const int WRITE = 1<<1;
		static const int ERROR = 1<<2;
		
		static const int ALL = READ|WRITE|ERROR;
	};

};

class IReactHandler{

public:
	virtual void onReadable(int fd,IReactor* reactor,void* tag) = 0;
	virtual void onWritable(int fd,IReactor* reactor,void* tag) = 0;
	virtual void onException(int fd,IReactor* reactor,int error,void* tag) = 0;
	virtual void onPeriodicallyCheck(int fd, IReactor* reactor,uint32_t interval, void* tag) = 0;

};


#endif