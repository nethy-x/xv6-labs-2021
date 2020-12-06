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

#define NBUCKETS 13

struct {
  struct buf buf[NBUF];
  struct spinlock buflock;


  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKETS];
  struct spinlock lock[NBUCKETS];
} bcache;

void
addticks(struct buf *b){
    acquire(&tickslock);
    b->ticks = ticks;
    release(&tickslock);
}

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.buflock, "bcache");
  for(int i = 0; i < NBUCKETS; i++){
      initlock(&bcache.lock[i], "bcachebucket");
      // Create linked list of buffers
      bcache.head[i].prev = &bcache.head[i];
      bcache.head[i].next = &bcache.head[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// moves buf from previous bucket to "bucket"
// locks of both have to be locked
void
move_bucket(struct buf *b, int bucket){
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[bucket].next;
    b->prev = &bcache.head[bucket];
    bcache.head[bucket].next->prev = b;
    bcache.head[bucket].next = b;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket = blockno % NBUCKETS;

  acquire(&bcache.lock[bucket]);

  // Is the block already cached?
  for(b = bcache.head[bucket].next; b != &bcache.head[bucket]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      addticks(b);
      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  acquire(&bcache.buflock);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // FIND LEAST USED BUFFER, CYCLE THROUGH ALL BUFFERS AND FIND A MINIMUM
  struct buf *minimum = 0;
  int minimum_bucket = 0;

  for(b = bcache.buf; b != bcache.buf + NBUF; b = b + 1){
     int current_bucket = b->blockno % NBUCKETS;
     if(current_bucket != bucket) {
         acquire(&bcache.lock[current_bucket]);
     }
     if(b->refcnt == 0){
         minimum = b;
         minimum_bucket = current_bucket;
         break;
     }
     if(current_bucket != bucket) {
         release(&bcache.lock[current_bucket]);
     }
 }
  if(minimum == 0){
      panic("bget: no buffers");
  }
 for(b = bcache.buf; b != bcache.buf + NBUF; b = b + 1) {
     int current_bucket = b->blockno % NBUCKETS;
     if((current_bucket != bucket) && (current_bucket != minimum_bucket)) {
         acquire(&bcache.lock[current_bucket]);
     }
     if((b->refcnt == 0) && (b->ticks < minimum->ticks)) {
         if((minimum_bucket != bucket) && (current_bucket != minimum_bucket)) {
             release(&bcache.lock[minimum_bucket]);
         }
         minimum = b;
         minimum_bucket = current_bucket;
     } else {
         if((current_bucket != bucket) && (current_bucket != minimum_bucket)) {
             release(&bcache.lock[current_bucket]);
         }
     }
 }
 if(minimum){
     minimum->dev = dev;
     minimum->blockno = blockno;
     minimum->valid = 0;
     minimum->refcnt = 1;
     addticks(minimum);
     if(minimum_bucket != bucket){
       move_bucket(minimum, bucket);
     }
     if(minimum_bucket != bucket) {
         release(&bcache.lock[minimum_bucket]);
     }
     release(&bcache.lock[bucket]);
     release(&bcache.buflock);
     acquiresleep(&minimum->lock);

     return minimum;
 }else {
     panic("bget: no buffers");
 }
     /*
     if(b->refcnt == 0) {
     b->dev = dev;
     b->blockno = blockno;
     b->valid = 0;
     b->refcnt = 1;
     if(current_bucket != bucket){
         move_bucket(b, bucket);
     }
     if(current_bucket != bucket){
         release(&bcache.lock[current_bucket]);
     }
     release(&bcache.lock[bucket]);
     release(&bcache.buflock);
     acquiresleep(&b->lock);
     return b;
   }
     if(current_bucket != bucket) {
         release(&bcache.lock[current_bucket]);
     }*/

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

  int bucket = b->blockno % NBUCKETS;
  acquire(&bcache.lock[bucket]);
  b->refcnt--;
//  if (b->refcnt == 0) {
//    // no one is waiting for it.
//    b->next->prev = b->prev;
//    b->prev->next = b->next;
//    b->next = bcache.head[bucket].next;
//    b->prev = &bcache.head[bucket];
//    bcache.head[bucket].next->prev = b;
//    bcache.head[bucket].next = b;
//  }
  addticks(b);
  release(&bcache.lock[bucket]);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % NBUCKETS;
  acquire(&bcache.lock[bucket]);
  b->refcnt++;
  release(&bcache.lock[bucket]);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % NBUCKETS;
  acquire(&bcache.lock[bucket]);
  b->refcnt--;
  release(&bcache.lock[bucket]);
}


