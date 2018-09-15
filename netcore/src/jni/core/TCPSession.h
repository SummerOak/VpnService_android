#ifndef TCPSESSION_H
#define TCPSESSION_H

#include "Session.h"
#include "ITunnel.h"
#include "EventDispatcher.h"
#include "Utils.h"

enum TCPF{
	TCPF_FIN = 1<<0,
    TCPF_SYN = 1<<1,
    TCPF_RST = 1<<2,
    TCPF_PSH = 1<<3,
    TCPF_ACK = 1<<4,
    TCPF_URG = 1<<5,
    TCPF_ECE = 1<<6,
    TCPF_CWR = 1<<7,
};

class TCPSession;

class TCPDatagram: public BufferSegment{

friend class TCPSession;
public:

	TCPDatagram(size_t size):BufferSegment(size){}

	~TCPDatagram(){};

	bool hasFlag(uint8_t flag){
		return (mFlag&flag) > 0;
	}

	void setAcked(bool ack){ mACKed = ack; }
	bool hasAcked(){return mACKed; }

	static int compare(TCPDatagram& a,TCPDatagram& b){
		return a.mSeq - b.mSeq;
	}

private:
	uint32_t mSeq;
	uint32_t mAck;
	uint8_t mFlag;
	uint16_t mChecksum;
	uint16_t mLength;
	bool mACKed = false;
	uint32_t mRetry = 0;
};

class TCPSession: public Session, public ITunnelClient{

	friend class Session;

public:
	
	int accept(IPHeader& ip);

protected:
	int init(IPHeader& ip, IReactor* reactor);
	int close();
	bool hasClosed();

	int pushInBackQueue(uint8_t flag,uint8_t* data=NULL,uint16_t len=0);
	int sendBackSegment(TCPDatagram* segment);
	int resetConnection();
	TCPDatagram* buildPackage(uint8_t flag,uint8_t* data,uint16_t len);
	ConnInfo* toConnInfo(ConnInfo *info, int size = 0, IPHeader* ip = NULL);
	bool timeout();

	int onTunnelConnected();
	int onTunnelDisconnected(uint8_t reason);
	int onTunnelWritable();
	int onTunnelDataBack(uint8_t* data, uint32_t len);

private:
	TCPSession():Session(){ 
		snprintf(TAG,sizeof(TAG),"VpnLib.TCPSession_%d",mID); 
		LOGD(TAG,"a TCPSession born %p",this);
	}

	~TCPSession();

	int sendBufferedData(bool proxy);
	void switchState(TCP_STATE state);
	int onReceiveSYN(IPHeader& ip);
	int onReceiveFIN(IPHeader& ip);
	int onReceiveRST(IPHeader& ip);
	int onRemoteFinished();
	int enqueueIfNeed(IPHeader& ip);
	int flushBackQueue();
	void clearBackQueue();
	void safeReleaseSegment(TCPDatagram* segment);
	void onSack(uint8_t* sack);

	int keepAlive();
	uint32_t updateWindowSize();

	void printQueue(BufferQueue<TCPDatagram>& queue);

	bool mAlive = false;
	BufferQueue<TCPDatagram> mForwardQueue;
	uint32_t mForwardQueueDataSize = 0;

	BufferQueue<TCPDatagram> mBackQueue;
	uint32_t mBackQueueDataSize = 0;

	TCP_STATE mState;
	bool mRemoteFinished = false;
	
	uint8_t mSack = false;//if enable sack options
	uint32_t mLocalSeq = 0;//the sequence of data being enqueued
	uint32_t mLocalNext = 0;// the window left edge
	uint32_t mLocalSeqBase = 0;
	uint32_t mRemoteNext = 0; // expected sequence of remote data
	uint32_t mRemoteSeqBase = 0;

    uint32_t mSendWindow = 0;
    uint32_t mReceiveWindow = 0;
    uint8_t mWindowScale = 0;
    uint16_t mMSS;

    int mCtrlType = 0;
    ITunnel* mTunnel = NULL;

    time_t mTimeStartConnectProxy;

};

#endif