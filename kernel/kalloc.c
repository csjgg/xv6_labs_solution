// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    initlock(&kmem[i].lock, "kmem");
    kmem[i].freelist = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  push_off();
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
  pop_off();
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  int cpun = cpuid();
  acquire(&kmem[cpun].lock);
  r->next = kmem[cpun].freelist;
  kmem[cpun].freelist = r;
  release(&kmem[cpun].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  struct run *tmp;
  push_off();
  int cpun = cpuid();
  acquire(&kmem[cpun].lock);
  r = kmem[cpun].freelist;
  if(r){
    kmem[cpun].freelist = r->next;
    release(&kmem[cpun].lock);
  }else{
    kmem[cpun].freelist = 0;
    release(&kmem[cpun].lock);
    for(int i = (cpun+1)%NCPU; i != cpun; i = ((i+1)%NCPU)){
      acquire(&kmem[i].lock);
      tmp = kmem[i].freelist;
      if(tmp){
        kmem[i].freelist = tmp->next;
        release(&kmem[i].lock);
        break;
      }else{
        release(&kmem[i].lock);
      }
    }
    r = tmp;
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}
