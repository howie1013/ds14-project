// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server(): nacquire (0)
{
	assert(pthread_mutex_init(&_m, 0) == 0);
	for (int i = 0; i < MAX_LOCK; i++) 
	{
		assert(pthread_mutex_init(&_locks_m[i], 0) == 0);
		_locks_s[i] = 0;
	}
	jsl_log(JSL_DBG_3, "intialized lock_server\n");
}

lock_server::~lock_server()
{
	for (int i = 0; i < MAX_LOCK; i++) 
	{
		assert(pthread_mutex_destroy(&_locks_m[i]) == 0);
	}
	assert(pthread_mutex_destroy(&_m) == 0);
}

lock_protocol::status lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    jsl_log(JSL_DBG_3, "stat request from clt %d\n", clt);
    r = nacquire;
    return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	jsl_log(JSL_DBG_3, "acquire request from clt %d, lockid %u\n", clt, (unsigned int)lid & 0xFF);
	assert(pthread_mutex_lock(&_locks_m[lid & 0xFF]) == 0);
	assert(pthread_mutex_lock(&_m) == 0);
	_locks_s[lid & 0xFF] = 1;
	assert(pthread_mutex_unlock(&_m) == 0);
	return ret;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	jsl_log(JSL_DBG_3, "release request from clt %d, lockid %u\n", clt, (unsigned int)lid & 0xFF);
	assert(pthread_mutex_unlock(&_locks_m[lid & 0xFF]) == 0);
	assert(pthread_mutex_lock(&_m) == 0);
	_locks_s[lid & 0xFF] = 0;
	assert(pthread_mutex_unlock(&_m) == 0);
	return ret;
}
