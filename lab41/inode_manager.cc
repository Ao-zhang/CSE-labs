#include "inode_manager.h"
#include "time.h"
// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks)); //将block全置零
}

void disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  for (int i = FILEBLOCK; i < BLOCK_NUM; i++)
  // i指向第一个fileblock，从头开始遍历，头部要跳过superblock，root block，以及，所以+3
  {
    if (!using_blocks[i])
    {
      using_blocks[i] = 1;
      return i;
    }
  }
  //printf("\tim: error! alloc_block failed!\n");
  return 0;
}

void block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  using_blocks[id] = 0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  /**********************************/
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
  for (uint i = 0; i < FILEBLOCK; i++)
    using_blocks[i] = 1;
  //前面的blocks: boot block, sb,bitmaps,和inode table都置1，因为已经被分配了
  d->write_block(1, (char *)&sb); //将sb的数据写入disk
}

void block_manager::read_block(uint32_t id, char *buf)
{

  d->read_block(id, buf);
}

void block_manager::write_block(uint32_t id, const char *buf)
{

  d->write_block(id, buf);
}

// void block_manager::print_status()
// {
//   for (int i = 0; i < BLOCK_NUM; i++)
//     if (using_blocks[i])
//       d->print_status(i);
// }

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1)
  {
    //printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

//获得inode中第n个block的id,返回block的索引
blockid_t
inode_manager::get_nth_blockid(inode_t *ino, uint32_t n)
{

  char buf[BLOCK_SIZE];
  blockid_t index;

  // assert(ino); //inode不能为空
  // //printf("\tim: get_nth_blockid %d\n", n);
  if (n < NDIRECT)
    index = ino->blocks[n];
  else if (n < MAXFILE) //大于NDIRECT小于MAXFILE
  {
    assert(ino->blocks[NDIRECT]);

    bm->read_block(ino->blocks[NDIRECT], buf);

    index = ((blockid_t *)buf)[n - NDIRECT];
  }
  else
  {
    //printf("\tim: get_nth_blockid out of range\n");
    exit(0);
  }

  return index;
}

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  int block_num = 0;
  int remain_size = 0;
  char buf[BLOCK_SIZE];
  int i = 0;

  // printf("\tim: read_file %d\n", inum);
  inode_t *ino = get_inode(inum);
  if (ino)
  {
    *size = ino->size;
    assert((unsigned int)*size <= MAXFILE * BLOCK_SIZE); //不能超过最大文件的大小
    *buf_out = (char *)malloc(*size);

    block_num = *size / BLOCK_SIZE;
    remain_size = *size % BLOCK_SIZE;
    while (i < block_num)
    {

      bm->read_block(get_nth_blockid(ino, i), buf);
      memcpy(*buf_out + i * BLOCK_SIZE, buf, BLOCK_SIZE);
      i++;
    }
    if (remain_size)
    { //不足一个block的部分
      bm->read_block(get_nth_blockid(ino, i), buf);
      memcpy(*buf_out + i * BLOCK_SIZE, buf, remain_size);
    }
    free(ino);
  }

  return;
}

void inode_manager::alloc_nth_block(inode_t *ino, uint32_t n)
{
  char buf[BLOCK_SIZE];
  assert(ino);
  // //printf("\tim: alloc_nth_block %d\n", n);

  if (n < NDIRECT)
    ino->blocks[n] = bm->alloc_block();
  else if (n < MAXFILE)
  {
    if (!ino->blocks[NDIRECT])
    {
      //printf("\tim: alloc_nth_block new NINDIRECT!\n");
      ino->blocks[NDIRECT] = bm->alloc_block();
    }
    bm->read_block(ino->blocks[NDIRECT], buf);
    ((blockid_t *)buf)[n - NDIRECT] = bm->alloc_block();
    bm->write_block(ino->blocks[NDIRECT], buf);
  }
  else
  {
    //printf("\tim: alloc_nth_block out of range\n");
    exit(0);
  }
}

/* alloc/free blocks if needed */
void inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */

  // 需要几个block
  int block_num = 0;
  //剩余不足一个block的部分
  int remain_size = 0;
  int old_blocks_num, new_blocks_num;
  char temp[BLOCK_SIZE];
  int i = 0;
  assert(size >= 0 && (uint32_t)size < MAXFILE * BLOCK_SIZE);
  //printf("\tim: write_file %d of size %d\n", inum, size);
  inode_t *ino = get_inode(inum);
  if (ino)
  {
    old_blocks_num = ino->size == 0 ? 0 : (ino->size - 1) / BLOCK_SIZE + 1;
    new_blocks_num = size == 0 ? 0 : (size - 1) / BLOCK_SIZE + 1;

    //旧的block size不够，alloc block
    if (old_blocks_num < new_blocks_num)
    {
      for (int j = old_blocks_num; j < new_blocks_num; j++)
      {
        alloc_nth_block(ino, j);
      }
    }
    //旧的block有多余的部分，free多余的block
    else if (old_blocks_num > new_blocks_num)
    {
      for (int j = new_blocks_num; j < old_blocks_num; j++)
      {
        bm->free_block(get_nth_blockid(ino, j));
      }
    }

    block_num = size / BLOCK_SIZE;
    remain_size = size % BLOCK_SIZE;

    while (i < block_num)
    {
      bm->write_block(get_nth_blockid(ino, i), buf + i * BLOCK_SIZE);
      i++;
    }

    if (remain_size)
    {
      memcpy(temp, buf + i * BLOCK_SIZE, remain_size);
      bm->write_block(get_nth_blockid(ino, i), temp);
    }
    ino->size = size;
    ino->atime = time(NULL);
    ino->mtime = time(NULL);
    ino->ctime = time(NULL);
    put_inode(inum, ino);
    free(ino);
  }

  return;
}

void inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  // printf("\tim: getattr %d\n", inum);
  inode_t *inode = get_inode(inum);
  if (!inode)
  {
    return;
  }
  a.type = inode->type;
  a.atime = inode->atime;
  a.mtime = inode->mtime;
  a.ctime = inode->ctime;
  a.size = inode->size;
  free(inode);
  return;
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  struct inode *tnode = NULL;
  for (uint32_t i = 1; i <= INODE_NUM; i++)
  {
    tnode = get_inode(i);

    if (tnode->type == 0)
    {
      tnode->type = (short)type;
      tnode->size = 0;
      tnode->atime = 0;
      tnode->ctime = 0;
      tnode->mtime = 0;
      put_inode(i, tnode);
      return i;
    }
  }
  return 1;
}

/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  // //printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM)
  {
    //printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  //读出inode对应的block中的数据
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode *)buf + inum % IPB;
  // if (ino_disk->type == 0)
  // {
  //   //printf("\tim: inode not exist\n");
  //   return NULL;
  // }

  ino = (struct inode *)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  // printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode *)buf + inum % IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

void inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  //printf("\tim: free_inode %d\n", inum);
  inode_t *inode = get_inode(inum);
  if (inode)
  {
    assert(inode->type != 0); //必须保证当前inode为已分配的
    inode->type = 0;
    inode->size = inode->atime = inode->ctime = inode->mtime = 0;
    put_inode(inum, inode);
    free(inode);
  }
  return;
}

void inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */

  //printf("\tim: remove_file %d\n", inum);
  inode_t *ino = get_inode(inum);
  assert(ino); //若inum超出范围，ino会是NULL
  size_t size = ino->size;
  int block_num = size == 0 ? 0 : (size - 1) / BLOCK_SIZE + 1;

  //清空blocks
  for (int i = 0; i < block_num; i++)
  {
    bm->free_block(get_nth_blockid(ino, i));
  }
  if (block_num > NDIRECT)
  {
    bm->free_block(ino->blocks[NDIRECT]);
  }
  bzero(ino, sizeof(inode_t));
  //清空inode
  free_inode(inum);
  //回收ino
  free(ino);

  return;
}