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
    // Lab2: Use lock_client_cache when you test lock_cache
    // lc = new lock_client(lock_dst);
    lc = new lock_client_cache(lock_dst);
    lc->acquire(1);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n");

    lc->release(1);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum)
{
    lc->acquire(inum);
    //printf("\tisfile acquire: %d\n", inum);
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {

        lc->release(inum);
        return false;
    }

    lc->release(inum);

    if (a.type == extent_protocol::T_FILE)
    {

        return true;
    }

    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?

    lc->acquire(inum);

    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {

        lc->release(inum);
        return false;
    }

    lc->release(inum);

    if (a.type == extent_protocol::T_DIR)
    {

        return true;
    }

    return false;
}

bool yfs_client::issymlink(inum inum)
{
    lc->acquire(inum);

    extent_protocol::attr a;

    //获取文件的属性attributes

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {

        lc->release(inum);
        return false;
    }

    lc->release(inum);
    if (a.type == extent_protocol::T_SYMLINK)
    {

        return true;
    }

    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    lc->acquire(inum);

    int r = OK;

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
    //printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    //printf("\tgetfile release: %d\n", inum);
    lc->release(inum);
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    lc->acquire(inum);
    //printf("\tgetdir acquire: %d\n", inum);
    int r = OK;

    //printf("getdir %016llx\n", inum);
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
    //printf("\tgetdir release: %d\n", inum);
    lc->release(inum);
    return r;
}

#define EXT_RPC(xx)                                                \
    do                                                             \
    {                                                              \
        if ((xx) != extent_protocol::OK)                           \
        {                                                          \
            printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
            r = IOERR;                                             \
            goto release;                                          \
        }                                                          \
    } while (0)

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size)
{
    lc->acquire(ino);
    //printf("\tsetattr acquire: %d\n", ino);
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    std::string buf;
    ec->get(ino, buf);

    buf.resize(size);
    ec->put(ino, buf);

    //printf("\tsetattr release: %d\n", ino);
    lc->release(ino);

    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    lc->acquire(parent);
    //printf("\tcreate release: %d\n", parent);
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found = false; //文件是否存在的标志
    inum inum;          //没啥用，但是调用函数需要
    std::string buf;

    lookup(parent, name, found, inum);
    if (found)
    {
        //已经存在
        //printf("create dup name\n");
        r = EXIST;
        goto release;
    }
    // ec->create(extent_protocol::T_FILE, ino_out);
    ec->createFileAndDir(extent_protocol::T_FILE, ino_out, name, parent);
release:
    //printf("\tcreate release: %d\n", parent);
    lc->release(parent);
    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    lc->acquire(parent);
    //printf("\tmkdir acquire: %d\n", parent);
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    //类似create file

    // For MKDIR, you do not have to create "." or ".." entries in the new directory since the Linux kernel handles them transparently to YFS
    bool found = false;
    inum inum;
    std::string buf;

    lookup(parent, name, found, inum);
    if (found)
    {
        //已经存在
        //printf("create dup name\n");
        r = EXIST;
        goto release;
    }

    ec->createFileAndDir(extent_protocol::T_DIR, ino_out, name, parent);
release:
    //printf("\tmkdir release: %d\n", parent);
    lc->release(parent);
    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::list<dirent> list;
    readdir(parent, list);

    if (list.empty())
    {
        found = false;
        return r;
    }

    for (std::list<dirent>::iterator it = list.begin(); it != list.end(); it++)
    {
        //找到与文件名对应的inode
        if (it->name.compare(name) == 0)
        {
            found = true;
            ino_out = it->inum;
            return r;
        }
    }

    found = false; //没有找到文件

    return r;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    std::string buf; //用于存dir的内容
    ec->get(dir, buf);

    //遍历目录的内容
    // name0&inum0 / name1&inum1
    int nm_start = 0;
    int nm_end = buf.find(":");
    while (nm_end != std::string::npos)
    {
        std::string name = buf.substr(nm_start, nm_end - nm_start);
        int inum_start = nm_end + 1;
        int inum_end = buf.find("/", inum_start);
        std::string inum = buf.substr(inum_start, inum_end - inum_start);

        yfs_client::dirent entry;
        entry.name = name;
        entry.inum = n2i(inum);

        list.push_back(entry);

        nm_start = inum_end + 1;
        nm_end = buf.find(":", nm_start);
    }

    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    lc->acquire(ino);
    //printf("\tread acquire: %d\n", ino);
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string buf;

    ec->get(ino, buf);
    size_t buf_size = buf.size();

    if (off > buf_size)
    {
        data = "";
    }
    else if (off + size > buf_size)
    {
        data = buf.substr(off);
    }
    else
    {
        data = buf.substr(off, size);
    }
    //printf("\tread release: %d\n", ino);
    lc->release(ino);
    return r;
}

int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written)
{
    lc->acquire(ino);
    //printf("\twrite acquire: %d\n", ino);
    int r = OK;
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    std::string buf;
    ec->get(ino, buf);
    size_t buf_size = buf.size();

    if (off + size > buf_size)
    {
        buf.resize(off + size);
        if (off > buf_size)
        {
            for (int j = buf_size; j < off; j++)
            {
                buf[j] = '\0';
            }
        }
    }
    for (int i = 0; i < size; i++)
    {
        buf[i + off] = data[i];
    }
    bytes_written = size;
    ec->put(ino, buf);
    //printf("\twrite release: %d\n", ino);
    lc->release(ino);
    return r;
}

int yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    lc->acquire(parent);
    //printf("\tsymlink acquire: %d\n", parent);
    int r = OK;

    // 检查link的name是否存在
    bool found = false;
    inum inum; // not necessary

    lookup(parent, name, found, inum);
    if (found)
    {
        // has existed
        //printf("\tsymlink release: %d\n", parent);
        lc->release(parent);
        return EXIST;
    }

    ec->createSymLink(ino_out, name, link, parent);

    //printf("\tsymlink release: %d\n", parent);
    lc->release(parent);
    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    lc->acquire(parent);
    //printf("\tunlink acquire: %d\n", parent);
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    bool found = false;
    inum inum;

    lookup(parent, name, found, inum);

    ec->remove(inum, parent, name);
    //printf("\tunlink release: %d\n", parent);
    lc->release(parent);
    return r;
}

int yfs_client::readlink(inum ino, std::string &data)
{
    int r = OK;

    std::string buf;
    lc->acquire(ino);
    //printf("\treadlink acquire: %d\n", ino);
    ec->get(ino, buf);

    data = buf;
    //printf("\treadlink release: %d\n", ino);
    lc->release(ino);
    return r;
}
