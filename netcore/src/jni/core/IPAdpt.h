#ifndef IPADPT_H
#define IPADPT_H

#include <netinet/in.h>
#include <netinet/in6.h>

namespace vpnlib{

	enum CTRL_TYPE{
		NONE = 1<<0,
		PROXY = 1<<1,
		BLOCK = 1<<2,
		CAPTURE = 1<<3,
	};

	typedef struct IPHeader{
		uint8_t VER_IHL;
		uint8_t SERVICE;
		uint16_t TTLEN;
		uint16_t ID;
		uint16_t FRAGMENT;
		uint8_t TTL;
		uint8_t PTOL;
		uint16_t CHECKSUM;
		uint32_t SRC;
		uint32_t DEST;
	
	}IPHeader;

	typedef struct UDPHeader{
		uint16_t src;
		uint16_t dest;
		uint16_t len;
		uint16_t checksum;
	} UDPHeader;

	typedef struct TCPHeader{
		uint16_t src;
		uint16_t dest;
		uint32_t seq;
		uint32_t ack_seq;
		


	# if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t ns :1;
		uint8_t reserve :3;
		uint8_t dataoff :4;

		uint8_t fin :1;
	    uint8_t syn :1;
	    uint8_t rst :1;
	    uint8_t psh :1;
	    uint8_t ack :1;
	    uint8_t urg :1;
	    uint8_t ece :1;
	    uint8_t cwr :1;

	#elif __BYTE_ORDER == __BIG_ENDIAN
	    uint8_t dataoff :4;
	    uint8_t reserve :3;
	    uint8_t ns :1;
		
	    uint8_t cwr :1;
	    uint8_t ece :1;
	    uint8_t urg :1;
	    uint8_t ack :1;
	    uint8_t psh :1;
	    uint8_t rst :1;
	    uint8_t syn :1;
	    uint8_t fin :1;
	# else
	# error "not found marco __BYTE_ORDER"
	#endif

	    uint16_t window;
	    uint16_t checksum;
	    uint16_t upointer;

	} TCPHeader;

	# if __BYTE_ORDER == __LITTLE_ENDIAN
		#define CWR (1<<7)
		#define ECE (1<<6)
		#define URG (1<<5)
		#define ACK (1<<4)
		#define PSH (1<<3)
		#define RST (1<<2)
		#define SYN (1<<1)
		#define FIN (1<<0)

	#elif __BYTE_ORDER == __BIG_ENDIAN
		#define CWR (1<<0)
		#define ECE (1<<1)
		#define URG (1<<2)
		#define ACK (1<<3)
		#define PSH (1<<4)
		#define RST (1<<5)
		#define SYN (1<<6)
		#define FIN (1<<7)
	# else
	# error "not found marco __BYTE_ORDER"
	#endif

	typedef enum TCP_STATE{
		LISTEN,
		SYN_SENT,
		SYN_RCVED,
		ESTABLISHED,
		FIN_WAIT1,
		FIN_WAIT2,
		CLOSE_WAIT,
		CLOSING,
		LAST_ACK,
		TIME_WAIT,
		CLOSED,
	}TCP_STATE;

	typedef enum TCP_OPT{
		END 	= 0,
		NOP		= 1,
		MSS 	= 2,
		WS 		= 3,
		SACKF 	= 4,
		SACK	= 5,
		STAMP	= 8,
	}TCP_OPT;

	typedef enum PROTO{
		TCP = 6,
		UDP = 17,
	}PROTO;

	typedef union Addr{
		uint32_t ip4;
		uint8_t ip6[16];
	}Addr;

	uint8_t getIPVer(IPHeader& h);
	uint16_t getTTLEN(IPHeader& h);
	uint8_t getProtocol(IPHeader& h);
	uint16_t getChecksum(IPHeader& h);
	uint32_t getSrc(IPHeader& h);
	uint32_t getDest(IPHeader& h);
	uint32_t getHeaderLen(IPHeader& h);
	uint8_t getTCPHeaderLen(TCPHeader& h);
	char* tcpHeader2Str(TCPHeader& tcpHeader,char* out, size_t len);
	int getPayloadOffset(IPHeader& ip);
	int getPayloadLen(IPHeader& ip);
	uint16_t getSrcPort(IPHeader& ip);
	uint16_t getDestPort(IPHeader& ip);

	uint8_t* getOptionAddr(TCPHeader& tcp,uint8_t option);
	const char* tcpstate2cstr(TCP_STATE& state);

	uint16_t computeChecksum(uint16_t start, const uint8_t *buffer, size_t len);
	char* toString(IPHeader& h,char* out,size_t len);
	char* IPInt2Str(uint32_t addr,char* out,size_t len);
	uint32_t IPStr2Int(const char* ipv4);
	char* sockAddr2Str(uint8_t ipVer, struct sockaddr& sockAddr,char* out,size_t len);
};

#endif