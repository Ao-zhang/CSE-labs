/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2021-01-02 19:28:13
 * @LastEditors: Seven
 * @LastEditTime: 2021-01-03 11:52:06
 */
// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
using namespace std;

class extent_client
{
private:
  rpcc *cl;
  int rextent_port;
  string hostname;
  string client_id;
  //对get进行缓存
  map<extent_protocol::extentid_t, std::string> dataCache;
  //对getattr进行缓存
  map<extent_protocol::extentid_t, extent_protocol::attr> attrCache;
  int option;
  extent_protocol::extentid_t optionId;

public:
  static int last_port;
  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status createFileAndDir(uint32_t type, extent_protocol::extentid_t &eid,
                                           const char *name, extent_protocol::extentid_t parent);
  extent_protocol::status createSymLink(extent_protocol::extentid_t &eid,
                                        const char *name, const char *link, extent_protocol::extentid_t parent);

  extent_protocol::status get(extent_protocol::extentid_t eid,
                              std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                  extent_protocol::attr &attr);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  // extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status remove(extent_protocol::extentid_t eid, extent_protocol::extentid_t parent, const char *name);

  extent_protocol::status remove_handler(extent_protocol::extentid_t eid, int &);

  extent_protocol::status update_handler(extent_protocol::extentid_t eid, extent_protocol::attr a,
                                         std::string data, int &);
};

#endif