#ifndef _MM_H_
#define _MM_H_
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "string.h"

#include "mm/vm_defs.h"

uintptr_t translate(uintptr_t va);
pte* pte_of_va(uintptr_t va, int page_table_levels);

void page_table_walker(pte* table, int level, uintptr_t vbase, bool print_pt, uintptr_t* va_max, bool* is_empty);
#define print_page_table(table, level, vbase)  page_table_walker(table, level, vbase, true, NULL, NULL)
uintptr_t find_max_user_va(pte* page_table);
bool is_page_table_empty(pte* table, int page_table_level);
uintptr_t map_page(uintptr_t vpn, uintptr_t ppn, int flags);
uintptr_t alloc_page_generic(uintptr_t vpn, int flags, int page_table_levels);
void free_page_generic(uintptr_t vpn, int page_table_levels);
#define alloc_page(vpn, flags)      alloc_page_generic(vpn, flags, 3)
#define free_page(vpn)              free_page_generic(vpn, 3)
#ifdef MEGAPAGE_MAPPING     //for 2MiB megapages we need a 2-page-table hierarchy
#define alloc_megapage(vpn, flags)  alloc_page_generic(vpn, flags, 2) 
#define free_megapage(vpn)          free_page_generic(vpn, 2)
#endif
#ifdef GIGAPAGE_MAPPING     //for 1GiB megapages we need a 1-page-table hierarchy
#define alloc_gigapage(vpn, flags)  alloc_page_generic(vpn, flags, 1) 
#define free_gigapage(vpn)          free_page_generic(vpn, 1)
#endif
uintptr_t realloc_page(uintptr_t vpn, int flags);
size_t alloc_pages(uintptr_t vpn, size_t count, int flags, bool is_largepage);
void free_pages(uintptr_t vpn, size_t count, bool is_largepage);
size_t test_va_range(uintptr_t vpn, size_t count);

uintptr_t get_program_break();
void set_program_break(uintptr_t new_break);

void map_with_reserved_page_table(uintptr_t base, uintptr_t size, uintptr_t ptr, pte* l2_pt, pte* l3_pt);

#endif /* _MM_H_ */
