#include "ShiftCoder.h"
#include<cstdlib>
#include<ctime>
#include "Defines.h"

const char* ShiftCoder::TAG = PTAG("ShiftCoder");

int ShiftCoder::encode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size){
	if(data == NULL || out == NULL){
		LOGE(TAG, "invalidate arguments.");
		return 0;
	}

	srand((unsigned)time(NULL));
	uint8_t shift = rand()%8;

	if(size <= len){
		LOGE(TAG, "encode failed, not enough space.");
		return 0;
	}

	out[0] = shift;
	int j = 1;

	for(int i=0; i<len; ++i, ++j){
		
		out[j] = (data[i]<<shift)|(data[i]>>(8-shift));
		
		if(++shift > 7){
			shift = 0;
		}
	}

	return len+1;

}


int ShiftCoder::decode(uint8_t* data, uint32_t len, uint8_t* out, uint32_t size){
	if(data == NULL || out == NULL){
		LOGE(TAG, "invalidate arguments.");
		return 0;
	}

	uint8_t shift = data[0];
	if(shift > 7){
		LOGE(TAG,"invalidate shift value.");
		return 0;
	}

	int j = 0;
	for(int i=1;i<len;++i,++j){
		out[j] = (data[i]>>shift)|(data[i]<<(8-shift));
		if(++shift > 7){
			shift = 0;
		}
	}

	return len-1;
}


uint32_t ShiftCoder::encodeLen(uint32_t len){
	return len+1;
}


uint32_t ShiftCoder::decodeLen(uint32_t len){
	return len -1;
}