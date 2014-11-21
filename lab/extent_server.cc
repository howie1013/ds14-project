// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


extent_server::extent_server() 
{
    
    extent_protocol::filelist fl, fl1;

    _attr[1];
    _data[1] = extent_protocol::serialize(fl);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
    if (_attr.count(id) == 0)
        _attr[id] = extent_protocol::attr();
    _data[id] = buf;
    return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
    if (_data.count(id) == 0)
    {
        return extent_protocol::NOENT;
    }
    buf = _data[id];
    return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
    if (_attr.count(id) == 0)
    {
        return extent_protocol::NOENT;
    }
    a = _attr[id];
    return extent_protocol::OK;
}

int extent_server::setattr(extent_protocol::extentid_t id, extent_protocol::attr a, int &r)
{
    _attr[id] = a;
    return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
    return extent_protocol::IOERR;
}
