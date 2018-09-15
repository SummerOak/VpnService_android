#ifndef IPROXY_LISTENER_H
#define IPROXY_LISTENER_H

class IProxyListener{

public:
	virtual int onProxyConnected() = 0;
	virtual int onProxyFailed(int code) = 0;
	virtual int onProxyDisconnected() = 0;

	// when proxy is available for writing, notify client to sent data to proxy,
	// the return value is the size of data be sent.
	virtual int onProxyWritable() = 0;
	virtual int onProxyDataBack(uint8_t* data, uint32_t len) = 0;

};

#endif