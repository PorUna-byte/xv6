// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NHBK 13
#define hash_fn(i) (i%NHBK)
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf free_head;
} bcache;
typedef struct BUKT{
  struct spinlock lock;
  struct buf head;
  char name[16];
}Bucket;
Bucket buckets[NHBK]; 

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i=0;i<NHBK;i++){
    snprintf(buckets[i].name,16,"bcache_%d",i);
    initlock(&buckets[i].lock,buckets[i].name);
    buckets[i].head.prev = &buckets[i].head;
    buckets[i].head.next = &buckets[i].head;
  }

  //Create linked list of buffers
  bcache.free_head.prev = &bcache.free_head;
  bcache.free_head.next = &bcache.free_head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.free_head.next;
    b->prev = &bcache.free_head;
    initsleeplock(&b->lock, "buffer");
    bcache.free_head.next->prev = b;
    bcache.free_head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bktno = hash_fn(blockno);

  acquire(&buckets[bktno].lock);
  // Is the block already cached in the corresponding bucket?
  for(b = buckets[bktno].head.next; b != &buckets[bktno].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&buckets[bktno].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached in the corresponding bucket.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = buckets[bktno].head.prev; b != &buckets[bktno].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&buckets[bktno].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // we steal a buffer from global free list
  acquire(&bcache.lock);
  b = bcache.free_head.next;
  if(b!=&bcache.free_head){
    //remove the buffer from free list
    b->next->prev = b->prev;
    b->prev->next = b->next;
    //Add the buffer to corresponding bucket
    b->next = buckets[bktno].head.next;
    b->next->prev = b;
    b->prev = &buckets[bktno].head;
    buckets[bktno].head.next = b;

    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;

    release(&bcache.lock);
    release(&buckets[bktno].lock);
    acquiresleep(&b->lock);
    return b;
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  uint bktno = hash_fn(b->blockno);
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&buckets[bktno].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // put it on the free list
    acquire(&bcache.lock);
    b->next = bcache.free_head.next;
    b->next->prev = b;
    b->prev = &bcache.free_head;
    bcache.free_head.next = b;
    release(&bcache.lock);
  }
  release(&buckets[bktno].lock);
}

void
bpin(struct buf *b) {
  uint bktno = hash_fn(b->blockno);
  acquire(&buckets[bktno].lock);
  b->refcnt++;
  release(&buckets[bktno].lock);
}

void
bunpin(struct buf *b) {
  uint bktno = hash_fn(b->blockno);
  acquire(&buckets[bktno].lock);
  b->refcnt--;
  release(&buckets[bktno].lock);
}


