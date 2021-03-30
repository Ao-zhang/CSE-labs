/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2020-11-26 22:06:55
 * @LastEditors: Seven
 * @LastEditTime: 2020-11-30 20:05:38
 */
#ifndef ydb_server_2pl_h
#define ydb_server_2pl_h

#include <string>
#include <map>
#include <set>
#include "extent_client.h"
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include "ydb_protocol.h"
#include "ydb_server.h"
#define DEPENDENCY 1025
typedef unsigned long long inum;
struct Transaction
{
	std::set<inum> locks;
	std::map<inum, std::string> log; //事务修改记录
};

class ydb_server_2pl : public ydb_server
{
private:
	//记录事务与其修改信息等
	bool dependency[INODE_NUM][INODE_NUM]; // dependency[x][y]=1表示x在等y
	ydb_protocol::transaction_id trans_id; //自增事务主键
	std::map<ydb_protocol::transaction_id, Transaction> transactionInfos;
	void release_locks(ydb_protocol::transaction_id id);
	bool checkCycle(ydb_protocol::transaction_id id, inum ino);
	bool dfs(ydb_protocol::transaction_id id1, ydb_protocol::transaction_id id2);
	std::map<inum, ydb_protocol::transaction_id> lock_handlers; //记录哪个锁由哪个事务占据
public:
	ydb_server_2pl(std::string, std::string);
	~ydb_server_2pl();
	ydb_protocol::status transaction_begin(int, ydb_protocol::transaction_id &);
	ydb_protocol::status transaction_commit(ydb_protocol::transaction_id, int &);
	ydb_protocol::status transaction_abort(ydb_protocol::transaction_id, int &);
	ydb_protocol::status get(ydb_protocol::transaction_id, const std::string, std::string &);
	ydb_protocol::status set(ydb_protocol::transaction_id, const std::string, const std::string, int &);
	ydb_protocol::status del(ydb_protocol::transaction_id, const std::string, int &);
};

#endif
