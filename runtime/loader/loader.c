#include "loader/loader.h"
#include "loader/elf.h"
#include "string.h"
#include "mm/mm.h"
#include "mm/common.h"
#include "mm/vm_defs.h"
#include "mm/vm.h"

static inline int pt_mode_from_elf(int elf_pt_mode) {
  return 
    (((elf_pt_mode & PF_X) > 0) * PTE_X) |
    (((elf_pt_mode & PF_W) > 0) * (PTE_W | PTE_R | PTE_D)) |
    (((elf_pt_mode & PF_R) > 0) * PTE_R)
  ;
}

int loadElf(elf_t* elf, bool user) {
  for (unsigned int i = 0; i < elf_getNumProgramHeaders(elf); i++) {
    if (elf_getProgramHeaderType(elf, i) != PT_LOAD) {
      continue;
    }

    uintptr_t start      = elf_getProgramHeaderVaddr(elf, i);
    uintptr_t file_end   = start + elf_getProgramHeaderFileSize(elf, i);
    uintptr_t memory_end = start + elf_getProgramHeaderMemorySize(elf, i);
    char* src            = (char*)(elf_getProgramSegment(elf, i));
    uintptr_t va         = start;
    int pt_mode          = pt_mode_from_elf(elf_getProgramHeaderFlags(elf, i));
    pt_mode             |= (user > 0) * PTE_U;

    /* va is not page-aligned, so it doesn't own some of the page. Page may already be mapped. */
    if (RISCV_PAGE_OFFSET(va)) {
      if (RISCV_PAGE_OFFSET(va) != RISCV_PAGE_OFFSET((uintptr_t) src)) {
        printf("loadElf: va and src are misaligned\n");
        return -1;
      }
      uintptr_t new_page = alloc_page(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      memcpy((void *) (new_page + RISCV_PAGE_OFFSET(va)), src, RISCV_PAGE_SIZE - RISCV_PAGE_OFFSET(va));
      va = PAGE_DOWN(va) + RISCV_PAGE_SIZE;
      src = (char *) (PAGE_DOWN((uintptr_t) src) + RISCV_PAGE_SIZE);
    }

    /* first load all pages that do not include .bss segment */
    while (va + RISCV_PAGE_SIZE <= file_end) {
      uintptr_t src_pa = __pa((uintptr_t) src);
      if (!map_page(vpn(va), ppn(src_pa), pt_mode))
        return -1;
      src += RISCV_PAGE_SIZE;
      va += RISCV_PAGE_SIZE;
    }

    /* load the .bss segments */
    while (va < memory_end) {
      uintptr_t new_page = alloc_page(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      /* copy over non .bss part of the page if it's a part of the page */
      if (va < file_end) {
        memcpy((void*) new_page, src, file_end - va);
      }
      va += RISCV_PAGE_SIZE;
    }
  }

   return 0;
}

#ifdef MEGAPAGE_MAPPING
int loadElf_megapage(elf_t* elf, bool user) {
  for (unsigned int i = 0; i < elf_getNumProgramHeaders(elf); i++) {
    if (elf_getProgramHeaderType(elf, i) != PT_LOAD) {
      continue;
    }

    uintptr_t start      = elf_getProgramHeaderVaddr(elf, i);
    uintptr_t file_end   = start + elf_getProgramHeaderFileSize(elf, i);
    uintptr_t memory_end = start + elf_getProgramHeaderMemorySize(elf, i);
    char* src            = (char*)(elf_getProgramSegment(elf, i));
    uintptr_t va         = start;
    int pt_mode          = pt_mode_from_elf(elf_getProgramHeaderFlags(elf, i));
    pt_mode             |= (user > 0) * PTE_U;

    /* va is not page-aligned, so it doesn't own some of the page. Page may already be mapped. */ 
    if (RISCV_MEGAPAGE_OFFSET(va)) {
      /* Eapp's starting physical address while creating EPM is not MEGAPAGE-ALIGNED, 
         only PAGE-ALIGNED, so scr pointer cannot be MEGAPAGE_ALIGNED with va */
      if (RISCV_PAGE_OFFSET(va) != RISCV_PAGE_OFFSET((uintptr_t) src)) {
        printf("loadElf: va and src are misaligned\n");
        return -1;
      }
      uintptr_t new_page = alloc_megapage(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      /* .bss segment should always remain zeroed-out */
      /* If the remaining space in this 2 MiB page is more than what’s 
         left in the segment, you copy just up to file_end - va.*/
      size_t bytes_to_copy = (file_end > va + RISCV_MEGAPAGE_SIZE) \
                           ? (RISCV_MEGAPAGE_SIZE - RISCV_MEGAPAGE_OFFSET(va)) \
                           : (file_end - va);
      memcpy((void *) (new_page + RISCV_MEGAPAGE_OFFSET(va)), src, bytes_to_copy);
      va = MEGAPAGE_DOWN(va) + RISCV_MEGAPAGE_SIZE;
      src = (char *) (MEGAPAGE_DOWN((uintptr_t) src) + RISCV_MEGAPAGE_SIZE);
    }

     /* first load all pages that do not include .bss segment */
    while (va + RISCV_MEGAPAGE_SIZE <= file_end) {
      uintptr_t new_page = alloc_megapage(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      memcpy((void *) new_page, src, RISCV_MEGAPAGE_SIZE);
      src += RISCV_MEGAPAGE_SIZE;
      va += RISCV_MEGAPAGE_SIZE;
    }

    while (va < memory_end) {
      uintptr_t new_page = alloc_megapage(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      /* copy over non .bss part of the page if it's a part of the page */
      if (va < file_end) {
        memcpy((void*) new_page, src, file_end - va);
      }
      va += RISCV_MEGAPAGE_SIZE;
    }
  }

   return 0;
}
#endif

#ifdef GIGAPAGE_MAPPING
int loadElf_gigapage(elf_t* elf, bool user) {
  for (unsigned int i = 0; i < elf_getNumProgramHeaders(elf); i++) {
    if (elf_getProgramHeaderType(elf, i) != PT_LOAD) {
      continue;
    }

    uintptr_t start      = elf_getProgramHeaderVaddr(elf, i);
    uintptr_t file_end   = start + elf_getProgramHeaderFileSize(elf, i);
    uintptr_t memory_end = start + elf_getProgramHeaderMemorySize(elf, i);
    char* src            = (char*)(elf_getProgramSegment(elf, i));
    uintptr_t va         = start;
    int pt_mode          = pt_mode_from_elf(elf_getProgramHeaderFlags(elf, i));
    pt_mode             |= (user > 0) * PTE_U;

    if (RISCV_GIGAPAGE_OFFSET(va)) {
      if (RISCV_PAGE_OFFSET(va) != RISCV_PAGE_OFFSET((uintptr_t) src)) {
        printf("loadElf: va and src are misaligned\n");
        return -1;
      }
      uintptr_t new_page = alloc_gigapage(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      /* .bss segment should always remain zeroed-out */
      /* If the remaining space in this 1GiB page is more than what’s 
         left in the segment, you copy just up to file_end - va.*/
      size_t bytes_to_copy = (file_end > va + RISCV_GIGAPAGE_SIZE) \
                           ? (RISCV_GIGAPAGE_SIZE - RISCV_GIGAPAGE_OFFSET(va)) \
                           : (file_end - va);
      memcpy((void *) (new_page + RISCV_GIGAPAGE_OFFSET(va)), src, bytes_to_copy);
      va = GIGAPAGE_DOWN(va) + RISCV_GIGAPAGE_SIZE;
      src = (char *) (GIGAPAGE_DOWN((uintptr_t) src) + RISCV_GIGAPAGE_SIZE);
    }

     /* first load all pages that do not include .bss segment */
    while (va + RISCV_GIGAPAGE_SIZE <= file_end) {
      uintptr_t new_page = alloc_gigapage(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      memcpy((void *) new_page, src, RISCV_GIGAPAGE_SIZE);
      src += RISCV_GIGAPAGE_SIZE;
      va += RISCV_GIGAPAGE_SIZE;
    }

    while (va < memory_end) {
      uintptr_t new_page = alloc_gigapage(vpn(va), pt_mode);
      if (!new_page)
        return -1;
      /* copy over non .bss part of the page if it's a part of the page */
      if (va < file_end) {
        memcpy((void*) new_page, src, file_end - va);
      }
      va += RISCV_GIGAPAGE_SIZE;
    }
  }

   return 0;
}
#endif

// assumes beginning and next file are page-aligned
static inline void freeUnusedElf(elf_t* elf) {
  assert(false); // TODO: needs free to be implemented properly
  for (unsigned int i = 0; i < elf_getNumProgramHeaders(elf); i++) {
    uintptr_t start      = elf_getProgramHeaderVaddr(elf, i);
    uintptr_t file_end   = start + elf_getProgramHeaderFileSize(elf, i);
    uintptr_t src        = (uintptr_t) elf_getProgramSegment(elf, i);

    if (elf_getProgramHeaderType(elf, i) != PT_LOAD) {
      uintptr_t src_end = file_end - start + src;
      for (; src < src_end; src += RISCV_PAGE_SIZE) {
        // free_page(vpn(src));
      }
      continue;
    }

    if (RISCV_PAGE_OFFSET(start)) {
      // free_page(vpn(start));
    }

    if (RISCV_PAGE_OFFSET(file_end)) {
      // free_page(vpn(file_end));
    }
  }
}
