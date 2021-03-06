#!/bin/bash
###
 # @Description: 
 # @Version: 1.0
 # @Author: Zhang AO
 # @studentID: 518021910368
 # @School: SJTU
 # @Date: 2021-01-02 16:55:42
 # @LastEditors: Seven
 # @LastEditTime: 2021-01-02 17:47:11
### 
#set -x

pkill -9 ydb_server
pkill -9 lock_server
pkill -9 extent_server

if ! [[ "$1" == "2PL" || "$1" == "OCC" || "$1" == "NONE" ]]; then
	echo "Usage: $0 [2PL/OCC/NONE]"
	exit 1
fi

sleep 1    # wait for kill

./extent_server 2772 >extent_server.log &
./lock_server 3772 >lock_server.log &
sleep 1
./ydb_server $1 4772 2772 3772 >ydb_server.log &
sleep 2    # enlarge the waiting time

