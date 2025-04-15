#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define uint64_t __uint64_t
#define asm __asm__

#define read_csr(reg) ({ unsigned long __tmp; \
		asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
		__tmp; })

int main(int argc, char *argv[])
{
	
	uint64_t total_cycles = read_csr(cycle);
	uint64_t total_instr = read_csr(instret);
	uint64_t dtlb_misses = read_csr(hpmcounter3);
	uint64_t itlb_misses = read_csr(hpmcounter4);
	int64_t l2tlb_misses = read_csr(hpmcounter5);
	printf("%lu %lu %lu %lu %lu\n", total_cycles, dtlb_misses, itlb_misses, l2tlb_misses, total_instr); 

	return 0;
}