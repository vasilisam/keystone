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
}

/* initialize user stack */
void
init_user_stack_and_env(ELF(Ehdr) *hdr)
{
  void* user_sp = (void*) EYRIE_USER_STACK_START;
  size_t count;
  uintptr_t stack_end = EYRIE_USER_STACK_END;
  size_t stack_count = EYRIE_USER_STACK_SIZE >> RISCV_PAGE_BITS;

  printf("Eapp's stack %zu pages allocation\n", stack_count);
  // allocated stack pages right below the runtime
  count = alloc_pages(vpn(stack_end), stack_count,
      PTE_R | PTE_W | PTE_D | PTE_A | PTE_U);

  assert(count == stack_count);

  printf("Eapp's stack allocation finished.\n");
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
    printf("[runtime] 2MiB megapages used for loading and executing the Eapp.\n");
  #else 
    printf("[runtime] 4KiB pages used for loading and executing the Eapp.\n");
  #endif
  /* set initial values */
  load_pa_start = dram_base;
  root_page_table = (pte*) __va(csr_read(satp) << RISCV_PAGE_BITS);
  shared_buffer = EYRIE_UNTRUSTED_START;
  shared_buffer_size = utm_size;
  runtime_va_start = (uintptr_t) &rt_base;
  kernel_offset = runtime_va_start - runtime_paddr;

  printf("[runtime] root_page_table: 0x%lx-0x%lx\n", root_page_table, root_page_table + RISCV_PAGE_SIZE);
  printf("%d\n", RISCV_PAGE_SIZE);
  printf("[runtime] UTM : 0x%lx-0x%lx (%u KB)\n", utm_vaddr, utm_vaddr+utm_size, utm_size/1024);
  printf("[runtime] DRAM: 0x%lx-0x%lx (%u KB)\n", dram_base, dram_base + dram_size, dram_size/1024);
  printf("[runtime] RT  : 0x%lx-0x%lx (%u KB)\n", runtime_paddr, user_paddr, (user_paddr-runtime_paddr)/1024);
  printf("[runtime] Eapp: 0x%lx-0x%lx (%u KB)\n", user_paddr, free_paddr, (free_paddr-user_paddr)/1024);

  /* set trap vector */
  csr_write(stvec, &encl_trap_handler);
  freemem_va_start = __va(free_paddr);
  freemem_size = dram_base + dram_size - free_paddr;

  printf("[runtime] FreeMem: 0x%lx-0x%lx (%u KB), va 0x%lx\n", free_paddr, dram_base + dram_size, freemem_size/1024, freemem_va_start);

  eapp_elf_size = free_paddr - user_paddr;

  printf("Initialize Free Memory\n");
  
  // align Freemem to the next 2MiB-aligned address. That would be the 
  // starting address of Eapp Elf to be loaded. Substract 4KB for the 
  // leaf page table to be allocated
  
  #ifdef MEGAPAGE_MAPPING
  free_paddr = MEGAPAGE_UP(free_paddr);
  free_paddr -= RISCV_PAGE_SIZE;
  freemem_va_start = __va(free_paddr);
  freemem_size = dram_base + dram_size - free_paddr;
  #endif

  /* initialize free memory */
  init_freemem();

  printf("[runtime] FreeMem: 0x%lx-0x%lx (%u KB), va 0x%lx\n", free_paddr, dram_base + dram_size, freemem_size/1024, freemem_va_start);

  /* load eapp elf */
  printf("[runtime] Start loading Eapp elf.\n");

  assert(!verify_and_load_elf_file(__va(user_paddr), eapp_elf_size, true));
  
  printf("[runtime] Stopped loading Eapp elf.\n");
  
  print_page_table_recursive(root_page_table, 2, 0);
  
  /* free leaking memory */
  // TODO: clean up after loader -- entire file no longer needed
  // TODO: load elf file doesn't map some pages; those can be re-used. runtime and eapp.

  //TODO: This should be set by walking the userspace vm and finding
  //highest used addr. Instead we start partway through the anon space
  set_program_break(EYRIE_ANON_REGION_START + (1024 * 1024 * 1024));

  
  #ifdef USE_PAGING
  init_paging(user_paddr, free_paddr);
  #endif /* USE_PAGING */

  /* initialize user stack */
  init_user_stack_and_env((ELF(Ehdr) *) __va(user_paddr));

  /* prepare edge & system calls */
  init_edge_internals();

  /* set timer */
  init_timer();

  /* Enable the FPU */
  csr_write(sstatus, csr_read(sstatus) | 0x6000);

  //print_page_table_recursive(root_page_table, 2, 0);

  warn("boot finished. drop to the user land ...");
  /* booting all finished, droping to the user land */
  return;
}
