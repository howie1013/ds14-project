// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

static void *revokethread(void *x)
{
    lock_server_cache *sc = (lock_server_cache *) x;
    sc->revoker();
    return 0;
}

static void *retrythread(void *x)
{
    lock_server_cache *sc = (lock_server_cache *) x;
    sc->retryer();
    return 0;
}

lock_server_cache::lock_server_cache()
{
    nacquire = 0;
    terminated = false;

    assert(pthread_mutex_init(&_mutex_cache, NULL) == 0);
    assert(pthread_mutex_init(&_mutex_retry, NULL) == 0);
    assert(pthread_mutex_init(&_mutex_revoke, NULL) == 0);
    assert(pthread_cond_init(&_cond_retry, NULL) == 0);
    assert(pthread_cond_init(&_cond_revoke, NULL) == 0);

    int r = pthread_create(&_thread_retry, NULL, &revokethread, (void *) this);
    assert (r == 0);
    r = pthread_create(&_thread_revoke, NULL, &retrythread, (void *) this);
    assert (r == 0);
}

lock_server_cache::~lock_server_cache()
{
    int r;
    terminated = true;
    pthread_join(_thread_retry, (void **)&r);
    pthread_join(_thread_revoke, (void **)&r);
    pthread_mutex_destroy(&_mutex_cache);
    pthread_mutex_destroy(&_mutex_retry);
    pthread_mutex_destroy(&_mutex_revoke);
    pthread_cond_destroy(&_cond_retry);
    pthread_cond_destroy(&_cond_revoke);
}

lock_protocol::status lock_server_cache::stat(std::string id, lock_protocol::lockid_t lid, int &r)
{
    lock_protocol::status ret = lock_protocol::OK;
    jsl_log(JSL_DBG_3, "stat request from id %s\n", id.c_str());
    r = nacquire;
    return ret;
}


lock_protocol::status lock_server_cache::acquire(std::string id, lock_protocol::lockid_t lid, int &)
{
    lock_protocol::status ret = lock_protocol::RPCERR;
    pthread_mutex_lock(&_mutex_cache);

    if (_cache.count(lid) == 0)
    {
        _cache[lid] = lock_cache();
    }

    lock_cache &cache = _cache[lid];
    //printf("[debug] acquire %02d, id %s stat %d\n", lid, id.c_str(), cache.stat);
    if (cache.stat == FREE && (!cache.retrying || cache.retryer == id))
    {
        //printf("[debug] grant lock %02d, id %s, stat %d, pending(%d)\n", lid, id.c_str(), cache.stat, cache.pending.size());

        cache.stat = LOCKED;
        cache.owner = id;
        cache.retrying = false;
        ret = lock_protocol::OK;
        nacquire++;

        /*
        if (cache.pending.size() > 0)
        {
            cache.stat = REVOKING;
            pthread_mutex_lock(&_mutex_revoke);
            _list_revoke.push_back(lock_info(cache.owner, lid));
            pthread_cond_signal(&_cond_revoke);
            //printf("[debug] schedule1 revoke %s -> %s %02d\n", id.c_str(), cache.owner.c_str(), lid);
            pthread_mutex_unlock(&_mutex_revoke);

        }
        */
    }
    else
    {
        ret = lock_protocol::RETRY;
        cache.pending.push_back(id);
        //printf("[debug] lid:%02d pending.push_back(%s)\n", lid, id.c_str());

    }

    if (cache.stat == LOCKED && cache.pending.size() > 0)
    {
        cache.stat = REVOKING;
        pthread_mutex_lock(&_mutex_revoke);
        _list_revoke.push_back(lock_info(cache.owner, lid));
        pthread_cond_signal(&_cond_revoke);
        //printf("[debug] schedule2 revoke %s -> %s %02d\n", id.c_str(), cache.owner.c_str(), lid);
        pthread_mutex_unlock(&_mutex_revoke);

    }

    pthread_mutex_unlock(&_mutex_cache);
    return ret;

}

lock_protocol::status lock_server_cache::release(std::string id, lock_protocol::lockid_t lid, int &)
{
    lock_protocol::status ret = lock_protocol::RPCERR;

    pthread_mutex_lock(&_mutex_cache);

    if (_cache.count(lid) == 0)
    {
        ret = lock_protocol::NOENT;
    }
    else
    {
        nacquire--;
        lock_cache &cache = _cache[lid];
        //printf("[debug] release %02d, id %s stat %d\n", lid, id.c_str(), cache.stat);
        cache.stat = FREE;
        cache.owner = "";

        if (cache.pending.size() > 0)
        {
            pthread_mutex_lock(&_mutex_retry);
            _list_retry.push_back(lock_info(cache.pending.front(), lid));
            cache.pending.pop_front();
            pthread_cond_signal(&_cond_retry);
            pthread_mutex_unlock(&_mutex_retry);
        }
        ret = lock_protocol::OK;
    }

    pthread_mutex_unlock(&_mutex_cache);
    return ret;
}


void lock_server_cache::revoker()
{
    // This method should be a continuous loop, that sends revoke
    // messages to lock holders whenever another client wants the
    // same lock
    sockaddr_in dstsock;
    int r, ret;
    //printf("[debug] revoker started\n");
    while (!terminated)
    {
        pthread_mutex_lock(&_mutex_revoke);
        while (_list_revoke.size() == 0)
            pthread_cond_wait(&_cond_revoke, &_mutex_revoke);
        //printf("[debug] _list_revoke.size() = %d\n", _list_revoke.size());
        while (_list_revoke.size() > 0)
        {
            lock_info info = _list_revoke.front();
            _list_revoke.pop_front();


            make_sockaddr(info.id.c_str(), &dstsock);
            rpcc cl(dstsock);
            //printf("[debug] revoking lid:%02d id:%s\n", info.lid, info.id.c_str());
            if (cl.bind() != 0 || (ret = cl.call(rlock_protocol::revoke, info.lid, r)) != rlock_protocol::OK)
            {
                printf("[error] call revoke error id:%s lid:%02d\n", info.id.c_str(), info.lid);
                pthread_mutex_lock(&_mutex_retry);
                lock_cache &cache = _cache[info.lid];
                cache.stat = FREE;
                cache.owner = "";
                cache.retrying = false;
                pthread_cond_signal(&_cond_retry);
                pthread_mutex_unlock(&_mutex_retry);
            }
        }
        pthread_mutex_unlock(&_mutex_revoke);
    }
}


void lock_server_cache::retryer()
{
    // This method should be a continuous loop, waiting for locks
    // to be released and then sending retry messages to those who
    // are waiting for it.
    sockaddr_in dstsock;
    int r, ret;
    while (!terminated)
    {
        pthread_mutex_lock(&_mutex_retry);
        while (_list_retry.size() == 0)
            pthread_cond_wait(&_cond_retry, &_mutex_retry);
        while (_list_retry.size() > 0)
        {
            lock_info info = _list_retry.front();
            _list_retry.pop_front();

            pthread_mutex_lock(&_mutex_cache);
            lock_cache &cache = _cache[info.lid];
            cache.retrying = true;
            cache.retryer = info.id;
            pthread_mutex_unlock(&_mutex_cache);

            make_sockaddr(info.id.c_str(), &dstsock);
            rpcc cl(dstsock);
            //printf("[debug] retrying lid:%02d id:%s\n", info.lid, info.id.c_str());
            if (cl.bind() != 0 || (ret = cl.call(rlock_protocol::retry, info.lid, r)) != rlock_protocol::OK)
            {
                printf("[error] call retry error id:%s lid:%02d\n", info.id.c_str(), info.lid);
                pthread_mutex_lock(&_mutex_cache);
                cache.retrying = false;
                cache.retryer = "";
                pthread_mutex_unlock(&_mutex_cache);

            }
        }
        pthread_mutex_unlock(&_mutex_retry);
    }
}

