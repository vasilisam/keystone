#include "mm/common.h"
#include "mm/mm.h"
#include "mm/vm.h"
#include "mm/freemem.h"
#include "mm/paging.h"

/* Page table utilities */
static pte*
__walk_create(pte* root, uintptr_t addr, int page_table_levels);

/* Hacky storage of current u-mode break */
static uintptr_t current_program_break;

void print_page_table_recursive(pte* table, int level, uintptr_t vbase) {
  for (int i = 0; i < 512; i++) {
    if ((table[i] & PTE_V)) {//} && (table[i] & PTE_U)) {
      uintptr_t vpn = vbase | ((uintptr_t)i << (RISCV_PAGE_BITS + RISCV_PT_INDEX_BITS * level));
      //printf("L%d: VPN 0x%lx -> PTE 0x%lx ", level, vpn >> RISCV_PAGE_BITS, table[i]);

      if (table[i] & (PTE_R | PTE_W | PTE_X)) {  // if it's a leaf (RWX present) in U-space
        if (!(table[i] & PTE_U))
          continue;   // skip kernel leaf mappings
        uintptr_t ppn = pte_ppn(table[i]);
        printf("L%d: VPN = 0x%03lx -> PTE 0x%lx ", level, RISCV_GET_PT_INDEX(vpn, RISCV_PT_LEVELS - level), table[i]);
        printf("-> PA 0x%lx\n", ppn << RISCV_PAGE_BITS);
      } else {
        //if it's an intermediate page table
        uintptr_t next_table_pa = pte_ppn(table[i]) << RISCV_PAGE_BITS;
        pte* next_table = (pte*)__va(next_table_pa);
        printf("L%d: VPN = 0x%03lx -> PTE 0x%lx ", level, RISCV_GET_PT_INDEX(vpn, RISCV_PT_LEVELS - level), table[i]);
        printf("-> Next table @ PA 0x%lx (VA %p)\n", next_table_pa, next_table);
        print_page_table_recursive(next_table, level - 1, vpn);
      }
    }
  }
}

uintptr_t get_program_break()
{
  return current_program_break;
}

void set_program_break(uintptr_t new_break)
{
  current_program_break = new_break;
}

static pte*
__continue_walk_create(pte* root, uintptr_t addr, pte* pte, int page_table_levels)
{
  uintptr_t new_page = spa_get_zero();
  assert(new_page);

  unsigned long free_ppn = ppn(__pa(new_page));
  *pte = ptd_create(free_ppn);

  printf("New PTE: 0x%p at 0x%p\n", *pte, pte);
  printf("4KB-Page allocated from FreeMem at 0x%llx\n", free_ppn << RISCV_PAGE_BITS);
  return __walk_create(root, addr, page_table_levels);
}

static pte*
__walk_internal(pte* root, uintptr_t addr, int create, int page_table_levels)
{
  pte* t = root;
  int i;
  for (i = 1; i < page_table_levels; i++)
  {
    size_t idx = RISCV_GET_PT_INDEX(addr, i);

    if (page_table_levels == 2)
      printf("Page Level: %d page table t = %p at index %zu\n", i, t, idx);
    if (!(t[idx] & PTE_V))
      return create ? __continue_walk_create(root, addr, &t[idx], page_table_levels) : 0;

    t = (pte*) __va(pte_ppn(t[idx]) << RISCV_PAGE_BITS);
    if (page_table_levels == 2)
      printf("page table points to 0x%p\n", t);
  }

  return &t[RISCV_GET_PT_INDEX(addr, page_table_levels)];
}

/* walk the page table and return PTE
 * return 0 if no mapping exists */
static pte*
__walk(pte* root, uintptr_t addr)
{
  return __walk_internal(root, addr, 0, 3);
}

/* walk the page table and return PTE
 * create the mapping if non exists */
static pte*
__walk_create(pte* root, uintptr_t addr, int page_table_levels)
{
  return __walk_internal(root, addr, 1, page_table_levels);
}

/* Create a virtual memory mapping between a physical and virtual page */
uintptr_t 
map_page(uintptr_t vpn, uintptr_t ppn, int flags)
{
  pte* pte = __walk_create(root_page_table, vpn << RISCV_PAGE_BITS, 3);

  // TODO: what is supposed to happen if page is already allocated?
  if (*pte & PTE_V) {
    return -1;
  }

  *pte = pte_create(ppn, PTE_D | PTE_A | PTE_V | flags);
  return 1;
}

/* allocate a new page to a given vpn
 * returns VA of the page, (returns 0 if fails) */
uintptr_t
alloc_page_generic(uintptr_t vpn, int flags, int page_table_levels)
{
  uintptr_t page;
  pte* pte = __walk_create(root_page_table, vpn << RISCV_PAGE_BITS, page_table_levels);

  if (!pte)
    return 0;

	/* if the page has been already allocated, return the page */
  if(*pte & PTE_V) {
    //return __va(*pte << RISCV_PAGE_BITS); // repo's code
    return __va(pte_ppn(*pte) << RISCV_PAGE_BITS);
  }

	/* otherwise, allocate one from the freemem */
#ifdef MEGAPAGE_MAPPING 
  if (page_table_levels == 2) {
    page = spa_get_zero_megapage();
  } else
#endif
  {
    page = spa_get_zero();
  }
  assert(page);

  *pte = pte_create(ppn(__pa(page)), PTE_D | PTE_A | PTE_V | flags);

#ifdef MEGAPAGE_MAPPING 
  if (page_table_levels == 2)
    printf("New PTE: 0x%p at 0x%p\n", *pte, pte);
#endif

#ifdef USE_PAGING
  paging_inc_user_page();
#endif

  return page;
}

uintptr_t
realloc_page(uintptr_t vpn, int flags)
{
  assert(flags & PTE_U);

  pte *pte = __walk(root_page_table, vpn << RISCV_PAGE_BITS);
  if(!pte)
    return 0;

  if(*pte & PTE_V) {
    *pte = pte_create(pte_ppn(*pte), flags);
    return __va(*pte << RISCV_PAGE_BITS);
  }

  return 0;
}

void
free_page(uintptr_t vpn)
{

  pte* pte = __walk(root_page_table, vpn << RISCV_PAGE_BITS);

  // No such PTE, or invalid
  if(!pte || !(*pte & PTE_V))
    return;

  assert(*pte & PTE_U);

  uintptr_t ppn = pte_ppn(*pte);
  // Mark invalid
  // TODO maybe do more here
  *pte = 0;

#ifdef USE_PAGING
  paging_dec_user_page();
#endif
  // Return phys page
  spa_put(__va(ppn << RISCV_PAGE_BITS), 1);

  return;

}

/* allocate n new pages from a given vpn
 * returns the number of pages allocated */
size_t
alloc_pages(uintptr_t vpn, size_t count, int flags)
{
  unsigned int i;
  for (i = 0; i < count; i++) {
    if(!alloc_page(vpn + i, flags))
      break;
  }

  return i;
}

void
free_pages(uintptr_t vpn, size_t count){
  unsigned int i;
  for (i = 0; i < count; i++) {
    free_page(vpn + i);
  }

}

/*
 * Check if a range of VAs contains any allocated pages, starting with
 * the given VA. Returns the number of sequential pages that meet the
 * conditions.
 */
size_t
test_va_range(uintptr_t vpn, size_t count){

  unsigned int i;
  /* Validate the region */
  for (i = 0; i < count; i++) {
    pte* pte = __walk_internal(root_page_table, (vpn+i) << RISCV_PAGE_BITS, 0, 3);
    // If the page exists and is valid then we cannot use it
    if(pte && *pte){
      break;
    }
  }
  return i;
}

/* get a mapped physical address for a VA */
uintptr_t
translate(uintptr_t va)
{
  pte* pte = __walk(root_page_table, va);

  if(pte && (*pte & PTE_V))
    return (pte_ppn(*pte) << RISCV_PAGE_BITS) | (RISCV_PAGE_OFFSET(va));
  else
    return 0;
}

/* try to retrieve PTE for a VA, return 0 if fail */
pte*
pte_of_va(uintptr_t va)
{
  pte* pte = __walk(root_page_table, va);
  return pte;
}


void
__map_with_reserved_page_table_32(uintptr_t dram_base,
                               uintptr_t dram_size,
                               uintptr_t ptr,
                               pte* l2_pt)
{
  uintptr_t offset = 0;
  uintptr_t leaf_level = 2;
  pte* leaf_pt = l2_pt;
  unsigned long dram_max =  RISCV_GET_LVL_PGSIZE(leaf_level - 1);

  /* use megapage if l2_pt is null */
  if (!l2_pt) {
    leaf_level = 1;
    leaf_pt = root_page_table;
    dram_max = -1UL; 
  }

  assert(dram_size <= dram_max);
  assert(IS_ALIGNED(dram_base, RISCV_GET_LVL_PGSIZE_BITS(leaf_level)));
  assert(IS_ALIGNED(ptr, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)));

  if(l2_pt) {
       /* set root page table entry */
       root_page_table[RISCV_GET_PT_INDEX(ptr, 1)] =
       ptd_create(ppn(kernel_va_to_pa(l2_pt)));
  }

  for (offset = 0;
       offset < dram_size;
       offset += RISCV_GET_LVL_PGSIZE(leaf_level))
  {
        leaf_pt[RISCV_GET_PT_INDEX(ptr + offset, leaf_level)] =
        pte_create(ppn(dram_base + offset),
                 PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }

}

void
__map_with_reserved_page_table_64(uintptr_t dram_base,
                               uintptr_t dram_size,
                               uintptr_t ptr,
                               pte* l2_pt,
                               pte* l3_pt)
{
  uintptr_t offset = 0;
  uintptr_t leaf_level = 3;
  pte* leaf_pt = l3_pt;
  /* use megapage if l3_pt is null */
  if (!l3_pt) {
    leaf_level = 2;
    leaf_pt = l2_pt;
  }
  
  assert(dram_size <= RISCV_GET_LVL_PGSIZE(leaf_level - 1));
  assert(IS_ALIGNED(dram_base, RISCV_GET_LVL_PGSIZE_BITS(leaf_level)));
  assert(IS_ALIGNED(ptr, RISCV_GET_LVL_PGSIZE_BITS(leaf_level - 1)));

  /* set root page table entry */
  root_page_table[RISCV_GET_PT_INDEX(ptr, 1)] =
    ptd_create(ppn(kernel_va_to_pa(l2_pt)));

  /* set L2 if it's not leaf */
  if (leaf_pt != l2_pt) {
    l2_pt[RISCV_GET_PT_INDEX(ptr, 2)] =
      ptd_create(ppn(kernel_va_to_pa(l3_pt)));
  }

  /* set leaf level */
  for (offset = 0;
       offset < dram_size;
       offset += RISCV_GET_LVL_PGSIZE(leaf_level))
  {
    leaf_pt[RISCV_GET_PT_INDEX(ptr + offset, leaf_level)] =
      pte_create(ppn(dram_base + offset),
          PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);

    printf("[map EPM] VA: 0x%llx -> PA: 0x%llx\n", ptr + offset, dram_base + offset);
  }

}

void
map_with_reserved_page_table(uintptr_t dram_base,
                             uintptr_t dram_size,
                             uintptr_t ptr,
                             pte* l2_pt,
                             pte* l3_pt)
{
  #if __riscv_xlen == 64
  if (dram_size > RISCV_GET_LVL_PGSIZE(2)) {
    warn("DRAM SIZE > 2MiB");
    __map_with_reserved_page_table_64(dram_base, dram_size, ptr, l2_pt, 0);
  } else {
    warn("DRAM SIZE < 2MiB");
    __map_with_reserved_page_table_64(dram_base, dram_size, ptr, l2_pt, l3_pt);
  }
    #elif __riscv_xlen == 32
  if (dram_size > RISCV_GET_LVL_PGSIZE(1))
    __map_with_reserved_page_table_32(dram_base, dram_size, ptr, 0);
  else
    __map_with_reserved_page_table_32(dram_base, dram_size, ptr, l2_pt);
  #endif
}

