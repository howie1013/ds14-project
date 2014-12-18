// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>


static void *releasethread(void *x)
{
    lock_client_cache *cc = (lock_client_cache *) x;
    cc->releaser();
    return 0;
}

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, class lock_release_user *_lu) : lock_client(xdst), lu(_lu)
{
    srand(time(NULL)^last_port);

    terminated = false;

    /* initialize */
    assert(pthread_mutex_init(&_mutex_cache, NULL) == 0);
    assert(pthread_mutex_init(&_mutex_release, NULL) == 0);
    assert(pthread_cond_init(&_cond_release, NULL) == 0);

    rlock_port = ((rand() % 32000) | (0x1 << 10));
    const char *hname;
    // assert(gethostname(hname, 100) == 0);
    hname = "127.0.0.1";
    std::ostringstream host;
    host << hname << ":" << rlock_port;
    id = host.str();
    last_port = rlock_port;

    rpcs *rlsrpc = new rpcs(rlock_port);
    /* register RPC handlers with rlsrpc */
    rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke);
    rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry);


    int r = pthread_create(&_thread_releaser, NULL, &releasethread, (void *) this);
    assert (r == 0);


}


lock_client_cache::~lock_client_cache()
{
    int r, ret;
    terminated = true;
    pthread_join(_thread_releaser, (void **)&r);
    for (std::map<lock_protocol::lockid_t, lock_cache>::iterator it = _cache.begin(); it != _cache.end(); ++it)
    {
        printf("[debug] ~lock_client_cache release %016llx\n", it->first);
        ret = cl->call(lock_protocol::release, id, it->first, r);
        pthread_mutex_destroy(&it->second.mutex);
        pthread_cond_destroy(&it->second.cond);
        pthread_cond_destroy(&it->second.cond_retry);
        pthread_cond_destroy(&it->second.cond_release);
    }
    pthread_mutex_destroy(&_mutex_cache);
    pthread_mutex_destroy(&_mutex_release);
    pthread_cond_destroy(&_cond_release);
}


void lock_client_cache::releaser()
{
    // This method should be a continuous loop, waiting to be notified of
    // freed locks that have been revoked by the server, so that it can
    // send a release RPC.
    int r, ret, skip;
    while (!terminated)
    {
        pthread_mutex_lock(&_mutex_release);
        pthread_cond_wait(&_cond_release, &_mutex_release);
        skip = 0;
        while (_list_release.size() > skip)
        {
            lock_protocol::lockid_t lid = _list_release.front();
            _list_release.pop_front();
            lock_cache &cache = _cache[lid];
            printf("[debug] %s try to release lid:%016llx stat:%d\n", id.c_str(), lid, cache.stat);
            pthread_mutex_lock(&cache.mutex);
            if (cache.stat == FREE)
            {
                printf("[debug] releasing %s lid %016llx\n", id.c_str(), lid);
                cache.stat = RELEASING;
                ret = cl->call(lock_protocol::release, id, lid, r);
                if (ret == lock_protocol::OK)
                {
                    cache.stat = NONE;
                    pthread_cond_signal(&cache.cond_release);
                }
                printf("[debug] released %s lid %016llx\n", id.c_str(), lid);
            }
            else
            {
                //pthread_mutex_unlock(&cache.mutex);
                //break;
                _list_release.push_back(lid);
                skip++;
            } 
            pthread_mutex_unlock(&cache.mutex);   
        }
        pthread_mutex_unlock(&_mutex_release);
    }
}

lock_protocol::status lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
    pthread_mutex_lock(&_mutex_cache);
    if (_cache.count(lid) == 0)
    {
        _cache[lid] = lock_cache();
    }
    lock_cache &cache = _cache[lid];
    pthread_mutex_unlock(&_mutex_cache);

    int r, ret = lock_protocol::OK;
    pthread_mutex_lock(&cache.mutex);
    while (cache.stat != FREE)
    {
        while (cache.stat == RELEASING)
            pthread_cond_wait(&cache.cond_release, &cache.mutex);
        if (cache.stat == NONE)
        {
            cache.stat = ACQUIRING;
            while (cache.stat != FREE)
            {
                printf("[debug] %s acquiring %016llx\n", id.c_str(), lid);
                ret = cl->call(lock_protocol::acquire, id, lid, r);
                printf("[debug] %s acquire %016llx ret %d\n", id.c_str(), lid, ret);
                if (ret == lock_protocol::OK)
                {
                    cache.stat = FREE;
                }
                else if (ret == lock_protocol::RETRY)
                {
                    pthread_cond_wait(&cache.cond_retry, &cache.mutex);
                }
                else
                {
                    break;
                }
            }
        }
        else
            pthread_cond_wait(&cache.cond, &cache.mutex);
    }

    if (ret == lock_protocol::OK)
    {
        printf("[debug] %s get lock %016llx\n", id.c_str(), lid);
        cache.stat = LOCKED;
    }
    pthread_mutex_unlock(&cache.mutex);
    return ret;
}

lock_protocol::status lock_client_cache::release(lock_protocol::lockid_t lid)
{
    lock_cache &cache = _cache[lid];
    pthread_mutex_lock(&cache.mutex);
    printf("[debug] %s release %016llx\n", id.c_str(), lid);
    cache.stat = FREE;
    pthread_cond_signal(&cache.cond);
    pthread_mutex_unlock(&cache.mutex);

    pthread_mutex_lock(&_mutex_release);
    pthread_cond_signal(&_cond_release);
    pthread_mutex_unlock(&_mutex_release);

    return lock_protocol::OK;
}

lock_protocol::status lock_client_cache::stat(lock_protocol::lockid_t lid)
{
    return lock_protocol::OK;
}

rlock_protocol::status lock_client_cache::revoke(lock_protocol::lockid_t lid, int &r)
{
    printf("[debug] %s revoke %016llx stat:%d\n", id.c_str(), lid, _cache[lid].stat);
    pthread_mutex_lock(&_mutex_release);
    _list_release.push_back(lid);
    pthread_cond_signal(&_cond_release);
    pthread_mutex_unlock(&_mutex_release);
    return rlock_protocol::OK;
}

rlock_protocol::status lock_client_cache::retry(lock_protocol::lockid_t lid, int &r)
{
    lock_cache &cache = _cache[lid];
    printf("[debug] %s retry %016llx stat:%d\n", id.c_str(), lid, cache.stat);
    pthread_mutex_lock(&cache.mutex);
    pthread_cond_signal(&cache.cond_retry);
    pthread_mutex_unlock(&cache.mutex);
    return rlock_protocol::OK;
}

