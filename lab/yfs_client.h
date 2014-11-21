#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client
{
    extent_client *ec;
public:

    typedef unsigned long long inum;
    enum xxstatus { OK, RPCERR, NOENT, IOERR, FBIG };
    typedef int status;

    struct fileinfo
    {
        unsigned long long size;
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirinfo
    {
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirent
    {
        std::string name;
        unsigned long long inum;
    };

private:
    static std::string filename(inum);
    static inum n2i(std::string);
public:

    static inum new_inum(unsigned int isfile)
    {
        return ((rand() % 0x7FFFFFFF) | (isfile << 31));
    }

    yfs_client(std::string, std::string);

    bool isfile(inum);
    bool isdir(inum);
    inum ilookup(inum di, std::string name);

    int getfile(inum, fileinfo &);
    int getdir(inum, dirinfo &);
    int setfile(inum, const fileinfo&);
    int setdir(inum, const dirinfo&);

    int lookup(inum p_id, std::string name, inum &id);
    int readdir(inum id, extent_protocol::filelist &);
    int createfile(inum p_id, std::string name, inum &id);

    int readfile(inum id, size_t size, size_t off, std::string &rbuf, size_t &bytes_read);
    int writefile(inum id, std::string buf, size_t size, size_t off, size_t &bytes_written);
};

#endif
