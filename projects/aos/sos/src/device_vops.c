#include "vfs.h"
#include "vnode.h"
#include "device_vops.h"

static int dev_eachopen(struct vnode *v, int flags)
{
	struct device *d = v->vn_data;

    return d->open(flags);
}

static
int
dev_write(struct vnode *v, struct uio *uio)
{
	struct device *d = v->vn_data;
	int result;

	return d->write(uio);
}

static
int
dev_read(struct vnode *v, struct uio *uio)
{
	struct device *d = v->vn_data;
	
	d->read(uio);
}

static int dev_getdirent(){
    return 0;
}

static int dev_stat(){
    return 0;
}

static int dev_lookup(){
    return 0;
}

static int dev_lookparent(){
    return 0;
}

static int dev_reclaim(struct vnode *v){
	struct device *d = v->vn_data;
    return d->close();
}

static const struct vnode_ops dev_vnode_ops = {
	.vop_magic = VOP_MAGIC,

	.vop_eachopen = dev_eachopen,

	.vop_read = dev_read,
    .vop_getdirentry = dev_getdirent,

	.vop_write = dev_write,
	.vop_reclaim = dev_reclaim,
	.vop_stat = dev_stat,
	.vop_lookup = dev_lookup,
	.vop_lookparent = dev_lookparent,
};

struct vnode *
dev_create_vnode(void * data)
{
	int result;
	struct vnode *v;

	v = malloc(sizeof(struct vnode));
	if (v==NULL) {
		return NULL;
	}

	result = vnode_init(v, &dev_vnode_ops, data);
	if (result != 0) {
		ZF_LOGV("While creating vnode for device: vnode_init: %s\n",
		      strerror(result));
	}

	return v;
}

void
dev_uncreate_vnode(struct vnode *vn)
{
	assert(vn->vn_ops == &dev_vnode_ops);
	vnode_cleanup(vn);
	free(vn);
}
