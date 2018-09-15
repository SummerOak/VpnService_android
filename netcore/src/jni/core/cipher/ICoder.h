#ifndef ICODER_H
#define ICODER_H

#include <stdint.h>

class ICoder{


public:
	virtual ~ICoder(){};
	virtual int encode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size) = 0;
	virtual int decode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size) = 0;
	virtual uint32_t encodeLen(uint32_t len) = 0;
	virtual uint32_t decodeLen(uint32_t len) = 0;


};

#endif