// extent wire protocol

#ifndef extent_protocol_h
#define extent_protocol_h

#include "rpc.h"

#include "stdio.h"

#define foreach(container,it) \
    for(typeof((container).begin()) it = (container).begin();it!=(container).end();++it)

class extent_protocol
{
public:
    typedef int status;
    typedef unsigned long long extentid_t;
    typedef std::vector< std::pair<extentid_t, std::string> > filelist;
    enum xxstatus { OK, RPCERR, NOENT, IOERR, FBIG};
    enum rpc_numbers
    {
        put = 0x6001,
        get,
        getattr,
        remove,
        setattr,
    };
    static const unsigned int maxextent = 8192 * 1000;

    struct attr
    {
        unsigned int atime;
        unsigned int mtime;
        unsigned int ctime;
        unsigned int size;
    };

    static time_t cur_sec()
    {
        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        return now.tv_sec;
    }


    static std::string serialize(filelist &fl)
    {
        std::string buf;
        unsigned int size = fl.size();
        buf.append((char *)&size, sizeof(unsigned int));
        foreach(fl, it)
        {
            size = it->second.size();
            buf.append((char *)&it->first, sizeof(extentid_t));
            buf.append((char *)&size, sizeof(unsigned int));
            buf.append(it->second.c_str(), it->second.size());
        }
        return buf;
    }

    static bool deserialize(std::string &buf, filelist &fl)
    {
        const char *cbuf = buf.c_str();
        unsigned int size_buf = buf.size();
        unsigned int size_fl = *(unsigned int *)cbuf;
        unsigned int p = 0;
        unsigned int size_name;
        extentid_t id;
        std::string name;


        if (size_fl * (sizeof(unsigned int) + sizeof(extentid_t)) + 4 > size_buf)
        {
            return false;
        }
        //printf("deserialize size = %d\n", size);
        p += sizeof(unsigned int);
        for (unsigned int i = 0; i < size_fl; i++)
        {
            id = *(extentid_t *)(cbuf + p);
            p += sizeof(extentid_t);
            if (p >= size_buf)
                return false;
            size_name = *(unsigned int *)(cbuf + p);
            p += sizeof(unsigned int);
            if (p >= size_buf)
                return false;
            name = std::string((cbuf + p), size_name);
            p += size_name;
            if (p >= size_buf + (i == size_fl - 1))
                return false;
            fl.push_back(std::make_pair<extentid_t, std::string>(id, name));
            //printf("id = %016llx name = %s\n", id, name.c_str());
        }
        return true;
    }
};

inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr &a)
{
    u >> a.atime;
    u >> a.mtime;
    u >> a.ctime;
    u >> a.size;
    return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr a)
{
    m << a.atime;
    m << a.mtime;
    m << a.ctime;
    m << a.size;
    return m;
}

#endif
