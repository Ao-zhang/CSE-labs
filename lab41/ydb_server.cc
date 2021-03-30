/*
 * @Description: 
 * @Version: 1.0
 * @Author: Zhang AO
 * @studentID: 518021910368
 * @School: SJTU
 * @Date: 2020-11-26 22:06:55
 * @LastEditors: Seven
 * @LastEditTime: 2020-11-30 11:12:43
 */
#include "ydb_server.h"
#include "extent_client.h"
#define HASHSIZE INODE_NUM

//#define DEBUG 1

static long timestamp(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}
inum hash(const std::string key)
{
	inum h = 0;
	uint32_t size = key.length();
	for (int i = 0; i < size; i++)
		h = key[i] + h * 31;
	return h % HASHSIZE;
}

ydb_server::ydb_server(std::string extent_dst, std::string lock_dst)
{
	ec = new extent_client(extent_dst);
	lc = new lock_client(lock_dst);
	//lc = new lock_client_cache(lock_dst);

	long starttime = timestamp();

	for (int i = 2; i < 1024; i++)
	{ // for simplicity, just pre alloc all the needed inodes
		extent_protocol::extentid_t id;
		ec->create(extent_protocol::T_FILE, id);
	}

	long endtime = timestamp();
	printf("time %ld ms\n", endtime - starttime);
}

ydb_server::~ydb_server()
{
	delete lc;
	delete ec;
}

ydb_protocol::status ydb_server::transaction_begin(int, ydb_protocol::transaction_id &out_id)
{ // the first arg is not used, it is just a hack to the rpc lib
	// no imply, just return OK
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::transaction_commit(ydb_protocol::transaction_id id, int &)
{
	// no imply, just return OK
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::transaction_abort(ydb_protocol::transaction_id id, int &)
{
	// no imply, just return OK
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::get(ydb_protocol::transaction_id id, const std::string key, std::string &out_value)
{
	// lab3: your code here
	inum ino = hash(key);
	printf("get->ino:%d\n", ino);
	ec->get(ino, out_value);
	printf("get->ino:%d\tvalue:%s\n", ino, out_value.data());
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::set(ydb_protocol::transaction_id id, const std::string key, const std::string value, int &)
{
	// lab3: your code here

	inum ino = hash(key);
	printf("set->ino:%d\n", ino);
	ec->put(ino, value);
	printf("set->ino:%d\tvalue:%s\n", ino, value.data());
	return ydb_protocol::OK;
}

ydb_protocol::status ydb_server::del(ydb_protocol::transaction_id id, const std::string key, int &)
{
	// lab3: your code here
	inum ino = hash(key);
	printf("del->ino:%d\n", ino);
	ec->remove(ino);
	printf("del->ino:%d\t success\n", ino);
	return ydb_protocol::OK;
}
