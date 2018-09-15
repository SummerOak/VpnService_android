#include "UDPSession.h"
#include "JniBridge.h"
#include "Utils.h"
#include "Socks5Proxy.h"
#include "PacketCapture.h"
#include <netinet/ip.h>
#include <netinet/udp.h>

UDPSession::UDPSession():Session(){
    snprintf(TAG,sizeof(TAG),"VpnLib.UDPSession_%d",mID);
    LOGD(TAG,"a UDPSession born %p",this);
}

UDPSession::~UDPSession(){
    LOGD(TAG,"a UDPSession dying %p",this);

    SAFE_DELETE(mTunnel);

    mOutBuffer.clear();

    close();
}

int UDPSession::init(IPHeader& ip, IReactor* reactor){

    char connid[64];
    char sip[16];
    snprintf(connid,sizeof(connid),"%d_%d_%s_%s", mID, mUID, mAppName, vpnlib::IPInt2Str(vpnlib::getDest(ip),sip,16)); 
    snprintf(TAG,sizeof(TAG),"%s_%s", PTAG("UDPSession"), connid); 

    mReactor = reactor;

    uint16_t srcPort = SWAP_INT16(getSrcPort(ip));
    uint16_t destPort = SWAP_INT16(getDestPort(ip));

    mCtrlType = JniBridge::queryControlStrategy(mUID, mIPVer, sip, destPort, mProtocol, "");

    int err = 0;
    if((mCtrlType&CTRL_TYPE::PROXY) > 0){
        const char* proxyAddr = Settings::getSSetting(Settings::Key::SK_PROXY_ADDR, "");
        int proxyPort = Settings::getISetting(Settings::Key::SK_PROXY_PORT, 0);
        int proxyIPVer = Settings::getISetting(Settings::Key::SK_PROXY_IPVER, 0);
        LOGR(TAG,"connect to proxy: %s(%u)", proxyAddr, proxyPort);

        Socks5Proxy* s5Proxy = NULL;
        if(proxyIPVer == 4){
            uint32_t nIP = vpnlib::IPStr2Int(proxyAddr);
            s5Proxy = new Socks5Proxy(&nIP, proxyPort, proxyIPVer, reactor, this);
        }else{
            LOGE(TAG, "ip version not support %u", proxyIPVer);
            return 1;
        }
        
        s5Proxy->setTag(connid);
        s5Proxy->setProtocol(mProtocol);
        s5Proxy->setDestAddr(mIPVer, &mDestAddr, mDestPort);
        mTunnel = s5Proxy;
        err = mTunnel->establish();
    }else {
        err = connectRemote();
    }

    if(err){
        LOGE(TAG, "establish to remote failed");
        return err;
    }

    mAlive = true;
    return 0;
}

int UDPSession::connectRemote(){

    LOGD(TAG,"open a udp socket...");
    mSocket = socket(mIPVer==4?PF_INET:PF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    if(mSocket < 0){
        LOGE(TAG,"open socket failed sock=%d %s(%d)",mSocket,errno,strerror(errno));
        return 1;
    }

    if(JniBridge::protectSock(mSocket)){
        LOGE(TAG,"protect socket failed sock=%d",mSocket);
        mSocket = -1;
        return 1;
    }

    if (mIPVer == 4) {
        if (mDestAddr.ip4 == INADDR_BROADCAST) {
            LOGD(TAG, "UDP4 broadcast");
            int on = 1;
            if (setsockopt(mSocket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on))){
                LOGE(TAG, "UDP setsockopt SO_BROADCAST error %d: %s",errno, strerror(errno));
            }
        }
    } else {
        LOGE(TAG, "ip version not support yet %u", mIPVer);
    }

    mReactor->addEvent(mSocket,IReactor::EVENT::ALL, this);

    return 0;
}

int UDPSession::accept(IPHeader& ip){
    if(!mAlive){
        return 1;
    }

    size_t len = getPayloadLen(ip);
    mTotalAccept += len;
    uint8_t* data = ((uint8_t*)&ip)+getPayloadOffset(ip);

    LOGR(TAG,">>> %s", toString(ip,(char*)sTemporary, MAX_SHARE_BUFFER));

    if((mCtrlType&CTRL_TYPE::CAPTURE) != 0){
        PacketCapture::get().write((uint8_t*)(&ip), vpnlib::getTTLEN(ip));
    }

    mTimeCost = time(NULL);

    dispatcher::get().dispatch(new Event(Event::ID::CONN_TRAFFIC_ACCEPT,0, toConnInfo(new ConnInfo, len, &ip)));

    BufferSegment* segment = new BufferSegment(len);
    size_t w = segment->write(data,len);
    if(w == len){
        mOutBuffer.push(segment);
        if(mCtrlType == CTRL_TYPE::PROXY && mTunnel != NULL){
            mTunnel->requestSendData();
        }else{
            mReactor->addEvent(mSocket,IReactor::EVENT::WRITE,this);
        }
    }else{
        SAFE_DELETE(segment);
    }
    
    mLastTime = time(NULL);

    return 0;
}

int UDPSession::close(){
    LOGD(TAG,"closing...");
    mAlive = false;
    SAFE_CLOSE_SOCKET(mSocket, mReactor);
    
    return 0;
}

bool UDPSession::hasClosed(){
    return !mAlive;
}

void UDPSession::onReadable(int fd,IReactor* reactor,void* tag){
	LOGD(TAG,"onReadable fd=%d",fd);

	ssize_t length = read(fd, mBuffer, MAX_IP_SIZE);
	LOGD(TAG,"read %d bytes",length);

    if (length < 0){
        close();
    	return;
    }

    mTotalRecv += length;
    dispatcher::get().dispatch(new Event(Event::ID::CONN_TRAFFIC_RECV, 0, toConnInfo(new ConnInfo, length)));

    BufferSegment* segment = buildPacket(mBuffer, length);
    if(segment != NULL){
        segment->setNeedCapture(((mCtrlType&CTRL_TYPE::CAPTURE) != 0));
        mTotalBack += length;

        ConnInfo* connInfo = toConnInfo(new ConnInfo, length, (IPHeader *) segment->begin());
        
        time_t cost = time(NULL) - mTimeCost;
        LOGD(TAG,"<<< cost(%u) %s", cost, toString(*(IPHeader *) segment->begin(),(char*)sTemporary, MAX_SHARE_BUFFER));

        Event* e = new Event(Event::ID::WRITE_PACKAGE,0,segment);
        dispatcher::get().dispatchSync(e);

        dispatcher::get().dispatch(new Event(Event::ID::CONN_TRAFFIC_BACK, 0, connInfo));
    }
    
    // close();
}

BufferSegment* UDPSession::buildPacket(uint8_t* data, size_t length){
    BufferSegment* segment = NULL;
    struct udphdr* udpHeader = NULL;
    uint16_t checksum = 0;
    if (mIPVer == 4) {

        size_t ttlen = sizeof(IPHeader) + sizeof(struct udphdr) + length;
        segment = new BufferSegment(ttlen);
        segment->limit(ttlen);

        IPHeader* ip4 = (IPHeader *) segment->begin();
        udpHeader = (struct udphdr*) (((uint8_t*)ip4) + sizeof(IPHeader));
        memcpy(((uint8_t*)ip4) + sizeof(IPHeader) + sizeof(struct udphdr), data, length);

        // Build IP4 header
        ip4->VER_IHL = (4<<4)|(sizeof(IPHeader) >> 2);
        ip4->TTLEN = SWAP_INT16(ttlen);
        ip4->TTL = 64;
        ip4->PTOL = PROTO::UDP;
        ip4->SRC = mDestAddr.ip4;
        ip4->DEST = mSrcAddr.ip4;

        // Calculate IP4 checksum
        ip4->CHECKSUM = ~computeChecksum(0, (uint8_t *)ip4, sizeof(IPHeader));

        struct ippseudo pseudo;
        memset(&pseudo, 0, sizeof(struct ippseudo));
        pseudo.ippseudo_src.s_addr = ip4->SRC;
        pseudo.ippseudo_dst.s_addr = ip4->DEST;
        pseudo.ippseudo_p = ip4->PTOL;
        pseudo.ippseudo_len = htons(sizeof(struct udphdr) + length);

        checksum = computeChecksum(0, (uint8_t *) &pseudo, sizeof(struct ippseudo));
    } else {
        LOGE(TAG,"ip version not support %d",mIPVer);
        return NULL;
    }

    udpHeader->source = mDestPort;
    udpHeader->dest = mSrcPort;
    udpHeader->len = htons(sizeof(struct udphdr) + length);
    checksum = computeChecksum(checksum, (uint8_t *)udpHeader, sizeof(struct udphdr));
    checksum = computeChecksum(checksum, data, length);
    udpHeader->check = ~checksum;

    return segment;
}

void UDPSession::onWritable(int fd,IReactor* reactor,void* tag){
	LOGD(TAG,"onWritable pipe: %d",fd);

	if(!mOutBuffer.empty()){

        uint32_t sent = 0;
		while(!mOutBuffer.empty()){
			BufferSegment* segment = mOutBuffer.front();
            const void* data = (const void*)(segment->begin());
            int size = segment->size();
            socklen_t addrLen = (mIPVer==4)?sizeof(struct sockaddr_in):sizeof(struct sockaddr_in6);
            char t[20];
            LOGD(TAG,"sending (%p)%d bytes to %s",data,size,sockAddr2Str(mIPVer,*mDestSockAddr,t,sizeof(t)));

			ssize_t send = sendto(mSocket,data, size , MSG_NOSIGNAL, mDestSockAddr, addrLen);

			if(send < 0) {
				LOGE(TAG,"send to error %d(%s)",errno,strerror(errno));
                break;
			} else {
				LOGD(TAG,"%d bytes have send out",send);
				segment->consume(send);
                sent += send;

				if(segment->size() <= 0){
					mOutBuffer.pop();
                    SAFE_DELETE(segment);
				}
			}
		}

        if(sent > 0){
            mTotalSent += sent;
            dispatcher::get().dispatch(new Event(Event::ID::CONN_TRAFFIC_SENT, 0, toConnInfo(new ConnInfo, sent)));
        }

	}else{
		LOGD(TAG,"out buffer is empty, nothing need to send.");
        mReactor->delEvent(mSocket,IReactor::EVENT::WRITE);
	}
}

void UDPSession::onException(int fd,IReactor* reactor,int exception,void* tag){
	LOGD(TAG,"onException pipe: %d ,exception %d ",fd,exception);
    close();
}

void UDPSession::onPeriodicallyCheck(int fd, IReactor*, uint32_t interval, void* tag){
    LOGD(TAG,"handleReactorCheck");
    if(timeout()){
        LOGD(TAG,"timeout, close");
        close();
    }
}

bool UDPSession::timeout(){
    time_t now = time(NULL);
    int t = 120;
    
    LOGD(TAG,"timeout mLastTime(%ld) now(%ld) t(%d)", mLastTime, now, t);

    return now > (mLastTime + t);
}

int UDPSession::onTunnelConnected(){
    LOGD(TAG,"onTunnelConnected");
    return 1;
}

int UDPSession::onTunnelDisconnected(uint8_t reason){
    LOGR(TAG,"tunnel disconnectd %u", reason);
    close();
    return 1;
}

int UDPSession::onTunnelDataBack(uint8_t* data, uint32_t length){
    LOGD(TAG,"receive %u bytes from tunnel",length);
    if(length > MAX_IP_SIZE){
        LOGD(TAG,"receive %u bytes from tunnel",length);
    }

    if(length <= 0){
        mTunnel->pauseReceiveData();
        close();
    }else{

        uint32_t i = 0;
        while(i < length){
            uint16_t l = (length-i) > MAX_IP_SIZE?MAX_IP_SIZE:(length-i);
            dispatcher::get().dispatch(new Event(Event::ID::CONN_TRAFFIC_RECV, 0, toConnInfo(new ConnInfo, length)));

            BufferSegment* segment = buildPacket(data+i, l);
            if(segment != NULL){
                segment->setReleaseAfterBuffering(true);
                
                mTimeCost = time(NULL) - mTimeCost;
                LOGD(TAG,"<<< proxy cost(%u), %s", mTimeCost, toString(*(IPHeader *) segment->begin(),(char*)sTemporary, MAX_SHARE_BUFFER));
                Event* e = new Event(Event::ID::WRITE_PACKAGE,0,segment);
                dispatcher::get().dispatchSync(e);
            }
            
            i += l;
        }

        if(i > 0){
            mTotalRecv += i;
            mTotalBack += i;
            dispatcher::get().dispatch(new Event(EVENT::CONN_TRAFFIC_RECV, 0,toConnInfo(new ConnInfo, i)));
            dispatcher::get().dispatch(new Event(EVENT::CONN_TRAFFIC_BACK, 0,toConnInfo(new ConnInfo, i)));
        }
    }

    mLastTime = time(NULL);

    return 0;
}

int UDPSession::onTunnelWritable(){
    LOGD(TAG,"onTunnelWritable");
    
    uint32_t sent = 0;
    if(!mOutBuffer.empty()){

        while(!mOutBuffer.empty()){
            BufferSegment* segment = mOutBuffer.front();
            int s = mTunnel->sendData(segment->begin(), segment->size(), false);

            if(s < 0) {
                LOGE(TAG,"send to error %d(%s)",errno,strerror(errno));
                break;
            } else {
                LOGD(TAG,"%d bytes have send out",send);
                segment->consume(s);
                sent += s;

                if(segment->size() <= 0){
                    mOutBuffer.pop();
                    SAFE_DELETE(segment);
                }
            }
        }

        if(sent > 0){
            mTotalSent += sent;
            dispatcher::get().dispatch(new Event(Event::ID::CONN_TRAFFIC_SENT, 0, toConnInfo(new ConnInfo, sent)));
        }

    }else{
        LOGD(TAG,"out buffer is empty, nothing need to send.");
        mTunnel->pauseSendData();
    }

    return sent;
}

ConnInfo* UDPSession::toConnInfo(ConnInfo *info, int size, IPHeader* ip){
    Session::toConnInfo(info, size, ip);
    info->state = hasClosed()?0:1;
    return info;
}

