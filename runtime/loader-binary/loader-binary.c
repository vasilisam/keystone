#include "loader/loader.h"
#include "mm/vm.h"
#include "mm/mm.h"
#include "mm/common.h"
#include "mm/freemem.h"
#include "util/printf.h"
#include <asm/csr.h>

/* root page table */
pte root_page_table_storage[BIT(RISCV_PT_INDEX_BITS)] __attribute__((aligned(RISCV_PAGE_SIZE)));
/* page tables for loading physical memory */
pte load_l2_page_table_storage[BIT(RISCV_PT_INDEX_BITS)] __attribute__((aligned(RISCV_PAGE_SIZE)));
pte load_l3_page_table_storage[BIT(RISCV_PT_INDEX_BITS)] __attribute__((aligned(RISCV_PAGE_SIZE)));

uintptr_t free_base_final = 0;

uintptr_t satp_new(uintptr_t pa)
{
  return (SATP_MODE | (pa >> RISCV_PAGE_BITS));
}

void map_physical_memory(uintptr_t dram_base, uintptr_t dram_size) {
  uintptr_t ptr = EYRIE_LOAD_START;
  /* load address should not override kernel address */
  assert(RISCV_GET_PT_INDEX(ptr, 1) != RISCV_GET_PT_INDEX(RUNTIME_VA_START, 1));
  printf("[loader] Start mapping EPM physical memory.\n");
  map_with_reserved_page_table(dram_base, dram_size,
      ptr, load_l2_page_table_storage, load_l3_page_table_storage);
}

int map_untrusted_memory(uintptr_t untrusted_ptr, uintptr_t untrusted_size) {
  uintptr_t va        = EYRIE_UNTRUSTED_START;
  while (va < EYRIE_UNTRUSTED_START + untrusted_size) {
    if (!map_page(vpn(va), ppn(untrusted_ptr), PTE_W | PTE_R | PTE_D)) {
      return -1;
    }
    va += RISCV_PAGE_SIZE;
    untrusted_ptr += RISCV_PAGE_SIZE;
  }
  return 0;
}

int load_runtime(uintptr_t dummy,
                uintptr_t dram_base, uintptr_t dram_size, 
                uintptr_t runtime_base, uintptr_t user_base, 
                uintptr_t free_base, uintptr_t untrusted_ptr, 
                uintptr_t untrusted_size) {
  int ret = 0;

  root_page_table = root_page_table_storage;

  printf("[loader] root_page_table: 0x%lx-0x%lx\n", (uintptr_t) root_page_table_storage, (uintptr_t) root_page_table_storage + RISCV_PAGE_SIZE);
  printf("[loader] l2_page_table  : 0x%lx-0x%lx\n", (uintptr_t) load_l2_page_table_storage, (uintptr_t) load_l2_page_table_storage + RISCV_PAGE_SIZE);
  printf("[loader] l3_page_table  : 0x%lx-0x%lx\n", (uintptr_t) load_l3_page_table_storage, (uintptr_t) load_l3_page_table_storage + RISCV_PAGE_SIZE);
 
  // initialize freemem
  spa_init(free_base, dram_base + dram_size - free_base);

  // validate runtime elf 
  size_t runtime_size = user_base - runtime_base;

  if (((void*) runtime_base == NULL) || (runtime_size <= 0)) {
    return -1; 
  }

  // create runtime elf struct
  elf_t runtime_elf;
  ret = elf_newFile((void*) runtime_base, runtime_size, &runtime_elf);
  if (ret != 0) {
    return ret;
  }
  
  printf("[loader] Before loading RT elf (%zu B)\n", runtime_size);
  printf("[loader] FreeMem: 0x%llx\n", dram_base + dram_size - spa_available() * RISCV_PAGE_SIZE);
  

  // map runtime memory
  ret = loadElf(&runtime_elf, 0);
  if (ret != 0) {
    return ret;
  }

  //print_page_table_recursive(root_page_table_storage, 2, 0);
  
  printf("[loader] After loading RT elf ....\n");
  printf("[loader] FreeMem: 0x%llx\n", dram_base + dram_size - spa_available() * RISCV_PAGE_SIZE);
  
  // map enclave physical memory, so that runtime will be able to access all memory
  map_physical_memory(dram_base, dram_size);

  printf("After mapping EPM....\n");
  printf("[loader] FreeMem: 0x%llx\n", dram_base + dram_size - spa_available() * RISCV_PAGE_SIZE);
  // map untrusted memory
  ret = map_untrusted_memory(untrusted_ptr, untrusted_size);
  if (ret != 0) {
    return ret;
  }

  free_base_final = dram_base + dram_size - spa_available() * RISCV_PAGE_SIZE;
  printf("After mapping Untrusted Memory....\n");
  printf("[loader] FreeMem: 0x%lx-0x%lx\n", (uintptr_t) free_base_final, (uintptr_t) dram_base + dram_size);
  return ret;
}

void error_and_exit() {
  printf("[loader] FATAL: failed to load.\n");
  sbi_exit_enclave(-1);
}

