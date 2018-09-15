#include "TCPSession.h"
#include "JniBridge.h"
#include "Utils.h"
#include "Socks5Proxy.h"
#include "PacketCapture.h"
#include "TCPTunnel.h"
#include <netinet/ip.h>

#define TCP_RELATIVE_SEQ(seq, base) (seq>base?(seq-base):((((uint32_t)0xFFFFFFFF) - base)+seq)) 
#define TCP_SESSION_BUFFER_SIZE (1<<18)
#define TCP_MAX_SEND_WINDOW (1<<16)
#define TCP_MAX_SEND_PACKET (1)
#define TCP_IP_PAYLOAD_SIZE (61440)

int TCPSession::init(IPHeader& ip, IReactor* reactor){
    char connid[64];
    char sip[16];

    snprintf(connid,sizeof(connid),"%d_%d_%s_%s", mID, mUID, mAppName, vpnlib::IPInt2Str(vpnlib::getDest(ip),sip,16)); 
    snprintf(TAG,sizeof(TAG),"%s_%s", PTAG("TCPSession"), connid); 
    
    mCtrlType = JniBridge::queryControlStrategy(mUID, mIPVer, sip, mDestPort, mProtocol, "");

    TCPHeader* tcp = (TCPHeader*)(((uint8_t*)&ip) + getHeaderLen(ip));
    if(!tcp->syn){
        LOGD(TAG,"ip: %s", toString(ip,(char*)sTemporary, MAX_SHARE_BUFFER));
        LOGD(TAG,"tcp connect without handshake, ignore.");
        return 1;
    }

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
    }else{
        TCPTunnel* tcpTunnel = new TCPTunnel(&mDestAddr, SWAP_INT16(mDestPort), mIPVer, reactor, this);
        tcpTunnel->setTag(connid);
        mTunnel = tcpTunnel;
    }

    mTimeStartConnectProxy = time(NULL);
    int err = mTunnel->establish();
    if(err){
        LOGE(TAG, "connect remote failed %d", err);
        return 1;
    }

    mAlive = true;
    mState = TCP_STATE::LISTEN;

    return 0;
}

int TCPSession::close(){
    mAlive = false;
    if(mTunnel != NULL){
        mTunnel->destroyTunnel();
    }

    return 0;
}

bool TCPSession::hasClosed(){
    return !mAlive || timeout();
}

TCPSession::~TCPSession(){
    LOGD(TAG,"a TCPSession dying %p",this);

    if(mTunnel != NULL){
        mTunnel->destroyTunnel();
        SAFE_DELETE(mTunnel);
    }
    
    mForwardQueue.clear();

    clearBackQueue();
}

int TCPSession::accept(IPHeader& ip){

    size_t len = getPayloadLen(ip);
    mTotalAccept += len;
    TCPHeader* tcp = (TCPHeader*)(((uint8_t*)&ip) + getHeaderLen(ip));
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack_seq);
    seq = TCP_RELATIVE_SEQ(seq, mRemoteSeqBase);
    ack = TCP_RELATIVE_SEQ(ack, mLocalSeqBase);

    LOGR(TAG,">>> %u/%u, seq(%u), ack(%u), s%ua%uf%ur%u",len, mTotalAccept, seq, ack, tcp->syn, tcp->ack, tcp->fin, tcp->rst);
    LOGD(TAG,"ip: %s", toString(ip,(char*)sTemporary, MAX_SHARE_BUFFER));

    if((mCtrlType&CTRL_TYPE::CAPTURE) != 0){
        PacketCapture::get().write((uint8_t*)(&ip), vpnlib::getTTLEN(ip));
    }

    ConnInfo* connInfo = toConnInfo(new ConnInfo, len, &ip);
    connInfo->seq = seq;
    connInfo->ack = ack;
    dispatcher::get().dispatch(new Event(Event::ID::CONN_TRAFFIC_ACCEPT, 0, connInfo));

    if(tcp->syn){
        onReceiveSYN(ip);
    }else if(tcp->fin){
        onReceiveFIN(ip);
    }else if(tcp->rst){
        onReceiveRST(ip);
    }else{

        mSendWindow = ((uint32_t) ntohs(tcp->window)) << mWindowScale;
        if(mLocalNext <= ack){
            mLocalNext = ack;

            bool flushed = false;
            if(mState == TCP_STATE::SYN_RCVED){
                switchState(TCP_STATE::ESTABLISHED);
                mRemoteNext += enqueueIfNeed(ip);
            }else if(mState == TCP_STATE::FIN_WAIT1){
                switchState(TCP_STATE::FIN_WAIT2);
            }else if(mState == TCP_STATE::LAST_ACK){
                switchState(TCP_STATE::CLOSED);
            }else if(mState == TCP_STATE::CLOSING){
                switchState(TCP_STATE::TIME_WAIT);
            }else if(mState == TCP_STATE::ESTABLISHED){     
                if(mSack){
                    uint8_t* sack = getOptionAddr(*tcp, TCP_OPT::SACK);
                    if(sack != NULL){
                        onSack(sack);
                    }
                }

                int l = enqueueIfNeed(ip);
                if(l > 0){
                    mRemoteNext += l;
                    pushInBackQueue(ACK);
                    flushed = true;
                }
            }
            
            if(!flushed){
                flushBackQueue();
            }
            
        }else{
            if(ack+1 == mLocalNext){
                mTunnel->keepAlive();
            }else{
                LOGE(TAG, "repeated ack: %u , expected %u", ack, mLocalNext);
            }
        }
    }

    mLastTime = time(NULL);

    return 0;
}

int TCPSession::onReceiveSYN(IPHeader& ip){
    LOGD(TAG,"receive syn, current state is %s",tcpstate2cstr(mState));
    if(mState != TCP_STATE::LISTEN){
        LOGE(TAG,"repeated syn, current state(%s)",tcpstate2cstr(mState));
        flushBackQueue();
        return 1;
    }

    TCPHeader* tcp = (TCPHeader*)(((uint8_t*)&ip) + getHeaderLen(ip));
    uint8_t* sackf = getOptionAddr(*tcp, TCP_OPT::SACKF);
    if(sackf != NULL){
        mSack = true;
        LOGD(TAG, "enable sack: %u", mSack);
    }

    uint8_t* pmss = getOptionAddr(*tcp,TCP_OPT::MSS);
    if(pmss != NULL){
        mMSS = *((uint16_t*)(pmss+2));
    }

    uint8_t* pws = getOptionAddr(*tcp,TCP_OPT::WS);
    if(pws != NULL){
        mWindowScale = *(pws+2);
    }
    
    mSendWindow = ((uint32_t) ntohs(tcp->window)) << mWindowScale;
    mRemoteSeqBase = ntohl(tcp->seq);
    mLocalSeqBase = (uint32_t) rand();
    mLocalSeq = mLocalNext = 0;
    mRemoteNext = 1;

    if(pushInBackQueue(SYN|ACK) < 0){// step 2 of 3 ways handshake (syn/ack)
        LOGE(TAG,"handshake syn/ack failed.");
        return 1;
    }

    switchState(TCP_STATE::SYN_RCVED);

    return 0;
}

int TCPSession::onReceiveRST(IPHeader& ip){
    TCPHeader* tcp = (TCPHeader*)(((uint8_t*)&ip) + getHeaderLen(ip));
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack_seq);
    seq = TCP_RELATIVE_SEQ(seq, mRemoteSeqBase);
    ack = TCP_RELATIVE_SEQ(ack, mLocalSeqBase);
    LOGD(TAG,"receive rst, seq(%u), expected seq(%u), ack(%u), local next(%u), while %s", seq, mRemoteNext, ack, mLocalNext, tcpstate2cstr(mState));
    switchState(TCP_STATE::CLOSED);
    return 0;
}

int TCPSession::onReceiveFIN(IPHeader& ip){
    TCPHeader* tcp = (TCPHeader*)(((uint8_t*)&ip) + getHeaderLen(ip));
    uint32_t seq = ntohl(tcp->seq);
    seq = TCP_RELATIVE_SEQ(seq, mRemoteSeqBase);
    uint32_t ack = ntohl(tcp->ack_seq);
    ack = TCP_RELATIVE_SEQ(ack, mLocalSeqBase);

    LOGD(TAG,"receive fin, seq(%u), expected seq(%u), ack(%u), local next(%u) , while %s", seq, mRemoteNext, ack, mLocalNext, tcpstate2cstr(mState));

    if(mLocalNext <= ack){
        mLocalNext = ack;
    }

    mRemoteNext = seq + 1;

    if(mState == TCP_STATE::ESTABLISHED){
        if(pushInBackQueue(ACK) < 0){
            LOGE(TAG,"relay back fin/ack failed.");
        }else{
            switchState(TCP_STATE::CLOSE_WAIT);
        }
    }else if(mState == TCP_STATE::FIN_WAIT1){
        if(pushInBackQueue(ACK) < 0){
            LOGE(TAG,"pushInBackQueue ACK failed while FIN_WAIT1");
        }else{
            if(ack == mLocalNext){// 3 ways of fallware
                switchState(TCP_STATE::TIME_WAIT);
            }else{
                switchState(TCP_STATE::CLOSING);
            }
        }
        
    }else if(mState == TCP_STATE::FIN_WAIT2){
        if(pushInBackQueue(ACK) < 0){
            LOGE(TAG,"pushInBackQueue fin/ack failed while FIN_WAIT2");
        }else{
            switchState(TCP_STATE::TIME_WAIT);
        }
    }else{
        LOGE(TAG,"unexpected fin, seq(%u), expected seq(%u), ack(%u), local next(%u) while state(%s)", seq, mRemoteNext, ack, mLocalNext, tcpstate2cstr(mState));
        pushInBackQueue(ACK);
    }

    return 0;
}

void TCPSession::onSack(uint8_t* sack){
    uint8_t n = *sack++;
    LOGD(TAG, "onSack>>> block size: %u", n);
    if(n != 10 && n != 18 && n != 26 && n != 34){
        return;
    }

    n -= 2;
    for(int i = 0; (i + 8) <= n; i += 8){
        uint32_t s = ntohl(*(uint32_t*)(sack+i));
        uint32_t t = ntohl(*(uint32_t*)(sack+i+4));
        s = TCP_RELATIVE_SEQ(s, mLocalSeqBase);
        t = TCP_RELATIVE_SEQ(t, mLocalSeqBase);

        LOGD(TAG, "sack block: %u-%u", s, t);
        for(BufferQueue<TCPDatagram>::Iterator it = mBackQueue.begin(); it != mBackQueue.end(); it++){
            TCPDatagram* segment = *it;
            if(segment->mSeq >= t){
                break;
            }

            if(s <= segment->mSeq){
                segment->setAcked(true);
            }
        }
    }

}

int TCPSession::enqueueIfNeed(IPHeader& ip){
    size_t len = getPayloadLen(ip);
    if(len == 0){
        LOGD(TAG,"payload size is 0, ignore");
        return 0;
    }

    TCPHeader* tcp = (TCPHeader*)(((uint8_t*)&ip) + getHeaderLen(ip));
    uint32_t seq = ntohl(tcp->seq);
    seq = TCP_RELATIVE_SEQ(seq, mRemoteSeqBase);
    if(vpnlib::compare_u32(seq, mRemoteNext) < 0){
        LOGE(TAG,"repeated packet that had send out. seq(%u), acked(%u)", seq, mRemoteNext);
        pushInBackQueue(ACK);
        return 0;
    }

    TCPDatagram* datagram = new TCPDatagram(len);
    datagram->mSeq = seq;
    datagram->mFlag = *((uint8_t*)tcp+13);
    datagram->mChecksum = tcp->checksum;
    datagram->mLength = len;

    typename BufferQueue<TCPDatagram>::Iterator itr = mForwardQueue.findNotSmaller(*datagram, &TCPDatagram::compare);
    if(itr != mForwardQueue.end()){
        TCPDatagram* t = *itr;
        if(t->mSeq == datagram->mSeq){
            LOGI(TAG,"a datagram with seq(%u) queued already", t->mSeq);
            SAFE_DELETE(datagram);
            return 0;
        }
    }

    LOGD(TAG,">>> queue seq(%u) len(%u)", datagram->mSeq, len);
    datagram->write(((uint8_t*)&ip) + getPayloadOffset(ip), len);
    mForwardQueue.insert(itr,datagram);
    mForwardQueueDataSize += datagram->size();

    printQueue(mForwardQueue);

    if(mForwardQueueDataSize > 0){
        mTunnel->requestSendData();
    }

    return datagram->size();
}

int TCPSession::flushBackQueue(){
    LOGD(TAG, "flushBackQueue: window left edge=%u, seq=%u, window size(%u) while %s", mLocalNext, mLocalSeq, mSendWindow, tcpstate2cstr(mState));
    printQueue(mBackQueue);
    
    //pop all packets that had acdk
    while(!mBackQueue.empty()){
        TCPDatagram* segment = mBackQueue.front();
        if(segment->mSeq < mLocalNext || (segment->mLength <= 0 && segment->mFlag == ACK && segment->mRetry > 0)){
            mBackQueue.pop();
            if(segment->mRetry > 10){
                LOGE(TAG, "retransmision>>> segment seq(%u) transmit success after try %u times", segment->mSeq, segment->mRetry);
            }
            
            int length = segment->mLength;
            mTotalBack += length;
            mBackQueueDataSize -= segment->capacity();
            safeReleaseSegment(segment);
            
            continue;
        }

        break;
    }

    //finish connection if all data back and remote server finished.
    if(mRemoteFinished && (mState == TCP_STATE::ESTABLISHED || mState == TCP_STATE::CLOSE_WAIT)){
        if(mBackQueue.empty()){
            LOGD(TAG, "push fin in back queue local seq(%u)", mLocalSeq);
            if(pushInBackQueue(FIN|ACK) >= 0){
                switchState(mState == TCP_STATE::ESTABLISHED? TCP_STATE::FIN_WAIT1 : TCP_STATE::LAST_ACK);
            }
            return 0;
        }
    }

    //do send back data
    uint32_t w = 0;
    uint32_t sent = 0;
    uint8_t packet = 0;
    uint32_t limit = mSendWindow > TCP_MAX_SEND_WINDOW? TCP_MAX_SEND_WINDOW:mSendWindow;
    for(BufferQueue<TCPDatagram>::Iterator it = mBackQueue.begin(); it != mBackQueue.end(); it++){
        
        TCPDatagram* segment = *it;
        if(w > 0 && segment->hasFlag(FIN)){
            LOGD(TAG, "fin better be sent after all data has been received.");
            break;
        }

        if(w == 0 && mLocalNext < segment->mSeq){
            LOGE(TAG, "disorder seq(%u) expected(%u), reset connection.", segment->mSeq, mLocalNext);
            resetConnection();
            return 0;
        }

        int len = segment->size();
        if((w += len) < limit && packet++ < TCP_MAX_SEND_PACKET){
            if(!segment->isBuffering() && !segment->hasAcked()){
                if(sendBackSegment(segment) > 0){
                    sent += len;
                }else{
                    break;
                }
            }
            
            continue;
        }
        
        break;
    }

    //if buffer has enough space then begin receive data from remote
    if(mState == ESTABLISHED && mBackQueueDataSize < (TCP_SESSION_BUFFER_SIZE>>1)){
        LOGD(TAG, "back buffer queue has enough space %u, open remote read event", mBackQueueDataSize);
        mTunnel->resumeReceiveData();
    }

    LOGD(TAG,"relayback %u bytes, remain %u bytes", sent, mBackQueueDataSize);
    return sent;
}

void TCPSession::safeReleaseSegment(TCPDatagram* segment){
    if(segment->isBuffering()){
        segment->setAbort(true);
        segment->setReleaseAfterBuffering(true);
    }else{
        SAFE_DELETE(segment);
    }
}

void TCPSession::switchState(TCP_STATE state){
    if(mState != state){
        LOGR(TAG,"switch state from %s to %s", tcpstate2cstr(mState), tcpstate2cstr(state));
        mState = state;
        if(mState == TCP_STATE::CLOSED){
            close();
        }

        dispatcher::get().dispatch(new Event(Event::ID::CONN_STATE,0, toConnInfo(new ConnInfo)));
    }
    
    return;
}

uint32_t TCPSession::updateWindowSize(){
    uint32_t max = 0xFFFF<<mWindowScale;
    uint32_t windowSize = mTunnel->isAlive()?mTunnel->getAvailableBufferSize():max;
    
    // LOGD(TAG,"get window size %d, max %d", windowSize,max);
    if(windowSize > max){
        windowSize = max;
    }

    if(windowSize <= mForwardQueueDataSize){//the data still in buffer can not be sent out stop receive from local
        LOGD(TAG,"window size smaller than queue data windowSize(%d), queued(%d)",windowSize,mForwardQueueDataSize);
        mReceiveWindow = 0;
    }else{
        mReceiveWindow = windowSize - mForwardQueueDataSize;
    }

    if(mReceiveWindow <= 64){
        LOGE(TAG,"receive window size too small %d",mReceiveWindow);
    }

    return mReceiveWindow;
}

void TCPSession::clearBackQueue(){
    while(!mBackQueue.empty()){
        safeReleaseSegment(mBackQueue.front());
        mBackQueue.pop();
    }

    mBackQueueDataSize = 0;
}

int TCPSession::sendBackSegment(TCPDatagram* segment){
    if(segment == NULL || segment->size() <= 0){
        return -1;
    }

    segment->mRetry++;
    if(segment->mRetry > 100){
        LOGE(TAG, "segment retransmition %u times. seq(%u) ack(%u)", segment->mRetry, segment->mSeq, segment->mAck);
    }

    int result = 1;
    Event* e = new Event(Event::ID::WRITE_PACKAGE,0,segment);
    Result* r = new Result();
    r->code = 1;
    int dr = dispatcher::get().dispatchSync(e,r);
    if(dr || r->code){
        LOGE(TAG,"write package(seq=%u, len=%d) failed", segment->mSeq, segment->size());
        return -1;
    }else{
        LOGR(TAG,"<<< %u/%u, seq(%u), ack(%u), s%ua%uf%ur%u", segment->mLength, mTotalBack, segment->mSeq, segment->mAck, segment->hasFlag(SYN), segment->hasFlag(ACK), segment->hasFlag(FIN), segment->hasFlag(RST));
        LOGD(TAG,"ip: %s", toString(*(IPHeader*)segment->begin(), (char*)sTemporary, MAX_SHARE_BUFFER));

        ConnInfo* connInfo = toConnInfo(new ConnInfo, segment->mLength, (IPHeader*)segment->begin());
        connInfo->seq = segment->mSeq;
        connInfo->ack = segment->mAck;
        dispatcher::get().dispatch(new Event(EVENT::CONN_TRAFFIC_BACK, 0, connInfo));
    }

    SAFE_DELETE(r);
    return segment->size();
}

int TCPSession::resetConnection(){
    updateWindowSize();
    
    mLocalSeq = mLocalNext;
    TCPDatagram* segment = buildPackage(RST|ACK,NULL,0);
    segment->setReleaseAfterBuffering(true);
    if(sendBackSegment(segment) > 0){
        switchState(TCP_STATE::CLOSED);
        return 1;
    }else{
        SAFE_DELETE(segment);
    }

    return 0;
}

int TCPSession::onRemoteFinished(){
    LOGD(TAG, "remote finished");
    mRemoteFinished = true;
    flushBackQueue();
    return 0;
}

int TCPSession::pushInBackQueue(uint8_t flag,uint8_t* data,uint16_t length){
    LOGD(TAG, "enqueue back: flag = %u len = %u", flag, length);

    if(mLocalNext > mLocalSeq){
        LOGE(TAG, "CAN NOT push delayed packet: local seq %u, left edge %u", mLocalSeq, mLocalNext);
        return -1;
    }

    updateWindowSize();
    
    TCPDatagram* segment = buildPackage(flag,data,length);
    if(segment == NULL){
        return -1;
    }

    segment->setNeedCapture(((mCtrlType&CTRL_TYPE::CAPTURE) != 0));
    mBackQueue.push(segment);
    mBackQueueDataSize += segment->capacity();
    if(segment->hasFlag(FIN|SYN)){
        ++mLocalSeq;
    }else{
        mLocalSeq += length;
    }

    LOGD(TAG, "after enqueue, local seq = %u", mLocalSeq);
    flushBackQueue();
    return length;
}

TCPDatagram* TCPSession::buildPackage(uint8_t flag,uint8_t* data,uint16_t length){

    size_t ttlen = 0;
    uint16_t checksum = 0;
    uint16_t optlen = flag&SYN?10:0;
    TCPDatagram* segment = NULL;
    TCPHeader* tcpHeader = NULL;
    if (mIPVer == 4) {
        ttlen = sizeof(IPHeader) + sizeof(TCPHeader) + optlen + length;
        segment = new TCPDatagram(ttlen);
        segment->limit(ttlen);

        IPHeader* ip4 = (IPHeader *) segment->begin();
        tcpHeader = (TCPHeader *) (((uint8_t*)ip4) + sizeof(TCPHeader));
        memcpy(((uint8_t*)ip4) + sizeof(IPHeader) + sizeof(TCPHeader) + optlen, data, length);

        // Build IP4 header
        ip4->VER_IHL = (4<<4)|(sizeof(IPHeader) >> 2);
        ip4->TTLEN = SWAP_INT16(ttlen);
        ip4->TTL = 64;
        ip4->PTOL = PROTO::TCP;
        ip4->SRC = mDestAddr.ip4;
        ip4->DEST = mSrcAddr.ip4;

        // Calculate IP4 checksum
        ip4->CHECKSUM = ~computeChecksum(0, (uint8_t *)ip4, sizeof(IPHeader));


        struct ippseudo pseudo;
        memset(&pseudo, 0, sizeof(struct ippseudo));
        pseudo.ippseudo_src.s_addr = ip4->SRC;
        pseudo.ippseudo_dst.s_addr = ip4->DEST;
        pseudo.ippseudo_p = ip4->PTOL;
        pseudo.ippseudo_len = htons(sizeof(TCPHeader) + length + optlen);

        checksum = computeChecksum(0, (uint8_t *) &pseudo, sizeof(struct ippseudo));
    } else {
        LOGE(TAG,"ip version not support %d",mIPVer);
        return NULL;
    }

    memset(tcpHeader, 0, sizeof(TCPHeader));
    tcpHeader->src = mDestPort;
    tcpHeader->dest = mSrcPort;
    tcpHeader->seq = htonl(mLocalSeq+mLocalSeqBase);
    tcpHeader->dataoff = (sizeof(TCPHeader)+optlen) >> 2;
    *((uint8_t*)(((uint8_t*)tcpHeader)+13)) = flag;
    tcpHeader->ack_seq = htonl(mRemoteNext+mRemoteSeqBase);
    tcpHeader->window = htons(mReceiveWindow >> mWindowScale);

    uint8_t* options = ((uint8_t*)tcpHeader) + sizeof(TCPHeader);
    if (tcpHeader->syn) {
        *(options) = 2; // MSS
        *(options + 1) = 4; 
        *((uint16_t *) (options + 2)) = 10000;

        *(options + 4) = 3;// window scale
        *(options + 5) = 3; 
        *(options + 6) = mWindowScale;

        *(options + 7) = 4;
        *(options + 8) = 1;

        *(options + 9) = 0; // end
    }

    checksum = computeChecksum(checksum, (uint8_t *)tcpHeader, sizeof(TCPHeader));
    checksum = computeChecksum(checksum, options, (size_t) optlen);
    checksum = computeChecksum(checksum, data, length);
    tcpHeader->checksum = ~checksum;

    segment->mSeq = mLocalSeq;
    segment->mAck = mRemoteNext;
    segment->mFlag = flag;
    segment->mLength = length;
    
    return segment;
}

int TCPSession::onTunnelDataBack(uint8_t* data, uint32_t length){
    LOGD(TAG,"receive %u bytes from tunnel",length);
    if(length > TCP_IP_PAYLOAD_SIZE){
        LOGD(TAG,"receive %u bytes from tunnel",length);
    }

    if(length <= 0){
        mTunnel->pauseReceiveData();
        onRemoteFinished();
    }else{

        uint32_t i = 0;
        while(i < length){
            uint16_t l = (length-i) > TCP_IP_PAYLOAD_SIZE?TCP_IP_PAYLOAD_SIZE:length-i;
            if(pushInBackQueue(ACK,data+i, l) < 0){
                LOGE(TAG,"relay data failed while %s", tcpstate2cstr(mState));
                break;
            }
            
            i += l;
        }

        if(i > 0){
            mTotalRecv += i;
            dispatcher::get().dispatch(new Event(EVENT::CONN_TRAFFIC_RECV, 0,toConnInfo(new ConnInfo, i)));
        }

        if(mBackQueueDataSize >= TCP_SESSION_BUFFER_SIZE){
            LOGE(TAG, "back queue has not enough space(%u), pause receiving data", mBackQueueDataSize);
            mTunnel->pauseReceiveData();
        }
    }

    mLastTime = time(NULL);

    return 0;
}

int TCPSession::onTunnelWritable(){
    LOGD(TAG,"onTunnelWritable, buffer size %u", mForwardQueueDataSize);
    uint32_t sent = 0;

    if(!mForwardQueue.empty()){
        while(!mForwardQueue.empty()){
            TCPDatagram* segment = mForwardQueue.front();
            uint8_t* data = segment->begin();
            int size = segment->size();

            int s = mTunnel->sendData(segment->begin(), segment->size(), !segment->hasFlag(PSH));

            if(s <= 0) {
                LOGE(TAG,"send data failed: %d(%s)",errno,strerror(errno));
                break;
            } else {
                LOGD(TAG,"%d bytes have send out",s);

                sent += s;
                segment->consume(s);

                if(segment->size() <= 0){
                    mForwardQueue.pop();
                    SAFE_DELETE(segment);
                }
            }
        }

        if(sent > 0){
            mTotalSent += sent;
            mForwardQueueDataSize -= sent;
            dispatcher::get().dispatch(new Event(EVENT::CONN_TRAFFIC_SENT, 0, toConnInfo(new ConnInfo, sent)));
        }else {
            mTunnel->pauseSendData();
        }

        LOGD(TAG, "%u bytes sent, %u bytes left in buffer", sent, mForwardQueueDataSize);
    }else{
        LOGD(TAG,"out buffer is empty, nothing need to send.");
        mTunnel->pauseSendData();
    }

    return sent;
}

bool TCPSession::timeout(){
    time_t s = time(NULL) - mLastTime;
    int t = 10;
    if(mState == TCP_STATE::ESTABLISHED){
        t = 3600;
    }

    if(s > t){
        LOGR(TAG,"timeout %u(%u), while %s", s, t, tcpstate2cstr(mState));
    }

    return s > t;
}

int TCPSession::onTunnelConnected(){
    LOGD(TAG,"tunnel connected while %s", tcpstate2cstr(mState));
    LOGR(TAG, "const %u to connect to remote", time(NULL)-mTimeStartConnectProxy);
    return 1;
}

int TCPSession::onTunnelDisconnected(uint8_t reason){
    LOGR(TAG,"tunnel disconnectd %u,  while %s", reason, tcpstate2cstr(mState));

    switch(mState){
        case TCP_STATE::CLOSE_WAIT:
        case TCP_STATE::ESTABLISHED:{
            onRemoteFinished();
            break;
        }
        case TCP_STATE::TIME_WAIT:
        case TCP_STATE::CLOSING:
        case TCP_STATE::FIN_WAIT1:
        case TCP_STATE::FIN_WAIT2:{
            switchState(TCP_STATE::CLOSED);
            break;
        }

        default:{
            resetConnection();
            break;
        }
    }

    return 1;
}

void TCPSession::printQueue(BufferQueue<TCPDatagram>& queue){
    if(queue.empty()){
        LOGD(TAG, "queue is empty");
        return;
    }

    LOGD(TAG,"queueinfo>>>>>>>>>>>>>>>>>>>>>>");
    for(typename BufferQueue<TCPDatagram>::Iterator itr = queue.begin(); itr != queue.end(); itr++){
        TCPDatagram* s = *itr;

        LOGD(TAG,"queueinfo: seq(%u)len(%u)checksum(%u)size(%u), "
            "isBuffering(%u), hasAcked(%u),"
            "syn(%u)ack(%u)fin(%u)rst(%u)",s->mSeq,s->mLength,s->mChecksum,s->size(), 
            s->isBuffering(), s->hasAcked(),
            s->hasFlag(SYN), s->hasFlag(ACK), s->hasFlag(FIN), s->hasFlag(RST));

    }
    LOGD(TAG,"queueinfo<<<<<<<<<<<<<<<<<<<<<<");
}

ConnInfo* TCPSession::toConnInfo(ConnInfo *info, int size, IPHeader* ip){
    Session::toConnInfo(info, size, ip);
    info->state = mState;

    if(ip != NULL){
        TCPHeader* tcp = (TCPHeader*)(((uint8_t*)ip) + getHeaderLen(*ip));
        if(tcp->fin){
            info->flag |= TCPF::TCPF_FIN;
        }
        if(tcp->syn){
            info->flag |= TCPF::TCPF_SYN;
        }
        if(tcp->rst){
            info->flag |= TCPF::TCPF_RST;
        }
        if(tcp->psh){
            info->flag |= TCPF::TCPF_PSH;
        }
        if(tcp->ack){
            info->flag |= TCPF::TCPF_ACK;
        }
        if(tcp->urg){
            info->flag |= TCPF::TCPF_URG;
        }
        if(tcp->ece){
            info->flag |= TCPF::TCPF_ECE;
        }
        if(tcp->cwr){
            info->flag |= TCPF::TCPF_CWR;
        }
    }
    

    return info;
}
