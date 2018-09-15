#include "BufferQueue.h"
#include "Log.h"


Segment::Segment(size_t size){
	if(size < 1 || size > (1<<19)){
		LOGE(PTAG("Segment"), "invalidate size %u", size);
		return;
	}

	mSize = size;
	mData = new uint8_t[mSize];
	memset(mData,0,mSize);
	mPos = mLimit = 0;
}

Segment::~Segment(){
	delete[] mData;
}

size_t Segment::capacity(){
	return mSize;
}

void Segment::consume(size_t size){
	if(mPos + size <= mLimit){
		mPos += size;
	}
	
	return;
}

void Segment::limit(size_t limit){
	if(limit <= mSize){
		mLimit = limit;
	}
	
	return;
}

size_t Segment::limit(){
	return mLimit;
}

uint8_t* Segment::begin(){
	return mData+mPos;
}

size_t Segment::write(uint8_t* data,size_t len){
	size_t l = (mLimit+len)>mSize?(mSize-mLimit):len;
	if(l > 0){
		memcpy(mData+mLimit,data,l);
		mLimit += l;
	}

	return l;
}

void Segment::flip(){
	if(mLimit > 0){
		memcpy(mData, mData+mPos, mLimit-mPos);
		mLimit = mLimit-mPos;
		mPos = 0;
	}
}

void Segment::reset(){
	mMarkLimit = mMarkPos = mPos = mLimit = 0;
}

void Segment::mark(){
	mMarkPos = mPos;
	mMarkLimit = mLimit;
}

void Segment::resume(){
	mPos = mMarkPos;
	mLimit = mMarkLimit;
}

size_t Segment::remain(){
	return mSize - mLimit;
}

size_t Segment::size(){
	return mLimit-mPos;
}