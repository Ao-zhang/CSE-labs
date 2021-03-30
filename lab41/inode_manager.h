// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h" // TODO: delete it
//磁盘大小，单位byte
#define DISK_SIZE 1024 * 1024 * 16
//block 大小，单位byte
#define BLOCK_SIZE 512
//磁盘里的block数量 2^15
#define BLOCK_NUM (DISK_SIZE / BLOCK_SIZE)

// <--boot block--><--super block--><--Bitmap for free blocks--><--inode table--><--FILE block_0 .....FILE block_(n-1)-->

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk
{
private:
  unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

public:
  disk();
  void read_block(uint32_t id, char *buf);
  //给出block的id，即第几个block，读出对应block的数据值
  void write_block(uint32_t id, const char *buf);
  //同上，写

  // void print_status(uint32_t id);
};

// block layer -----------------------------------------

typedef struct superblock
{
  //Size of file system image (blocks)
  // BLOCK_SIZE * BLOCK_NUM;
  //size of all the disk ,byte
  uint32_t size;
  //number of blocks
  // BLOCK_NUM;
  uint32_t nblocks;
  //number of blocks the inode uses
  //INODE_NUM;1024个
  uint32_t ninodes;
} superblock_t;

//super block在block的首位

class block_manager
{
private:
  disk *d;
  std::map<uint32_t, int> using_blocks;

public:
  block_manager();
  struct superblock sb;

  uint32_t alloc_block();
  void free_block(uint32_t id);
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
  // void print_status();
};

// inode layer -----------------------------------------

#define INODE_NUM 1024

// Inodes per block.
#define IPB 1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
//nblocks/ BPB
//返回inode i指向的block的下标
#define IBLOCK(i, nblocks) ((nblocks) / BPB + (i) / IPB + 3)

//一个block有多少bit (512 Bytes *8 bit/Byte)=4096 bits
#define BPB (BLOCK_SIZE * 8)

// Block containing bit for block b
#define BBLOCK(b) ((b) / BPB + 2)

#define NDIRECT 100
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT) //最大文件block数

#define FILEBLOCK IBLOCK(sb.ninodes, sb.nblocks) //返回fileblock的block下标

typedef struct inode
{
  short type;
  unsigned int size;
  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;
  blockid_t blocks[NDIRECT + 1]; // Data block addresses
} inode_t;

class inode_manager
{
private:
  block_manager *bm;
  struct inode *get_inode(uint32_t inum);
  void put_inode(uint32_t inum, struct inode *ino);
  blockid_t get_nth_blockid(inode_t *ino, uint32_t n);
  void alloc_nth_block(inode_t *ino, uint32_t n);

public:
  inode_manager();
  uint32_t alloc_inode(uint32_t type);
  void free_inode(uint32_t inum);
  void read_file(uint32_t inum, char **buf, int *size);
  void write_file(uint32_t inum, const char *buf, int size);
  void remove_file(uint32_t inum);
  void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif
