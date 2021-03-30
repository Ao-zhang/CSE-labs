/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2020-11-26 22:06:55
 * @LastEditors: Seven
 * @LastEditTime: 2021-01-03 12:26:26
 */
#include "ydb_server_2pl.h"
#include "extent_client.h"
#define HASHSIZE INODE_NUM
#define REMOVESTRING "to->Remove"
#define TRANSBEGIN 1027
//#define DEBUG 1

inum hash_2pl(const std::string key)
{
	inum h = 0;
	uint32_t size = key.length();
	for (uint32_t i = 0; i < size; i++)
		h = key[i] + h * 31;
	return h % HASHSIZE;
}

//有cycle返回true，无cycle返回false
bool ydb_server_2pl::checkCycle(ydb_protocol::transaction_id id, inum ino)
{
	// printf("check cycle: \ttransaction:%d\t ino%d\n", id, ino);
	lc->acquire(DEPENDENCY);
	std::map<inum, ydb_protocol::transaction_id>::iterator iter = lock_handlers.find(ino);

	if (iter != lock_handlers.end())
	{
		//此锁已经被某个进程占有，由于check之前判断了不是自己拿的，所以不用判断是自己
		// printf("lock inum:%d \t transId:%d\n", ino, iter->second);
		ydb_protocol::transaction_id id2 = iter->second;
		assert(id2 < trans_id);
		if (dfs(id2, id))
		{
			lc->release(DEPENDENCY);
			return true; //加入会形成cycle
		}
		dependency[id][id2] = true; //加入不会形成cycle，但是会形成依赖。添加依赖关系
	}
	lc->release(DEPENDENCY);
	return false; //加入不会形成cycle
}

bool ydb_server_2pl::dfs(ydb_protocol::transaction_id id1, ydb_protocol::transaction_id id2)
{
	if (id1 == id2)
	{
		return true;
	}
	// printf("dfs:%d->%d\n", id1, id2);
	for (int i = 0; i < trans_id; i++)
	{
		if (dependency[id1][i] && i != id1) //i不能等于id1不然就会一直递归了
		{
			bool flag = dfs(i, id2);
			if (flag)
			{
				return true;
			}
		}
	}
	return false;
}

void ydb_server_2pl::release_locks(ydb_protocol::transaction_id id)
{
	// printf("2pl:release locks trans_id:%d\n", id);
	std::map<ydb_protocol::transaction_id, Transaction>::iterator iter;
	iter = transactionInfos.find(id);
	assert(iter != transactionInfos.end());
	lc->acquire(DEPENDENCY);
	for (int i = 0; i < trans_id; i++)
	{ //消除其他事务对此事物的依赖，以及此事物对其他事务的依赖
		dependency[id][i] = false;
		dependency[i][id] = false;
	}

	std::set<inum>::iterator l_iter;
	for (l_iter = iter->second.locks.begin(); l_iter != iter->second.locks.end(); l_iter++)
	{
		lock_handlers.erase(*l_iter);
		// printf("release lock inum:%d\n", *l_iter);
		lc->release(*l_iter);
	}
	lc->release(DEPENDENCY);
	transactionInfos.erase(iter);
}

ydb_server_2pl::ydb_server_2pl(std::string extent_dst, std::string lock_dst) : ydb_server(extent_dst, lock_dst)
{
	trans_id = 0;
	for (int i = 0; i < INODE_NUM; i++)
	{
		memset(dependency[i], 0, sizeof(bool) * INODE_NUM);
	}
}

ydb_server_2pl::~ydb_server_2pl()
{
}

ydb_protocol::status ydb_server_2pl::transaction_begin(int, ydb_protocol::transaction_id &out_id)
{ // the first arg is not used, it is just a hack to the rpc lib
	// lab3: your code here
	lc->acquire(TRANSBEGIN);
	out_id = trans_id++;
	lc->release(TRANSBEGIN);
	// printf("2pl:transaction:%d begin\n", out_id);
	Transaction newTrans;
	transactionInfos[out_id] = newTrans; //在事务管理记录中记录此事务
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::transaction_commit(ydb_protocol::transaction_id id, int &)
{
	// printf("2pl:transaction:%d commit\n", id);
	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{ //没有找到当前事务
		return ydb_protocol::TRANSIDINV;
	}
	else
	{
		// Transaction cur = iter->second;
		lc->acquire(DEPENDENCY);
		std::map<inum, std::string>::iterator v_iter;
		for (v_iter = iter->second.log.begin(); v_iter != iter->second.log.end(); v_iter++)
		{
			if (v_iter->second == REMOVESTRING)
			{
				ec->remove(v_iter->first);
			}
			else
			{
				if (v_iter->second.compare(REMOVESTRING) == 0)
				{
					ec->remove(v_iter->first);
				}
				else
					ec->put(v_iter->first, v_iter->second);
			}

			lock_handlers.erase(v_iter->first);
			lc->release(v_iter->first);
		}
		for (int i = 0; i < trans_id; i++)
		{ //消除其他事务对此事物的依赖，以及此事物对其他事务的依赖
			dependency[id][i] = false;
			dependency[i][id] = false;
		}
		transactionInfos.erase(iter);
		lc->release(DEPENDENCY);
	}

	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::transaction_abort(ydb_protocol::transaction_id id, int &)
{
	// printf("2pl:transaction:%d abort\n", id);
	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{ //没有找到当前事务
		return ydb_protocol::TRANSIDINV;
	}
	else
	{
		release_locks(id);
	}
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value)
{
	// lab3: your code here
	// printf("2pl:transaction:%d \t get:%s", id, key.data());
	std::map<ydb_protocol::transaction_id, Transaction>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{ //没有找到当前事务
		return ydb_protocol::TRANSIDINV;
	}
	else
	{
		inum ino = hash_2pl(key);
		// printf("2pl:transaction:%d \t get:%s inum %d\n", id, key.data(), ino);
		std::set<inum>::iterator l_iter = iter->second.locks.find(ino);
		if (l_iter != iter->second.locks.end())
		{
			//已经拿到了此锁
			std::map<inum, std::string>::iterator v_iter;
			v_iter = iter->second.log.find(ino);
			assert(v_iter != iter->second.log.end()); //一定能在log中找到数据
			out_value = v_iter->second;
		}
		else
		{ //没有拿到过此锁

			if (checkCycle(id, ino))
			{ //拿锁会造成死锁
				release_locks(id);
				return ydb_protocol::ABORT;
			}
			//依赖关系已经加入
			lc->acquire(ino);
			lock_handlers[ino] = id;
			iter->second.locks.insert(ino);
			ec->get(ino, out_value);
			std::string value = out_value;
			iter->second.log[ino] = value;
		}
		printf("2pl:transaction:%d \t get:%s\t inum %d\t value %s\n", id, key.data(), ino, out_value.data());
	}

	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &)
{

	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{ //没有找到当前事务
		return ydb_protocol::TRANSIDINV;
	}
	else
	{
		// Transaction cur = iter->second;
		inum ino = hash_2pl(key);
		// printf("2pl:transaction:%d \t set:%s\t inum %d\t value %s\n", id, key.data(), ino, value.data());
		std::set<inum>::iterator l_iter = iter->second.locks.find(ino);
		if (l_iter != iter->second.locks.end())
		{
			//已经拿到了此锁
			std::map<inum, std::string>::iterator v_iter;
			v_iter = iter->second.log.find(ino);
			assert(v_iter != iter->second.log.end()); //一定能在log中找到数据
			// iter->second.log[ino] = value;
			iter->second.log[ino] = value;
		}
		else
		{ //没有拿到过此锁

			if (checkCycle(id, ino))
			{ //拿锁会造成死锁
				release_locks(id);
				return ydb_protocol::ABORT;
			}
			//依赖关系已经加入
			lc->acquire(ino);
			lock_handlers[ino] = id;
			iter->second.locks.insert(ino);
			iter->second.log[ino] = value;
		}
		printf("2pl:transaction:%d \t set:%s\t inum %d\t value %s\n", id, key.data(), ino, value.data());
	}

	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_2pl::del(ydb_protocol::transaction_id id, const std::string key, int &)
{
	// lab3: your code here

	std::map<ydb_protocol::transaction_id, Transaction>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{ //没有找到当前事务
		return ydb_protocol::TRANSIDINV;
	}
	else
	{
		// Transaction cur = iter->second;
		inum ino = hash_2pl(key);
		printf("2pl:transaction:%d \t del:%s inum %d\n", id, key.data(), ino);
		std::set<inum>::iterator l_iter = iter->second.locks.find(ino);
		if (l_iter != iter->second.locks.end())
		{
			//已经拿到了此锁
			std::map<inum, std::string>::iterator v_iter;
			v_iter = iter->second.log.find(ino);
			assert(v_iter != iter->second.log.end()); //一定能在log中找到数据
			iter->second.log[ino] = REMOVESTRING;
		}
		else
		{ //没有拿到过此锁

			if (checkCycle(id, ino))
			{ //拿锁会造成死锁
				release_locks(id);
				return ydb_protocol::ABORT;
			}
			//依赖关系已经加入
			lc->acquire(ino);
			lock_handlers[ino] = id;
			iter->second.locks.insert(ino);
			iter->second.log[ino] = REMOVESTRING;
		}
		printf("2pl:transaction:%d \t del:%s\t inum %d \n", id, key.data(), ino);
	}

	return ydb_protocol::OK;
}
