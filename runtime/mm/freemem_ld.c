#include "mm/freemem.h"
#include "mm/common.h"
#include "mm/vm_defs.h"
#include "util/string.h"

static uintptr_t freeBase;
static uintptr_t freeEnd;

void spa_init_generic(uintptr_t base, size_t size, unsigned int page_bits)
{
  freeBase = base;
  freeEnd = freeBase + size;
}

uintptr_t spa_get()
{
  return spa_get_zero(); // not allowed, so change to safe
}

uintptr_t spa_get_zero()
{
  if (freeBase >= freeEnd) {
    return 0;
  }
  uintptr_t new_page = freeBase;
  memset((void *) new_page, 0, RISCV_PAGE_SIZE);

  freeBase += RISCV_PAGE_SIZE;
  return new_page;
}

void spa_put(uintptr_t page, bool is_4K_allocator)
{
  assert(false); // not implemented
}

unsigned long spa_available()
{
  return (freeEnd - freeBase) / RISCV_PAGE_SIZE;
}

#ifdef MEGAPAGE_MAPPING
uintptr_t spa_get_megapage(void)
{
  assert(false); // not implemented for the loader
  return 0;
}

uintptr_t spa_get_zero_megapage(void)
{
  assert(false); // not implemented for the loader
  return 0;
}

unsigned long spa_megapages_available()
{
  assert(false); // not implemented for the loader
  return 0;
}
#endif

#ifdef GIGAPAGE_MAPPING
uintptr_t spa_get_gigapage(void)
{
  assert(false); // not implemented for the loader
  return 0;
}

uintptr_t spa_get_zero_gigapage(void)
{
  assert(false); // not implemented for the loader
  return 0;
}

unsigned long spa_gigapages_available()
{
  assert(false); // not implemented for the loader
  return 0;
}
#endif