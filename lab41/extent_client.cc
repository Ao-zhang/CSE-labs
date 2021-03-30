/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2021-01-02 19:28:13
 * @LastEditors: Seven
 * @LastEditTime: 2021-01-03 16:27:29
 */
// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int extent_client::last_port = 12345;

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0)
  {
    printf("extent_client: bind failed\n");
  }
  srand(time(NULL) ^ last_port);
  rextent_port = ((rand() % 32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname,   100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rextent_port;
  client_id = host.str();
  last_port = rextent_port;
  rpcs *server = new rpcs(rextent_port);
  server->reg(rextent_protocol::update_handler, this, &extent_client::update_handler);
  server->reg(rextent_protocol::remove_handler, this, &extent_client::remove_handler);
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  VERIFY(cl->call(extent_protocol::create, type, id) == ret);
  return ret;
}

extent_protocol::status
extent_client::createFileAndDir(uint32_t type, extent_protocol::extentid_t &eid,
                                const char *name, extent_protocol::extentid_t parent)
{
  cl->call(extent_protocol::create, type, client_id, parent, std::string(name), eid);
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::createSymLink(extent_protocol::extentid_t &eid,
                             const char *name, const char *link, extent_protocol::extentid_t parent)
{
  cl->call(extent_protocol::createSymLink, client_id, parent, std::string(name), std::string(link), eid);
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // VERIFY(cl->call(extent_protocol::get, eid, buf) == ret);
  // Your lab2 part1 code goes here
  if (dataCache.find(eid) == dataCache.end())
  {
    VERIFY(cl->call(extent_protocol::get, client_id, eid, buf) == ret);
    dataCache[eid] = buf;
  }
  else
  {
    buf = dataCache[eid];
  }

  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
                       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  // VERIFY(ret == cl->call(extent_protocol::getattr, eid, attr));
  if (attrCache.find(eid) == attrCache.end())
  {
    VERIFY(ret == cl->call(extent_protocol::getattr, client_id, eid, attr));
    attrCache[eid] = attr;
  }
  else
  {
    attr = attrCache[eid];
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  int r;
  // VERIFY(ret == cl->call(extent_protocol::put, eid, buf, r));
  VERIFY(ret == cl->call(extent_protocol::put, client_id, eid, buf, r));
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid, extent_protocol::extentid_t parent, const char *name)
{
  int r;
  cl->call(extent_protocol::remove, client_id, parent, std::string(name), eid, r);
  // dataCache.erase(eid);
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::remove_handler(extent_protocol::extentid_t eid, int &)
{
  dataCache.erase(eid);
  attrCache.erase(eid);
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::update_handler(extent_protocol::extentid_t eid, extent_protocol::attr a,
                              std::string data, int &)
{
  dataCache[eid] = data;
  attrCache[eid] = a;
  return extent_protocol::OK;
}