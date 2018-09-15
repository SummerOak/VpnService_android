#ifndef IPROXY_H
#define IPROXY_H

#include "IProxyListener.h"


class IProxy{

public:
	virtual int connectProxy() = 0;

	virtual uint32_t getAvailableBufferSize();

	/**
	*	return the size of data be sent or queued
	*/
	virtual int send2Proxy(uint8_t* data, uint32_t len) = 0;

	virtual int requestSendData();
	virtual int pauseReceiveData();
	virtual int resumeReceiveData();

	virtual void closeProxy() = 0;

	virtual ~IProxy(){};
};

#endif