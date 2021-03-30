// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user
{
public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user(){};
};

class lock_client_cache : public lock_client
{
private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

  enum Message
  {
    EMPTY, //没有此锁
    RETYR, //服务器为获取RPC返回的值，以指示请求的锁当前不可用。
    REVOEK //表示有另一个client想申请此客户端的锁
  };
  enum State
  {
    NONE,      //客户端对此锁一无所知
    FREE,      //客户端拥有锁，但没有线程拥有该锁
    LOCKED,    //客户端拥有该锁，而一个线程拥有它
    ACQUIRING, //客户正在获得所有权
    RELEASEING //客户正在释放所有权
  };

  //每个线程有自己的条件变量，便于唤醒
  struct thread
  {
    pthread_cond_t cv;
    thread()
    {
      pthread_cond_init(&cv, NULL);
    }
  };

  //存储客户端中lock的信息,
  struct lockinfo
  {
    State stat;
    Message msg;

    std::list<thread *> threads;
    lockinfo()
    {
      stat = NONE;
      msg = EMPTY;
    }
  };

  pthread_mutex_t mutex;

  std::map<lock_protocol::lockid_t, lockinfo *> locksmap;
  lock_protocol::status wait_lock(lockinfo *,
                                  lock_protocol::lockid_t,
                                  thread *latest_thread);

public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t,
                                       int &);
};

#endif
