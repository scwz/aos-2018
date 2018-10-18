
#include <sel4/sel4.h>
#include <stdint.h>

#include "../uio.h"
#include "../vfs/vfs.h"
#include "../proc/proc.h"
#include "file_syscalls.h"
#include "../vm/shared_buf.h"
#include "../fs/libnfs_vops.h"
#include <sos.h>

static int 
syscall_readwrite(struct proc *curproc, int how) 
{
    int fd = seL4_GetMR(1);
    size_t nbyte = seL4_GetMR(2);
    size_t bytes_processed = 0;
    if (curproc->fdt->openfiles[fd] == NULL || nbyte <= 0) {
        seL4_SetMR(0, 0);
        return 1;
    }
    if (!(curproc->fdt->openfiles[fd]->flags & how)) {
        seL4_SetMR(0, 0);
        return 1;
    }
    
    struct vnode *vn = curproc->fdt->openfiles[fd]->vn; 
    struct uio *u = malloc(sizeof(struct uio));
    if (how == FM_WRITE) {
        uio_init(u, UIO_WRITE, nbyte, curproc->fdt->openfiles[fd]->offset, curproc->pid);
        bytes_processed = VOP_WRITE(vn, u);
    }
    else if (how == FM_READ) { 
        uio_init(u, UIO_READ, nbyte, curproc->fdt->openfiles[fd]->offset, curproc->pid);
        bytes_processed = VOP_READ(vn, u);
    }
    else {
        ZF_LOGE("Unknown flag encountered!");
        seL4_SetMR(0, 0);
        return 1;
    }
    curproc->fdt->openfiles[fd]->offset += bytes_processed;
    free(u);
    seL4_SetMR(0, bytes_processed);
    return 1;
}

int 
syscall_write(struct proc *curproc) 
{
    return syscall_readwrite(curproc, FM_WRITE);
}

int 
syscall_read(struct proc *curproc) 
{
    return syscall_readwrite(curproc, FM_READ);
}

int
syscall_open(struct proc *curproc) 
{
    fmode_t mode = seL4_GetMR(1);
    size_t size = seL4_GetMR(2);
    struct vnode *res;
    char path[size];
    sos_copyout(curproc->pid, (seL4_Word) path, size);

    if (vfs_open(path, mode, &res, curproc->pid)) {
        seL4_SetMR(0, 1);
        return 1;
    }

    bool full = true;
    for (unsigned int i = 0; i < PROCESS_MAX_FILES; i ++) {
        if (curproc->fdt->openfiles[i] == NULL) {
            curproc->fdt->openfiles[i] = malloc(sizeof(struct open_file));
            curproc->fdt->openfiles[i]->vn = res;
            curproc->fdt->openfiles[i]->refcnt = 1;
            curproc->fdt->openfiles[i]->offset = 0;
            curproc->fdt->openfiles[i]->flags = mode;
            seL4_SetMR(0, 0);
            seL4_SetMR(1, i);
            full = false;
            break;
        }
    } 
    if (full) {
        seL4_SetMR(0, 1);
        return 1;
    }
    return 2;
}

int 
syscall_close(struct proc *curproc) 
{
    int fd = seL4_GetMR(1);
    struct open_file *of = curproc->fdt->openfiles[fd];
    VOP_RECLAIM(of->vn, curproc->pid);
    free(of);
    curproc->fdt->openfiles[fd] = NULL;
    seL4_SetMR(0, 0);
    return 1;
}

int 
syscall_getdirent(struct proc *curproc)
{
    int pos = seL4_GetMR(1);
    size_t nbyte = seL4_GetMR(2);
    char path[nbyte];
    sos_copyout(curproc->pid, (seL4_Word) path, nbyte);
    struct vnode *res;
    if (vfs_lookup("", &res, 0, curproc->pid)){
        seL4_SetMR(0, 0);
        return 1;
    }
    struct uio *u = malloc(sizeof(struct uio));
    uio_init(u, UIO_READ, pos, 0, curproc->pid);
    size_t bytes = VOP_GETDIRENTRY(res, u);
    seL4_SetMR(0, bytes);
    free(u);
    return 1;
}

int 
syscall_stat(struct proc *curproc)
{
    size_t nbyte = seL4_GetMR(1);
    char path[nbyte];
    sos_copyout(curproc->pid, (seL4_Word) path, nbyte);
    sos_stat_t buf;
    struct vnode *res;
    if (vfs_lookup("", &res, 0, curproc->pid)){
        seL4_SetMR(0, -1);
        return 1;
    }
    if (nfs_get_statbuf(res, path, &buf, curproc->pid)){
        seL4_SetMR(0, -1);
        return 1;
    }
    sos_copyin(curproc->pid, (seL4_Word) &buf, sizeof(buf));
    seL4_SetMR(0, 0);
    return 1;
}

