/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2021-01-02 19:28:12
 * @LastEditors: Seven
 * @LastEditTime: 2021-01-03 12:10:56
 */
#include "rpc.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include "lock_server.h"
#include "lock_server_cache.h"
#include <unistd.h>
#include "jsl_log.h"

// Main loop of lock_server

int main(int argc, char *argv[])
{
  int count = 0;

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  srandom(getpid());

  if (argc != 2)
  {
    // fprintf(stderr, "Usage: %s port\n", argv[0]);
    exit(1);
  }

  char *count_env = getenv("RPC_COUNT");
  if (count_env != NULL)
  {
    count = atoi(count_env);
  }

  //jsl_set_debug(2);

// Lab2: uncomment this line when you begin to test lock_cache
// #define USE_LOCK_CACHE
#ifndef USE_LOCK_CACHE

  lock_server_cache lsc;
  rpcs server(atoi(argv[1]), count);
  server.reg(lock_protocol::stat, &lsc, &lock_server_cache::stat);
  server.reg(lock_protocol::release, &lsc, &lock_server_cache::release);
  server.reg(lock_protocol::acquire, &lsc, &lock_server_cache::acquire);

#endif

  while (1)
    sleep(1000);
}