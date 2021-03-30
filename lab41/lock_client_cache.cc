// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu)
{
  srand(time(NULL) ^ last_port);
  rlock_port = ((rand() % 32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
  pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  // Your lab2 part3 code goes here
  //加入mutex互斥锁保证一次只有一个线程申请
  pthread_mutex_lock(&mutex);
  lockinfo *l_info;
  if (locksmap.find(lid) == locksmap.end()) //客户端还未有当前锁
  {
    l_info = new lockinfo();
    locksmap[lid] = l_info;
  }
  else
    l_info = locksmap[lid];
  thread *lateset_thread = new thread();

  if (l_info->threads.empty())
  {
    State s = l_info->stat;
    l_info->threads.push_back(lateset_thread);
    if (s == FREE)
    { //没有线程占有该锁，获得该lock
      l_info->stat = LOCKED;
      goto locked;
    }
    else if (s == NONE)
    {
      return wait_lock(l_info, lid, lateset_thread);
    }
    else
    {
      pthread_cond_wait(&lateset_thread->cv, &mutex);
      return wait_lock(l_info, lid, lateset_thread);
    }
  }
  else
  {
    //客户端已有多个线程在等待此锁
    l_info->threads.push_back(lateset_thread);
    pthread_cond_wait(&lateset_thread->cv, &mutex); //挂起此线程
    switch (l_info->stat)
    {
    case FREE:
      l_info->stat = LOCKED;
    case LOCKED:
      goto locked;
    case NONE:
      return wait_lock(l_info, lid, lateset_thread);
    default:
      ret = lock_protocol::IOERR;
    }
  }
locked:
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  // Your lab2 part3 code goes here
  int r;
  bool revoked = false; //是否被撤回
  pthread_mutex_lock(&mutex);
  lockinfo *l_info = locksmap[lid];
  VERIFY(l_info != NULL);
  if (l_info->msg == REVOEK)
  {
    revoked = true;
    l_info->stat = RELEASEING;

    pthread_mutex_unlock(&mutex);
    //告诉server这边准备放锁了
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&mutex);
    l_info->msg = EMPTY;
    l_info->stat = NONE;
  }
  else
  {
    //因为此线程放锁了，stat变为FREE
    l_info->stat = FREE;
  }
  //在lock的状态中删除此线程
  delete l_info->threads.front();
  l_info->threads.pop_front();
  //如果有线程在等待
  if (l_info->threads.size() >= 1)
  {
    //且没有被要求撤回，就锁定此锁
    if (!revoked)
      l_info->stat = LOCKED;
    pthread_cond_signal(&l_info->threads.front()->cv);
  }
  pthread_mutex_unlock(&mutex);

  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                  int &)
{
  int ret = rlock_protocol::OK;
  // Your lab2 part3 code goes here
  int r;
  pthread_mutex_lock(&mutex);
  lockinfo *l_info = locksmap[lid];
  if (l_info->stat == FREE)
  {
    //lock没有被线程使用,就直接退锁
    l_info->stat = RELEASEING;
    pthread_mutex_unlock(&mutex);
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&mutex);
    l_info->stat = NONE;
    if (l_info->threads.size() >= 1) //在给server返回锁的时候有其他线程申请了此锁，唤醒其中一个线程，避免死锁
      pthread_cond_signal(&l_info->threads.front()->cv);
  }
  else
  { //不是FREE，在msg中记录已经收到REVOKE的要求
    l_info->msg = REVOEK;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                 int &)
{
  int ret = rlock_protocol::OK;
  // Your lab2 part3 code goes here
  //收到RETRY的信号，唤醒一个等待的线程，且在msg中记录RETRY
  pthread_mutex_lock(&mutex);
  lockinfo *l_info = locksmap[lid];
  l_info->msg = RETYR;
  //要先修改lock的信息再唤醒线程
  pthread_cond_signal(&l_info->threads.front()->cv);
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status lock_client_cache::
    wait_lock(lock_client_cache::lockinfo *info,
              lock_protocol::lockid_t lid,
              thread *latest_thread)
{
  int r;
  info->stat = ACQUIRING;
  while (info->stat == ACQUIRING)
  {
    pthread_mutex_unlock(&mutex);
    //尝试acquire
    int ret = cl->call(lock_protocol::acquire, lid, id, r);
    pthread_mutex_lock(&mutex);
    //如果获得lock
    if (ret == lock_protocol::OK)
    {
      info->stat = LOCKED;
      pthread_mutex_unlock(&mutex);
      return lock_protocol::OK;
    }
    else
    {
      if (info->msg == EMPTY)
        pthread_cond_wait(&latest_thread->cv, &mutex);
      info->msg = EMPTY;
    }
  }
  assert(0);
}

lock_client_cache::~lock_client_cache()
{
  pthread_mutex_lock(&mutex);
  for (std::map<lock_protocol::lockid_t, lockinfo *>::iterator iter = locksmap.begin(); iter != locksmap.end(); iter++)
  {
    delete iter->second;
  }
  pthread_mutex_unlock(&mutex);
}
