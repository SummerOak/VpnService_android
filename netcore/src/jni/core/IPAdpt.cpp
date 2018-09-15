#include "IPAdpt.h"
#include "Defines.h"
#include "Dns.h"
#include "Log.h"
#include "Utils.h"
#include <sstream>
#include <arpa/inet.h>

uint8_t vpnlib::getIPVer(IPHeader& h){
	return h.VER_IHL>>4;
}

uint16_t vpnlib::getTTLEN(IPHeader& h){
	return SWAP_INT16(h.TTLEN);
}

uint8_t vpnlib::getProtocol(IPHeader& h){
	return h.PTOL;
}

uint16_t vpnlib::getChecksum(IPHeader& h){
	return h.CHECKSUM;
}

uint32_t vpnlib::getSrc(IPHeader& h){
	return h.SRC;
}

uint32_t vpnlib::getDest(IPHeader& h){
	return h.DEST;
}

uint32_t vpnlib::getHeaderLen(IPHeader& h){
	return (h.VER_IHL&0x0F)<<2;
}

uint8_t vpnlib::getTCPHeaderLen(TCPHeader& h){
	return (h.dataoff)<<2;
}

int vpnlib::getPayloadOffset(IPHeader& ip){
	uint8_t p = getProtocol(ip);
	if(p == PROTO::UDP){
		return getHeaderLen(ip)+8;
	}else if(p == PROTO::TCP){
		TCPHeader* tcpHeader = (TCPHeader*)(((uint8_t*)&ip)+getHeaderLen(ip));
		return getHeaderLen(ip)+getTCPHeaderLen(*tcpHeader);
	}
	
	return 0;
}

int vpnlib::getPayloadLen(IPHeader& ip){
	return getTTLEN(ip)-getPayloadOffset(ip);
}

uint16_t vpnlib::getSrcPort(IPHeader& ip){
	uint8_t p = getProtocol(ip);
	if(p == PROTO::UDP){
		UDPHeader* udpHeader = (UDPHeader*)(((uint8_t*)&ip)+getHeaderLen(ip));
		return udpHeader->src;
	}else if(p == PROTO::TCP){
		TCPHeader* tcpHeader = (TCPHeader*)(((uint8_t*)&ip)+getHeaderLen(ip));
		return tcpHeader->src;
	}
	
	return 0;
}

uint16_t vpnlib::getDestPort(IPHeader& ip){
	uint8_t p = getProtocol(ip);
	if(p == PROTO::UDP){
		UDPHeader* udpHeader = (UDPHeader*)(((uint8_t*)&ip)+getHeaderLen(ip));
		return udpHeader->dest;
	}else if(p == PROTO::TCP){
		TCPHeader* tcpHeader = (TCPHeader*)(((uint8_t*)&ip)+getHeaderLen(ip));
		return tcpHeader->dest;
	}
	
	return 0;
}

uint8_t* vpnlib::getOptionAddr(TCPHeader& tcp,uint8_t option){
	int optlen = (tcp.dataoff-5)<< 2;
    uint8_t* options = ((uint8_t *)&tcp) + sizeof(TCPHeader);
    while (optlen > 0) {
        uint8_t kind = *options;
        if(option == kind){
    		return options;
    	}

        if (kind == 0){
            break;
        }

        uint8_t len = *(options+1);
        if (kind == TCP_OPT::NOP) {
            optlen--;
            options++;
        } else {
            optlen -= len;
            options += len;
        }
    }

    return NULL;
}

uint16_t vpnlib::computeChecksum(uint16_t start, const uint8_t *buffer, size_t length) {
    uint32_t sum = start;
    uint16_t *buf = (uint16_t *) buffer;
    size_t len = length;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len > 0){
        sum += *((uint8_t *) buf);
    }

    while (sum >> 16){
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t) sum;
}

char* vpnlib::toString(IPHeader& ip,char* out,size_t len){
	if(out!=NULL && len > 0){
		std::stringstream ss;

		ss<<"[";

		ss<<"v"<<(int)(getIPVer(ip))<<";";
		
		int prot = getProtocol(ip);
		ss<<(prot==PROTO::TCP?"TCP":(prot==PROTO::UDP?"UDP":""))<<"("<<prot<<")"<<";";

		char* temp = new char[16];
		char* szSrcAddr = IPInt2Str(getSrc(ip), temp,16);
		uint16_t srcPort = getSrcPort(ip);
		srcPort = SWAP_INT16(srcPort);
		ss<<szSrcAddr<<"/"<<srcPort<<">";
		char* szDestAddr = IPInt2Str(getDest(ip),temp,16);
		uint16_t destPort = getDestPort(ip);
		destPort = SWAP_INT16(destPort);
		ss<<szDestAddr<<"/"<<destPort<<";";
		delete[] temp;

		ss<<"hl("<<getHeaderLen(ip)<<")tl("<<getTTLEN(ip)<<");";

		if(prot == PROTO::UDP){
			UDPHeader* udpHeader = (UDPHeader*)(((uint8_t*)&ip) + vpnlib::getHeaderLen(ip));
			if(destPort == 53 || srcPort == 53){
				DnsHeader* dnsHeader = (DnsHeader*)(((uint8_t*)udpHeader)+8);
				ss<<"dns: "<<Dns::toString(*dnsHeader,vpnlib::getTTLEN(ip)-getPayloadOffset(ip),out,len);
			}
		}else if(prot == PROTO::TCP){

			TCPHeader* tcpHeader = (TCPHeader*)(((uint8_t*)&ip) + vpnlib::getHeaderLen(ip));
			uint16_t tcpLen = (tcpHeader->dataoff)<<2;
			ss<<(tcpHeader->syn?"SYN|":"")<<(tcpHeader->fin?"FIN|":"")<<(tcpHeader->ack?"ACK|":"")<<(tcpHeader->rst?"RST|":"")<<(tcpHeader->psh?"PSH|":"");
			ss<<"; seq("<<ntohl(tcpHeader->seq)<<")"<<"ack("<<ntohl(tcpHeader->ack_seq)<<")"<<"tcpl("<<tcpLen<<")";
			ss<<"; win("<<ntohs(tcpHeader->window)<<") ";
			ss<<"raw["<<bytes2hex((uint8_t*)tcpHeader,out,tcpLen>len?len:tcpLen)<<"],";

			uint8_t* pmss = getOptionAddr(*tcpHeader,TCP_OPT::MSS);
			if(pmss != NULL){
				ss<<" mss("<<*((uint16_t*)(pmss+2))<<")";
			}

			uint8_t* pws = getOptionAddr(*tcpHeader,TCP_OPT::WS);
			if(pws != NULL){
				uint8_t ws = *(pws+2);
				ss<<" ws("<<((uint16_t)ws)<<")";
			}

			uint8_t* psackf = getOptionAddr(*tcpHeader,TCP_OPT::SACKF);
			if(psackf != NULL){
				ss<<" sack, ";
			}

			uint8_t* psack = getOptionAddr(*tcpHeader,TCP_OPT::SACK);
			if(psack != NULL){
				ss<<" with sacks ";
			}
		}

		ss<<"]";
		strncpy(out,ss.str().c_str(),len-1);
		out[len-1] = '\0';
	}

	return out;
}

const char* vpnlib::tcpstate2cstr(TCP_STATE& state){
	switch(state){
		case LISTEN:return "LISTEN";
		case SYN_SENT: return "SYN_SENT";
		case SYN_RCVED: return "SYN_RCVED";
		case ESTABLISHED: return "ESTABLISHED";
		case FIN_WAIT1: return "FIN_WAIT1";
		case FIN_WAIT2: return "FIN_WAIT2";
		case CLOSE_WAIT: return "CLOSE_WAIT";
		case CLOSING: return "CLOSING";
		case LAST_ACK: return "LAST_ACK";
		case TIME_WAIT: return "TIME_WAIT";
		case CLOSED:return "CLOSED";
	}
	return "";
}

char* vpnlib::IPInt2Str(uint32_t addr,char* out,size_t len){
	return (char*)inet_ntop(AF_INET, (void*)&addr, out, len);
}

uint32_t vpnlib::IPStr2Int(const char* ipv4){
	return inet_addr(ipv4);
}

char* vpnlib::sockAddr2Str(uint8_t ipVer, struct sockaddr& sockAddr,char* out,size_t len){
	if(out!=NULL && len > 0){
		std::stringstream ss;

		if(ipVer == 4){
			struct sockaddr_in* addr4 = (struct sockaddr_in*)&sockAddr;
			uint16_t port = SWAP_INT16(addr4->sin_port);
			ss<<IPInt2Str(addr4->sin_addr.s_addr,out,len)<<"/"<<port;
		}else{
			LOGE("VpnLib.IPAdpt","ip ver not support %d",ipVer);
		}
		
		strncpy(out,ss.str().c_str(),len);
		out[len-1] = '\0';
	}

	return out;
}


