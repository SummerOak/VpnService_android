#ifndef CIPHER_H
#define CIPHER_H

#include <stdint.h>
#include "ICoder.h"


/**
*	provide a set of APIs to encrypt/decrypt data, NOT thread safe!
*/

#define MAX_SECTION_SIZE (1<<10)
#define MAX_ENCRYPT_SIZE ((MAX_SECTION_SIZE)<<2)
#define MAX_PARCEL_SIZE	((MAX_ENCRYPT_SIZE)<<1)
#define CIPHER_BUFFER_SIZE (MAX_PARCEL_SIZE)
#define CODER_TYPE_SIZE (3)

enum CODER_TYPE{
	PADDING = 1,
	SHIFT,
};


class Cipher{

public:

	static Cipher& get(){
		return *sInstance;
	}

	/**
	*	encrypt data *origin point at;
	*	return the length of out and the *origin will point to first unencrypted byte;
	*/
	int encrypt(uint8_t** origin, uint32_t len, uint8_t* out, uint32_t size);

	/**
	*	decrypt encrypted data *encrypted pointed to;
	*	return the length of out and *encrypted will point to the first undecrypted byte;
	*/
	int decrypt(uint8_t** encrypted, uint32_t len, uint8_t* out, uint32_t size);

	uint32_t estimateDecryptLen(uint32_t len);
	uint32_t estimateEncryptLen(uint32_t len);


private:

	Cipher();
	~Cipher();

	uint32_t estimatePackLen(uint32_t len);
	//pack data into a package, *data will point to first byte not packed in package
	//return the length of out
	int pack(uint8_t** data, uint32_t len, uint8_t* out, uint32_t size);
	//unpack a package from *data into a package, after that, *data will point to first byte not unpacked in package
	//return the length of out
	int unpack(uint8_t** data, uint32_t len, uint8_t* out, uint32_t size);

	ICoder* getCoder(uint8_t id);


	uint8_t mCodes[2] = {SHIFT, PADDING};
	ICoder* mCoders[CODER_TYPE_SIZE];

	uint8_t mBuffer[CIPHER_BUFFER_SIZE];
	uint8_t mBuffer1[CIPHER_BUFFER_SIZE];

	static Cipher* sInstance;
	static const char* TAG;
};

#endif