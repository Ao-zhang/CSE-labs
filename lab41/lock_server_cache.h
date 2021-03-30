#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <map>
#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_server_cache
{
private:
  int nacquire;
  enum State
  {
    NONE,     //没有客户端占有
    LOCKED,   //此锁目前只被一个client占有，没有waiting
    REVOKING, //正在从某个客户端申请撤回该锁，有waiting
    RETRYING, //正在尝试重新获得该锁
  };
  struct lockinfo
  {
    string owner;
    string retrying_cid;
    set<string> waiting_cids; //waiting client ids
    State stat;
    lockinfo()
    {
      stat = NONE;
    }
  };
  map<lock_protocol::lockid_t, lockinfo *> lockmap;
  pthread_mutex_t mutex;

public:
  lock_server_cache();
  ~lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
