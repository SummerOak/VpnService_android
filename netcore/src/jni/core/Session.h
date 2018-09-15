#ifndef SESSION_H
#define SESSION_H

#include "BufferQueue.h"
#include "IPAdpt.h"
#include "IReactor.h"
#include <list>

#define MAX_SHARE_BUFFER 512
#define MAX_IP_SIZE (61440)

using namespace vpnlib;

typedef struct ConnInfo{
	uint32_t id;
	int uid;
	char dest[64];
	int destPort;
	uint8_t type;
	uint8_t state;
	int64_t accept = 0l;
	int64_t back = 0l;
	int64_t sent = 0l;
	int64_t recv = 0l;

	uint32_t flag = 0;
	uint32_t size = 0;

	uint32_t seq = 0;
	uint32_t ack = 0;
}ConnInfo;

class Session{

public:
	static const char* STAG;
	static const uint16_t MSS;

	virtual int accept(IPHeader& ip) = 0;
	virtual int close() = 0;
	virtual bool hasClosed() = 0;

	static Session* findSession(std::list<Session*>& sessions,IPHeader& ip);
	static Session* buildSession(IPHeader& ip,IReactor* reactor);
	static int releaseSession(Session* session);

protected:

	static int initBaseInfo(Session& session,IPHeader& ip);
	virtual int init(IPHeader& ip, IReactor* reactor) = 0;
	
	virtual ConnInfo* toConnInfo(ConnInfo *info, int size = 0, IPHeader* ip = NULL);

	Session();
	virtual ~Session();

	static uint32_t sID;
	uint32_t mID;

	char TAG[128];

	uint8_t mIPVer;
	uint8_t mProtocol;

	uint16_t mSrcPort;
	uint16_t mDestPort;

	Addr mSrcAddr;
	Addr mDestAddr;
	struct sockaddr* mDestSockAddr;

	int mUID;
	char* mAppName;

	uint32_t mTotalAccept = 0l;
	uint32_t mTotalBack = 0l;
	uint32_t mTotalRecv = 0l;
	uint32_t mTotalSent = 0l;
	time_t mLastTime;

	uint8_t mBuffer[MAX_IP_SIZE];

	static uint8_t sTemporary[];
};

#endif