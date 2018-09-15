#ifndef SHIFT_CODER_H
#define SHIFT_CODER_H

#include "ICoder.h"

class ShiftCoder: public ICoder{


public:

	ShiftCoder(){};
	~ShiftCoder(){};

	int encode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size);
	int decode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size);

	uint32_t encodeLen(uint32_t len);
	uint32_t decodeLen(uint32_t len);


private:
	static const char* TAG;

};

#endif