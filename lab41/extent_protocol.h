/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2021-01-02 19:28:13
 * @LastEditors: Seven
 * @LastEditTime: 2021-01-02 22:55:14
 */
// extent wire protocol

#ifndef extent_protocol_h
#define extent_protocol_h

#include "rpc.h"

class extent_protocol
{
public:
  typedef int status;
  typedef unsigned long long extentid_t;
  enum xxstatus
  {
    OK,
    RPCERR,
    NOENT,
    IOERR
  };
  enum rpc_numbers
  {
    put = 0x6001,
    get,
    getattr,
    remove,
    create,
    createSymLink
  };

  enum types
  {
    T_DIR = 1,
    T_FILE,
    T_SYMLINK
  };

  struct attr
  {
    uint32_t type;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    unsigned int size;
  };
};

class rextent_protocol
{
public:
  enum rpc_numbers
  {
    put_handler = 0x5001,
    remove_handler,
    update_handler
  };
};

inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr &a)
{
  u >> a.type;
  u >> a.atime;
  u >> a.mtime;
  u >> a.ctime;
  u >> a.size;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr a)
{
  m << a.type;
  m << a.atime;
  m << a.mtime;
  m << a.ctime;
  m << a.size;
  return m;
}

#endif
