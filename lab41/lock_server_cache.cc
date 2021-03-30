// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&mutex, NULL);
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &)
{
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab2 part3 code goes here
  pthread_mutex_lock(&mutex);
  int r;

  if (lockmap.find(lid) == lockmap.end())
  {
    lockmap[lid] = new lockinfo();
  }
  lockinfo *info = lockmap[lid];
  switch (info->stat)
  {
  case NONE:
    VERIFY(info->owner.empty());
    info->stat = LOCKED;
    info->owner = id;
    pthread_mutex_unlock(&mutex);
    break;
  case LOCKED:
    //waiting client之中没有此client！并且此lock拥有者不是这个client！
    VERIFY(!info->waiting_cids.count(id) && !info->owner.empty() && info->owner != id);

    info->waiting_cids.insert(id);
    info->stat = REVOKING;
    pthread_mutex_unlock(&mutex);
    handle(info->owner).safebind()->call(rlock_protocol::revoke, lid, r);
    ret = lock_protocol::RETRY;
    break;
  case REVOKING:
    //waiting client之中没有此client！
    VERIFY(!info->waiting_cids.count(id));
    info->waiting_cids.insert(id);
    pthread_mutex_unlock(&mutex);
    ret = lock_protocol::RETRY;
    break;
  case RETRYING:
    if (id == info->retrying_cid)
    {
      VERIFY(!info->waiting_cids.count(id)); //一个id只能存在于一个list
      info->retrying_cid.clear();
      info->stat = LOCKED;
      info->owner = id;
      if (!info->waiting_cids.empty())
      { //又有新的client进入waiting
        info->stat = REVOKING;
        pthread_mutex_unlock(&mutex);
        handle(id).safebind()->call(rlock_protocol::revoke, lid, r);
      }
      else
      {
        pthread_mutex_unlock(&mutex);
      }
    }
    else
    {
      VERIFY(!info->waiting_cids.count(id));
      info->waiting_cids.insert(id);
      pthread_mutex_unlock(&mutex);
      ret = lock_protocol::RETRY;
    }
    break;
  default:
    return lock_protocol::RPCERR;
  }
  return ret;
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // Your lab2 part3 code goes here
  pthread_mutex_lock(&mutex);
  lockinfo *info = lockmap[lid];
  VERIFY(info->stat == REVOKING);
  VERIFY(info->owner == id);

  string nextid = *(info->waiting_cids.begin());
  info->waiting_cids.erase(info->waiting_cids.begin());
  info->retrying_cid = nextid;
  info->owner.clear();
  info->stat = RETRYING;
  pthread_mutex_unlock(&mutex);
  handle(nextid).safebind()->call(rlock_protocol::retry, lid, r);

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

lock_server_cache::~lock_server_cache()
{
  pthread_mutex_lock(&mutex);
  //回收lockmap空间
  for (map<lock_protocol::lockid_t, lockinfo *>::iterator iter = lockmap.begin(); iter != lockmap.end(); iter++)
    delete iter->second;
  pthread_mutex_unlock(&mutex);
}