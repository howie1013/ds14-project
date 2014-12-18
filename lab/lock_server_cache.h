#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <map>
#include <list>


class lock_server_cache
{
private:
    enum status {FREE, LOCKED, REVOKING};

    struct lock_cache
    {
        status stat;
        std::string owner;
        std::string retryer;
        bool retrying;
        std::list<std::string> pending;

        lock_cache() : stat(FREE), retrying(false) {};
    };

    struct lock_info
    {
        std::string id;
        lock_protocol::lockid_t lid;
        lock_info(std::string id_, lock_protocol::lockid_t lid_) : id(id_), lid(lid_) {};
    };

    int nacquire;
    bool terminated;
    pthread_t _thread_retry;
    pthread_t _thread_revoke;
    pthread_mutex_t _mutex_cache;
    pthread_mutex_t _mutex_retry;
    pthread_mutex_t _mutex_revoke;
    pthread_cond_t _cond_retry;
    pthread_cond_t _cond_revoke;
    std::map<lock_protocol::lockid_t, lock_cache> _cache;
    std::list<lock_info> _list_retry;
    std::list<lock_info> _list_revoke;
public:
    lock_server_cache();
    ~lock_server_cache();
    void revoker();
    void retryer();

    lock_protocol::status stat(std::string id, lock_protocol::lockid_t, int &);
    lock_protocol::status acquire(std::string id, lock_protocol::lockid_t lid, int &);
    lock_protocol::status release(std::string id, lock_protocol::lockid_t lid, int &);
};

#endif
