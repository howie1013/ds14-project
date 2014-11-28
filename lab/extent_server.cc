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

    time_t now = cur_sec();
    _attr[1].atime = now;
    _attr[1].mtime = now;
    _attr[1].ctime = now;
    _data[1] = extent_protocol::serialize(fl);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
    time_t now = cur_sec();
    _attr[id].mtime = now;
    _attr[id].ctime = now;
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
    _attr[id].atime = cur_sec();
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
    _attr[id].ctime = cur_sec();
    return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
    if (_attr.count(id) == 0 or _data.count(id) == 0)
    {
        return extent_protocol::IOERR;
    }
    _attr.erase(_attr.find(id));
    _data.erase(_data.find(id));
    return extent_protocol::OK;
}

time_t extent_server::cur_sec()
{
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now.tv_sec;
}
