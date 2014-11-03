// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include <pthread.h>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include "jsl_log.h"

class lock_server
{
public:
	static const int MAX_LOCK = 256;

protected:
    int nacquire;
	//pthread_mutex_t _locks_m[MAX_LOCK];
	pthread_mutex_t _m;
    std::map<lock_protocol::lockid_t, pthread_mutex_t> _locks_m;

public:
    lock_server();
    ~lock_server();
    lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif







