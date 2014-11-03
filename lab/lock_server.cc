// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server(): nacquire (0)
{
	assert(pthread_mutex_init(&_m, 0) == 0);
	jsl_log(JSL_DBG_3, "intialized lock_server\n");
}

lock_server::~lock_server()
{
	for (std::map<lock_protocol::lockid_t, pthread_mutex_t>::iterator it = _locks_m.begin(); it != _locks_m.end(); it++)
	{
		assert(pthread_mutex_destroy(&it->second) == 0);
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

	assert(pthread_mutex_lock(&_m) == 0);
	if (_locks_m.find(lid) == _locks_m.end())
		assert(pthread_mutex_init(&_locks_m[lid], 0) == 0);
	assert(pthread_mutex_unlock(&_m) == 0);

	assert(pthread_mutex_lock(&_locks_m[lid]) == 0);

	assert(pthread_mutex_lock(&_m) == 0);
	nacquire += 1;
	jsl_log(JSL_DBG_3, "[lock_server:acquire] acquire request from clt %d, lockid %u nacquire %d\n", clt, (unsigned int)lid & 0xFF, nacquire);
	assert(pthread_mutex_unlock(&_m) == 0);

	return ret;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;

	assert(pthread_mutex_unlock(&_locks_m[lid]) == 0);

	assert(pthread_mutex_lock(&_m) == 0);
	nacquire -= 1;
	jsl_log(JSL_DBG_3, "[lock_server:release] release request from clt %d, lockid %u nacquire %d\n", clt, (unsigned int)lid & 0xFF, nacquire);
	assert(pthread_mutex_unlock(&_m) == 0);

	
	return ret;
}
