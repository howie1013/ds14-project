// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"


class extent_client
{
private:
    rpcc *cl;
    bool is_cached;
    std::map<extent_protocol::extentid_t, std::string> _data;
    std::map<extent_protocol::extentid_t, extent_protocol::attr> _attr;
    std::map<extent_protocol::extentid_t, bool> _dirty_data;
    std::map<extent_protocol::extentid_t, bool> _dirty_attr;
    std::map<extent_protocol::extentid_t, bool> _deleted;

public:
    extent_client(std::string dst);
    ~extent_client();

    extent_protocol::status get(extent_protocol::extentid_t eid,
                                std::string &buf);
    extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                    extent_protocol::attr &a);
    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
    extent_protocol::status setattr(extent_protocol::extentid_t eid,
                                    extent_protocol::attr a);
    extent_protocol::status remove(extent_protocol::extentid_t eid);
    extent_protocol::status flush(extent_protocol::extentid_t eid);
};


#endif

