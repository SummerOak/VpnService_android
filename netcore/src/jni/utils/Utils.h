#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>
#include "IPAdpt.h"

namespace vpnlib{

	template <typename T>
	inline int compareArr(T a1[],int l1,T a2[],int l2);
	char* bytes2hex(uint8_t* data,char* out,size_t len);

	uint8_t char2hex(const char c);
	void hex2bytes(const char *hex, uint8_t *out);
	int compare_u32(uint32_t s1,uint32_t s2);
	void replaceChar(char* s, char a, char b);
	char* bytes2string(uint8_t* data, int len, char* out, size_t size);
	void logbytes2string(const char* tag,const char* desc, uint8_t* data, int len);

	uint32_t getSockBufferSize(int socket);

	int getUID(uint8_t version, uint8_t protocol, void *saddr,  uint16_t sport,  void *daddr, uint16_t dport);
	
}

#endif