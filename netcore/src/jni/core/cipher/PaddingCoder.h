#ifndef PADDOING_CODER_H
#define PADDOING_CODER_H

#include "ICoder.h"

class PaddingCoder: public ICoder{


public:

	PaddingCoder(){};
	~PaddingCoder(){};

	int encode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size);
	int decode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size);
	uint32_t encodeLen(uint32_t len);
	uint32_t decodeLen(uint32_t len);

private:

	uint16_t paddingRange(uint32_t len);
	static const char* TAG;

};

#endif