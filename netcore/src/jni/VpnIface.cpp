#include <jni.h>
#include <sys/system_properties.h>
#include "Log.h"
#include "Settings.h"
#include "Router.h"
#include "Error.h"
#include "Defines.h"
#include "IReactor.h"
#include "EpollReactor.h"
#include "EventDispatcher.h"
#include "TestOfEvent.h"
#include "JniBridge.h"


#define P_TAG PTAG("Iface")

#ifdef __cplusplus
extern "C" {
#endif

static Router* sRouter = NULL;

JNIEXPORT jint JNICALL
Java_com_summer_netcore_VpnCore_setConfig(JNIEnv *env,jobject obj,jint key,jstring value)
{
	const char* pvalue = env->GetStringUTFChars(value,0);
	int l = strlen(pvalue) + 1;
	char* v = new char[l];
	strncpy(v,pvalue,l);
	env->ReleaseStringUTFChars(value,pvalue);
	return Settings::setSetting(static_cast<Settings::Key>(key),v);
}

JNIEXPORT jstring JNICALL
Java_com_summer_netcore_VpnCore_getSystemProperty(JNIEnv *env, jobject obj, jstring key){
	const char *skey = env->GetStringUTFChars(key, 0);

    char value[PROP_VALUE_MAX + 1] = "";
    __system_property_get(skey, value);

    env->ReleaseStringUTFChars(key, skey);

    return env->NewStringUTF(value);
}

JNIEXPORT jint JNICALL
Java_com_summer_netcore_VpnCore_start(JNIEnv *env,jobject obj,jint tun)
{
	Settings::print();
	LOGD(P_TAG,"start %d",tun);

	if(sRouter == NULL){
		JniBridge::setBridge(env, obj);
		sRouter = new Router(tun);
		if(sRouter->init()){
			LOGE(P_TAG,"init router failed.");
		}else{
			sRouter->start();
		}
		
		SAFE_DELETE(sRouter);
		return Error::SUCCESS;
	}

	LOGE(P_TAG,"vpn is running...");
	return Error::DUPLICATE;
}

JNIEXPORT jint JNICALL
Java_com_summer_netcore_VpnCore_stop(JNIEnv *env,jobject obj)
{
	LOGD(P_TAG,"stop");
	int ret = 0;
	if(sRouter != NULL){
		ret = sRouter->stop();
		//DO NOT RELEASE sRouter HERE!!!  the actual stop action is async and it depends on sRouter
	}
    
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_summer_netcore_VpnCore_moduleTest(JNIEnv *env,jobject obj)
{
	LOGI(P_TAG,"moduleTest");

	return event_module_test();
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) 
{
	LOGR(P_TAG,"jni onload...");
	JniBridge::setVM(vm);
	return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif