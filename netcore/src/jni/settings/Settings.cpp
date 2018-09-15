
#include "Settings.h"
#include "Log.h"
#include "Error.h"
#include "Defines.h"

pthread_mutex_t Settings::s_lock;
std::map<Settings::Key,const char*> Settings::s_settings;
int Settings::LOG_LEVEL = 10;

int Settings::setSetting(Key key, const char* value){
	if(pthread_mutex_lock(&Settings::s_lock) == 0){
		s_settings[key] = value;	

		if(pthread_mutex_unlock(&Settings::s_lock)){
			LOGE("Settings","unlock settings failed");
		}

		if(key == SK_LOG_LEVEL){
			// LOG_LEVEL = atoi(value);
		}

		return Error::SUCCESS;
	}else{
		LOGE("Settings","lock settings map failed.");
	}

	return Error::FAILED;
}

const char* Settings::getSSetting(Key key, const char* def){
	const char* ret = def;
	if(pthread_mutex_lock(&Settings::s_lock) == 0){
		map<Settings::Key,const char*>::iterator itr = s_settings.find(key);
		if(itr != s_settings.end()){
			ret = itr->second;
		}

		if(pthread_mutex_unlock(&Settings::s_lock)){
			LOGE("Settings","unlock settings failed");
		}
	}else{
		LOGE("Settings","lock settings map failed.");
	}

	return ret;
}

int Settings::getISetting(Key key, int def){
	LOGD("settings","getISetting");
	const char* szValue = Settings::getSSetting(key, NULL);
	LOGD("settings","szValue geted1");
	if(szValue != NULL){
		LOGD("settings","szValue geted");
		return atoi(szValue);
	}

	LOGD("settings","szValue is NULL");

	return def;
}

void Settings::print(){
	for(map<Settings::Key,const char*>::iterator itr = s_settings.begin();itr!=s_settings.end();itr++){
        LOGR("settings","%d:%s",itr->first,itr->second);
    }
}


