#ifndef TEST_OF_EVENT_H
#define TEST_OF_EVENT_H

#include "EventDispatcher.h"


class ModuleTestEventA: public EventHandler<Event,Result>{
public:
	int onSyncEvent(Event& event,Result* result){
		LOGD(TAG,"onSyncEvent %d",event.id);
		if(result != NULL){
			result->code = 3;
		}
		return 0;
	}

	int onEvent(Event& event){
		LOGD(TAG,"onEvent %d",event.id);

		if(event.id == 3){
			
			LOGD(TAG,"destroying this");
			delete this;
		}

		return 0;
	}

	static const char* TAG;
};
const char* ModuleTestEventA::TAG = "VpnLib.ModuleTestEventA";

int event_module_test(){

	ModuleTestEventA* a = new ModuleTestEventA();
	delete a;
	
	dispatcher::get().registerEvent(1,a);
	dispatcher::get().registerEvent(2,a);
	dispatcher::get().registerEvent(2,a);
	dispatcher::get().registerEvent(3,a);

	dispatcher::get().dispatch(new Event(1));
	dispatcher::get().dispatchSync(new Event(2));

	Result result = Result();
	dispatcher::get().dispatchSync(new Event(3),&result);
	dispatcher::get().dispatch(new Event(3));
	LOGD(ModuleTestEventA::TAG,"dispatchSync 3 return: %d",result.code);

	return 0;
}

#endif