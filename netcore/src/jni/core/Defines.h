#ifndef DEFINES_H
#define DEFINES_H
#include "Log.h"

#define PTAG(tag) "VpnCore." tag

#define SAFE_FREE(p) if(p != NULL) {LOGD(PTAG("memory_release"), "free %p", p); free(p);p=NULL;}
#define SAFE_DELETE(p) if(p != NULL) {LOGD(PTAG("memory_release"), "delete %p", p);  delete p; p = NULL;}

#define SWAP_INT32(value)	(((value) & 0x000000FF) << 24)|\
               				(((value) & 0x0000FF00) << 8) |\
               				(((value) & 0x00FF0000) >> 8) |\
               				(((value) & 0xFF000000) >> 24)  



#define SWAP_INT16(value)	(((value) & 0x000000FF) << 8)|\
               				(((value) & 0x0000FF00) >> 8)


#define OFFSET(type,field)	((char *) &((type *) 0)->field - (char *) 0)

#define ARRAY_SIZE(a)	(sizeof (a) / sizeof ((a)[0]))


#define SAFE_CLOSE_SOCKET(socket, reactor) if(reactor != NULL && socket >= 0){\
									        	reactor->delEvent(socket, IReactor::EVENT::ALL);\
										        if(::close(socket)){\
										            LOGE(TAG,"close socket(%d) error %d(%s)", socket, errno, strerror(errno));\
										        }\
										    }\
										    socket = -1;\
										    reactor = NULL;\


#endif
