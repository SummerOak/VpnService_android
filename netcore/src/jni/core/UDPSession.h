#ifndef UDPSESSION_H
#define UDPSESSION_H

#include "IPAdpt.h"
#include "Session.h"
#include "ITunnel.h"
#include "EventDispatcher.h"
#include "IReactor.h"
#include <unistd.h>
#include <sys/socket.h>

class UDPSession: public Session, public ITunnelClient, public IReactHandler{

	friend class Session;
public:

	int accept(IPHeader& ip);

protected:
	
	int init(IPHeader& ip, IReactor* reactor);
	int connectRemote();
	int close();
	bool hasClosed();

	ConnInfo* toConnInfo(ConnInfo *info, int size = 0, IPHeader* ip = NULL);
	bool timeout();

	int onTunnelConnected();
	int onTunnelDisconnected(uint8_t reason);
	int onTunnelWritable();
	int onTunnelDataBack(uint8_t* data, uint32_t len);

	void onReadable(int fd,IReactor* reactor,void* tag);
	void onWritable(int fd,IReactor* reactor,void* tag);
	void onException(int fd,IReactor* reactor,int error,void* tag);
	void onPeriodicallyCheck(int fd, IReactor* reactor, uint32_t interval, void* tag);

private:

	UDPSession();
	~UDPSession();

	BufferSegment* buildPacket(uint8_t* data, size_t length);

	int mCtrlType = 0;
	ITunnel* mTunnel = NULL;

	bool mAlive = false;
	int mSocket = -1;
	BufferSegment* mOutSegment;
	BufferQueue<BufferSegment> mOutBuffer;
	
	BufferQueue<BufferSegment> mBackQueue;

	IReactor* mReactor;

	time_t mTimeCost;

};

#endif