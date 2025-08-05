#ifdef USE_LINUX_SYSCALL

#define _GNU_SOURCE
#include "call/linux_wrap.h"

#include <signal.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <time.h>

#include "mm/freemem.h"
#include "mm/mm.h"
#include "util/rt_util.h"
#include "call/syscall.h"
#include "uaccess.h"

#define CLOCK_FREQ 1000000000

//TODO we should check which clock this is
uintptr_t linux_clock_gettime(__clockid_t clock, struct timespec *tp){
  print_strace("[runtime] clock_gettime not fully supported (clock %x, assuming)\r\n", clock);
  unsigned long cycles;
  __asm__ __volatile__("rdcycle %0" : "=r"(cycles));

  unsigned long sec = cycles / CLOCK_FREQ;
  unsigned long nsec = (cycles % CLOCK_FREQ);

  copy_to_user(&(tp->tv_sec), &sec, sizeof(unsigned long));
  copy_to_user(&(tp->tv_nsec), &nsec, sizeof(unsigned long));

  return 0;
}

uintptr_t linux_set_tid_address(int* tidptr_t){
  //Ignore for now
  print_strace("[runtime] set_tid_address, not setting address (%p), IGNORING\r\n",tidptr_t);
  return 1;
}

uintptr_t linux_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset){
  print_strace("[runtime] rt_sigprocmask not supported (how %x), IGNORING\r\n", how);
  return 0;
}

uintptr_t linux_RET_ZERO_wrap(unsigned long which){
  print_strace("[runtime] Cannot handle syscall %lu, IGNORING = 0\r\n", which);
  return 0;
}

uintptr_t linux_RET_BAD_wrap(unsigned long which){
  print_strace("[runtime] Cannot handle syscall %lu, FAILING = -1\r\n", which);
  return -1;
}

uintptr_t linux_getpid(){
  uintptr_t fakepid = 2;
  print_strace("[runtime] Faking getpid with %lx\r\n",fakepid);
  return fakepid;
}

uintptr_t linux_getrandom(void *buf, size_t buflen, unsigned int flags){

  uintptr_t ret = rt_util_getrandom(buf, buflen);
  print_strace("[runtime] getrandom IGNORES FLAGS (size %lx), PLATFORM DEPENDENT IF SAFE = ret %lu\r\n", buflen, ret);
  return ret;
}

#define UNAME_SYSNAME "Linux\0"
#define UNAME_NODENAME "Encl\0"
#define UNAME_RELEASE "5.16.0\0"
#define UNAME_VERSION "Eyrie\0"
#define UNAME_MACHINE "NA\0"

uintptr_t linux_uname(void* buf){
  // Here we go

  struct utsname *user_uname = (struct utsname *)buf;
  uintptr_t ret;

  ret = copy_to_user(&user_uname->sysname, UNAME_SYSNAME, sizeof(UNAME_SYSNAME));
  if(ret != 0) goto uname_done;

  ret = copy_to_user(&user_uname->nodename, UNAME_NODENAME, sizeof(UNAME_NODENAME));
  if(ret != 0) goto uname_done;

  ret = copy_to_user(&user_uname->release, UNAME_RELEASE, sizeof(UNAME_RELEASE));
  if(ret != 0) goto uname_done;

  ret = copy_to_user(&user_uname->version, UNAME_VERSION, sizeof(UNAME_VERSION));
  if(ret != 0) goto uname_done;

  ret = copy_to_user(&user_uname->machine, UNAME_MACHINE, sizeof(UNAME_MACHINE));
  if(ret != 0) goto uname_done;



 uname_done:
  print_strace("[runtime] uname = %x\n",ret);
  return ret;
}

uintptr_t syscall_munmap(void *addr, size_t length){
  uintptr_t ret = (uintptr_t)((void*)-1);
#if defined(MEGAPAGE_MAPPING)
  free_pages(vpn((uintptr_t)addr), MEGAPAGE_UP(length)/RISCV_MEGAPAGE_SIZE, true);
#elif defined(GIGAPAGE_MAPPING)
  free_pages(vpn((uintptr_t)addr), GIGAPAGE_UP(length)/RISCV_GIGAPAGE_SIZE, true);
#else
  free_pages(vpn((uintptr_t)addr), length/RISCV_PAGE_SIZE, false);
#endif
  ret = 0;
  tlb_flush();
  message("[runtime] munmapped was called.\n");
  print_page_table(root_page_table, 1, 0);
  return ret;
}

uintptr_t syscall_mmap(void *addr, size_t length, int prot, int flags,
                 int fd, __off_t offset){
  uintptr_t ret = (uintptr_t)((void*)-1);

  int pte_flags = PTE_U | PTE_A;

  if(flags != (MAP_ANONYMOUS | MAP_PRIVATE) || fd != -1){
    // we don't support mmaping any other way yet
    goto done;
  }

  // Set flags
  if(prot & PROT_READ)
    pte_flags |= PTE_R;
  if(prot & PROT_WRITE)
    pte_flags |= PTE_W | PTE_D;
  if(prot & PROT_EXEC)
    pte_flags |= PTE_X;

  bool is_largepage = false;

#if defined(MEGAPAGE_MAPPING)
  // Find a continuous VA space that will fit the req. size
  unsigned long req_pages = vpn(MEGAPAGE_UP(length));
  is_largepage = true;

  // Do we have enough available phys pages?
  //divide by 2^9 = 512 to get the required 2MiB megapages
  if ((req_pages >> RISCV_PT_INDEX_BITS) > spa_megapages_available()){
    goto done;
  }
#elif defined(GIGAPAGE_MAPPING)
  // Find a continuous VA space that will fit the req. size
  unsigned long req_pages = vpn(GIGAPAGE_UP(length));
  is_largepage = true;

  // Do we have enough available phys pages?
  //divide by 2^18 to get the required 1GiB pages
  if ((req_pages >> 2 * RISCV_PT_INDEX_BITS) > spa_gigapages_available()){
    goto done;
  }
#else
  unsigned long req_pages = vpn(PAGE_UP(length));

  if (req_pages > spa_available()){
    goto done;
  }
#endif

  uintptr_t valid_pages;
  // Start looking at  EYRIE_ANON_REGION_START for VA space
  uintptr_t starting_vpn = vpn(EYRIE_ANON_REGION_START);

  while((starting_vpn + req_pages) <= vpn(EYRIE_ANON_REGION_END)){
    valid_pages = test_va_range(starting_vpn, req_pages);

    if(req_pages == valid_pages){
      if (is_largepage) {
      #ifdef MEGAPAGE_MAPPING
        req_pages = req_pages >> RISCV_PT_INDEX_BITS;  // #required 2MiB megapages
      #else
        req_pages = req_pages >> 2 * RISCV_PT_INDEX_BITS;
      #endif
      }
      // Set a successful value if we allocate
      // TODO free partial allocation on failure
      if(alloc_pages(starting_vpn, req_pages, pte_flags, is_largepage) == req_pages)
      {
        ret = starting_vpn << RISCV_PAGE_BITS;
      }
      break;
    }
    else {
        if(!is_largepage) {
          starting_vpn = starting_vpn + valid_pages + 1;
        } else {
          #ifdef MEGAPAGE_MAPPING
            starting_vpn = vpn(MEGAPAGE_UP((starting_vpn + valid_pages + 1) << RISCV_PAGE_BITS));
          #else
            starting_vpn = vpn(GIGAPAGE_UP((starting_vpn + valid_pages + 1) << RISCV_PAGE_BITS));
          #endif
        }
    }
  }

 done:
  tlb_flush();
  message("[runtime] [mmap]: addr: 0x%p, length %lu, prot 0x%x, flags 0x%x, fd %i, offset %lu (%lu pages %x) = 0x%p\r\n", addr, length, prot, flags, fd, offset, req_pages, pte_flags, ret);
  print_page_table(root_page_table, 1, 0);
  
  // If we get here everything went wrong
  return ret;
}

uintptr_t syscall_mprotect(void *addr, size_t len, int prot) {
  print_strace("mprotect is called for %zu bytes starting at addr %p and for protection flags %d\n", len, addr, prot);
  int i, ret;
  size_t pages = len / RISCV_PAGE_SIZE;

  int pte_flags = PTE_U | PTE_A;
  if(prot & PROT_READ)
    pte_flags |= PTE_R;
  if(prot & PROT_WRITE)
    pte_flags |= (PTE_W | PTE_D);
  if(prot & PROT_EXEC)
    pte_flags |= PTE_X;

  for(i = 0; i < pages; i++) {
    ret = realloc_page(vpn((uintptr_t) addr) + i, pte_flags);
    if(!ret)
      return -1;
  }

  return 0;
}

uintptr_t syscall_brk(void* addr){
  // Two possible valid calls to brk we handle:
  // NULL -> give current break
  // ADDR -> give more pages up to ADDR if possible

  uintptr_t req_break = (uintptr_t)addr;

  uintptr_t current_break = get_program_break();
  uintptr_t ret = -1;
  unsigned long req_page_count = 0;
  bool is_largepage = false;

  // Return current break if null or current break
  if (req_break == 0) {
    ret = current_break;
    goto done;
  }

  if(req_break <= current_break){
    ret = req_break;
    goto done;
  }

  // Otherwise try to allocate pages

#ifdef MEGAPAGE_MAPPING
  //Pack smaller allocations into existing megapages until a new one is needed
  is_largepage = true;

  req_page_count = (MEGAPAGE_UP(req_break) - current_break) / RISCV_MEGAPAGE_SIZE;
  if (spa_megapages_available() < req_page_count){
    goto done;
  }

  //Align current break to a 2MiB-aligned address for correct 2MiB-mapping creation
  if( alloc_pages(vpn(MEGAPAGE_UP(current_break)),
                  req_page_count,
                  PTE_W | PTE_R | PTE_D | PTE_U | PTE_A, is_largepage)
      != req_page_count){
    goto done;
  }

  // Keep program break still page-aligned for minimum memory waste.
  // Page permissions remain the same among brk calls
  set_program_break(PAGE_UP(req_break));
  ret = req_break;
  goto done;
#endif

#ifdef GIGAPAGE_MAPPING
  is_largepage = true;

  req_page_count = (GIGAPAGE_UP(req_break) - current_break) / RISCV_GIGAPAGE_SIZE;
  if (spa_gigapages_available() < req_page_count){
    goto done;
  }

  //Align current break to a 1GiB-aligned address for correct 1GiB-mapping creation
  if( alloc_pages(vpn(GIGAPAGE_UP(current_break)),
                  req_page_count,
                  PTE_W | PTE_R | PTE_D | PTE_U | PTE_A, is_largepage)
      != req_page_count){
    goto done;
  }

  // Keep program break still page-aligned for minimum memory waste.
  // Page permissions remain the same among brk calls
  set_program_break(PAGE_UP(req_break));
  ret = req_break;
  goto done;
#endif
  
  // Fallback to 4KB mapping
  // Can we allocate enough phys pages?
  req_page_count = (PAGE_UP(req_break) - current_break) / RISCV_PAGE_SIZE;
  if (spa_available() < req_page_count){
    goto done;
  }

  // Allocate pages
  // TODO free pages on failure
  if( alloc_pages(vpn(current_break),
                  req_page_count,
                  PTE_W | PTE_R | PTE_D | PTE_U | PTE_A, is_largepage)
      != req_page_count){
    goto done;
  }

  // Success
  set_program_break(PAGE_UP(req_break));
  ret = req_break;


 done:
  tlb_flush();
  message("[runtime] brk (0x%p) (req pages %lu) = 0x%p, curr break = 0x%p\r\n",req_break, req_page_count, ret, get_program_break());
  print_page_table(root_page_table, 1, 0);
  return ret;

}
#endif /* USE_LINUX_SYSCALL */
