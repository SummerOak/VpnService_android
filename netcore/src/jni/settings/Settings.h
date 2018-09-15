#ifndef SETTINGS_H
#define SETTINGS_H

#include <map>
#include <pthread.h>
#include <stdlib.h>

using namespace std;

class Settings{

public:
	enum Key{
		SK_LOG_LEVEL = 0,
		SK_PROXY_ADDR = 1,
		SK_PROXY_PORT = 2,
		SK_SOCKS5_VERIFY_METHOD = 3,
		SK_SOCKS5_USERNAME = 4,
		SK_SOCKS5_PASSWORD	= 5,
		SK_PROXY_IPVER		= 6,
		SK_PROXY_DNS_SERVER	= 7,
		SK_CAPTURE_DIRECTORY	= 8,
	};

	static int setSetting(Key key, const char* value);
	static const char* getSSetting(Key key, const char* def);
	static int getISetting(Key key, int def);

	static void print();

	static int LOG_LEVEL;

private:
	static pthread_mutex_t s_lock; 
	static std::map<Key,const char*> s_settings;
};

#endif