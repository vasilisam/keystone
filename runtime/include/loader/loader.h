#include "loader/elf.h"

int loadElf(elf_t* elf, bool user);
#ifdef MEGAPAGE_MAPPING
int loadElf_megapage(elf_t* elf, bool user);
#endif
