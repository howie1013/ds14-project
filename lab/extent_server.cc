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

    //fl.push_back(std::make_pair<extent_protocol::extentid_t, std::string>(2, ".Trash"));
    //fl.push_back(std::make_pair<extent_protocol::extentid_t, std::string>(3, ".Trash-1000"));

    _attr[1];
    _data[1] = extent_protocol::serialize(fl);
    /*
    _attr[2];
    _data[2] = extent_protocol::serialize(fl1);
    _attr[3];
    _data[3] = extent_protocol::serialize(fl1);
    */
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
    // You replace this with a real implementation. We send a phony response
    // for now because it's difficult to get FUSE to do anything (including
    // unmount) if getattr fails.
    /*
    a.size = 0;
    a.atime = 0;
    a.mtime = 0;
    a.ctime = 0;
    return extent_protocol::OK;
    */
    if (_attr.count(id) == 0)
    {
        return extent_protocol::NOENT;
    }
    a = _attr[id];
    return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
    return extent_protocol::IOERR;
}
