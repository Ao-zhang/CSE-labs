/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2020-11-26 22:06:55
 * @LastEditors: Seven
 * @LastEditTime: 2020-12-01 20:00:40
 */
#ifndef ydb_server_occ_h
#define ydb_server_occ_h

#include <string>
#include <map>
#include "extent_client.h"
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "ydb_protocol.h"
#include "ydb_server.h"
#define WRITESET 1026

struct Transaction_occ
{
	std::map<inum, std::string> read_log;
	std::map<inum, std::string> write_log;
};

class ydb_server_occ : public ydb_server
{
private:
	ydb_protocol::transaction_id trans_id;
	std::map<ydb_protocol::transaction_id, Transaction_occ> transactionInfos;

public:
	ydb_server_occ(std::string, std::string);
	~ydb_server_occ();
	ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
	ydb_protocol::status transaction_commit(ydb_protocol::transaction_id, int &);
	ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
	ydb_protocol::status get(ydb_protocol::transaction_id, const std::string, std::string &);
	ydb_protocol::status set(ydb_protocol::transaction_id, const std::string, const std::string, int &);
	ydb_protocol::status del(ydb_protocol::transaction_id, const std::string, int &);
};

#endif
