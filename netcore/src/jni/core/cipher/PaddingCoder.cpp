#include "PaddingCoder.h"
#include "Cipher.h"
#include "Defines.h"
#include<cstdlib>
#include<ctime>


const char* PaddingCoder::TAG = PTAG("PaddingCoder");

int PaddingCoder::encode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size){
	if(len+3 >= size){
		LOGE(TAG,"encode failed, not enough space.");
		return 0;
	}

	uint16_t range = paddingRange(len);
	if(range > (size-len-2)){
		range = size-len-2;
	}

	srand((unsigned)time(NULL));
	uint16_t padding = range>0?((rand()%range)+1):0;
	out[0] = (padding>>8)&0xFF;
	out[1] = (padding)&0xFF;

	int t = padding + 2;
	for(int i=2;i<t;i++){
		out[i] = rand()%256;
	}

	memcpy(out+t, data, len);

	return len + t;
}


int PaddingCoder::decode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size){
	uint16_t padding = ((*data)<<8)|(*(data+1));
	int l = len - padding - 2;
	if(l < 0){
		LOGE(TAG,"decode failed, data not finished, padding=%u, len=%u", padding, len);
		return 0;
	}

	memcpy(out, data+padding+2, l);
	return l;
}

uint16_t PaddingCoder::paddingRange(uint32_t len){
	return len > MAX_SECTION_SIZE? 2:MAX_SECTION_SIZE-len;
}


uint32_t PaddingCoder::encodeLen(uint32_t len){
	return paddingRange(len) + len;
}


uint32_t PaddingCoder::decodeLen(uint32_t len){
	return len;
}