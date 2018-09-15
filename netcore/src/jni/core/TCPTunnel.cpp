#include "TCPTunnel.h"
#include "JniBridge.h"
#include "Utils.h"

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

uint8_t TCPTunnel::SHARED_BUFFER[SHARED_BUFFER_SIZE];

TCPTunnel::TCPTunnel(const void* addr, uint16_t port, uint8_t ipver, IReactor* reactor, ITunnelClient* client)
:ITunnel(addr, port, ipver, client){
	mReactor = reactor;
}

TCPTunnel::~TCPTunnel(){
	mReactor = NULL;
	mSocket = -1;
}

void TCPTunnel::setTag(const char* tag){
	snprintf(TAG,sizeof(TAG),"%s_%s", PTAG("TCPTunnel"), tag); 
}

int TCPTunnel::establish(){
	if(mAlive){
		return 1;
	}
	if(mRemoteSockAddr == NULL){
		LOGE(TAG, "remote addr is NULL");
		return 1;
	}

    mSocket = socket(mIPVer==4?PF_INET:PF_INET6, SOCK_STREAM, 0);

    if(mSocket < 0){
        LOGE(TAG,"open socket failed sock=%d %s(%d)",mSocket,errno,strerror(errno));
        return 1;
    }

    if(JniBridge::protectSock(mSocket)){
        LOGE(TAG,"protect socket failed sock=%d",mSocket);
        mSocket = -1;
        return 1;
    }

    int on = 1;
    if (setsockopt(mSocket, SOL_TCP, TCP_NODELAY, &on, sizeof(on)) < 0){
        LOGW(TAG, "setsockopt TCP_NODELAY error %d: %s",errno, strerror(errno));
    }

    int flags = fcntl(mSocket, F_GETFL, 0);
    if (flags < 0 || fcntl(mSocket, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOGE(TAG, "fcntl socket O_NONBLOCK error %d: %s",errno, strerror(errno));
        return 1;
    }
    
    socklen_t addrLen = (mIPVer==4)?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6);
    int err = ::connect(mSocket, mRemoteSockAddr, addrLen);
    if (err < 0 && errno != EINPROGRESS) {
        LOGE(TAG,"connect to remote failed, error: %d: %s", errno, strerror(errno));
        return 1;
    }

    mReactor->addEvent(mSocket,IReactor::EVENT::ALL, this);
    mAlive = true;
    mLastTime = time(NULL) + TIMEOUT;

    LOGD(TAG,"connect to remote success");

    return 0;

}

bool TCPTunnel::isAlive(){
	return mAlive;
}

uint32_t TCPTunnel::getAvailableBufferSize(){
	return vpnlib::getSockBufferSize(mSocket);
}

int TCPTunnel::keepAlive(){
	LOGD(TAG, "keepAlive");
	if(mSocket >= 0){
		int on = 1;
	    int err = setsockopt(mSocket, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
	    if(err){
	        LOGE(TAG, "setsockopt SO_KEEPALIVE error %d: %s", errno, strerror(errno));
	    }
	    return err;
	}
	
	return 1;
}

int TCPTunnel::requestSendData(){
	LOGD(TAG, "requestSendData socket(%d) mReactor(%p)", mSocket, mReactor);
	if(mSocket >= 0 && mReactor != NULL){
        mReactor->addEvent(mSocket,IReactor::EVENT::WRITE,this);
    }
    return 0;
}

int TCPTunnel::sendData(uint8_t* data, uint32_t len, bool more){
	LOGD(TAG, "sendData: %u bytes", len);
	int s = 0;
	if(mSocket >= 0){
		s = send(mSocket, data, len, (MSG_NOSIGNAL|(more?MSG_MORE:0)));
		if(s < 0){
			LOGE(TAG,"send data error: %d(%s)",errno,strerror(errno));

			destroyTunnel();
			mClient->onTunnelDisconnected(errno);
		}else{
			mLastTime = time(NULL) + TIMEOUT;
		}
	}
	
	return s;
}

int TCPTunnel::pauseSendData(){
	LOGD(TAG, "pauseSendData");
	if(mSocket >= 0 && mReactor != NULL){
		mReactor->delEvent(mSocket,IReactor::EVENT::WRITE);
	}
	return 0;
}

int TCPTunnel::pauseReceiveData(){
	LOGD(TAG, "pauseReceiveData");
	if(mSocket >= 0 && mReactor != NULL){
		mReactor->delEvent(mSocket,IReactor::EVENT::READ);
	}
	return 0;
}

int TCPTunnel::resumeReceiveData(){
	LOGD(TAG, "resumeReceiveData");
	if(mSocket >= 0 && mReactor != NULL){
		mReactor->addEvent(mSocket,IReactor::EVENT::READ, this);
	}
	return 0;
}

void TCPTunnel::destroyTunnel(){
	mAlive = false;
	mConnected = false;
	SAFE_CLOSE_SOCKET(mSocket, mReactor);
}

void TCPTunnel::onReadable(int fd,IReactor*,void* tag){
	LOGD(TAG, "onReadable");
	ssize_t length = read(mSocket, SHARED_BUFFER, SHARED_BUFFER_SIZE);

    if (length < 0){
        LOGE(TAG,"read error, %s(%d)",strerror(errno),errno);
        if (errno != EINTR && errno != EAGAIN){
        	destroyTunnel();
            mClient->onTunnelDisconnected(errno);
        }
    }else if(length == 0){
    	destroyTunnel();
        mClient->onTunnelDisconnected(0);
    }else {
    	if(!mConnected){
			mConnected = true;
			mClient->onTunnelConnected();
		}
	
    	mClient->onTunnelDataBack(SHARED_BUFFER, length);
    	mLastTime = time(NULL) + TIMEOUT;
    }
}

void TCPTunnel::onWritable(int fd,IReactor*,void* tag){
	LOGD(TAG, "onWritable");
	if(!mConnected){
		mConnected = true;
		mClient->onTunnelConnected();
	}
	
	mClient->onTunnelWritable();
}

void TCPTunnel::onException(int fd,IReactor*,int error,void* tag){
	LOGE(TAG, "onException fd=%d, error=%d", fd, error);
	mClient->onTunnelDisconnected(error);
	destroyTunnel();
}

void TCPTunnel::onPeriodicallyCheck(int fd, IReactor*, uint32_t interval, void* tag){
	LOGD(TAG, "onPeriodicallyCheck %u", interval);

	time_t now = time(NULL);
	if(now > mLastTime){
		destroyTunnel();
		mClient->onTunnelDisconnected(0);
	}

}


