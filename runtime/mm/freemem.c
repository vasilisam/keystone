#include "util/string.h"
#include "mm/common.h"
#include "mm/vm.h"
#include "mm/freemem.h"
#include "mm/paging.h"

/* This file implements a simple page allocator (SPA)
 * which stores the pages based on a linked list.
 * Each of the element of linked list is stored in the header of each free page.

 * SPA does not use any additional data structure (e.g., linked list).
 * Instead, it uses the pages themselves to store the linked list pointer.
 * Thus, each of the free pages contains the pointer to the next free page
 * which can be dereferenced by NEXT_PAGE() macro.
 * spa_free_pages will only hold the head and the tail pages so that
 * SPA can allocate/free a page in constant time. */

static struct pg_list spa_free_pages;
#ifdef MEGAPAGE_MAPPING
static struct pg_list spa_free_megapages;
#endif 
#ifdef GIGAPAGE_MAPPING
static struct pg_list spa_free_gigapages;
#endif 

/* get a free page from the simple page allocator */
uintptr_t
__spa_get(bool zero, bool is_megapage)
{
  uintptr_t free_page;
  struct pg_list* list;

#ifdef MEGAPAGE_MAPPING
  if (is_megapage) {
    list = &spa_free_megapages;
  } else
#endif
#ifdef GIGAPAGE_MAPPING
  if (is_megapage) {
    list = &spa_free_gigapages;
  } else
#endif
  {
    list = &spa_free_pages;
  }

  if (LIST_EMPTY(*list)) {
    /* try evict a page */
#ifdef USE_PAGING
    uintptr_t new_pa = paging_evict_and_free_one(0);
    if(new_pa)
    {
      spa_put(__va(new_pa), 1);
    }
    else
#endif
    {
      warn("eyrie simple page allocator cannot evict and free pages");
      return 0;
    }
  }

  free_page = list->head;
  assert(free_page);

  /* update list head */
  uintptr_t next = NEXT_PAGE(list->head);
  list->head = next;
  list->count--;

#ifdef MEGAPAGE_MAPPING
  if (is_megapage) {
    assert(free_page > EYRIE_LOAD_START && free_page < (freemem_va_start_2m  + freemem_size_2m));
    if (zero)
      memset((void*)free_page, 0, RISCV_MEGAPAGE_SIZE);
  } else
#endif
#ifdef GIGAPAGE_MAPPING
  if (is_megapage) {
    assert(free_page > EYRIE_LOAD_START && free_page < (freemem_va_start_1g  + freemem_size_1g));
    if (zero)
      memset((void*)free_page, 0, RISCV_GIGAPAGE_SIZE);
  } else
#endif
  {
    assert(free_page > EYRIE_LOAD_START && free_page < (freemem_va_start + freemem_size));
    if (zero)
      memset((void*)free_page, 0, RISCV_PAGE_SIZE);
  }

  return free_page;
}

uintptr_t spa_get() { return __spa_get(false, 0); }

uintptr_t spa_get_zero() { 
  return __spa_get(true, 0); 
}

#ifdef MEGAPAGE_MAPPING
uintptr_t spa_get_megapage() { return __spa_get(false, 1); }

uintptr_t spa_get_zero_megapage() {
 return  __spa_get(true, 1);
}

unsigned long
spa_megapages_available(){
#ifndef USE_PAGING
  return spa_free_megapages.count;
#else
  return spa_free_megapages.count + paging_remaining_pages();
#endif
}
#endif

#ifdef GIGAPAGE_MAPPING
uintptr_t spa_get_gigapage() { return __spa_get(false, 1); }

uintptr_t spa_get_zero_gigapage() {
 return  __spa_get(true, 1);
}

unsigned long
spa_gigapages_available(){
#ifndef USE_PAGING
  return spa_free_gigapages.count;
#else
  return spa_free_gigapages.count + paging_remaining_pages();
#endif
}
#endif

/* put a page to the simple page allocator */
void
spa_put(uintptr_t page_addr, bool is_4K_allocator)
{
  uintptr_t prev;
  struct pg_list* list;

#ifdef MEGAPAGE_MAPPING
  if (!is_4K_allocator) {
    assert(IS_ALIGNED(page_addr, RISCV_MEGAPAGE_BITS));
    assert(page_addr >= EYRIE_LOAD_START && page_addr < (freemem_va_start_2m  + freemem_size_2m));
    list = &spa_free_megapages;
  } else
#endif
#ifdef GIGAPAGE_MAPPING
  if (!is_4K_allocator) {
    assert(IS_ALIGNED(page_addr, RISCV_GIGAPAGE_BITS));
    assert(page_addr >= EYRIE_LOAD_START && page_addr < (freemem_va_start_1g  + freemem_size_1g));
    list = &spa_free_gigapages;
  } else
#endif
  {
    assert(IS_ALIGNED(page_addr, RISCV_PAGE_BITS));
    assert(page_addr >= EYRIE_LOAD_START && page_addr < (freemem_va_start  + freemem_size));
    list = &spa_free_pages;
  }

  if (!LIST_EMPTY(*list)) {
    prev = list->tail;
    assert(prev);
    NEXT_PAGE(prev) = page_addr;
  } else {
    list->head = page_addr;
  }

  NEXT_PAGE(page_addr) = 0;
  list->tail = page_addr;

  list->count++;
  return;
}

unsigned long
spa_available(){
#ifndef USE_PAGING
  return spa_free_pages.count;
#else
  return spa_free_pages.count + paging_remaining_pages();
#endif
}

void
spa_init_generic(uintptr_t base, size_t size, unsigned int page_bits)
{
  uintptr_t cur;
  bool is_4K_allocator = true;

#ifdef MEGAPAGE_MAPPING
  if (page_bits == RISCV_MEGAPAGE_BITS) {
    LIST_INIT(spa_free_megapages);
    is_4K_allocator = false;
  } else
#endif
#ifdef GIGAPAGE_MAPPING
  if (page_bits == RISCV_GIGAPAGE_BITS) {
    LIST_INIT(spa_free_gigapages);
    is_4K_allocator = false;
  } else
#endif
  {
    LIST_INIT(spa_free_pages);
  }

  // both base and size must be page-aligned
  assert(IS_ALIGNED(base, page_bits));
  assert(IS_ALIGNED(size, page_bits));

  /* put all free pages in freemem (base) into spa_free_pages */
  for(cur = base;
      cur < base + size;
      cur += BIT(page_bits)) {
      spa_put(cur, is_4K_allocator);
  }
}
