// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
    sockaddr_in dstsock;

    make_sockaddr(dst.c_str(), &dstsock);
    cl = new rpcc(dstsock);
    if (cl->bind() != 0)
    {
        printf("extent_client: bind failed\n");
    }
    is_cached = true;
}

extent_client::~extent_client()
{
    delete cl;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
    extent_protocol::status ret = extent_protocol::OK;
    extent_protocol::attr attr;
    printf("[debug] extent_client::get %016llx cached = %d\n", eid, _data.count(eid));
    if (!is_cached || _data.count(eid) == 0)
    {
        ret = cl->call(extent_protocol::get, eid, buf);
        if (ret == extent_protocol::OK)
        {
            _deleted[eid] = false;
            _data[eid] = buf;
            _dirty_data[eid] = false;
            getattr(eid, attr); // cache attr
        }
    }

    if (ret == extent_protocol::OK)
    {
        if (_deleted.count(eid) > 0 && _deleted[eid] == false)
        {
            buf = _data[eid];

            if (_attr.count(eid) > 0)
            {
                _attr[eid].atime = extent_protocol::cur_sec();
                _dirty_attr[eid] = true;
            }
        }
        else
        {
            ret = extent_protocol::NOENT;
        }
    }
    return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
                       extent_protocol::attr &attr)
{
    extent_protocol::status ret = extent_protocol::OK;
    if (!is_cached || _attr.count(eid) == 0)
    {
        ret = cl->call(extent_protocol::getattr, eid, attr);
        if (is_cached && ret == extent_protocol::OK)
        {
            _deleted[eid] = false;
            _attr[eid] = attr;
            _dirty_attr[eid] = false;
        }
    }

    if (is_cached && ret == extent_protocol::OK)
    {
        if (_deleted.count(eid) > 0 && _deleted[eid] == false)
        {
            attr = _attr[eid];
        }
        else
        {
            ret = extent_protocol::NOENT;
        }
    }
    return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
    extent_protocol::status ret = extent_protocol::OK;
    extent_protocol::attr attr;
    int r;
    printf("[debug] extent_client::put %016llx cached = %d\n", eid, _data.count(eid));
    if (!is_cached || _data.count(eid) == 0)
    {
        ret = cl->call(extent_protocol::put, eid, buf, r);
        if (is_cached && ret == extent_protocol::OK)
        {
            getattr(eid, attr); // cache attr
        }
    }

    if (is_cached && ret == extent_protocol::OK)
    {
        _deleted[eid] = false;

        _data[eid] = buf;
        _dirty_data[eid] = true;

        if (_attr.count(eid) > 0)
        {
            _attr[eid].mtime = extent_protocol::cur_sec();
            _attr[eid].ctime = extent_protocol::cur_sec();
            _dirty_attr[eid] = true;
        }
    }
    return ret;
}

extent_protocol::status
extent_client::setattr(extent_protocol::extentid_t eid,
                       extent_protocol::attr attr)
{
    extent_protocol::status ret = extent_protocol::OK;
    int r;
    if (!is_cached || _attr.count(eid) == 0)
    {
        ret = cl->call(extent_protocol::setattr, eid, attr, r);
    }

    if (is_cached && ret == extent_protocol::OK)
    {
        _deleted[eid] = false;

        _attr[eid] = attr;
        _attr[eid].ctime = extent_protocol::cur_sec();
        _dirty_attr[eid] = true;
    }
    return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
    extent_protocol::status ret = extent_protocol::OK;
    int r;
    if (!is_cached || _data.count(eid) == 0)
    {
        ret = cl->call(extent_protocol::remove, eid, r);
    }

    if (is_cached && ret == extent_protocol::OK)
    {
        _deleted[eid] = true;

        _dirty_attr[eid] = false;
        _dirty_attr[eid] = false;
    }
    return ret;
}

extent_protocol::status extent_client::flush(extent_protocol::extentid_t eid)
{
    printf("[debug] extent_client::flush %016llx\n", eid);
    extent_protocol::status ret = extent_protocol::OK;
    int r;
    if (is_cached && _data.count(eid) > 0)
    {
        if (_dirty_data[eid] == true)
        {
            ret = cl->call(extent_protocol::put, eid, _data[eid], r);
            if (ret != extent_protocol::OK)
            {
                printf("[error] extent_client::flush flush data failed %016llx\n", eid);
            }
        }
    }

    if (is_cached && _attr.count(eid) > 0)
    {
        if (_dirty_attr[eid] == true)
        {
            ret = cl->call(extent_protocol::setattr, eid, _attr[eid], r);
            if (ret != extent_protocol::OK)
            {
                printf("[error] extent_client::flush flush attr failed %016llx\n", eid);
            }
        }
    }
    if (is_cached && _deleted.count(eid) > 0)
    {
        if (_deleted[eid] == true)
        {
            ret = cl->call(extent_protocol::remove, eid, r);
            if (ret != extent_protocol::OK)
            {
                printf("[error] extent_client::flush flush delete failed %016llx\n", eid);
            }
        }
    }

    if (is_cached)
    {
        if (_data.count(eid) > 0)
        {
            _data.erase(_data.find(eid));
            _dirty_data.erase(_dirty_data.find(eid));
        }
        if (_attr.count(eid) > 0)
        {
            _attr.erase(_attr.find(eid));
            _dirty_attr.erase(_dirty_attr.find(eid));
        }
        if (_deleted.count(eid) > 0)
            _deleted.erase(_deleted.find(eid));
    }
    printf("[debug] extent_client::flush %016llx done\n", eid);
    return ret;
}
