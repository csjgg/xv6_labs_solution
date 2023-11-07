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

#define NBUCKET 13
struct bucket{
  struct spinlock lock;
  struct buf* head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buck[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int n = 0; n < NBUCKET; n++){
    initlock(&bcache.buck[n].lock, "bcache");
    bcache.buck[n].head = 0;
  }
  int i = 0;
  int j = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++, i++) {
    if( i == NBUF/NBUCKET){
      i = 0;
      j = j+1>=NBUCKET?NBUCKET:j+1;
    }
    b->next = bcache.buck[j].head;
    bcache.buck[j].head = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int bufn = (dev*10 + blockno) % NBUCKET; 
  // Is the block already cached?
  for(int i = 0; i < NBUCKET; i++){
    acquire(&bcache.buck[(bufn+i)%NBUCKET].lock);
    struct buf* head = bcache.buck[(bufn+i)%NBUCKET].head;
    while(head){
      if(head->dev == dev && head->blockno == blockno){
        head->refcnt++;
        release(&bcache.buck[(bufn+i)%NBUCKET].lock);
        acquiresleep(&head->lock);
        return head;
      }
      head = head->next;
    }
    release(&bcache.buck[(bufn+i)%NBUCKET].lock);
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  for (int i = 0; i < NBUCKET; i++){
    acquire(&bcache.buck[(bufn+i)%NBUCKET].lock);
    struct buf* head = bcache.buck[(bufn+i)%NBUCKET].head;
    while(head){
      if(head->refcnt == 0){
        head->dev = dev;
        head->blockno = blockno;
        head->valid = 0;
        head->refcnt = 1;
        release(&bcache.buck[(bufn+i)%NBUCKET].lock);
        acquiresleep(&head->lock);
        return head;
      }
      head = head->next;
    }
    release(&bcache.buck[(bufn+i)%NBUCKET].lock);
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  b->refcnt--;
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


