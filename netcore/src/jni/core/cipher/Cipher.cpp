#include "Cipher.h"
#include "Defines.h"
#include "PaddingCoder.h"
#include "ShiftCoder.h"
#include "Utils.h"

Cipher* Cipher::sInstance = new Cipher();
const char* Cipher::TAG = PTAG("Cipher");

Cipher::Cipher(){
	memset(mCoders, NULL , sizeof(mCoders));
}

Cipher::~Cipher(){
	for(uint8_t i=0;i<CODER_TYPE_SIZE;i++){
		if(mCoders[i] != NULL){
			SAFE_DELETE(mCoders[i]);
		}
	}
}

int Cipher::encrypt(uint8_t** origin, uint32_t len, uint8_t* out, uint32_t size){
	if(origin == NULL || *origin == NULL || len <= 0 || out == NULL || size <= 0){
		return 0;
	}

	if(len > MAX_ENCRYPT_SIZE){
		len = MAX_ENCRYPT_SIZE;
	}

	if(len > CIPHER_BUFFER_SIZE){
		LOGE(TAG, "not enough space to encrypt");
		return 0;
	}

	uint32_t estl = estimateEncryptLen(len);
	if(estl > size){
		LOGE(TAG, "not enough space to encrypt, need %u, have %u", estl, size);
		return 0;
	}

	uint8_t *s, *t, *ss;
	uint32_t i, l;

	mBuffer[0] = 0;
	memcpy(mBuffer+1, *origin, len);
	*origin += len;

	s = mBuffer; t = mBuffer1;

	for(i=0, l = len+1; i<sizeof(mCodes); ++i){

		ICoder *c = getCoder(mCodes[i]);
		if(c == NULL){
			if(mCodes[i] != 0){
				LOGE(TAG,"coder is NULL %u", mCodes[i]);
			}
			
			continue;
		}

		t[0] = mCodes[i];
		l = c->encode(s, l, t+1, CIPHER_BUFFER_SIZE-1) + 1;

		if(l <= 1){
			LOGE(TAG,"encode failed %d", l);
			break;
		}

		ss = s; s = t; t = ss;
	}

	l = pack(&s, l, out, size);

	return l;
}

int Cipher::decrypt(uint8_t** data, uint32_t len, uint8_t* out, uint32_t size){
	if(data == NULL || *data == NULL || len <= 0 || out == NULL || size <= 0){
		LOGD(TAG, "decrypt>>> invalidate input arguments");
		return 0;
	}

	uint32_t estl = estimateDecryptLen(len);
	if(estl > size){
		LOGE(TAG,"not enough space to decrypt, need %u, have %u", estl, size);
		return 0;
	}

	uint8_t *s, *t, *ss;
	uint32_t i, l, rlen;
	uint8_t* p1 = *data;
	uint8_t* p0 = p1;

	rlen = len;
	i = 0;

	vpnlib::logbytes2string(TAG, "decrypt", *data, len);

	while(rlen > 0 && (l = unpack(&p1, rlen, mBuffer, CIPHER_BUFFER_SIZE)) > 0){
		rlen -= (p1-p0);
		p0 = p1;

		s = mBuffer;
		t = mBuffer1;

		vpnlib::logbytes2string(TAG, "parcel", s, l);

		while(true){
			if(l < 1){
				LOGD(TAG,"package length is 0");
				break;
			}

			ss = s;
			uint8_t cid = *s++; --l;
			if(cid == 0){
				break;
			}

			ICoder* coder = getCoder(cid);
			if(coder == NULL){
				LOGE(TAG,"invalidate code id: %u", cid);
				break;
			}

			l = coder->decode(s, l, t, CIPHER_BUFFER_SIZE);

			if(l <= 0){
				LOGE(TAG,"decode failed! %u", cid);
				return 0;
			}

			s = t; t = ss;
		}

		vpnlib::logbytes2string(TAG, "decrypt parcel", s, l);

		if(l > 0){
			if((i+l) < size){
				memcpy(out+i, s, l);
				i+=l;
			}else{
				LOGE(TAG, "not enough space to decrypt, need %u, remain %d", l, size - i);
				break;
			}
			
		}else{
			LOGE(TAG, "length is %u, after decrypt", l);
			break;
		}
		
	}

	*data = p1;

	return i;
}

ICoder* Cipher::getCoder(uint8_t id){
	if(0<id&&id<CODER_TYPE_SIZE){
		if(mCoders[id] == NULL){
			switch(id){
				case PADDING:{
					mCoders[id] = new PaddingCoder();
					break;
				}

				case SHIFT:{
					mCoders[id] = new ShiftCoder();
					break;
				}
			}
		}

		return mCoders[id];
	}
	
	return NULL;
}

uint32_t Cipher::estimateDecryptLen(uint32_t len){
	return len;
}


uint32_t Cipher::estimateEncryptLen(uint32_t len){
	++len;//cid = 0
	for(uint8_t i=0; i <sizeof(mCodes); ++i){
		ICoder *c = getCoder(mCodes[i]);
		if(c != NULL){
			len = c->encodeLen(len) + 1;
		}
	}

	return estimatePackLen(len);
}

uint32_t Cipher::estimatePackLen(uint32_t len){
	return len + (((len+MAX_SECTION_SIZE)/MAX_SECTION_SIZE) << 1) + 2;
}


int Cipher::pack(uint8_t** data, uint32_t len, uint8_t* out, uint32_t size){
	LOGD(TAG,"pack %u %u", len, size);
	if(data == NULL || (*data) == NULL || len <= 0 || out == NULL || size < len){
		return 0;
	}

	if(estimatePackLen(len) > size){
		LOGE(TAG,"not enough space to pack in");
		return 0;
	}

	uint32_t r = len;
	uint8_t* s = *data;
	uint8_t* t = out;
	uint16_t l;
	while(r > 0){
		l = r<MAX_SECTION_SIZE? r:MAX_SECTION_SIZE;
		*t++ = (l>>8)&0xFF;
		*t++ = (l)&0xFF;

		if(l > 0){
			memcpy(t, s, l);
			r -= l; s += l; t += l;
		}
	}

	if(l == MAX_SECTION_SIZE){
		*t++ = 0;
		*t++ = 0;
	}

	*data = s;

	return t-out;
}

int Cipher::unpack(uint8_t** data, uint32_t len, uint8_t* out, uint32_t size){
	if(data == NULL || *data == NULL || len <= 0 || out == NULL){
		LOGE(TAG, "unpack, wrong input arguments");
		return 0;
	}

	uint32_t r = len;
	uint32_t sizet = size;
	uint8_t* s = *data;
	uint8_t* t = out;
	uint16_t l = 0;
	while(r >= 2){

		l = (((*s)&0xFF)<<8)|((*(s+1))&0xFF); s += 2; r -= 2;

		if(l > MAX_SECTION_SIZE){
			LOGE(TAG, "section size exceeded %u, dis %u", l, (s-(*data)));
			vpnlib::logbytes2string(TAG	, "section", s-2, 10);
			return 0;
		}

		if(l > 0){
			if(l > r){
				LOGD(TAG,"section not finished. %u", l);
				return 0;
			}

			if(sizet < l){
				LOGE(TAG, "not enough space to unpack, need %u, remain %u", l, sizet);
				vpnlib::logbytes2string(TAG, "unpack", *data, len);
				return 0;
			}

			memcpy(t, s, l);
			r -= l; s += l; t += l; sizet -= l;
		}

		if(l != MAX_SECTION_SIZE){
			LOGD(TAG, "parcel end, last section %u", l);
			*data = s;
			return t-out;
		}

	}

	return 0;
}




