#include "Utils.h"
#include <string.h>
#include "IPAdpt.h"
#include "Dns.h"
#include "Defines.h"
#include <sys/socket.h>
#include <sys/ioctl.h>

template <typename T>
inline int vpnlib::compareArr(T a1[],int l1,T a2[],int l2){
	for(int i=0;i<l1&&i<l2;i++){
		if(a1[i] > a2[i]){
			return 1;
		}else if(a1[i] < a2[i]){
			return -1;
		}
	}

	return l1>l2?1:(l1<l2?-1:0);
}

char* vpnlib::bytes2string(uint8_t* data, int len, char* out, size_t size){
    int j = 0;
    for(int i=0; i<len && j < size;++i, j+=4){
        sprintf(out+j, "%03u|",data[i]);
    }

    out[j] = 0;

    return out;
}

void vpnlib::logbytes2string(const char* tag,const char* desc, uint8_t* data, int len){
    int logLen = len>10?10:len;

    char* sdata = new char[logLen<<2];
    LOGD(tag, "%s %d bytes: %s", desc, len, bytes2string(data, logLen, sdata, logLen<<2));

    delete[] sdata;
}

char* vpnlib::bytes2hex(uint8_t* data,char* out,size_t len){

	for(int i=0;i<len;i++){
		sprintf(&out[i<<1], "%02X", data[i]);
	}

	out[len<<1] = '\0';
	return out;
}

uint8_t vpnlib::char2hex(const char c) {
    if (c >= '0' && c <= '9') return (uint8_t) (c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t) ((c - 'a') + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t) ((c - 'A') + 10);
    return 255;
}

void vpnlib::hex2bytes(const char *hex, uint8_t *out) {
    size_t len = strlen(hex);
    for (int i = 0; i<len; i+=2){
        out[i>>1] = (char2hex(hex[i]) << 4) | char2hex(hex[i+1]);
    }
    return;
}

int vpnlib::compare_u32(uint32_t s1, uint32_t s2) {
    // https://tools.ietf.org/html/rfc1982
    if (s1 == s2)
        return 0;

    int i1 = s1;
    int i2 = s2;
    if ((i1 < i2 && i2 - i1 < 0x7FFFFFFF) ||
        (i1 > i2 && i1 - i2 > 0x7FFFFFFF))
        return -1;
    else
        return 1;
}

void vpnlib::replaceChar(char* s, char a, char b){
    while(*s){
        if(*s == a){
            *s = b;
        }

        ++s;
    }
}

uint32_t vpnlib::getSockBufferSize(int socket){
    const char* TAG = PTAG("getSockBufferSize");
    if(socket < 0){
        LOGD(TAG,"sock < 0");
        return 0;
    }

    int bufferSize = 0;
    int sz = sizeof(bufferSize);
    if (getsockopt(socket, SOL_SOCKET, SO_SNDBUF, &bufferSize, (socklen_t *) &sz) < 0){
        LOGE(TAG, "getsockopt SO_RCVBUF failed %d: %s", errno, strerror(errno));
    }

    int used = 0;
    if (ioctl(socket, SIOCOUTQ, &used)){
        LOGE(TAG, "ioctl SIOCOUTQ %d: %s", errno, strerror(errno));
    }

    // LOGD(TAG,"sock buffer size = %d, used = %d", bufferSize,used);

    return bufferSize - used;
}

int vpnlib::getUID(uint8_t version, uint8_t protocol, void *saddr,  uint16_t sport,  void *daddr, uint16_t dport){
	const char* TAG = PTAG("getUID");
	int ret = -1;

	char source[16 + 1];
    char dest[16 + 1];
    source[16] = 0;
    dest[16] = 0;
    vpnlib::IPInt2Str(*((uint32_t*)saddr),source,sizeof(source));
    vpnlib::IPInt2Str(*((uint32_t*)daddr),dest,sizeof(dest));

    LOGD(TAG,"retrive uid: version(%u), protocol(%u), saddr(%s/%u),daddr(%s/%u)",version,protocol,source,sport,dest,dport);

    const char* fpath;
    if (protocol == IPPROTO_ICMP && version == 4){
        fpath = "/proc/net/icmp";
    } else if (protocol == IPPROTO_ICMPV6 && version == 6){
        fpath = "/proc/net/icmp6";
    } else if (protocol == IPPROTO_TCP){
        fpath = (version == 4 ? "/proc/net/tcp" : "/proc/net/tcp6");
    } else if (protocol == IPPROTO_UDP){
        fpath = (version == 4 ? "/proc/net/udp" : "/proc/net/udp6");
    } else{
    	LOGD(TAG,"ip version not support %d",version);
    	return -1;
    }

    FILE *fd = fopen(fpath, "r");
    if (fd == NULL) {
        LOGE(TAG, "fopen %s error %d: %s", fpath, errno, strerror(errno));
        return -2;
    }

    char line[250];
    *line = 0;
    int fields;

    char shex[16 * 2 + 1];
    int tsport;
    char dhex[16 * 2 + 1];
    int tdport;
    int uid = -1;
    uint8_t _saddr[16];
    uint8_t _daddr[16];

	int l = 0;
    
    size_t addrlen = (version==4?4:16);
    const char *fmt = (version == 4
                       ? "%*d: %8s:%X %8s:%X %*X %*lX:%*lX %*X:%*X %*X %d %*d %*ld"
                       : "%*d: %32s:%X %32s:%X %*X %*lX:%*lX %*X:%*X %*X %d %*d %*ld");

    static uint8_t zero[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // char t1[33];
    // char t2[33];

    while (fgets(line, sizeof(line), fd) != NULL) {
        if (!l++){// skip the first line(table header)
            continue;
        }

        // LOGD(TAG,"line %d: %s",l,line);

        fields = sscanf(line, fmt, shex, &tsport, dhex, &tdport, &uid);
        // if(protocol == IPPROTO_TCP){
        // 	LOGD(TAG,"parse: fields(%d), shex(%s), dhex(%s), sport(%u), dport(%u), uid(%d)",fields,shex,dhex,tsport,tdport,uid);
        // }
		

        if (fields == 5 && strlen(shex) == (addrlen<<1) && strlen(dhex) == (addrlen<<1)) {
            vpnlib::hex2bytes(shex, _saddr);
            vpnlib::hex2bytes(dhex, _daddr);

         //    if(protocol == IPPROTO_TCP){
	        //     LOGD(TAG,"tsport(%d), tdport(%d), saddr1(%s), saddr2(%s)", tsport, tdport,
	        //     	vpnlib::bytes2hex(_saddr,t1,addrlen), vpnlib::bytes2hex((uint8_t*)saddr,t2,addrlen));
	        // }

            if ((tsport == sport) &&
                (tdport == dport) &&
                (memcmp(_saddr, saddr, addrlen) == 0 ||
                 memcmp(_saddr, zero, addrlen) == 0 ||
                 (addrlen == 16 && memcmp(_saddr+12,(uint8_t*)saddr+12,4) == 0 && memcmp(_saddr,zero,8) == 0)) && 
                (memcmp(_daddr, daddr, addrlen) == 0 ||
                 memcmp(_daddr, zero, addrlen) == 0 ||
                 (addrlen == 16 && memcmp(_daddr+12,(uint8_t*)daddr+12,4) == 0 && memcmp(_daddr,zero,8) == 0))
                ){

            	ret = uid;
            	LOGD(TAG,"uid find: %d",uid);
            	break;
            }

        } else {
        	LOGE(TAG, "Invalid field #%d: %s", fields, line);
            break;
        }
	}

	if(version == 4 && ret < 0){
		LOGD(TAG,"try ipv6...");
		uint8_t saddr128[16];
        memset(saddr128, 0, 10);
        saddr128[10] = (uint8_t) 0xFF;
        saddr128[11] = (uint8_t) 0xFF;
        memcpy(saddr128 + 12, saddr, 4);

        uint8_t daddr128[16];
        memset(daddr128, 0, 10);
        daddr128[10] = (uint8_t) 0xFF;
        daddr128[11] = (uint8_t) 0xFF;
        memcpy(daddr128 + 12, daddr, 4);

        ret = getUID(6, protocol, saddr128, sport, daddr128, dport);
        if(ret >= 0){
        	LOGD(TAG,"try ipv6 success");
        }
	}else if(ret < 0){
		LOGE(TAG, "get uid failed.");
	}

	if(fclose(fd)){
		LOGE(TAG, "close file(%s) error, %s(%d)",fpath,strerror(errno),errno);
	}

	return ret;
}
