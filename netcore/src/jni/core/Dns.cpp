#include <Dns.h>
#include <Log.h>
#include <iostream>
#include <sstream>

const char* Dns::TAG = "VpnLib.Dns";

// bool Dns::isDnsReq(IPHeader& ip){
//     uint16_t srcPort = getSrcPort(ip);
//     srcPort = SWAP_INT16(srcPort);
//     uint16_t destPort = getDestPort(ip);
//     destPort = SWAP_INT16(destPort);
//     int prot = getProtocol(ip);

//     if(prot == PROTO::UDP && destPort == 53){
        
//     }
// }

uint16_t Dns::getNameNotation(const uint8_t *dns, const size_t datalen, uint16_t off, char *out,size_t size) {
    LOGD(TAG,"get notation dataLen=%d, off=%d",datalen,off);

    *out = 0;

    uint16_t c = 0;
    uint8_t rlen = 0;
    uint16_t ptr = off;
    uint8_t loop = 10;
    uint8_t len = *(dns + ptr);
    while (len && loop-- > 0) {
        if (len & 0xC0) {
            ptr = (uint16_t) (((len&0x3F)<<8) | (uint8_t)(*(dns+ptr+1)));
            len = *(dns + ptr);
            LOGD(TAG, "ptr=%d len=%d", ptr, len);
            if (!c) {
                c = 1;
                off += 2;
            }
        } else if ((ptr+1+len) < datalen) {
            LOGD(TAG,"lable len=%d",len);
        	int tlen = (rlen+len)>=size?(size-rlen-1):len;
        	if(tlen > 0){
        		memcpy(out+rlen, dns+ptr+1, tlen);
            	*(out + rlen + tlen) = '.';
        	}else{
                LOGE(TAG,"truncation occured. len=%d ,size = %d",len,size);
            }

            rlen += (len + 1);
            ptr += (len + 1);
            len = *(dns + ptr);

            if(!c){
                off = ptr+1;
            }
        } else {
            LOGE(TAG,"notation over flow. dataLen=%d, ptr=%d,len=%d",datalen,ptr,len);
            break;
        }
    }

    if(loop <= 0){
        LOGE(TAG,"getNameNotation dead loop.");
        return off;
    }

    if (len > 0 || rlen == 0) {
        LOGE(TAG, "DNS qname invalid len %d rlen %d", len, rlen);
        return off;
    }

    ++ptr;

    *(out + rlen - 1) = 0;
    // LOGD(TAG, "get notation succ: %s , next=%d", out,c?off:ptr);

    return (c ? off : ptr);
}

char* Dns::toString(uint8_t* dns,uint16_t rdOff,size_t dataLen,char* out, int len, uint16_t* next){
    if(out!=NULL && len > 0){
        char name[255];
        stringstream ss;
        uint16_t off = getNameNotation(dns,dataLen,rdOff,name,sizeof(name));
        ss<<"name="<<name<<"#";

        uint16_t type = ntohs(*((uint16_t*)(dns+off)));
        uint16_t cls = ntohs(*((uint16_t*)(dns+off+2)));
        uint32_t ttl = ntohl(*((uint32_t*)(dns+off+4)));
        uint16_t rdlen = ntohs(*((uint16_t*)(dns+off+8)));
        LOGD(TAG,"offset = %d, type=%d cls=%d ttl=%d rdlen=%d",off,type,cls,ttl,rdlen);
        ss<<"type="<<type<<"#class="<<cls<<"#ttl="<<ttl<<"#rdlen="<<rdlen;

        if(cls == DNS_QCLASS_IN){
            if(type == DNS_QTYPE_A){
                ss<<"#ip="<<vpnlib::IPInt2Str(*((uint32_t*)(dns+off+10)), out, len);
            }
        }

        *next = off + 10 + rdlen;

        strncpy(out,ss.str().c_str(),len);
    }

    return out;
}

char* Dns::toString(DnsHeader& dnsHeader,int payloadLen,char* out,int len){

	if(out!=NULL && len > 0){
		stringstream ss;

		ss<<"dns[";

		ss<<"id("<<(dnsHeader.id)<<")";
		
		int qnum = SWAP_INT16(dnsHeader.qustNum);
        int anum = SWAP_INT16(dnsHeader.ansNum);
        int authNum = SWAP_INT16(dnsHeader.authNum);
        int addNum = SWAP_INT16(dnsHeader.addtNum);

		ss<<"qr("<<(dnsHeader.qr)<<")rd("<<(dnsHeader.rd)<<")tc("<<(dnsHeader.tc)<<")aa("<<(dnsHeader.aa)
		<<")qnum("<<(qnum)<<")anum("<<anum<<")authNum("<<(authNum)<<")addtNum("<<(addNum)<<");";

		uint16_t offset = sizeof(DnsHeader);
		uint16_t offset2;
		char name[255];
		
		for(int i=0;i<qnum;i++){
			offset2 = getNameNotation((uint8_t*)(&dnsHeader),payloadLen,offset,name,sizeof(name));
			if(offset2 != offset){
				uint16_t qtype = ntohs(*((uint16_t *) (((uint8_t*)&dnsHeader) + offset2)));
				uint16_t qclass = ntohs(*((uint16_t *) (((uint8_t*)&dnsHeader) + offset2+2)));
				ss<<"q"<<i<<":"<<"name="<<name<<""<<"#type="<<qtype<<"#class="<<qclass<<";";
                offset = offset2+4;

                continue;
			}

            LOGE(TAG,"getNameNotation failed");
            break;
		}

        if(anum > 0){
            LOGD(TAG,"parsing answers...");
            ss<<"  ";

            for(int i=0;i<anum;i++){
                offset2 = offset;
                ss<<"a"<<i<<":"<<toString((uint8_t*)(&dnsHeader), offset,payloadLen,out,len,&offset)<<"; ";

                if(offset == offset2){
                    LOGE(TAG,"parse ans failed.");
                    break;
                }
            }
        }

		ss<<"]";

		strncpy(out,ss.str().c_str(),len);
		out[len-1] = '\0';
	}

	return out;

}