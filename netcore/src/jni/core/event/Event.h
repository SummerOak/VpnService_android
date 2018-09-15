#ifndef EVENT_H
#define EVENT_H

#include "IPAdpt.h"

class Event{


public:

	enum ID{
		WRITE_PACKAGE,

		CONN_BORN = 1001,
		CONN_DIE,
		CONN_TRAFFIC_ACCEPT,
		CONN_TRAFFIC_BACK,
		CONN_TRAFFIC_SENT,
		CONN_TRAFFIC_RECV,
		CONN_STATE,
		TCPPACKAGE_IN,
		TCPPACKAGE_OUT,

		CAPTURE_START,
		CAPTURE_PACKET,
		CAPTURE_STOP,
	};

	Event(int id,int actId=0,void* data = NULL):id(id),action(actId),data(data){}
	~Event(){};

	const int id;
	const int action;
	const void* data;

private:

};

typedef Event::ID EVENT;


#endif