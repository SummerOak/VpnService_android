#ifndef DNS_H
#define DNS_H

#include <endian.h>
#include <Router.h>

#define DNS_QCLASS_IN 1
#define DNS_QTYPE_A 1 // IPv4
#define DNS_QTYPE_AAAA 28 // IPv6

#define DNS_QNAME_MAX 255
#define DNS_TTL (10 * 60) // seconds

typedef struct DnsHeader{
	uint16_t id; // identification number
# if __BYTE_ORDER == __LITTLE_ENDIAN
    uint16_t rd :1; // recursion desired
    uint16_t tc :1; // truncated message
    uint16_t aa :1; // authoritive answer
    uint16_t opcode :4; // purpose of message
    uint16_t qr :1; // query/response flag
    uint16_t rcode :4; // response code
    uint16_t cd :1; // checking disabled
    uint16_t ad :1; // authenticated data
    uint16_t z :1; // its z! reserved
    uint16_t ra :1; // recursion available
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint16_t qr :1; // query/response flag
    uint16_t opcode :4; // purpose of message
    uint16_t aa :1; // authoritive answer
    uint16_t tc :1; // truncated message
    uint16_t rd :1; // recursion desired
    uint16_t ra :1; // recursion available
    uint16_t z :1; // its z! reserved
    uint16_t ad :1; // authenticated data
    uint16_t cd :1; // checking disabled
    uint16_t rcode :4; // response code
# else
# error "Adjust your <bits/endian.h> defines"
#endif
    uint16_t qustNum; // number of question entries
    uint16_t ansNum; // number of answer entries
    uint16_t authNum; // number of authority entries
    uint16_t addtNum; // number of resource entries

}DnsHeader;

#define DNS_QCLASS_IN 1
#define DNS_QTYPE_A 1 // IPv4
#define DNS_QTYPE_AAAA 28 // IPv6


class Dns{

public:
    // static bool isDnsReq(IPHeader& ip);
    
	static char* toString(DnsHeader& dnsHeader,int payloadLen,char* out,int len);
    

private:

    static char* toString(uint8_t* dns, uint16_t rdOff, size_t dataLen,char* out, int len, uint16_t* next);
    static uint16_t getNameNotation(const uint8_t *dns, const size_t datalen, uint16_t off, char *out,size_t size);

	static const char* TAG;

};

#endif