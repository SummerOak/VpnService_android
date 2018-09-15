#include "TrafficMgr.h"
#include "Log.h"
#include "JniBridge.h"
#include "Defines.h"    

const char* TrafficMgr::TAG = PTAG("TrafficMgr");
TrafficMgr* TrafficMgr::sInstance = new TrafficMgr;

int TrafficMgr::init(){
    return 0;
}

TrafficMgr::TrafficMgr(){


	dispatcher::get().registerEvent(EVENT::CONN_BORN, this);
    dispatcher::get().registerEvent(EVENT::CONN_DIE, this);
    dispatcher::get().registerEvent(EVENT::CONN_TRAFFIC_ACCEPT, this);
    dispatcher::get().registerEvent(EVENT::CONN_TRAFFIC_BACK, this);
    dispatcher::get().registerEvent(EVENT::CONN_TRAFFIC_SENT, this);
    dispatcher::get().registerEvent(EVENT::CONN_TRAFFIC_RECV, this);
    dispatcher::get().registerEvent(EVENT::CONN_STATE, this);

}

int TrafficMgr::onSyncEvent(Event& event,Result* result){
    return 1;
}

int TrafficMgr::onEvent(Event& event){
    LOGD(TAG,"onEvent %d",event.id);

    switch(event.id){
        case EVENT::CONN_TRAFFIC_RECV:
        case EVENT::CONN_TRAFFIC_ACCEPT:
        case EVENT::CONN_TRAFFIC_BACK:
        case EVENT::CONN_TRAFFIC_SENT:
        case EVENT::CONN_STATE:
        case EVENT::CONN_DIE:
        case EVENT::CONN_BORN:{
            ConnInfo* info = (ConnInfo*)(event.data);
            JniBridge::notifyConnInfo(event.id, *info);
            SAFE_DELETE(info);

            break;
        }
    }

    return 0;
}