/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 1970-01-01 08:00:00
 * @LastEditors: Seven
 * @LastEditTime: 2021-01-03 16:14:25
 */
// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "handle.h"

extent_server::extent_server()
{
  im = new inode_manager();
}

std::string
extent_server::filename(unsigned long long inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

void extent_server::update_cache(extent_protocol::extentid_t id)
{
  id &= 0x7fffffff;
  int size = 0;
  char *cbuf = NULL;

  std::string buf;
  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else
  {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);

  dataCache[id] = buf;
  attrCache[id] = attr;
  //告诉其他client要更新buf
  broadcast(id);
}
//遍历clients查找需要更新buf的client
void extent_server::broadcast(extent_protocol::extentid_t id, bool isRemove)
{
  for (map<string, eid_list>::iterator it = clients.begin(); it != clients.end(); it++)
  {
    // printf("\tbroadcast to %s\n", it->c_str());
    if (it->second.count(id) > 0)
    {
      handle h(it->first);
      if (h.safebind())
      {
        int r;
        if (isRemove)
          h.safebind()->call(rextent_protocol::remove_handler, id, r);
        else
          h.safebind()->call(rextent_protocol::update_handler, id, attrCache[id], dataCache[id], r);
      }
    }
  }
}

int extent_server::create(uint32_t type, std::string client_id, extent_protocol::extentid_t parent, std::string name, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  // printf("extent_server: create inode\n");
  id = im->alloc_inode(type);
  if (id == INODE_NUM)
    return extent_protocol::NOENT;
  string buf = dataCache[parent];
  buf.append(name + ':' + filename(id) + '/');
  im->write_file(parent, buf.c_str(), buf.size());

  update_cache(parent);
  update_cache(id);

  clients[client_id].insert(id);
  return extent_protocol::OK;
}

int extent_server::createSymLink(string client_id, extent_protocol::extentid_t parent,
                                 std::string name, std::string link, extent_protocol::extentid_t &id)
{
  id = im->alloc_inode(extent_protocol::T_SYMLINK);
  im->write_file(id, link.c_str(), link.size());

  string buf = dataCache[parent];
  buf.append(std::string(name) + ":" + filename(id) + "/");
  im->write_file(parent, buf.c_str(), buf.size());

  update_cache(parent);
  update_cache(id);

  clients[client_id].insert(id);
  return extent_protocol::OK;
}

int extent_server::put(string client_id, extent_protocol::extentid_t id, std::string buf, int &)
{
  id &= 0x7fffffff;

  const char *cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  update_cache(id);
  clients[client_id].insert(id);

  return extent_protocol::OK;
}

int extent_server::get(string client_id, extent_protocol::extentid_t id, std::string &buf)
{
  // printf("extent_server: get %lld\n", id);
  buf = dataCache[id];

  // id &= 0x7fffffff;
  // int size = 0;
  // char *cbuf = NULL;
  // im->read_file(id, &cbuf, &size);
  // if (size == 0)
  //   buf = "";
  // else
  // {
  //   buf.assign(cbuf, size);
  //   free(cbuf);
  // }
  clients[client_id].insert(id);

  return extent_protocol::OK;
}

int extent_server::getattr(string client_id, extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  a = attrCache[id];
  // printf("extent_server: getattr %lld\n", id);

  // id &= 0x7fffffff;

  // extent_protocol::attr attr;
  // memset(&attr, 0, sizeof(attr));
  // im->getattr(id, attr);
  // a = attr;
  clients[client_id].insert(id);
  return extent_protocol::OK;
}

int extent_server::remove(string client_id, extent_protocol::extentid_t parent, string name, extent_protocol::extentid_t id, int &)
{
  // printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);

  string buf = dataCache[parent];
  int erase_start = buf.find(name);
  int erase_end = buf.find('/', erase_start);
  buf.erase(erase_start, erase_end - erase_start + 1);
  im->write_file(parent, buf.c_str(), buf.size());

  update_cache(parent);
  dataCache.erase(id);
  attrCache.erase(id);
  broadcast(id, true);
  clients[client_id].insert(id);

  return extent_protocol::OK;
}
