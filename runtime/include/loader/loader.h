#include "loader/elf.h"

int loadElf(elf_t* elf, bool user);
#if defined(MEGAPAGE_MAPPING)
int loadElf_megapage(elf_t* elf, bool user);
#elif defined(GIGAPAGE_MAPPING)
int loadElf_gigapage(elf_t* elf, bool user);
#endif
