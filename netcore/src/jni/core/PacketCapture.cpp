#include "PacketCapture.h"
#include "Log.h"
#include "BufferQueue.h"
#include "Defines.h" 
#include <stdio.h>

const char* PacketCapture::TAG = PTAG("PacketCapture");
PacketCapture* PacketCapture::sInstance = new PacketCapture;

#define PCAP_RECORD_SIZE (1<<16)
#define MAX_PACP_FILE_SIZE (1<<26)

int PacketCapture::init(){
    return 0;
}

PacketCapture::PacketCapture(){


	mDispatcher.registerEvent(EVENT::CAPTURE_START, this);
    mDispatcher.registerEvent(EVENT::CAPTURE_STOP, this);
    mDispatcher.registerEvent(EVENT::CAPTURE_PACKET, this);

}

int PacketCapture::start(){
    mDispatcher.dispatch(new Event(Event::ID::CAPTURE_START));
    return 0;
}

int PacketCapture::stop(){
    mDispatcher.dispatch(new Event(Event::ID::CAPTURE_STOP));
    return 0;
}

int PacketCapture::write(uint8_t* data, uint32_t len){

    if(data != NULL && len > 0){
        Segment* s = new Segment(len);
        s->write(data, len);
        Event* e = new Event(Event::ID::CAPTURE_PACKET, 0, s);

        mDispatcher.dispatch(new Event(Event::ID::CAPTURE_PACKET, 0, s));
    }

    return 0;
}

int PacketCapture::onSyncEvent(Event& event,Result* result){
    return 1;
}

int PacketCapture::generatePcapFilePath(char* out, uint8_t len){
    const char* dir = Settings::getSSetting(Settings::Key::SK_CAPTURE_DIRECTORY, "");
    if(dir == NULL){
        LOGE(TAG,"capture output director not setted.");
        return 0;
    }

    time_t rawtime;
    struct tm * timeinfo;
    char strDate[64];
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    strftime(strDate,sizeof(strDate), "%m%d_%H_%M_%S", timeinfo);

    if(len > strlen(strDate) + strlen(dir)){
        snprintf(out, len, "%s%s.pcap", dir, strDate);
        return 1;
    }

    return 0;
}

int PacketCapture::writeFile(const void * data, uint32_t len){
    if(mPcapFd != NULL){
        if (fwrite(data, len, 1, mPcapFd) < 1){
            LOGE(TAG, "PCAP fwrite error %d: %s", errno, strerror(errno));
        } else {
            long fsize = ftell(mPcapFd);
            LOGD(TAG, "PCAP wrote %u/%ld", len, fsize);

            if (fsize > MAX_PACP_FILE_SIZE) {
                LOGE(TAG, "pcap file too large, stop writing");

                fclose(mPcapFd);
                mPcapFd = NULL;
            }

            return len;
        }
    }

    return 0;
}

int PacketCapture::onEvent(Event& event){
    LOGD(TAG,"onEvent %d",event.id);

    switch(event.id){
        case EVENT::CAPTURE_START:{
            if(mPcapFd == NULL){
                char pcapPath[128];
                if(generatePcapFilePath(pcapPath, sizeof(pcapPath))){
                    mPcapFd = fopen(pcapPath, "ab+");
                    if(mPcapFd == NULL){
                        LOGE(TAG, "open pcap file failed. %s", pcapPath);
                    }else{
                        PcapHdr pcap_hdr;
                        pcap_hdr.magic_number = 0xa1b2c3d4;
                        pcap_hdr.version_major = 2;
                        pcap_hdr.version_minor = 4;
                        pcap_hdr.thiszone = 0;
                        pcap_hdr.sigfigs = 0;
                        pcap_hdr.snaplen = 64;
                        pcap_hdr.network = 101;
                        writeFile(&pcap_hdr, sizeof(PcapHdr));
                    }
                }
            }
            break;
        }

        case EVENT::CAPTURE_STOP:{
            if(mPcapFd != NULL){
                if(fclose(mPcapFd)){
                    LOGE(TAG, "close pcap file filed. %s(%d)", strerror(errno), errno);
                }

                mPcapFd = NULL;
            }
            break;
        }

        case EVENT::CAPTURE_PACKET:{
            
            Segment* segment = (Segment*)(event.data);
            uint32_t size = segment->size();
            if(mPcapFd != NULL){
                struct timespec ts;
                if (clock_gettime(CLOCK_REALTIME, &ts)){
                    LOGE(TAG, "clock_gettime error %d: %s", errno, strerror(errno));
                }

                uint32_t len = (size < PCAP_RECORD_SIZE ? size : PCAP_RECORD_SIZE);
                PcapRecHdr pcap_rec;

                pcap_rec.ts_sec = (uint32_t) ts.tv_sec;
                pcap_rec.ts_usec = (uint32_t) (ts.tv_nsec / 1000);
                pcap_rec.incl_len = (uint32_t) len;
                pcap_rec.orig_len = size;
                LOGR(TAG, "incl_len = %u, orig_len = %u", pcap_rec.incl_len, pcap_rec.orig_len);
                writeFile(&pcap_rec, sizeof(PcapRecHdr));
                writeFile(segment->begin(), len);
            }

            SAFE_DELETE(segment);

            break;
        }
        
    }

    return 0;
}
