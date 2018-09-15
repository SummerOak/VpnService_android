#ifndef SOCKS5_PROXY_H
#define SOCKS5_PROXY_H

#include <stdint.h>
#include "ITunnel.h"
#include "IReactor.h"
#include "BufferQueue.h"
#include "IPAdpt.h"
#include "Cipher.h"

#define SOCKS5_SHARE_BUFFER_SZ (MAX_PARCEL_SIZE)
#define MAX_SOCKS5_PROXY_SEND_BUFFER (1<<18) //256kb

class Socks5{
public:

	static const uint8_t S_INIT		= 0;
	static const uint8_t S_GREET	= 1;
	static const uint8_t S_VERIFY	= 2;
	static const uint8_t S_CONNECT	= 3;
	static const uint8_t S_TRANS	= 4;
	static const uint8_t S_END		= 5;
	static const uint8_t S_FAILED 	= 6;
	

	static const uint8_t VERIFY_NONE = 0;
	static const uint8_t VERIFY_ACCOUNT = 2;
};

class Socks5Proxy: public ITunnel, public IReactHandler{

public:

	Socks5Proxy(const void* addr, uint16_t port, uint8_t ipver, IReactor* reactor, ITunnelClient* client);
	~Socks5Proxy();

	void setTag(const char* tag);
	void setProtocol(int protocol);
	void setDestAddr(uint8_t ipver, void* addr, uint16_t port);

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

	static const char* stateDesc(uint8_t state);

private:

	static bool containS5Verify(uint32_t vfm, uint8_t m);
	int encryptAndQueue(BufferQueue<Segment>& queue, uint8_t* data, uint32_t len);
	int sendQueue(BufferQueue<Segment>& queue);
	void switchS5State(uint8_t state);
	void onProxyFailed();
	uint8_t getS5VerifyMethods(uint8_t* out, int len);
	int sendS5Greet();
	int sendS5Account();
	int sendS5Connect();
	int handleS5GreetBack(uint8_t* data, int len);
	int handleS5VerifyBack(uint8_t* data, int len);
	int handleS5ConnBack(uint8_t* data, int len);
	int handlePayload(uint8_t* data, int len);

	bool mAlive = false;
	bool mConnected = false;
	vpnlib::Addr mDestAddr;
	uint16_t mDestPort = 0;
	uint8_t mProtocol = 0;
	uint8_t mDestIPVer = 0;
	uint8_t mS5State = Socks5::S_INIT;
	uint32_t mPayloadDataBack = 0;

	uint32_t mBufferedSize = 0;
	BufferQueue<Segment> mSocks5;
	BufferQueue<Segment> mOutQueue;
	Segment mDecryptBuffer = Segment(MAX_PARCEL_SIZE<<2);

	IReactor* mReactor = NULL;
	int mSocket = -1;

	static uint8_t sTemporary[];

	char TAG[128];

};

#endif