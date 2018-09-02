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
#include "frametable.h"


void * shared_buf;

void shared_buf_init();

void sos_map_buf();


size_t sos_copyin(seL4_Word kernel_vaddr, size_t len);
size_t sos_copyout(seL4_Word kernel_vaddr, size_t len);
