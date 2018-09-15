#ifndef ISENDER_H
#define ISENDER_H

#include <stdint.h>
#include <netinet/in.h>
#include "Defines.h"
#include "Log.h"
#include "IPAdpt.h"

class ITunnelClient;

class ITunnel{

public:

	ITunnel(const void* addr, uint16_t port, uint8_t ipver, ITunnelClient* client){
		mClient = client;
		if(addr != NULL && port != 0){
			if(ipver == 4){
				struct sockaddr_in* addr4 = new sockaddr_in();
				addr4->sin_family = AF_INET;
		        addr4->sin_addr.s_addr = *(uint32_t*)addr;
		        addr4->sin_port = SWAP_INT16(port);
		        mRemoteSockAddr = (struct sockaddr*)addr4;
			}else{
				LOGE(PTAG("ITunnel"),"ip version not supported %u", ipver);
			}

	        mIPVer = ipver;
		}
	}

	/**
	*	notify tunnel connect to remote
	*/
	virtual int establish() = 0;

	/**
	*	if tunnel alive
	*/
	virtual bool isAlive() = 0;

	/**
	*	get data size the tunnel can take
	*/
	virtual uint32_t getAvailableBufferSize() = 0;

	/**
	*	try keep alive while not data exchange for a period of time
	*/
	virtual int keepAlive() = 0;

	/**
	*	request a write event if tunnel is writable
	*/
	virtual int requestSendData() = 0;

	/**
	*	return the size of data be sent or queued
	*/
	virtual int sendData(uint8_t* data, uint32_t len, bool more = true) = 0;

	/**
	*	notify tunnel stop write event
	*/
	virtual int pauseSendData() = 0;

	/**
	*	notify tunnel stop relay data back, 
	*  	usually because the client's buffer is full and can't take data anymore.
	*/
	virtual int pauseReceiveData() = 0;

	/**
	*	notify tunnel to relay data from remote.
	*/
	virtual int resumeReceiveData() = 0;

	/**
	*	destroy tunnel
	*/
	virtual void destroyTunnel() = 0;

	virtual ~ITunnel(){ 
		mClient = NULL; 
		if(mRemoteSockAddr != NULL){
			if(mIPVer == 4){
				struct sockaddr_in* p = (struct sockaddr_in*)mRemoteSockAddr;
				SAFE_DELETE(p);
			}

			mRemoteSockAddr = NULL;
		}
	}

protected:
	ITunnelClient* mClient = NULL;
	uint8_t mIPVer = 0;
	struct sockaddr* mRemoteSockAddr = NULL;
};

class ITunnelClient{

public:
	/**
	*	after tunnel connected to remote
	*/
	virtual int onTunnelConnected() = 0;

	/**
	*	tunnel disconnected from remote
	*/
	virtual int onTunnelDisconnected(uint8_t reason) = 0;

	/**
	* 	when tunnel is available for writing, notify client to sent data into tunnel,
	* 	the return value is the size of data be sent.
	*/
	virtual int onTunnelWritable() = 0;

	/**
	*	Be called when receive data from remote, tunnel notify client to handle this data;
	*	the data will be release after this call;
	*/
	virtual int onTunnelDataBack(uint8_t* data, uint32_t len) = 0;

};

#endif