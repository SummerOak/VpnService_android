#ifndef JNI_BRIDGE_H
#define JNI_BRIDGE_H

#include <jni.h>
#include "Session.h"

enum NativeEvent{
	EV_PROTECT,

};

class JniBridge{

public:
	static void setBridge(JNIEnv* env, jobject ins);
	static void setVM(JavaVM* vm);

	static int protectSock(int socket);
	static char* getAppName(int uid);
	static int notifyConnInfo(int event, ConnInfo& info);
	static int queryControlStrategy(int uid, int ipver, const char* ip, int port, uint8_t protocol, const char* dns);
	static int checkException();
	static JNIEnv* getEnv();

private:
	static const char* TAG;
	static JavaVM* sVM;
	static jobject sJInstance;

	static jmethodID sMidProtect;
	static jmethodID sMidGetAppName;
	static jmethodID sMidOnConnInfo;
	static jmethodID sMidQueryControlStrategy;
};


#endif