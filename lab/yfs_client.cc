// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);

}

yfs_client::inum yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum)
{
    if (inum & 0x80000000)
        return true;
    return false;
}

bool yfs_client::isdir(inum inum)
{
    return ! isfile(inum);
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;


    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:

    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;


    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


int yfs_client::lookup(inum p_id, std::string name, inum &id)
{
    std::string buf;

    if (ec->get(p_id, buf) != extent_protocol::OK)
    {
        printf("[error] yfs_client::lookup ec->get(%016llx, buf) failed\n", p_id);
        return IOERR;
    }

    extent_protocol::filelist fl;
    if (extent_protocol::deserialize(buf, fl))
    {
        foreach(fl, it)
        {
            if (it->second == name)
            {
                id = it->first;
                return OK;
            }
        }
        printf("[info] yfs_client::lookup [%016llx] not entry [%s]\n", p_id, name.c_str());
        return NOENT;
    }
    else
    {
        printf("[error] yfs_client::lookup [%016llx] deserialize failed\n", p_id);
    }
    return IOERR;
}

int yfs_client::readdir(inum id, extent_protocol::filelist &fl)
{
    std::string buf;

    if (ec->get(id, buf) != extent_protocol::OK)
    {
        printf("[error] yfs_client::readdir ec->get(%016llx, buf) failed\n", id);
        return IOERR;
    }

    if (extent_protocol::deserialize(buf, fl))
    {
        return OK;
    }
    else
    {
        printf("[error] yfs_client::lookup [%016llx] deserialize failed\n", id);
    }
    return IOERR;
}

int yfs_client::createfile(inum p_id, std::string name, inum &id)
{
    int r;
    extent_protocol::filelist fl;
    std::string p_buf;
    std::string buf;

    r  = lookup(p_id, name, id);
    if (r == OK)
    {
        return OK;
    }
    else if (r == NOENT)
    {

        r = readdir(p_id, fl);
        if (r == OK)
        {
            id = new_inum(1);

            fl.push_back(std::make_pair(id, name));
            p_buf = extent_protocol::serialize(fl);
            if (ec->put(p_id, p_buf) != extent_protocol::OK)
            {
                printf("[error] yfs_client::createfile ec->put(%016llx, p_buf) != extent_protocol::OK\n", p_id);
                return IOERR;
            }

            if (ec->put(id, buf) != extent_protocol::OK)
            {
                printf("[error] yfs_client::createfile ec->put(%016llx, buf) != extent_protocol::OK\n", id);
                return IOERR;
            }
            return OK;
        }
    }
    return IOERR;
}


int yfs_client::setfile(inum id, const fileinfo& fin)
{
    int r;
    extent_protocol::attr a;

    a.atime = fin.atime;
    a.mtime = fin.mtime;
    a.ctime = fin.ctime;
    a.size = fin.size;

    r = ec->setattr(id, a);
    if (r != extent_protocol::OK)
    {
        printf("[error] yfs_client::setfile ec->setattr(%016llx, a) failed. err(%d)\n", id, r);
        return IOERR;
    }

    return OK;
}

int yfs_client::setdir(inum id, const dirinfo& din)
{
    int r;
    extent_protocol::attr a;

    a.atime = din.atime;
    a.mtime = din.mtime;
    a.ctime = din.ctime;

    r = ec->setattr(id, a);
    if (r != extent_protocol::OK)
    {
        printf("[error] yfs_client::setdir ec->setattr(%016llx, a) failed. err(%d)\n", id, r);
        return IOERR;
    }
    return OK;
}

int yfs_client::readfile(inum id, size_t size, size_t off, std::string &rbuf, size_t &bytes_read)
{
    int r;
    extent_protocol::attr a;
    std::string buf;

    r = ec->getattr(id, a);
    if (r != extent_protocol::OK)
    {
        printf("[error] yfs_client::readfile ec->getattr(%016llx, a) failed. err(%d)\n", id, r);
        return IOERR;
    }

    r = ec->get(id, buf);
    if (r != extent_protocol::OK)
    {
        printf("[error] yfs_client::readfile ec->get(%016llx, buf) failed. err(%d)\n", id, r);
        return IOERR;
    }

    if (a.size > buf.size())
    {
        buf.resize(a.size);
    }

    rbuf = buf.substr(off, size);
    bytes_read = rbuf.size();

    return OK;
}

int yfs_client::writefile(inum id, std::string wbuf, size_t size, size_t off, size_t &bytes_written)
{
    int r;
    extent_protocol::attr a;
    std::string buf;

    // get attr
    r = ec->getattr(id, a);
    if (r != extent_protocol::OK)
    {
        printf("[error] yfs_client::writefile ec->getattr(%016llx, a) failed. err(%d)\n", id, r);
        return IOERR;
    }

    // get content
    if (a.size > 0)
    {
        r = ec->get(id, buf);
        if (r != extent_protocol::OK)
        {
            printf("[error] yfs_client::writefile ec->get(%016llx, buf) failed. err(%d)\n", id, r);
            return IOERR;
        }
        buf.resize(a.size);
    }

    // resize if need, and rewrite content
    if (off + size > a.size)
    {
        buf.resize(off + size);
    }
    buf.replace(buf.begin() + off, buf.begin() + off + size, wbuf.begin(), wbuf.end());

    // put content
    r = ec->put(id, buf);
    if (r != extent_protocol::OK)
    {
        printf("[error] yfs_client::writefile ec->put(%016llx, buf) failed. err(%d)\n", id, r);
        return IOERR;
    }

    // set attr
    a.size = buf.size();
    r = ec->setattr(id, a);
    if (r != extent_protocol::OK)
    {
        printf("[error] yfs_client::writefile ec->setattr(%016llx, a) failed. err(%d)\n", id, r);
        return IOERR;
    }

    bytes_written = size;

    return OK;
}