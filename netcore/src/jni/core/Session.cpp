#include "Session.h"
#include "UDPSession.h"
#include "TCPSession.h"
#include "JniBridge.h"
#include "Utils.h"
#include <unistd.h>
#include <netinet/tcp.h>

const char* Session::STAG = PTAG("Session");
uint32_t Session::sID = 0;
uint8_t Session::sTemporary[MAX_SHARE_BUFFER];

Session::Session(){
	mDestSockAddr = NULL;
	mAppName = NULL;
	mID = ++sID;
	return;
}

Session::~Session(){
	LOGD(TAG,"session dying %p",this);

	if(mIPVer == 4){
		struct sockaddr_in* p = (struct sockaddr_in*)mDestSockAddr;
		SAFE_DELETE(p);
		mDestSockAddr = NULL;
	}

	SAFE_DELETE(mAppName);
	return;
}

Session* Session::findSession(std::list<Session*>& sessions,IPHeader& ip){
	if(sessions.empty()){
		return NULL;
	}

	uint8_t ipVer = getIPVer(ip);
	uint16_t srcPort = getSrcPort(ip);
	uint16_t destPort = getDestPort(ip);

	Addr srcAddr;
	Addr destAddr;
	if(ipVer == 4){
		srcAddr.ip4 = getSrc(ip);
		destAddr.ip4 = getDest(ip);
	}else{
		LOGE(STAG,"ip version not supported %d ",ipVer);
		return NULL;
	}

	for(std::list<Session*>::iterator itr = sessions.begin(); itr!=sessions.end();itr++){
		Session* session = *itr;
		// LOGD(STAG,"find session %p",session);
		if(!session->hasClosed()
			&& session->mIPVer == ipVer 
			&& session->mSrcAddr.ip4 == srcAddr.ip4
			&& session->mDestAddr.ip4 == destAddr.ip4
			&& session->mSrcPort == srcPort
			&& session->mDestPort == destPort){
			return session;
		}
	}

	return NULL;
}

int Session::initBaseInfo(Session& session,IPHeader& ip){
	session.mIPVer = vpnlib::getIPVer(ip);
	if(session.mIPVer == 4){
		session.mProtocol = ip.PTOL;
		session.mSrcAddr.ip4 = vpnlib::getSrc(ip);
		session.mDestAddr.ip4 = vpnlib::getDest(ip);
		session.mSrcPort = vpnlib::getSrcPort(ip);
		session.mDestPort = vpnlib::getDestPort(ip);
		struct sockaddr_in* addr4 = new sockaddr_in();
		addr4->sin_family = AF_INET;
        addr4->sin_addr.s_addr = session.mDestAddr.ip4;
        addr4->sin_port = session.mDestPort;
        session.mDestSockAddr = (struct sockaddr*)addr4;

        uint32_t saddr = SWAP_INT32(session.mSrcAddr.ip4);
        uint32_t daddr = SWAP_INT32(session.mDestAddr.ip4);
        session.mUID = vpnlib::getUID(session.mIPVer,session.mProtocol,
        	(void*)&(saddr),SWAP_INT16(session.mSrcPort),
        	(void*)&(daddr),SWAP_INT16(session.mDestPort));

        session.mAppName = JniBridge::getAppName(session.mUID);

        LOGD(STAG,"uid %d , appName(%s), tag [%s]",session.mUID,session.mAppName,session.TAG);

	}else{
		LOGE(STAG,"addr version not support now.");
		return 1;
	}

	return 0;
}

Session* Session::buildSession(IPHeader& ip,IReactor* reactor){
	IPHeader ipHeader = (IPHeader)ip;
	Session* session = NULL;
	if(ipHeader.PTOL == PROTO::UDP){
		session = new UDPSession();
		if(initBaseInfo(*session,ip)){
			LOGE(STAG,"init base info failed.");
			SAFE_DELETE(session);
			return NULL;
		}
	}else if(ipHeader.PTOL == PROTO::TCP){
		session = new TCPSession();
		if(initBaseInfo(*session,ip)){
			LOGE(STAG,"init base info failed.");
			SAFE_DELETE(session);
			return NULL;
		}
	}

	if(session->init(ip, reactor)){
    	SAFE_DELETE(session);
    	return NULL;
    }

    dispatcher::get().dispatch(new Event(Event::ID::CONN_BORN,0, session->toConnInfo(new ConnInfo)));

    LOGD(STAG,"buildSession success %p: %s", session, session->TAG);
	return session;
}

int Session::releaseSession(Session* session){
	if(session == NULL){
		LOGE(STAG,"session is NULL");
		return 1;
	}

	LOGD(STAG,"releaseSession %p: %s",session, session->TAG);
	dispatcher::get().dispatch(new Event(Event::ID::CONN_DIE,0, session->toConnInfo(new ConnInfo)));
	SAFE_DELETE(session);

	return 1;
}

inline ConnInfo* Session::toConnInfo(ConnInfo *info, int size, IPHeader* ip){
	if(info != NULL){
		info->id = mID;
		info->uid = mUID;
		info->type = mProtocol;
		info->accept = mTotalAccept;
		info->back = mTotalBack;
		info->sent = mTotalSent;
		info->recv = mTotalRecv;

		IPInt2Str(mDestAddr.ip4, info->dest, sizeof(info->dest));
		info->destPort =  SWAP_INT16(mDestPort);

		if(ip != NULL){
			info->size = getPayloadLen(*ip);
		}else{
			info->size = size;
		}
	}
	
	return info;
}



