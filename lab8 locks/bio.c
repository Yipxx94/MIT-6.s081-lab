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

struct {
  struct spinlock eviction_lock;    // 驱逐锁
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf bufmap[NBUFMAP_BUCKET];
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];
//  struct buf head;
} bcache;

void
binit(void)
{
//  struct buf *b;
  for (int i = 0; i < NBUFMAP_BUCKET; i ++ )
  {
      initlock(&bcache.bufmap_locks[i], "bcache_bufmap");
      bcache.bufmap[i].next = 0;
  }

  for (int i = 0; i < NBUF; i ++ )
  {
      struct buf *b = &bcache.buf[i];
      initsleeplock(&b->lock, "buffer");
      b->lastuse = 0;
      b->refcnt = 0;
      // 把所有的磁盘块放到bufmap[0]后面
      b->next = bcache.bufmap[0].next;
      bcache.bufmap[0].next = b;
  }
  initlock(&bcache.eviction_lock, "bcache_eviction");

  // Create linked list of buffers
/*  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
    */
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = BUFMAP_HASH(dev, blockno);

  acquire(&bcache.bufmap_locks[key]);

  // Is the block already cached?
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.

  release(&bcache.bufmap_locks[key]);

  acquire(&bcache.eviction_lock);

  // 再次检查
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno)
    {
      acquire(&bcache.bufmap_locks[key]);
      b->refcnt ++ ;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);
//      b->dev = dev;
//      b->blockno = blockno;
//      b->valid = 0;
//      b->refcnt = 1;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
      return b;
    }
  }


  // still not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf *before_least = 0;
  uint holding_bucket = -1;
  for (int i = 0; i < NBUFMAP_BUCKET; i ++ )
  {
    acquire(&bcache.bufmap_locks[i]);
    int newfound = 0; // new least-recently-used buf found in this bucket
    for(b = &bcache.bufmap[i]; b->next; b = b->next)
    {
      if(b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse))
      {
        before_least = b;
        newfound = 1;
      }
    }
    if(!newfound)
    {
      release(&bcache.bufmap_locks[i]);
    }
    else
    {
      if(holding_bucket != -1) release(&bcache.bufmap_locks[holding_bucket]);
      holding_bucket = i;
      // keep holding this bucket's lock....
    }
  }
  if(!before_least)
  {
    panic("bget: no buffers");
  }
  b = before_least->next;

  if(holding_bucket != key)
  {
    // remove the buf from it's original bucket
    before_least->next = b->next;
    release(&bcache.bufmap_locks[holding_bucket]);
    // rehash and add it to the target bucket
    acquire(&bcache.bufmap_locks[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);
  acquiresleep(&b->lock);
  return b;

//  panic("bget: no buffers");
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);

  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastuse = ticks;
    // no one is waiting for it.
//    b->next->prev = b->prev;
//    b->prev->next = b->next;
//    b->next = bcache.head.next;
//    b->prev = &bcache.head;
//    bcache.head.next->prev = b;
//    bcache.head.next = b;
  }
  
  release(&bcache.bufmap_locks[key]);
}

void
bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}


