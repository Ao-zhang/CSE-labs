/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2021-01-02 16:55:42
 * @LastEditors: Seven
 * @LastEditTime: 2021-01-03 15:52:37
 */
// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <set>
#include <list>
#include "extent_protocol.h"
#include "inode_manager.h"
#define eid_list set<extent_protocol::extentid_t>
using namespace std;

class extent_server
{
protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;
  // 保存client IDs
  map<string, eid_list> clients;
  //get缓存
  map<extent_protocol::extentid_t, std::string> dataCache;
  // getattr缓存
  map<extent_protocol::extentid_t, extent_protocol::attr> attrCache;

  std::string filename(unsigned long long inum);
  void update_cache(extent_protocol::extentid_t id);
  void broadcast(extent_protocol::extentid_t id, bool isRemove = false);

public:
  extent_server();

  int create(uint32_t type, std::string client_id, extent_protocol::extentid_t parent,
             std::string name, extent_protocol::extentid_t &id);
  int createSymLink(std::string client_id, extent_protocol::extentid_t parent,
                    std::string name, std::string link, extent_protocol::extentid_t &id);
  int put(std::string client_id, extent_protocol::extentid_t id, std::string buf, int &);
  int get(std::string client_id, extent_protocol::extentid_t id, std::string &);
  int getattr(std::string clien_id, extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(std::string client_id, extent_protocol::extentid_t parent, std::string name,
             extent_protocol::extentid_t id, int &);
};

#endif
