/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2020-11-26 22:06:55
 * @LastEditors: Seven
 * @LastEditTime: 2020-11-30 22:26:49
 */
#include "ydb_server_occ.h"
#include "extent_client.h"
#define HASHSIZE INODE_NUM
//#define DEBUG 1
inum hash_occ(const std::string key)
{
	inum h = 0;
	uint32_t size = key.length();
	for (uint32_t i = 0; i < size; i++)
		h = key[i] + h * 31;
	return h % HASHSIZE;
}

ydb_server_occ::ydb_server_occ(std::string extent_dst, std::string lock_dst) : ydb_server(extent_dst, lock_dst)
{
	trans_id = 0;
}

ydb_server_occ::~ydb_server_occ()
{
}

ydb_protocol::status ydb_server_occ::transaction_begin(int, ydb_protocol::transaction_id &out_id)
{ // the first arg is not used, it is just a hack to the rpc lib
	// lab3: your code here
	out_id = trans_id++;
	Transaction_occ newTrans;
	transactionInfos[out_id] = newTrans; //在事务管理记录中记录此事务
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::transaction_commit(ydb_protocol::transaction_id id, int &)
{
	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction_occ>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{
		return ydb_protocol::TRANSIDINV;
	}
	else
	{
		lc->acquire(WRITESET); //不能一边判断还又一边写
		std::map<inum, std::string>::iterator r_iter;
		for (r_iter = iter->second.read_log.begin(); r_iter != iter->second.read_log.end(); r_iter++)
		{
			std::string disk_value;
			ec->get(r_iter->first, disk_value);
			if (disk_value.compare(r_iter->second))
			{
				//不相等
				lc->release(WRITESET);
				transactionInfos.erase(iter);
				return ydb_protocol::ABORT;
			}
		}
		//上面已经验证过read set中的数据都相同
		std::map<inum, std::string>::iterator w_iter;
		for (w_iter = iter->second.write_log.begin(); w_iter != iter->second.write_log.end(); w_iter++)
		{
			ec->put(w_iter->first, w_iter->second);
		}
		lc->release(WRITESET);
	}
	transactionInfos.erase(iter);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::transaction_abort(ydb_protocol::transaction_id id, int &)
{
	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction_occ>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{
		return ydb_protocol::TRANSIDINV;
	}
	transactionInfos.erase(iter);
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value)
{
	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction_occ>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{
		return ydb_protocol::TRANSIDINV;
	}
	std::map<inum, std::string>::iterator r_iter;
	std::map<inum, std::string>::iterator w_iter;
	inum ino = hash_occ(key);
	r_iter = iter->second.read_log.find(ino);
	w_iter = iter->second.write_log.find(ino);
	bool r_find = (r_iter != iter->second.read_log.end());
	bool w_find = (w_iter != iter->second.write_log.end());
	if (w_find)
	{
		out_value = w_iter->second;
	}
	else if (r_find)
	{
		ec->get(ino, out_value);
		if (out_value.compare(r_iter->second) != 0)
		{
			// 前后读取数据不一致
			transactionInfos.erase(iter);
			return ydb_protocol::ABORT;
		}
	}
	else
	{
		ec->get(ino, out_value);
		std::string value = out_value;
		iter->second.read_log[ino] = value;
	}
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &)
{
	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction_occ>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{
		return ydb_protocol::TRANSIDINV;
	}
	inum ino = hash_occ(key);
	iter->second.write_log[ino] = value;
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server_occ::del(ydb_protocol::transaction_id id, const std::string key, int &)
{
	// lab3: your code here
	std::map<ydb_protocol::transaction_id, Transaction_occ>::iterator iter;
	iter = transactionInfos.find(id);
	if (iter == transactionInfos.end())
	{
		return ydb_protocol::TRANSIDINV;
	}
	inum ino = hash_occ(key);
	iter->second.write_log[ino] = "";
	return ydb_protocol::OK;
}
