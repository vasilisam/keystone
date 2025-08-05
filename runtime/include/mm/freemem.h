#ifndef __FREEMEM_H__
#define __FREEMEM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NEXT_PAGE(page) *((uintptr_t*)page)
#define LIST_EMPTY(list) ((list).count == 0 || (list).head == 0)
#define LIST_INIT(list) { (list).count = 0; (list).head = 0; (list).tail = 0; }

struct pg_list
{
	uintptr_t head;
	uintptr_t tail;
	unsigned long count;
};

void spa_init_generic(uintptr_t base, size_t size, unsigned int page_bits);
#define spa_init(base, size)	       spa_init_generic(base, size, 12)
uintptr_t spa_get(void);
uintptr_t spa_get_zero(void);
#ifdef MEGAPAGE_MAPPING
uintptr_t spa_get_megapage(void);
uintptr_t spa_get_zero_megapage(void);
#define spa_init_megapage(base, size)  spa_init_generic(base, size, 21)
unsigned long spa_megapages_available();
#endif
#ifdef GIGAPAGE_MAPPING
uintptr_t spa_get_gigapage(void);
uintptr_t spa_get_zero_gigapage(void);
#define spa_init_gigapage(base, size)  spa_init_generic(base, size, 30)
unsigned long spa_gigapages_available();
#endif
void spa_put(uintptr_t page, bool is_4K_allocator);
unsigned long spa_available();
#endif
