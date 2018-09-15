#include "EpollReactor.h"
#include "Log.h"
#include "Defines.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

const char* EpollReactor::TAG = "VpnLib.EpollReactor";

EpollReactor::EpollReactor(){

	mEpollFD = epoll_create(1);
	if(mEpollFD < 0){
		LOGE(TAG,"create epoll fd failed %s(%d)",errno,strerror(errno));
		return;
	}

	if(pipe(mCmdPipe)){
		LOGE(TAG,"create command pipe failed. %d(%s)",errno,strerror(errno));
		return;
	}

	for (int i = 0; i < 2; i++) {
        int flags = fcntl(mCmdPipe[i], F_GETFL, 0);
        if (flags < 0 || fcntl(mCmdPipe[i], F_SETFL, flags | O_NONBLOCK) < 0){
            LOGE(TAG, "fcntl cmd pipe [%d] O_NONBLOCK error %d: %s",i, errno, strerror(errno));
        }
    }

    addEvent(mCmdPipe[0],EVENT::READ,this);
}

EpollReactor::~EpollReactor(){
	LOGR(TAG,"destroy reactor>>>");
	if(mEpollFD >= 0){
		for(map<int,EventStub*>::iterator itr = mFd2Events.begin();itr!=mFd2Events.end();itr++){
			int fd = itr->first;
	        EventStub* pEventStub = itr->second;
			struct epoll_event* pEpollEvent = pEventStub->event;
	        if(epoll_ctl(mEpollFD,EPOLL_CTL_DEL,itr->first,pEpollEvent)){
	        	LOGE(TAG,"remvoe fd(%d) failed, %s(%d)",fd, strerror(errno), errno);
	        	continue;
	        }

	        SAFE_DELETE(pEpollEvent);
			SAFE_DELETE(pEventStub);
			mFd2Events.erase(itr->first);
	    }

		close(mEpollFD);
		LOGD(TAG,"close epoll fd success.");
	}else{
		LOGE(TAG,"nothing need to release, epoll fd < 0");
	}

	for(int i=0;i<2;i++){
		if(close(mCmdPipe[i])){
			LOGE(TAG,"close cmd pipe %d failed, %s(%d)",i,strerror(errno),errno);
		}
	}
}

int EpollReactor::addEvent(int fd,int events,IReactHandler *listener,void* tag){
	if(fd < 0){
		return -1;
	}
	
	if(mEpollFD < 0){
		LOGE(TAG,"addEvent failed,epoll fd < 0");
		return -1;
	}

	map<int,EventStub*>::iterator itr = mFd2Events.find(fd);
	if(itr != mFd2Events.end()){
		EventStub* pEventStub = itr->second;
		pEventStub->events |= events;
		struct epoll_event* pEpollEvent = pEventStub->event;
		pEpollEvent->events = convertEvent(pEventStub->events);
		pEventStub->listener = listener;
		pEventStub->tag = tag;
		if (epoll_ctl(mEpollFD, EPOLL_CTL_MOD, fd, pEpollEvent)) {
			LOGE(TAG,"modify events failed %d(%s)",errno, strerror(errno));
			return 1;
		}

		LOGD(TAG,"modify events(%p) success. fd(%d) events(%d,%d)",pEpollEvent,fd,events,pEpollEvent->events);
	}else{
		EventStub* pEventStub = new EventStub();
		pEventStub->events = events;
		struct epoll_event* pEpollEvent = new struct epoll_event();
		pEpollEvent->events = convertEvent(pEventStub->events);
		pEpollEvent->data.ptr = pEventStub;
		pEventStub->fd = fd;
		pEventStub->event = pEpollEvent;
		pEventStub->listener = listener;
		pEventStub->tag = tag;

		if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, fd, pEpollEvent)) {
			LOGE(TAG,"add events failed, fd=%d, %d(%s)", fd, errno, strerror(errno));
			SAFE_DELETE(pEpollEvent);
			SAFE_DELETE(pEventStub);
			return 1;
		}

		mFd2Events.insert(pair<int,EventStub*>(fd,pEventStub));

		LOGD(TAG,"addEvent success. ev(%p) fd(%d) events(%d,%d)",pEpollEvent, fd,events,pEpollEvent->events);
	}

	return 0;
}

int EpollReactor::delEvent(int fd,int events){
	LOGD(PTAG("test"),"delEvent fd(%d), events(%d)",fd,events);

	if(mEpollFD < 0){
		LOGE(TAG,"delEvent failed,epoll fd(%d) < 0",fd);
		return -1;
	}

	map<int,EventStub*>::iterator itr = mFd2Events.find(fd);
	if(itr == mFd2Events.end()){
		LOGI(TAG,"not find events relate to fd(%d)",fd);
		return -1;
	}

	EventStub* pEventStub = itr->second;
	if(pEventStub->listener == NULL){
		return 0;
	}

	pEventStub->events &= (~events);
	struct epoll_event* pEpollEvent = pEventStub->event;
	pEpollEvent->events = convertEvent(pEventStub->events);

	int op = EPOLL_CTL_MOD;
	if(pEventStub->events == 0){
		LOGD(TAG,"rm fd(%d)",fd);
		op = EPOLL_CTL_DEL;
	}

	if(epoll_ctl(mEpollFD,op,fd,pEpollEvent)){
		LOGE(TAG,"delEvent,modify events failed %d", fd);
		return -1;
	}

	if(op == EPOLL_CTL_DEL){
		SAFE_DELETE(pEpollEvent);
		pEventStub->listener = NULL;
		mDelEventStubs.push_back(pEventStub);
		mFd2Events.erase(itr);
	}

	LOGD(TAG,"delEvent success, ev(%p) fd(%d) events(%d)",pEpollEvent,fd,events);
	return 0;
}

int EpollReactor::containEvent(int fd,int events){
	if(mEpollFD < 0){
		LOGE(TAG,"containEvent epoll fd < 0");
		return 0;
	}

	LOGD(TAG,"containEvent check %d",events);
	int cevents = convertEvent(events);
	map<int,EventStub*>::iterator itr = mFd2Events.find(fd);
	if(itr != mFd2Events.end()){
		EventStub* pEventStub = itr->second;
		if(pEventStub->listener == NULL){
			return 0;
		}

		struct epoll_event* pEpollEvent = pEventStub->event;
		if((pEpollEvent->events&cevents) == cevents){
			LOGD(TAG,"containEvent found ev(%p) events(%d, %d), fd(%d,%d)",pEpollEvent, pEpollEvent->events,events,fd,pEventStub->fd);
			return 1;
		}
		
		LOGD(TAG,"containEvent not found ev(%p) events(%d, %d), fd(%d,%d)",pEpollEvent, pEpollEvent->events,events,fd,pEventStub->fd);
		return 0;
	}else{
		LOGD(TAG,"containEvent not found registed fd.");
		return 0;
	}
}

int EpollReactor::start(){
	if(mWorking){
		LOGE(TAG,"reactor already running.");
		return 1;
	}

	time_t lastCheck = time(NULL);
	mWorking = true;
	struct epoll_event ev[MAX_EVENTS];
	while(mWorking){

        int ready = epoll_wait(mEpollFD, ev, MAX_EVENTS, CHECK_PERIOD*1000);
		LOGD(TAG,"%d events are ready",ready);
        if(ready < 0){
        	if (errno == EINTR || errno == EINPROGRESS) {
                LOGW(TAG,"epoll_wait error %d: %s",errno, strerror(errno));
                continue;
            }

            LOGE(TAG,"epoll_wait error %d: %s",errno, strerror(errno));
            break;
        }

        dispatchEvent(ev,ready);

        time_t now = time(NULL);
		LOGD(TAG,"check now(%ld) lastCheck(%ld)", now, lastCheck);
		if(now > (lastCheck + CHECK_PERIOD)){
			lastCheck = now;
			dispatchCheck();
		}

		cleanDeletedEvents();
	}


	return 0;
}

int EpollReactor::stop(){
	if (writeCmd(1) <= 0){
		LOGE(TAG,"write stop cmd failed %d(%s)",errno,strerror(errno));
		return 1;
	}

	LOGD(TAG,"write stop cmd success");
	return 0;
}

void EpollReactor::cleanDeletedEvents(){
	for(std::list<EventStub*>::iterator it = mDelEventStubs.begin(); it != mDelEventStubs.end(); it++){
		SAFE_DELETE(*it);
	}

	mDelEventStubs.clear();
}

void EpollReactor::dispatchCheck(){
	int l = mFd2Events.size();
	if(l > 0){
		int i = 0;
		EventStub** stubs = new EventStub* [l];
		for(map<int,EventStub*>::iterator itr = mFd2Events.begin();itr!=mFd2Events.end();itr++){
			stubs[i++] = itr->second;
	    }

	    for(i=0;i<l;i++){
	    	EventStub* stub = stubs[i];
	    	IReactHandler* l = stub->listener;
	        if(l != NULL){
	        	l->onPeriodicallyCheck(stub->fd, this, CHECK_PERIOD, stub->tag);
	        }
	    }

	    delete[] stubs;
	}
}

void EpollReactor::dispatchEvent(struct epoll_event events[],int size){

	for(int i=0;i<size;i++){

		EventStub* stub = (EventStub*)events[i].data.ptr;
		if(stub->listener == NULL){
			LOGD(TAG, "listener has dispeared");
			continue;
		}

		LOGD(TAG,"dispatchEvent ev(%p) fd(%d) events(%d)", &events[i], stub->fd, events[i].events);

		if(stub->listener && (events[i].events & (EPOLLIN|EPOLLPRI)) > 0){
			//readable
			stub->listener->onReadable(stub->fd,this,stub->tag);
		}

		if(stub->listener && (events[i].events & EPOLLOUT) > 0){
			//writable
			stub->listener->onWritable(stub->fd,this,stub->tag);
		}

		if(stub->listener && (events[i].events & (EPOLLRDHUP|EPOLLERR|EPOLLHUP)) > 0){
			//exception
			LOGD(TAG,"epoll exception %d %d %d %d",events[i].events, EPOLLRDHUP,EPOLLERR,EPOLLHUP);
			stub->listener->onException(stub->fd,this,events[i].events,stub->tag);
		}

	}

}

int EpollReactor::writeCmd(uint8_t cmd){
	LOGD(TAG,"writing cmd %d", cmd);
	uint8_t arr[1] = {cmd};
	ssize_t w = write(mCmdPipe[1], arr, 1);
	LOGD(TAG,"write cmd = %d, pipe = %d, write = %d",cmd,mCmdPipe[1],w);

	return w;
}

int EpollReactor::convertEvent(int events){
	int ne = 0;
	if(events & IReactor::EVENT::READ){
		ne |= EPOLLIN;
	}

	if(events & IReactor::EVENT::WRITE){
		ne |= EPOLLOUT;
	}

	return ne;
}

void EpollReactor::onReadable(int fd,IReactor* reactor,void* tag){
	LOGD(TAG,"readable pipe: %d",fd);
	uint8_t buffer[1];
    if (read(mCmdPipe[0], buffer, 1) < 0){
    	LOGE(TAG,"read cmd pipe(%d) failed",mCmdPipe[0]);
    	return;
    }

    LOGD(TAG,"read %u",buffer[0]);
    switch(buffer[0]){
    	case 1:{
    		LOGE(TAG, "set working false");
    		mWorking = false;

    		break;
    	}
    }

}

void EpollReactor::onWritable(int fd,IReactor* reactor,void* tag){
	LOGD(TAG,"writable pipe: %d",fd);
}

void EpollReactor::onException(int fd,IReactor* reactor,int exception,void* tag){
	LOGD(TAG,"exception pipe: %d ,exception %d ",fd,exception);
}

void EpollReactor::onPeriodicallyCheck(int fd, IReactor* reactor, uint32_t interval, void* tag){
	LOGD(TAG,"onPeriodicallyCheck pipe: %d, interval %u",fd, interval);
}

