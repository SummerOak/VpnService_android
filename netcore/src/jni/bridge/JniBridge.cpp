#include "JniBridge.h"
#include "Log.h"
#include "Defines.h"
#include "Utils.h"

const char* JniBridge::TAG = PTAG("JniBridge");
JavaVM* JniBridge::sVM = NULL;
jobject JniBridge::sJInstance = 0;
jmethodID JniBridge::sMidOnConnInfo = 0;
jmethodID JniBridge::sMidProtect = 0;
jmethodID JniBridge::sMidGetAppName = 0;
jmethodID JniBridge::sMidQueryControlStrategy = 0;

void JniBridge::setVM(JavaVM* vm){
	sVM = vm;
	LOGD(TAG, "setVM %p, sVM %p", vm, sVM);
	return;
}

void JniBridge::setBridge(JNIEnv* env, jobject ins){

	if(sJInstance != 0){
		LOGE(TAG,"bridge has init already...");
		return;
	}

	sJInstance = (jobject)env->NewGlobalRef(ins);

	sMidProtect = env->GetStaticMethodID((jclass)ins, "protect", "(I)I");
    checkException();
    if(sMidProtect == NULL){
    	LOGE(TAG,"java method protect not found.");
    }

    sMidGetAppName = env->GetStaticMethodID((jclass)ins, "getAppName", "(I)Ljava/lang/String;");
    checkException();
    if(sMidGetAppName == NULL){
    	LOGE(TAG,"java method getAppName not found.");
    }

    sMidOnConnInfo = env->GetStaticMethodID((jclass)ins, "onConnInfo", "(IIIBBJJJJIIIILjava/lang/String;I)I");
    checkException();
    if(sMidOnConnInfo == NULL){
    	LOGE(TAG,"java method onConnInfo not found.");
    }

    sMidQueryControlStrategy = env->GetStaticMethodID((jclass)ins, "queryControlStrategy", "(IILjava/lang/String;IBLjava/lang/String;)I");
    checkException();
    if(sMidQueryControlStrategy == NULL){
    	LOGE(TAG,"java method onConnInfo not found.");
    }

	return;
}

int JniBridge::protectSock(int socket){
	if(sVM == NULL || sJInstance == 0 || sMidProtect == NULL){
		LOGE(TAG,"JniBridge not init properly.");
		return 1;
	}

	if(socket < 0){
		LOGE(TAG,"socket < 0");
		return 1;
	}

	JNIEnv* env = getEnv();
	if(env == NULL){
		LOGE(TAG,"get env failed.");
		return 1;
	}

	LOGD(TAG,"calling static protect in java...");
	jint ret = env->CallStaticIntMethod((jclass)sJInstance, sMidProtect, socket);
	checkException();

	if(ret){
		LOGE(TAG,"protected %d failed",socket);
		return 1;
	}

	LOGD(TAG,"protected %d success",socket);
	return 0;
}

char* JniBridge::getAppName(int uid){
	if(sVM == NULL || sJInstance == 0){
		LOGE(TAG,"JniBridge not init properly.");
		return NULL;
	}

	if(uid < 0){
		LOGE(TAG,"uid < 0");
		return NULL;
	}

	JNIEnv* env = getEnv();
	if(env == NULL){
		LOGE(TAG,"get env failed.");
		return NULL;
	}

	if(sMidGetAppName == 0){
    	LOGE(TAG,"java method getAppName not found.");
    	return NULL;
	}

	LOGD(TAG,"calling getAppName in java");
	jstring ret = (jstring)env->CallStaticObjectMethod((jclass)sJInstance, sMidGetAppName, uid);
	checkException();
	const char* jsz = env->GetStringUTFChars(ret,0);
	checkException();
	int l = strlen(jsz) + 1;
	char* appName = new char[l];
	strncpy(appName,jsz,l);
	env->DeleteLocalRef(ret);

	return appName;
}

int JniBridge::notifyConnInfo(int event, ConnInfo& info){
	LOGD(TAG,"notifyConnInfo...");
	if(sVM == NULL || sJInstance == 0 || sMidOnConnInfo == NULL){
		LOGE(TAG,"JniBridge not init properly.");
		return 1;
	}

	JNIEnv* env = getEnv();
	if(env == NULL){
		LOGE(TAG,"get env failed.");
		return 1;
	}

	LOGD(TAG,"calling static notifyConnInfo in java...");

	jint ret = env->CallStaticIntMethod((jclass)sJInstance, sMidOnConnInfo, event, info.id, info.uid, info.type, info.state,
	info.accept, info.back, info.sent, info.recv, info.size, info.flag, info.seq, info.ack,
	env->NewStringUTF(info.dest), info.destPort);
	checkException();

	LOGD(TAG,"post call notifyConnInfo ...");

    return ret;
}

int JniBridge::queryControlStrategy(int uid, int ipver, const char* ip, int port, uint8_t protocol, const char* dns){
	LOGD(TAG, "queryControlStrategy");
	if(sVM == NULL || sJInstance == 0 || sMidQueryControlStrategy == NULL){
		LOGE(TAG,"JniBridge not init properly.");
		return 1;
	}

	JNIEnv* env = getEnv();
	if(env == NULL){
		LOGE(TAG,"get env failed.");
		return 1;
	}

	LOGD(TAG,"calling static queryControlStrategy in java...");
	jint ret = env->CallStaticIntMethod((jclass)sJInstance, sMidQueryControlStrategy, 
		uid, ipver, env->NewStringUTF(ip), port, protocol, env->NewStringUTF(dns));
	checkException();

	LOGD(TAG,"post call queryControlStrategy %d", ret);

    return ret;

}

JNIEnv* JniBridge::getEnv(){
	JNIEnv* env = NULL;
    int state = sVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (state == JNI_EDETACHED || env == NULL) {
        LOGD(TAG,"getEnv, env not attached yet");
        if (sVM->AttachCurrentThread(&env, NULL) != JNI_OK) {
        	LOGE(TAG,"getEnv, attach env to current thread failed.");
            return NULL;
        }
    } else if (state == JNI_OK) {
        LOGD(TAG,"getEnv, env already attached.");
    } else if (state == JNI_EVERSION) {
    	LOGE(TAG,"getEnv, jni version not supported");
    	return NULL;
    }

    return env;
}

int JniBridge::checkException(){
	int ret = 0;
	JNIEnv* env = getEnv();
	if(env == NULL){
		LOGE(TAG,"checkException get env failed.");
		return 1;
	}

	jthrowable ex = (env)->ExceptionOccurred();
    if (ex) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(ex);
        ret = 1;
    }

    return 0;
}