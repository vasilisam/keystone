#include <asm/csr.h>

#include "util/printf.h"
#include "sys/interrupt.h"
#include "call/syscall.h"
#include "mm/vm.h"
#include "util/string.h"
#include "call/sbi.h"
#include "mm/freemem.h"
#include "mm/mm.h"
#include "sys/env.h"
#include "mm/paging.h"
#include "loader/elf.h"
#include "loader/loader.h"

/* defined in vm.h */
extern uintptr_t shared_buffer;
extern uintptr_t shared_buffer_size;

/* initial memory layout */
uintptr_t utm_base;
size_t utm_size;
size_t eapp_elf_size;

/* defined in entry.S */
extern void* encl_trap_handler;

int verify_and_load_elf_file(uintptr_t ptr, size_t file_size, bool is_eapp) {
  int ret = 0;
  // validate elf 
  if (((void*) ptr == NULL) || (file_size <= 0)) {
    return -1; 
  }
  
  // create elf struct
  elf_t elf_file;
  ret = elf_newFile((void*) ptr, file_size, &elf_file);
  if (ret < 0) {
    return ret;
  }

  // parse and load elf file
  #ifdef MEGAPAGE_MAPPING
    ret = loadElf_megapage(&elf_file, 1);
  #else
    ret = loadElf(&elf_file, 1);
  #endif

  if (is_eapp) { // setup entry point
    uintptr_t entry = elf_getEntryPoint(&elf_file);
    csr_write(sepc, entry);
  }
  return ret;
}

/* initialize free memory with a simple page allocator*/
void
init_freemem()
{
  spa_init(freemem_va_start, freemem_size);
  #ifdef MEGAPAGE_MAPPING
  spa_init_megapage(freemem_va_start_2m, freemem_size_2m);
  #endif
}

/* initialize user stack */
void
init_user_stack_and_env(ELF(Ehdr) *hdr)
{
  void* user_sp = (void*) EYRIE_USER_STACK_START;
  size_t count;
  uintptr_t stack_end = EYRIE_USER_STACK_END;
  #ifdef MEGAPAGE_MAPPING
  size_t stack_count = EYRIE_USER_STACK_SIZE >> RISCV_MEGAPAGE_BITS;
  #else
  size_t stack_count = EYRIE_USER_STACK_SIZE >> RISCV_PAGE_BITS;
  #endif

  message("[runtime] Stack allocation begins (%zu pages).\n", stack_count);
  // allocated stack pages right below the runtime
  count = alloc_pages(vpn(stack_end), stack_count,
      PTE_R | PTE_W | PTE_D | PTE_A | PTE_U, 1);

  assert(count == stack_count);

  message("[runtime] Stack allocation ends.\n");
  // setup user stack env/aux
  user_sp = setup_start(user_sp, hdr);

  // prepare user sp
  csr_write(sscratch, user_sp);
}

void
eyrie_boot(uintptr_t dummy, // $a0 contains the return value from the SBI
           uintptr_t dram_base,
           uintptr_t dram_size,
           uintptr_t runtime_paddr,
           uintptr_t user_paddr,
           uintptr_t free_paddr,
           uintptr_t utm_vaddr,
           uintptr_t utm_size)
{
  #ifdef MEGAPAGE_MAPPING
    message("[runtime] MAPPING MODE = 2MiB PAGES.\n");
  #else 
    message("[runtime] MAPPING MODE = 4KiB PAGES.\n");
  #endif

  /* set initial values */
  load_pa_start = dram_base;
  root_page_table = (pte*) __va(csr_read(satp) << RISCV_PAGE_BITS);
  shared_buffer = EYRIE_UNTRUSTED_START;
  shared_buffer_size = utm_size;
  runtime_va_start = (uintptr_t) &rt_base;
  kernel_offset = runtime_va_start - runtime_paddr;

  message("[runtime] root_page_table: 0x%p-0x%p\n", (uintptr_t) root_page_table, (uintptr_t) root_page_table + RISCV_PAGE_SIZE);
  message("[runtime] UTM : 0x%p-0x%p (%u KB)\n", utm_vaddr, utm_vaddr+utm_size, utm_size/1024);
  message("[runtime] DRAM: 0x%p-0x%p (%u KB)\n", dram_base, dram_base + dram_size, dram_size/1024);
  message("[runtime] RT  : 0x%p-0x%p (%u KB)\n", runtime_paddr, user_paddr, (user_paddr-runtime_paddr)/1024);
  message("[runtime] Eapp: 0x%p-0x%p (%u KB)\n", user_paddr, free_paddr, (free_paddr-user_paddr)/1024);

  /* set trap vector */
  csr_write(stvec, &encl_trap_handler);
  freemem_va_start = __va(free_paddr);
  freemem_size = dram_base + dram_size - free_paddr;

  message("[runtime] FreeMem: 0x%p-0x%p (%u KB), va 0x%p\n", free_paddr, dram_base + dram_size, freemem_size/1024, freemem_va_start);

  eapp_elf_size = free_paddr - user_paddr;

  message("[runtime] Initialize Free Memory\n");
  
  /* To minimize free memory waste when allocating 2 MiB megapages (for code, heap, stack) 
     but still needing 4 KiB pages (for page tables or small allocations), we can use a dual 
     allocator strategy with 4KiB and 2 MiB alignment for Free Memory. Thus, we align Free 
     Memory to the next 2MiB-aligned address. The remaining pages between the later and 
     Free Memory base before the alignement shall be used as page tables for the Eapp's 
     code, stack and heap. For that purpose we need at least 3 pages that will function 
     as leaf page tables for these 3 segments of an Eapp and then for every 1GiB of heap 
     allocation we need another leaf page table. For now, we reserve at least 16 pages. */
  
  #ifdef MEGAPAGE_MAPPING
  free_2m = MEGAPAGE_UP(free_paddr);          // for 2MiB-aligned pages
  if ((freemem_size >> RISCV_PAGE_BITS) < 16) // we need at least 16 free 4KiB pages
    free_2m += RISCV_MEGAPAGE_SIZE;
  freemem_va_start_2m = __va(free_2m);
  freemem_size = free_2m - free_paddr;
  freemem_size_2m = dram_base + dram_size - free_2m;
  message("[runtime] 4KB-SPA: 0x%p-0x%p (%u KB), va 0x%p\n", free_paddr, free_paddr + freemem_size, freemem_size/1024, freemem_va_start);
  message("[runtime] 2MB-SPA: 0x%p-0x%p (%u KB), va 0x%p\n", free_2m, dram_base + dram_size, freemem_size_2m/1024, freemem_va_start_2m);
  #endif

  /* initialize free memory */
  init_freemem();

  /* load eapp elf */
  message("[runtime] Eapp elf loading begins.\n");

  assert(!verify_and_load_elf_file(__va(user_paddr), eapp_elf_size, true));
  
  message("[runtime] Eapp elf loading ends.\n");
  
  /* free leaking memory */
  // TODO: clean up after loader -- entire file no longer needed
  // TODO: load elf file doesn't map some pages; those can be re-used. runtime and eapp.

  // For setting properly the program break,
  // walk the userspace vm and find highest used addr.
  uintptr_t user_va_max = find_max_user_va(root_page_table);
  set_program_break(user_va_max);
  message("[runtime] Program break = %p\n", user_va_max);

  #ifdef USE_PAGING
  init_paging(user_paddr, free_paddr);
  #endif /* USE_PAGING */

  /* initialize user stack */
  init_user_stack_and_env((ELF(Ehdr) *) __va(user_paddr));

  print_page_table(root_page_table, 1, 0);

  /* prepare edge & system calls */
  init_edge_internals();

  /* set timer */
  init_timer();

  /* Enable the FPU */
  csr_write(sstatus, csr_read(sstatus) | 0x6000);

  message("[runtime] boot finished. drop to the user land ...\n");
  /* booting all finished, droping to the user land */
  return;
}
