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
  struct spinlock steal_lock;    // cpu偷页的锁
  struct run *freelist;
} kmem[NCPU];    // 为每个CPU分配独立的freelist，并用独立的锁保护它

char *kmem_locks[] = {
    "kmem_cpu_0",
    "kmem_cpu_1",
    "kmem_cpu_2",
    "kmem_cpu_3",
    "kmem_cpu_4",
    "kmem_cpu_5",
    "kmem_cpu_6",
    "kmem_cpu_7",
};

char *kmem_steal_locks[] = {
    "kmem_steal_0",
    "kmem_steal_1",
    "kmem_steal_2",
    "kmem_steal_3",
    "kmem_steal_4",
    "kmem_steal_5",
    "kmem_steal_6",
    "kmem_steal_7",
};

void
kinit()
{
  // 初始化所有的锁
  for (int i = 0; i < NCPU; i ++ )
  {
      initlock(&kmem[i].lock, "kmem_locks[i]");
      initlock(&kmem[i].steal_lock, "kmem_steal_locks[i]");
  }

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();    // 关闭中断

  int cpu = cpuid();    // 获取当前cpu的编号

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();    // 打开中断

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();    // 关闭中断

  int cpu = cpuid();    // 获取当前cpu的编号

  acquire(&kmem[cpu].lock);

  if (!kmem[cpu].freelist)    // 当前cpu没有空闲内存
  {
      acquire(&kmem[cpu].steal_lock);
      release(&kmem[cpu].lock);

      int steal_page = STEALPAGE;
      for (int i = 0; i < NCPU; i ++ )
      {
          if (i == cpu)
              continue;
          acquire(&kmem[i].lock);
          struct run *rr = kmem[i].freelist;
          while (rr && steal_page)
          {
              kmem[i].freelist = rr->next;
              rr->next = kmem[cpu].freelist;
              kmem[cpu].freelist = rr;
              rr = kmem[i].freelist;
              steal_page -- ;
          }
          release(&kmem[i].lock);
          if (!steal_page)
              break;
      }

      acquire(&kmem[cpu].lock);
      release(&kmem[cpu].steal_lock);
  }

  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);

  pop_off();    // 打开中断

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
