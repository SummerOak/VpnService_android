#ifndef TrafficMgr_h
#define TrafficMgr_h

#include "EventDispatcher.h"



class TrafficMgr: public EventHandler<Event,Result>{

public:
	static int init();

	
protected:
	int onSyncEvent(Event& event,Result* result);
	int onEvent(Event& event);

private:
	static TrafficMgr *sInstance;
	static TrafficMgr& instance(){return *sInstance;}

	TrafficMgr();
	~TrafficMgr();

	static const char* TAG;
	
};

#endif