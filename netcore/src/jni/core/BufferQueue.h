#ifndef BUFFER_QUEUE_H
#define BUFFER_QUEUE_H

#include "Defines.h"
#include <stddef.h>
#include <string.h>
#include <list>

class Segment{

public:

	Segment(size_t size);
	virtual ~Segment();

	size_t write(uint8_t* data,size_t len);
	void consume(size_t size);
	void reset();
	void flip();
	size_t remain();
	size_t capacity();
	size_t size();
	uint8_t* begin();
	void limit(size_t l);
	size_t limit();
	void mark();
	void resume();


private:

	uint8_t* mData;
	size_t mSize = 0;
	size_t mPos = 0;
	size_t mLimit = 0;
	size_t mMarkPos = 0;
	size_t mMarkLimit = 0;
};

class BufferSegment: public Segment{

public:
	BufferSegment(size_t size):Segment(size){}

	~BufferSegment(){};

	bool isBuffering(){return mBuffering;};
	void setBuffering(bool buffering){mBuffering = buffering;}
	bool releaseAfterBuffering(){return mReleaseAfterBuffering;};
	void setReleaseAfterBuffering(bool releaseAfterBuffering){mReleaseAfterBuffering = releaseAfterBuffering;}
	bool isAbort(){return mAbort;}
	void setAbort(bool abort){mAbort = abort;}
	void setNeedCapture(bool value) { mNeedCapture = value; }
	bool needCapture() { return mNeedCapture; }

private:
	bool mBuffering = false;
	bool mReleaseAfterBuffering = false;
	bool mAbort = false;
	bool mNeedCapture = false;
};

template<typename T = class BufferSegment>
class BufferQueue{

public:

	typedef std::list<T*> Queue;
	typedef typename Queue::iterator Iterator;
	typedef int (*CompareFunc)(T&,T&);

	int size(){
		return mQueue.size();
	}

	int push(T* segment){
		mQueue.push_back(segment); 
		return 0;
	}

	void pop(){
		if(!mQueue.empty()) {
			mQueue.pop_front();
		}
	}

	T* front(){
		return mQueue.empty()?NULL:mQueue.front();
	}

	//find the first item not smaller than segment;
	//if found then return the iterator point to that item;
	//otherwise return iterator point to end;
	Iterator findNotSmaller(T& segment, CompareFunc compare){
		for(Iterator it = mQueue.begin(); it != mQueue.end(); it++){
			int diff = compare(segment,**it);
			if(diff == 0 || diff < 0){
				return it;
			}
		}

		return mQueue.end();
	}

	Iterator insert(Iterator pos,T* value){
		return mQueue.insert(pos,value);
	}

	Iterator remove(Iterator s,Iterator t){
		return mQueue.erase(s,t);
	}

	Iterator remove(Iterator itr){
		return mQueue.erase(itr);
	}

	bool empty(){
		return mQueue.empty();
	}

	void clear(){
		while(!mQueue.empty()){
			T* segment = front();
			SAFE_DELETE(segment);
			pop();
		}
	}

	Iterator begin(){
		return mQueue.begin();
	}

	Iterator end(){
		return mQueue.end();
	}

private:
	
	Queue mQueue;
};

#endif