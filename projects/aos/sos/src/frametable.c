#include <autoconf.h>
#include <utils/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <utils/page.h>

#include <cspace/cspace.h>
#include <aos/sel4_zf_logif.h>
#include <aos/debug.h>

#include "ut.h"
#include "mapping.h"
#include "vmem_layout.h"
#include "frametable.h"

static struct frame_table_entry *frame_table = NULL;
static seL4_Word top_paddr;
static seL4_Word bot_paddr;
static seL4_Word vaddr_index;
static seL4_Word base_vaddr;
static seL4_Word next_free_page;
static size_t frame_table_size;
static size_t frame_table_pages;
static cspace_t *cspace;

static seL4_Word vaddr_to_page_num(seL4_Word vaddr){
    return ((vaddr - base_vaddr) / PAGE_SIZE_4K); 
}


static seL4_Word page_num_to_vaddr(seL4_Word page){
    return  page * PAGE_SIZE_4K + base_vaddr;
}

static ut_t *alloc_retype_map(seL4_CPtr *cptr, uintptr_t *vaddr, uintptr_t *paddr) {
    ut_t *ut = ut_alloc_4k_untyped(paddr);
    if(ut == NULL){
        *vaddr = (seL4_Word) NULL;
        ZF_LOGE("Out of memory");
        return NULL;
    }

    *cptr = cspace_alloc_slot(cspace);
    if (*cptr == seL4_CapNull) {
        *vaddr = (seL4_Word) NULL;
        ZF_LOGE("Cspace full");
        ut_free(ut, seL4_PageBits);
        return NULL;
    }

    seL4_Error err = cspace_untyped_retype(cspace, 
                                            ut->cap, 
                                            *cptr, 
                                            seL4_ARM_SmallPageObject, 
                                            seL4_PageBits);
    ZF_LOGE_IFERR(err, "Failed retype untyped");
    if (err != seL4_NoError) {
        *vaddr = (seL4_Word) NULL;
        ut_free(ut, seL4_PageBits);
        cspace_delete(cspace, *cptr);
        cspace_free_slot(cspace, *cptr);
        return NULL;
    }
    
    err = map_frame(cspace, *cptr, seL4_CapInitThreadVSpace, *vaddr, 
                    seL4_AllRights, seL4_ARM_Default_VMAttributes);
    ZF_LOGE_IFERR(err, "Failed to map frame");
    if (err != seL4_NoError) {
        *vaddr = (seL4_Word) NULL;
        ut_free(ut, seL4_PageBits);
        cspace_delete(cspace, *cptr);
        cspace_free_slot(cspace, *cptr);
        return NULL;
    }
    return ut;
}

void frame_table_init(cspace_t *cs) {
    cspace = cs;

    frame_table_size = ut_size() / PAGE_SIZE_4K;
    frame_table_pages = BYTES_TO_4K_PAGES(frame_table_size * sizeof(struct frame_table_entry));
    seL4_Word frame_table_vaddr = SOS_FRAME_TABLE;
    seL4_CPtr cap;
    
    alloc_retype_map(&cap, &frame_table_vaddr, &top_paddr);
    frame_table = (struct frame_table_entry *) frame_table_vaddr;
    for(size_t i = 1; i < frame_table_pages; i++){
        frame_table_vaddr += PAGE_SIZE_4K;
        alloc_retype_map(&cap, &frame_table_vaddr, &top_paddr);
    }

    //base is the next page after the frame table
    base_vaddr = frame_table_vaddr + PAGE_SIZE_4K;

    //set up frame table initial values
    seL4_Word i;
    for(i = 0; i < frame_table_size; i++){
        frame_table[i].cap = seL4_CapNull;
        frame_table[i].next_free_page = i+1;
    }
    frame_table[i].next_free_page = 0;

    //init values (physical memory allocates downwards)
    top_paddr -= PAGE_SIZE_4K; // first page will be here
    bot_paddr = top_paddr - ut_size();
    
    // test
    assert(frame_table_vaddr);
    printf("frame_table_vaddr: %lx, value %lx\n", frame_table_vaddr, *(seL4_Word *)frame_table_vaddr);
    
}

seL4_Word frame_alloc(seL4_Word *vaddr) {
    uintptr_t paddr;
    seL4_CPtr cap;
    seL4_Word page = next_free_page;
    *vaddr = page_num_to_vaddr(page);
    //printf("pagenum %ld, vaddr %lx, freepage: %ld\n", page, *vaddr, next_free_page);
    ut_t *ut = alloc_retype_map(&cap, vaddr, &paddr);
    
    if (ut == NULL) {
        return (seL4_Word) NULL;
    }
    frame_table[page].cap = cap;
    frame_table[page].ut = ut;
    next_free_page = frame_table[page].next_free_page;
    return page;
}

void frame_free(seL4_Word page) {

    if(page > frame_table_size){
        ZF_LOGE("Page does not exist");
        return;
    }
    printf("freeing page %ld\n", page);
    if(frame_table[page].cap == seL4_CapNull){
        ZF_LOGE("Page is already free");
        return;
    }
    frame_table[page].next_free_page = next_free_page;
    next_free_page = page;
    seL4_ARM_Page_Unmap(frame_table[page].cap);
    cspace_delete(cspace, frame_table[page].cap);
    cspace_free_slot(cspace, frame_table[page].cap);
    ut_free(frame_table[page].ut, PAGE_BITS_4K);
    frame_table[page].cap = seL4_CapNull;
}

