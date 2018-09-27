#include "Router.h"
#include "Log.h"
#include "Defines.h"
#include "PacketCapture.h"
#include "EpollReactor.h"
#include "EventDispatcher.h"
#include "TrafficMgr.h"
#include "Utils.h"
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <sstream>

const char* Router::TAG = PTAG("Router");

Router::Router(int tunFd){
    LOGD(TAG,"build router on tun(%d)",tunFd);
    mTunFd = tunFd;
	mIPData = (uint8_t*)malloc(MTU);
	mReactor = new EpollReactor;

    dispatcher::get().registerEvent(EVENT::WRITE_PACKAGE, this);

    return;
}

Router::~Router(){
    releaseSessions(mUDPSessions);
    releaseSessions(mTCPSessions);
    releaseData();

   	SAFE_FREE(mIPData);
    SAFE_DELETE(mReactor);

    dispatcher::get().unregisterEvent(EVENT::WRITE_PACKAGE, this);

    return;
}

int Router::init(){
    int flags = fcntl(mTunFd, F_GETFL, 0);
    if (flags < 0 || fcntl(mTunFd, F_SETFL, flags & ~O_NONBLOCK) < 0){
        LOGE(TAG, "fcntl tun ~O_NONBLOCK error %d: %s", errno, strerror(errno));
        return 1;
    }
    return 0;
}

int Router::start(){
    TrafficMgr::init();
    LOGD(TAG, "add tun to reactor %d", mTunFd);
    mReactor->addEvent(mTunFd, IReactor::EVENT::READ, this);
    LOGD(TAG, "add tun to reactor SUCCESS %d", mTunFd);

    PacketCapture::get().start();

    int ret = mReactor->start();
    mReactor->delEvent(mTunFd,IReactor::EVENT::ALL);

    PacketCapture::get().stop();

    return ret;
}

int Router::stop(){
    return mReactor->stop();
}

int Router::onSyncEvent(Event& event,Result* result){
    if(event.id == EVENT::WRITE_PACKAGE){
        BufferSegment* segment = (BufferSegment*)(event.data);

        LOGD(TAG,"queue <<< %d bytes: %s",segment->size(), toString(*((IPHeader*)(segment->begin())),mTemporary,sizeof(mTemporary)));
        if(segment != NULL && segment->size() > 0 && !segment->isBuffering() && !segment->isAbort()){
            mData.push(segment);
            segment->setBuffering(true);
            segment->mark();
            if(result != NULL){
                result->code = 0;
            }

            if(!mData.empty() && !mReactor->containEvent(mTunFd,IReactor::EVENT::WRITE)){
                LOGD(TAG,"add write event");
                mReactor->addEvent(mTunFd,IReactor::EVENT::WRITE,this);
            }else{
                LOGD(TAG,"write event already exist.");
            }
        }else{
            LOGE(TAG,"data len < 0");
        }
    }else{
        LOGE(TAG,"receive sync event i can't handle %d ", event.id);
    }

    return 1;
}

int Router::onEvent(Event& event){
    LOGD(TAG,"onEvent %d",event.id);
    return 0;
}

void Router::onReadable(int fd,IReactor* reactor,void* tag){
    LOGD(TAG,"tun readable");
	ssize_t length = read(fd, mIPData, MTU);
    if (length < 0){
        LOGE(TAG,"read failed from tun %d(%s)",errno,strerror(errno));
    	return;
    }

    IPHeader* h = (IPHeader*)mIPData;
    LOGD(TAG,"tun >>> %d bytes: %s",length, toString(*h,mTemporary,sizeof(mTemporary)));

    if(h->PTOL == PROTO::UDP){
        route(mUDPSessions,*h);
    }else if(h->PTOL == PROTO::TCP){
        route(mTCPSessions,*h);
    }else{
        LOGE(TAG,"proto not support %d",h->PTOL);
    }

    return;
}

void Router::onWritable(int fd,IReactor* reactor,void* tag){
    LOGD(TAG,"tun writable");
    if(mData.empty()){
        LOGD(TAG,"tun is writable, but no data need to write back to tun, close write event");
    }else{
        uint32_t tw = 0;
        do{
            BufferSegment* segment = mData.front();
            if(segment->isAbort()){
                mData.pop();

                segment->setBuffering(false);
                if(segment->releaseAfterBuffering()){
                    SAFE_DELETE(segment);
                }

                continue;
            }

            ssize_t w = write(mTunFd, segment->begin(), segment->size());
            
            LOGD(TAG,"tun <<< %d bytes", w);

            if(w <= 0){
                LOGE(TAG,"write tun failed after write %u bytes, %s(%d)", tw,strerror(errno),errno);
                break;
            }

            tw += w;
            segment->consume(w);

            if(segment->size() <= 0){
                mData.pop();
                segment->resume();

                if(segment->needCapture()){
                    PacketCapture::get().write(segment->begin(), segment->size());
                }
                
                if(segment->releaseAfterBuffering()){
                    SAFE_DELETE(segment);
                }else{
                    segment->setBuffering(false);
                    
                }
            }
        }while(!mData.empty());
    }

    mReactor->delEvent(mTunFd,IReactor::EVENT::WRITE);

    return;
}

void Router::onException(int fd,IReactor* reactor,int exception,void* tag){
	LOGD(TAG,"exception pipe: %d ,exception %d ",fd,exception);

    return;
}

void Router::onPeriodicallyCheck(int fd, IReactor* reactor, uint32_t interval, void* tag){
    LOGD(TAG, "onPeriodicallyCheck %u, fd(%d)", interval, fd);
    cleanDeadSessions();
}

Error Router::route(std::list<Session*>& sessions,IPHeader& ip){

    Session* session = Session::findSession(sessions,ip);
    if(session == NULL){
        LOGD(TAG,"session not exist, create one");
        session = Session::buildSession(ip,mReactor);
        if(session == NULL){
            return Error::FAILED;
        }

        sessions.push_back(session);
    }else{
        LOGD(TAG,"session found %p",session);
    }

    if(session->accept(ip) == 0){
        return Error::SUCCESS;
    }

    LOGE(TAG,"route ip failed.");
    return Error::FAILED;
}

int Router::releaseSessions(std::list<Session*>& sessions){
    if(!sessions.empty()){
        for(std::list<Session*>::iterator itr = sessions.begin();itr != sessions.end();){
            Session* session = *itr;
            Session::releaseSession(session);
            itr = sessions.erase(itr);
        }
    }

    return 0;
}

int Router::cleanDeadSessions(){
    return cleanDeadSessions(mUDPSessions) + cleanDeadSessions(mTCPSessions);
}

int Router::cleanDeadSessions(std::list<Session*>& sessions){
    int r = 0;
    if(!sessions.empty()){
        for(std::list<Session*>::iterator itr = sessions.begin();itr != sessions.end();){
            Session* session = *itr;
            if(session->hasClosed()){
                Session::releaseSession(session);
                itr = sessions.erase(itr);
                ++r;
            }else{
                itr++;
            }
        }
    }

    return r;
}

void Router::releaseData(){
    mData.clear();
}




