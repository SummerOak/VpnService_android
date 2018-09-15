#ifndef PCAP_WRITER_H
#define PCAP_WRITER_H

#include <stdint.h>
#include "EventDispatcher.h"

typedef struct PcapHdr {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
}PcapHdr;

typedef struct PcapRecHdr {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
} PcapRecHdr;

class PacketCapture: public EventHandler<Event,Result>{

public:
	static int init();

	static PacketCapture& get(){return *sInstance; }


	int start();
	int stop();
	int write(uint8_t* data, uint32_t len);

	int onEvent(Event& event);
	int onSyncEvent(Event& event,Result* result);

private:
	static PacketCapture *sInstance;

	int generatePcapFilePath(char* out, uint8_t len);
	int writeFile(const void * data, uint32_t len);

	PacketCapture();
	~PacketCapture();

	static const char* TAG;

	FILE *mPcapFd = NULL;

	EventDispatcher<Event, Result> mDispatcher;


};

#endif