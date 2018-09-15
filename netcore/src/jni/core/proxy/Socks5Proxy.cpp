#include "Socks5Proxy.h"
#include "Defines.h"
#include "JniBridge.h"
#include "Utils.h"
#include <unistd.h>
#include <netinet/tcp.h>

uint8_t Socks5Proxy::sTemporary[SOCKS5_SHARE_BUFFER_SZ];

Socks5Proxy::Socks5Proxy(const void* addr, uint16_t port, uint8_t ipver, IReactor* reactor, ITunnelClient* client)
:ITunnel(addr, port, ipver, client){
	memcpy(TAG, PTAG("Socks5Proxy"), strlen(PTAG("Socks5Proxy")));
	mReactor = reactor;
}

Socks5Proxy::~Socks5Proxy(){
	
	mSocks5.clear();
	mOutQueue.clear();
}

void Socks5Proxy::setDestAddr(uint8_t ipver, void* addr, uint16_t port){
	if(ipver == 4){
		mDestAddr.ip4 = *(uint32_t*)addr;
		mDestPort = port;
	}else{
		memcpy(mDestAddr.ip6, addr, sizeof(mDestAddr.ip6));
		mDestPort = port;
	}

	mDestIPVer = ipver;
}

void Socks5Proxy::setProtocol(int protocol){
	mProtocol = protocol;
}

void Socks5Proxy::setTag(const char* tag){
	snprintf(TAG,sizeof(TAG),"%s_%s", PTAG("Socks5Proxy"), tag); 
}

int Socks5Proxy::establish(){
	LOGD(TAG,"connectProxy ...");

	if(mRemoteSockAddr == NULL){
		LOGE(TAG,"proxy not setted.");
		return 1;
	}

    mSocket = socket(mIPVer==4?PF_INET:PF_INET6, SOCK_STREAM, 0);

    if(mSocket < 0){
        LOGE(TAG,"open proxy socket failed sock=%d %s(%d)",mSocket,errno,strerror(errno));
        return 1;
    }

    if(JniBridge::protectSock(mSocket)){
        LOGE(TAG,"protect socket(%d) failed.",mSocket);
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

   	socklen_t addrLen = mIPVer==4?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6);
    int err = ::connect(mSocket, mRemoteSockAddr, addrLen);
    if (err < 0 && errno != EINPROGRESS) {
        LOGE(TAG, "connect to remote error %d: %s", errno, strerror(errno));
        return 1;
    }

    if(mReactor == NULL || mReactor->addEvent(mSocket,IReactor::EVENT::ALL,this)){
		LOGE(TAG,"add event failed, reactor %p", mReactor);
		return 1;
    }

    mAlive = true;

    return 0;
}

bool Socks5Proxy::isAlive(){
	return mAlive;
}

int Socks5Proxy::keepAlive(){
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

void Socks5Proxy::destroyTunnel(){
	mConnected = false;
	mAlive = false;
	if(mDecryptBuffer.size() > 0){
		LOGD(TAG, "tunnl destroyed remain %u bytes to relayback", mDecryptBuffer.size());
		logbytes2string(TAG, "data stuck in tunnel", mDecryptBuffer.begin(), mDecryptBuffer.size());
	}

	if(mOutQueue.size() > 0){
		LOGD(TAG, "tunnel destroyed remain %u segment %u bytes to send out", mOutQueue.size(), mBufferedSize);
	}

	SAFE_CLOSE_SOCKET(mSocket, mReactor);
}

uint32_t Socks5Proxy::getAvailableBufferSize(){
	if(!mAlive){
		return 0;
	}

	LOGD(TAG, "getAvailableBufferSize: max send buffer size %u, buffered %u", MAX_SOCKS5_PROXY_SEND_BUFFER, mBufferedSize);
	if(mBufferedSize > MAX_SOCKS5_PROXY_SEND_BUFFER){
		return 0;
	}

	return MAX_SOCKS5_PROXY_SEND_BUFFER - mBufferedSize;
}

int Socks5Proxy::requestSendData(){
	if(mAlive && mS5State == Socks5::S_TRANS){
		LOGD(TAG, "enable write event");
		mReactor->addEvent(mSocket,IReactor::EVENT::WRITE,this);
		return 1;
	}

	LOGD(TAG, "requestSendData failed, mS5State = %s", stateDesc(mS5State));
	return 0;
}

int Socks5Proxy::pauseSendData(){
	if(mAlive && mS5State == Socks5::S_TRANS){
		LOGD(TAG, "disable write event");
		mReactor->delEvent(mSocket,IReactor::EVENT::WRITE);
		return 1;
	}
	LOGD(TAG, "pauseSendData failed, mS5State = %s", stateDesc(mS5State));
	return 0;
}

int Socks5Proxy::pauseReceiveData(){
	if(mAlive && mS5State == Socks5::S_TRANS){
		LOGD(TAG, "disable read event");
		mReactor->delEvent(mSocket,IReactor::EVENT::READ);
		return 1;
	}
	LOGD(TAG, "pauseReceiveData failed, mS5State = %s", stateDesc(mS5State));
	return 0;
}

int Socks5Proxy::resumeReceiveData(){
	if(mAlive && mS5State == Socks5::S_TRANS){
		LOGD(TAG, "enable read event");
		mReactor->addEvent(mSocket,IReactor::EVENT::READ, this);
		return 1;
	}
	LOGD(TAG, "resumeReceiveData failed, mS5State = %s", stateDesc(mS5State));
	return 0;
}

int Socks5Proxy::sendData(uint8_t* data, uint32_t len, bool more){
	if(!mAlive){
		return 0;
	}

	if(getAvailableBufferSize() < len){
		LOGE(TAG, "buffer is full, try later, buffered size %u", mBufferedSize);
		return 0;
	}

	return encryptAndQueue(mOutQueue, data, len);
}

int Socks5Proxy::encryptAndQueue(BufferQueue<Segment>& queue, uint8_t* data, uint32_t len){
	uint8_t* t = data;
	int l = Cipher::get().encrypt(&t, len, sTemporary, SOCKS5_SHARE_BUFFER_SZ);

	LOGD(TAG, "encrypt %d bytes into %d bytes", (t-data), l);
	if(l > 0){
		Segment* segment = new Segment(l);
		segment->write(sTemporary, l);
		queue.push(segment);

		mReactor->addEvent(mSocket,IReactor::EVENT::WRITE,this);

	    LOGD(TAG,"enqueued %u bytes", l);

	    mBufferedSize += l;;

	    return t - data;
	}

	return l;
}

void Socks5Proxy::onReadable(int fd,IReactor* reactor,void* tag){
	LOGD(TAG,"onReadable %d", fd);
	ssize_t length = read(fd, sTemporary, SOCKS5_SHARE_BUFFER_SZ);
	if(length < 0){
		LOGD(TAG,"read socks5 response failed %s/%d while mS5State = %s, buffer size = %d, total data back %u", strerror(errno), errno, stateDesc(mS5State), mDecryptBuffer.size(), mPayloadDataBack);
		if (errno != EINTR && errno != EAGAIN){
			switchS5State(Socks5::S_FAILED);
		}
		
		return;
	}else if(length == 0){
		LOGD(TAG,"read socks5 response 0 %s/%d while mS5State = %s, buffer size = %d, total data back %u", strerror(errno), errno, stateDesc(mS5State), mDecryptBuffer.size(), mPayloadDataBack);
		switchS5State(Socks5::S_END);
		return;
	}

	if(!mConnected){
		mConnected = true;
		mClient->onTunnelConnected();
	}

	vpnlib::logbytes2string(TAG, "read from proxy", sTemporary, length);

	if(mDecryptBuffer.remain() < length){
		mDecryptBuffer.flip();
	}

	if(mDecryptBuffer.remain() < length){
		LOGE(TAG, "decrypt buffer overflow, buffer remain %u/%u, received %d", mDecryptBuffer.remain(), length, mDecryptBuffer.capacity());
		switchS5State(Socks5::S_FAILED);
		return;
	}

	mDecryptBuffer.write(sTemporary, length);
	while(true){
		uint8_t* s = mDecryptBuffer.begin();
		uint8_t* t = s;
		uint32_t size = mDecryptBuffer.size()>SOCKS5_SHARE_BUFFER_SZ?SOCKS5_SHARE_BUFFER_SZ:mDecryptBuffer.size();
		length = Cipher::get().decrypt(&s, size, sTemporary, SOCKS5_SHARE_BUFFER_SZ);
		if(length <= 0){
			LOGD(TAG, "decrypt received data failed, %d", length);
			return;
		}

		vpnlib::logbytes2string(TAG, "after decrypt", sTemporary, length);

		LOGD(TAG, "consume: %d", (s-t));
		mDecryptBuffer.consume(s-t);
		vpnlib::logbytes2string(TAG, "after consume", mDecryptBuffer.begin(), mDecryptBuffer.size());

		switch(mS5State){
			case Socks5::S_INIT:{
				LOGE(TAG,"receive from proxy while S_INIT");
				break;
			}
			case Socks5::S_GREET:{
				handleS5GreetBack(sTemporary, length);
				break;
			}
			case Socks5::S_VERIFY:{
				handleS5VerifyBack(sTemporary, length);
				break;
			}
			case Socks5::S_CONNECT:{
				handleS5ConnBack(sTemporary, length);
				break;
			}
			case Socks5::S_TRANS:{
				mPayloadDataBack += length;
				mClient->onTunnelDataBack(sTemporary, length);
				break;
			}
			default:{
				LOGE(TAG,"wrong socks5 state %s", stateDesc(mS5State));
				switchS5State(Socks5::S_FAILED);
				break;
			}
		}
	}
}

void Socks5Proxy::onWritable(int fd,IReactor* reactor,void* tag){
	LOGD(TAG,"proxy onWritable mS5State = %s", stateDesc(mS5State));

	if(!mConnected){
		mConnected = true;
		mClient->onTunnelConnected();
	}

	if(mS5State == Socks5::S_TRANS){
		if(!mOutQueue.empty()){
			sendQueue(mOutQueue);
		}else{
			int s = mClient->onTunnelWritable();

			if(s <= 0){
				LOGD(TAG,"no data need be sent from client, close write event.");
				mReactor->delEvent(mSocket,IReactor::EVENT::WRITE);
			}
		}
	}else if(mS5State == Socks5::S_INIT){
		sendS5Greet();
		switchS5State(Socks5::S_GREET);
	}else if(mSocks5.empty()){
		LOGD(TAG,"nothing need be send to proxy.");
		mReactor->delEvent(mSocket,IReactor::EVENT::WRITE);
	}else{
		sendQueue(mSocks5);
	}

	return;
}

void Socks5Proxy::onException(int fd,IReactor* reactor,int error,void* tag){
	LOGE(TAG,"tcp connecton exception on pipe: %d ,exception code(%d) , %s(%d)",fd,error, strerror(errno), errno);
   	
    switchS5State(Socks5::S_END);

    return;
}

void Socks5Proxy::onPeriodicallyCheck(int fd,IReactor* reactor, uint32_t interval, void* tag){
	LOGD(TAG, "onPeriodicallyCheck interval %u, fd(%d)", interval, fd)
	return;
}

void Socks5Proxy::switchS5State(uint8_t state){
	if(mS5State != state){
		LOGD(TAG,"switchS5State from %s to %s", stateDesc(mS5State), stateDesc(state));
		mS5State = state;

		switch(mS5State){
			case Socks5::S_TRANS:{
				mClient->onTunnelWritable();
				break;
			}

			case Socks5::S_FAILED:
			case Socks5::S_END:{
				LOGD(TAG, "proxy end, tunnel disconnected, %s", stateDesc(mS5State));
				mClient->onTunnelDisconnected(0);
				destroyTunnel();
				break;
			}
		}
	}
}

int Socks5Proxy::sendQueue(BufferQueue<Segment>& queue){
	uint32_t sent = 0;
    if(!queue.empty()){
        while(!queue.empty()){
            Segment* segment = queue.front();
            const void* data = (const void*)(segment->begin());
            int size = segment->size();
            ssize_t s = send(mSocket, data, size, MSG_NOSIGNAL);

            if(s < 0) {
                LOGE(TAG,"send to error %d(%s)",errno,strerror(errno));
                break;
            } else {
                LOGD(TAG,"%d bytes have send out",s);

                sent += s;
                segment->consume(s);

                if(segment->size() <= 0){
                    queue.pop();
                    SAFE_DELETE(segment);
                }
            }
        }

        if(sent > 0){
            mBufferedSize -= sent;
        }else{
            LOGD(TAG,"sock %d can't write anymore, close write event temporarily",mSocket);
           	mReactor->delEvent(mSocket,IReactor::EVENT::WRITE);
        }


    }else{
        LOGD(TAG,"out buffer is empty, nothing need to send.");
        mReactor->delEvent(mSocket,IReactor::EVENT::WRITE);
    }
    return sent;
}

bool Socks5Proxy::containS5Verify(uint32_t vfms, uint8_t m){
	uint8_t i = 0;
	while(i < 4){
		if((vfms&0xFF) == m){
			return true;
		}

		vfms >>= 8; ++i;
	}

	return false;
}

uint8_t Socks5Proxy::getS5VerifyMethods(uint8_t* out, int len){
	uint32_t vfms = Settings::getISetting(Settings::Key::SK_SOCKS5_VERIFY_METHOD, 0);
	LOGD(TAG,"socks5 verify methods: %x", vfms);
	uint8_t ms = 0;
	uint8_t i = 0;
	while(i < 4 && ms < len){
		uint8_t m = (vfms&0xFF);
		LOGD(TAG,"m=%u", m);
		if(m != 0xFF){
			out[ms++] = m;
		}

		vfms >>= 8; ++i;
	}

	return ms;
}

int Socks5Proxy::sendS5Greet(){
	LOGD(TAG,"sendS5Greet...");
	Segment* segment = new Segment(64);
	uint8_t greet[6] = {5, 0};
	uint8_t verifyMethods = getS5VerifyMethods(greet+2, 4);
	greet[1] = verifyMethods;
	
	LOGD(TAG, "verify methods = %u", verifyMethods);

	segment->write(greet, verifyMethods+2);
	int r = encryptAndQueue(mSocks5, segment->begin(), segment->size());
	SAFE_DELETE(segment);

	if(r <= 0){
		LOGE(TAG,"sendS5Greet failed %d", r);
	}

    return r;
}

int Socks5Proxy::sendS5Connect(){
	LOGD(TAG,"sendS5Connect...");
	Segment* segment = new Segment(32);
	uint8_t data = 0x05;
	segment->write(&data, 1);
	data = mProtocol == PROTO::TCP? 0x01:0x03;
	segment->write(&data, 1);
	data = 0x00;
	segment->write(&data, 1);
	if(mDestIPVer == 4){
		data = 0x01;
		segment->write(&data, 1);
		segment->write((uint8_t*)(&mDestAddr.ip4), 4);
		LOGD(TAG, "ipv4 = %d", mDestAddr.ip4);
	}else{
		data = 0x04;
		segment->write(&data, 1);
		segment->write(mDestAddr.ip6, 16);
	}

	LOGD(TAG, "port = %u", mDestPort);
	segment->write(((uint8_t*)&mDestPort), 2);
	
	int r = encryptAndQueue(mSocks5, segment->begin(), segment->size());
	SAFE_DELETE(segment);

	if(r <= 0){
		LOGE(TAG,"sendS5Connect failed %d", r);
	}

    return r;
}

int Socks5Proxy::sendS5Account(){
	const char* user = Settings::getSSetting(Settings::Key::SK_SOCKS5_USERNAME, NULL);
	const char* password = Settings::getSSetting(Settings::Key::SK_SOCKS5_PASSWORD, NULL);

	LOGD(TAG,"sendS5Account: %s %s", user, password);

	if(user == NULL || password == NULL){
		return 1;
	}

	uint8_t userLen = strlen(user);
	uint8_t passwordLen = strlen(password);
	Segment* segment = new Segment(userLen + passwordLen + 10);
	uint8_t ver = 0x05;
	segment->write(&ver, 1);
	segment->write(&userLen, 1);
	segment->write((uint8_t*)user, userLen);
	segment->write(&passwordLen, 1);
	segment->write((uint8_t*)password, passwordLen);

	int r = encryptAndQueue(mSocks5, segment->begin(), segment->size());
	SAFE_DELETE(segment);

	if(r <= 0){
		LOGE(TAG,"sendS5Account failed %d", r);
	}

    return r;
}

int Socks5Proxy::handleS5GreetBack(uint8_t* data, int len){
	LOGD(TAG,"handleS5GreetBack...");

	if(len != 2){
		LOGE(TAG,"wrong SOCKS5 greet back size %u", len);
		switchS5State(Socks5::S_FAILED);
		return 1;
	}

	if(data[0] == 0x05){
		int vfms = Settings::getISetting(Settings::Key::SK_SOCKS5_VERIFY_METHOD, 0);
		if(containS5Verify(vfms, data[1])){
			if(data[1] == Socks5::VERIFY_NONE){
				sendS5Connect();
				switchS5State(Socks5::S_CONNECT);
			}else if(data[1] == Socks5::VERIFY_ACCOUNT){
				if(sendS5Account()){
					switchS5State(Socks5::S_VERIFY);
				}else{
					switchS5State(Socks5::S_FAILED);
					return 1;
				}
			}

			return 0;
		}
	}else{
		LOGE(TAG,"wrong socks version: %u", data[0]);
	}

	switchS5State(Socks5::S_FAILED);

	return 1;
}

int Socks5Proxy::handleS5VerifyBack(uint8_t* data, int len){
	LOGD(TAG,"handleS5VerifyBack...");

	if(len != 2){
		LOGE(TAG,"wrong SOCKS5 greet back size %u", len);
		switchS5State(Socks5::S_FAILED);
		return 1;
	}

	if(data[0] != 0x05){
		LOGE(TAG,"wrong socks version: %u", data[0]);
		switchS5State(Socks5::S_FAILED);
		return 1;
	}

	if(data[1] != 0x00){
		LOGE(TAG,"verify socks failed: %u", data[1]);
		switchS5State(Socks5::S_FAILED);
		return 1;
	}

	sendS5Connect();
	switchS5State(Socks5::S_CONNECT);

	return 0;
}

int Socks5Proxy::handleS5ConnBack(uint8_t* data, int len){
	LOGD(TAG,"handleS5ConnBack...");

	vpnlib::logbytes2string(TAG, "handleS5ConnBack", data, len);

	if(len < 4){
		LOGE(TAG,"response of conn is too short %u", len);
		switchS5State(Socks5::S_FAILED);
		return 1;
	}

	if(data[0] != 0x05){
		LOGE(TAG,"wrong socks version: %u", data[0]);
		switchS5State(Socks5::S_FAILED);
		return 1;
	}

	if(data[1] != 0x00){
		LOGE(TAG,"socks5 conn request failed: %u", data[1]);
		switchS5State(Socks5::S_FAILED);
		return 1;
	}

	uint8_t addrType = data[3];
	switch(addrType){
		case 1:{
			uint32_t ip4 = (data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7];
			LOGD(TAG,"socks response with conn addr: %s", IPInt2Str(ip4, (char*)sTemporary, MAX_SHARE_BUFFER));
			break;
		}

		case 3:{
			uint8_t domainLen = data[4];
			memcpy(sTemporary, data+5, domainLen);
			sTemporary[domainLen+1] = 0;
			LOGD(TAG,"socks response with conn addr: %s", (char*)sTemporary);

			break;
		}

		case 4:{
			LOGD(TAG,"socks response with conn addr of ipv6");
			break;
		}

		default:{
			LOGE(TAG,"socks response with invalidate conn addr. %u", addrType);
			break;
		}
	}

	switchS5State(Socks5::S_TRANS);

	return 0;
}

const char* Socks5Proxy::stateDesc(uint8_t state){
	switch(state){
		case Socks5::S_INIT: return "init";
		case Socks5::S_VERIFY: return "verify";
		case Socks5::S_CONNECT: return "connect";
		case Socks5::S_TRANS: return "transmit";
		case Socks5::S_END: return "end";
		case Socks5::S_FAILED: return "failed";
		case Socks5::S_GREET: return "greet";
	}

	return "";
}
