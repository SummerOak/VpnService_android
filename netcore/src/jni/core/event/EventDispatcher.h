#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <memory>
#include <list>
#include <algorithm>
#include <condition_variable>
#include "Log.h"
#include "Result.h"
#include "Event.h"
#include "Defines.h"

template<typename T,typename R>
class EventHandler;

template<typename E,typename R>
class EventDispatcher{
public:

	typedef EventHandler<E,R> tp_handler;
	typedef std::list<tp_handler*> tp_handlers;
	typedef std::map<int,tp_handlers*> tp_mhandlers;

	static EventDispatcher<E,R>& get(){return *sDispatcher;}

	EventDispatcher(){
		mWork = true;
		if(pthread_create(&mThread, NULL, &EventDispatcher<E,R>::msgThread, this) != 0){
			LOGE(TAG,"create msg thread failed.");
		}
	};

	~EventDispatcher(){
		mWork = false;
	};

	int dispatch(E* event);
	int dispatchSync(E* event, R* result = NULL);
	int registerEvent(int event, tp_handler* handler);
	int unregisterEvent(int event, tp_handler* handler);

	void stop();
	

private:

	static EventDispatcher<E,R>* sDispatcher;
	static const char* TAG;

	static void* msgThread(void* arg);
	void loop();
	
	tp_handlers* getHandlers(int event);
	E* obtainEvent();

	std::mutex mMutexQueue;
	std::condition_variable mCond;
	std::queue<E*> mQueue;

	std::recursive_mutex mMutexHandlers; 
	
	tp_mhandlers mHandlers;

	bool mWork;
	pthread_t mThread;

	
};

typedef EventDispatcher<Event,Result> dispatcher;

template<typename E,typename R>
EventDispatcher<E,R>* EventDispatcher<E,R>::sDispatcher = new EventDispatcher<E,R>();
template<typename E,typename R>
const char* EventDispatcher<E,R>::TAG = PTAG("EventDispatcher");


template<typename T,typename R>
class EventHandler{
public:
	virtual int onSyncEvent(T& event,R* result = NULL) = 0;
	virtual int onEvent(T& event) = 0;
};

template<typename E,typename R>
int EventDispatcher<E,R>::registerEvent(int eventId,tp_handler* handler){
	if(handler == NULL){
		LOGE(TAG,"handler is NULL");
		return 1;
	}

	std::unique_lock<std::recursive_mutex> lck(mMutexHandlers);
	tp_handlers* handlers = getHandlers(eventId);
	if(handlers == NULL){
		handlers = new tp_handlers();
		mHandlers.insert(pair<int,tp_handlers*>(eventId,handlers));
	}

	LOGD(TAG,"handlers is %d ", handlers);

	typename tp_handlers::iterator itr = find(handlers->begin(),handlers->end(),handler);
	if(itr == handlers->end()){
		handlers->push_back(handler);
	}

	return 0;
}

template<typename E,typename R>
int EventDispatcher<E,R>::unregisterEvent(int eventId,tp_handler* handler){
	if(handler == NULL){
		LOGE(TAG,"handler is NULL");
		return 1;
	}

	std::unique_lock<std::recursive_mutex> lck(mMutexHandlers);
	tp_handlers* handlers = getHandlers(eventId);
	if(handlers != NULL && !handlers->empty()){
		typename tp_handlers::iterator itr = find(handlers->begin(),handlers->end(),handler);
		if(itr != handlers->end()){
			handlers->erase(itr);
		}
	}

	return 0;
}


template<typename E,typename R>
int EventDispatcher<E,R>::dispatch(E* event){
	LOGD(TAG,"dispatch %d, event(%p), data(%p)",((Event*)event)->id, event, ((Event*)event)->data);
	if(event == NULL){
		LOGE(TAG,"event is NULL");
		return 1;
	}

	std::unique_lock<std::mutex> lck(mMutexQueue);
	mQueue.push(event);
	mCond.notify_one();
	return 0;
}

template<typename E,typename R>
int EventDispatcher<E,R>::dispatchSync(E* event,R* result){
	LOGD(TAG,"dispatchSync %d(event(%p), data(%p), result(%p))",((Event*)event)->id, event, ((Event*)event)->data, result);
	if(event == NULL){
		LOGE(TAG,"event is NULL");
		return 1;
	}

	std::unique_lock<std::recursive_mutex> lck(mMutexHandlers);
	tp_handlers* handlers = getHandlers(((Event*)event)->id);
	if(handlers != NULL && !handlers->empty()){
		for(typename tp_handlers::const_iterator iter = handlers->begin(); iter != handlers->end(); iter++){
			EventHandler<E,R>& h = **iter;
			h.onSyncEvent(*event,result);
		}
	}

	SAFE_DELETE(event);

	return 0;
}

template<typename E,typename R>
typename EventDispatcher<E,R>::tp_handlers* EventDispatcher<E,R>::getHandlers(int eventId){
	std::unique_lock<std::recursive_mutex> lck(mMutexHandlers);
	typename tp_mhandlers::iterator itr = mHandlers.find(eventId);
	if(itr != mHandlers.end()){
		return (itr->second);
	}

	return NULL;
}

template<typename E,typename R>
E* EventDispatcher<E,R>::obtainEvent(){
	E* ret = NULL;
	std::unique_lock<std::mutex> lck(mMutexQueue);
	while(mQueue.empty()){
		mCond.wait(lck);
	}

	if(!mQueue.empty()){
		ret = mQueue.front();
		mQueue.pop();
	}

	return ret;
}

template<typename E, typename R>
void* EventDispatcher<E,R>::msgThread(void* arg){
	((EventDispatcher<E,R>*)arg)->loop();
	return NULL;
}

template<typename E,typename R>
void EventDispatcher<E,R>::loop(){
	LOGD(TAG,"start loop %d",mWork);
	while(mWork){

		E* event = obtainEvent();
		LOGD(TAG,"obtainEvent %p",event);
		if(event != NULL){
			LOGD(TAG,"dispatch event: %d, event(%p), data(%p)",((Event*)event)->id, event, ((Event*)event)->data);
			tp_handlers* handlers = getHandlers(((Event*)event)->id);
			if(handlers != NULL && !handlers->empty()){
				for(typename tp_handlers::const_iterator iter = handlers->begin(); iter != handlers->end(); iter++){
					EventHandler<E,R>& h = **iter;
					h.onEvent(*event);
				}
			}

			SAFE_DELETE(event);
		}

	}

}

template<typename E,typename R>
void EventDispatcher<E,R>::stop(){
	mWork = false;
	pthread_join(mThread);
}

#endif