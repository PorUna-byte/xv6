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
#define NHBK 7
#define hash_fn(i) (i%NHBK)
extern uint ticks;
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
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
  //Assign buffer uniformly first
  int idx=0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->valid = 0;
    b->refcnt = 0;
    b->timestamp =0;
    initsleeplock(&b->lock, "buffer");
    b->next = buckets[idx].head.next;
    b->prev = &buckets[idx].head;
    buckets[idx].head.next->prev = b;
    buckets[idx].head.next = b;
    idx = (idx+1)%NHBK;
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
  //we will compare oldest with timestamp, which is a unsigned int, hence oldest will be viewed as an unsigned int
  //So -1 is the largest number in unsigned int! 
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
  release(&buckets[bktno].lock);
  //It is OK to serialize eviction in bget 
  //and the above release(&buckets[bktno].lock) may introduce a window for parallelism
  struct buf *lrub=0;
  int oldest = -1; 
  acquire(&bcache.lock);
  acquire(&buckets[bktno].lock);
  //so check again
  for(b = buckets[bktno].head.next; b != &buckets[bktno].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&buckets[bktno].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // search for free buffer from current bucket first.
  for(b = buckets[bktno].head.prev; b != &buckets[bktno].head; b = b->prev){
    if(b->refcnt == 0 && oldest >= b->timestamp) {
      oldest = b->timestamp;
      lrub = b;
    }
  }
  b = lrub;
  if(b){
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&buckets[bktno].lock);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  //Now we need to steal a lru buffer from other buckets
  int prob;
  for(int i=1;i<NHBK;i++){
    prob = hash_fn(bktno+i);
    acquire(&buckets[prob].lock);
    for(b=buckets[prob].head.next;b!=&buckets[prob].head;b=b->next){
      if(b->refcnt==0 && oldest>=b->timestamp){
        oldest=b->timestamp;
        lrub = b;
      }     
    }
    b=lrub;
    if(b){
      //remove the buffer from other bucket
      b->next->prev = b->prev;
      b->prev->next = b->next;

      //Add the buffer to the corresponding bucket
      b->next = buckets[bktno].head.next;
      b->next->prev = b;
      b->prev = &buckets[bktno].head;
      buckets[bktno].head.next = b;

      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      release(&buckets[prob].lock);
      release(&buckets[bktno].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    } 
    release(&buckets[prob].lock);
  }
  release(&buckets[bktno].lock);
  release(&bcache.lock);
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
  if(b->refcnt==0)
    b->timestamp=ticks;
  release(&buckets[bktno].lock);
}

void
bpin(struct buf *b) {
  uint bktno = hash_fn(b->blockno);
  acquire(&buckets[bktno].lock);
  b->refcnt++;
  b->timestamp=ticks;
  release(&buckets[bktno].lock);
}

void
bunpin(struct buf *b) {
  uint bktno = hash_fn(b->blockno);
  acquire(&buckets[bktno].lock);
  b->refcnt--;
  b->timestamp=ticks;
  release(&buckets[bktno].lock);
}


