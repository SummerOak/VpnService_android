#ifndef TCPTUNNEL_H
#define TCPTUNNEL_H

#include "ITunnel.h"
#include "IReactor.h"
#include <stdint.h>

#define SHARED_BUFFER_SIZE (61440) //can not larger then 65536 (IP package maximum size)

class TCPTunnel: public ITunnel, public IReactHandler{

public:

	TCPTunnel(const void* addr, uint16_t port, uint8_t ipver,IReactor* reactor, ITunnelClient* client);
	~TCPTunnel();

	void setTag(const char* tag);
	int establish();
	bool isAlive();
	int keepAlive();
	uint32_t getAvailableBufferSize();
	int requestSendData();
	int sendData(uint8_t* data, uint32_t len, bool more = true);
	int pauseSendData();
	int pauseReceiveData();
	int resumeReceiveData();
	void destroyTunnel();

	void onReadable(int fd,IReactor*,void* tag);
	void onWritable(int fd,IReactor*,void* tag);
	void onException(int fd,IReactor*,int error,void* tag);
	void onPeriodicallyCheck(int fd, IReactor*, uint32_t interval, void* tag);


private:

	char TAG[128];

	bool mAlive = false;
	bool mConnected = false;
	time_t mLastTime = 0;
	const uint32_t TIMEOUT = 25*1000;
	int mSocket = -1;
	IReactor* mReactor = NULL;

	static uint8_t SHARED_BUFFER[];

};

#endif