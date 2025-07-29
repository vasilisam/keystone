#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define asm __asm__

#define read_csr(reg) ({ unsigned long __tmp; \
		asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
		__tmp; })

#define PAGE_SIZE (1UL << 12)
#define WAYS 64
#define REPL_RUNS 100
#define SIZE 32
#define RUNS 1000

void tlb_check(uint32_t size, uint64_t runs);
void random_fetch(uint64_t size, uint64_t runs);

int main() {
	
	uint32_t size = SIZE;
	uint64_t runs = RUNS;

	tlb_check(size, runs);
	random_fetch(size, runs);
	
	return 0;
}

void tlb_check(uint32_t size, uint64_t runs) {
	
	char *pages;
	uint64_t cycles,
		 dtlb_misses,
		 itlb_misses,
		 l2_tlb_misses;
	pages = malloc(size*PAGE_SIZE*sizeof(char));

	if (pages == NULL) {
		printf("Malloc failed\n");
		exit(-1);
	}

	for ( uint64_t i = 0; i < PAGE_SIZE*size; i += PAGE_SIZE )
		pages[i % (size * PAGE_SIZE)] = i % 3;



	cycles = read_csr(cycle);
	dtlb_misses = read_csr(hpmcounter3);
	itlb_misses = read_csr(hpmcounter4);
	l2_tlb_misses = read_csr(hpmcounter5);
	
	printf ("\nTLB sweep started at cycle %lu\n", read_csr(cycle));
	for (uint64_t i = 0; i < runs*PAGE_SIZE ; i += PAGE_SIZE) {
		pages[i % (size * PAGE_SIZE)] = i;
	}

	cycles = read_csr(cycle) - cycles;
	dtlb_misses = read_csr(hpmcounter3) - dtlb_misses;
	itlb_misses = read_csr(hpmcounter4) - itlb_misses;
	l2_tlb_misses = read_csr(hpmcounter5) - l2_tlb_misses;

	printf("\nPage sweep | %u pages, %lu runs\n", size, runs);
	printf("------------------------\n");
	printf("DTLB Misses     = %lu\n", dtlb_misses);
	printf("ITLB Misses     = %lu\n", itlb_misses);
	printf("L2 TLB Misses   = %lu\n", l2_tlb_misses);
	printf("Total cycles    = %lu\n", cycles);
	printf("------------------------\n");
	
	free(pages);
}

void random_fetch(uint64_t size, uint64_t runs) {

	uint64_t j,
		 cycles,
		 dtlb_misses,
		 itlb_misses,
		 l2_tlb_misses;

	char *pages;
	pages = malloc(size*PAGE_SIZE*sizeof(char));
	if (pages == NULL) {
		printf("Malloc failed\n");
		exit(-1);
	}

	for ( uint64_t i = 0; i < PAGE_SIZE*size; i += PAGE_SIZE )
		pages[i % (size * PAGE_SIZE)] = i % 3;

	cycles = read_csr(cycle);
	dtlb_misses = read_csr(hpmcounter3);
	itlb_misses = read_csr(hpmcounter4);
	l2_tlb_misses = read_csr(hpmcounter5);


	printf ("Random fetch started at cycle %lu\n", read_csr(cycle));
	for(uint32_t i = 0; i <= runs; i++) {
		j = rand() % (PAGE_SIZE*size);
		pages[j] = j % 2;
	}

	cycles = read_csr(cycle) - cycles;
	dtlb_misses = read_csr(hpmcounter3) - dtlb_misses;
	itlb_misses = read_csr(hpmcounter4) - itlb_misses;
	l2_tlb_misses = read_csr(hpmcounter5) - l2_tlb_misses;


	printf("\nRandom fetch | %lu pages, %lu runs\n", size, runs);
	printf("------------------------\n");
	printf("DTLB Misses     = %lu\n", dtlb_misses);
	printf("ITLB Misses     = %lu\n", itlb_misses);
	printf("L2 TLB Misses   = %lu\n", l2_tlb_misses);
	printf("Total cycles    = %lu\n", cycles);
	printf("------------------------\n");

	free(pages);

}