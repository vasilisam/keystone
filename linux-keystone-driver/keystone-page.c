//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "riscv64.h"
#include <linux/kernel.h>
#include "keystone.h"
#include <linux/dma-mapping.h>
#include <linux/version.h>

/* Destroy all memory associated with an EPM */
int epm_destroy(struct epm* epm) {

  if(!epm->ptr || !epm->size)
    return 0;

  /* free the EPM hold by the enclave */
  if (epm->is_cma) {
    dma_free_coherent(keystone_dev.this_device,
        epm->size,
        (void*) epm->ptr,
        epm->pa);
  } else {
    free_pages(epm->ptr, epm->order);
  }

  return 0;
}

/* Create an EPM and initialize the free list */
int epm_init(struct epm* epm, unsigned long min_pages)
{
  vaddr_t epm_vaddr = 0;
  unsigned long order = 0;
  unsigned long count = min_pages;
  phys_addr_t device_phys_addr = 0;

  /* try to allocate contiguous memory */
  epm->is_cma = 0;
  order = ilog2(min_pages - 1) + 1;
  count = 0x1 << order;

  /* prevent kernel from complaining about an invalid argument */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
  if (order < MAX_PAGE_ORDER)
#else
  if (order < MAX_ORDER)
#endif
    epm_vaddr = (vaddr_t) __get_free_pages(GFP_HIGHUSER, order);

#ifdef CONFIG_CMA
  /* If buddy allocator fails, we fall back to the CMA */
  if (!epm_vaddr) {
    epm->is_cma = 1;
    pr_info("[Driver] EPM is allocated within CMA");
  #if defined(MEGAPAGE_MAPPING)
  /* When using megapages, count*PAGE_SIZE should be 2MB-aligned for
   * the Free-Memory region's  end to have the same alignment. 
   */
  count = MEGAPAGE_UP(min_pages << PAGE_SHIFT) >> PAGE_SHIFT;
  #elif defined(GIGAPAGE_MAPPING)
  /* FreeMemory region's end address should be 1-GiB aligned as well */
  count = GIGAPAGE_UP(min_pages << PAGE_SHIFT) >> PAGE_SHIFT;
  #else
    count = min_pages;
  #endif
    /*epm_vaddr = (vaddr_t) dma_alloc_coherent(keystone_dev.this_device,
      count << PAGE_SHIFT,
      &device_phys_addr,
      GFP_KERNEL | __GFP_DMA32);*/

    epm_vaddr = (vaddr_t) dma_alloc_coherent(keystone_dev.this_device,
      count << PAGE_SHIFT,
      &device_phys_addr,
      GFP_KERNEL);

    if(!device_phys_addr)
      epm_vaddr = 0;
  }
#endif

  if(!epm_vaddr) {
    keystone_err("failed to allocate %lu page(s)\n", count);
    return -ENOMEM;
  }

  /* zero out */
  memset((void*)epm_vaddr, 0, PAGE_SIZE*count);

  epm->root_page_table = (void*)epm_vaddr;
  epm->pa = (epm->is_cma) ? device_phys_addr : __pa(epm_vaddr);
  epm->order = order;
  epm->size = count << PAGE_SHIFT;
  pr_info("[Driver] EPM is %lu pages long", count);
  epm->ptr = epm_vaddr;

  return 0;
}

int utm_destroy(struct utm* utm){

  if(utm->ptr != NULL){
    free_pages((vaddr_t)utm->ptr, utm->order);
  }

  return 0;
}

int utm_init(struct utm* utm, size_t untrusted_size)
{
  unsigned long req_pages = 0;
  unsigned long order = 0;
  unsigned long count;
  req_pages += PAGE_UP(untrusted_size)/PAGE_SIZE;
  order = ilog2(req_pages - 1) + 1;
  count = 0x1 << order;

  utm->order = order;

  /* Currently, UTM does not utilize CMA.
   * It is always allocated from the buddy allocator */
  utm->ptr = (void*) __get_free_pages(GFP_HIGHUSER, order);
  if (!utm->ptr) {
    keystone_err("failed to allocate UTM (size = %i bytes)\n",(1<<order));
    return -ENOMEM;
  }

  utm->size = count * PAGE_SIZE;
  if (utm->size != untrusted_size) {
    /* Instead of failing, we just warn that the user has to fix the parameter. */
    keystone_warn("shared buffer size is not multiple of PAGE_SIZE\n");
  }

  return 0;
}
