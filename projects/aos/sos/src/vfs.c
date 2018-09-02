#include "vfs.h"
#include "vnode.h"
#include <string.h>
#include "device_vops.h"
#include "ringbuffer.h"
#include "picoro.h"
#include <serial/serial.h>

static struct device_entry{
	char* name[20];
	struct vnode * vn;
	struct device_entry * next;
};

static struct device_entry * device_list;

static struct vnode * find_device(char * name){
	struct device_entry * curr = device_list;
	for(; curr != NULL; curr = curr->next){
		if(!strcmp(name, curr->name)){
			return curr->vn;
		}
	}
	return NULL;
}

int vfs_lookup(char *path, struct vnode **retval) {
	printf("LOOKUP %s\n", path);
    struct vnode *startvn;
	int result;

	startvn = find_device(path);
	if(startvn!= NULL){
		*retval = startvn;
		return 0;
	}


	result = VOP_LOOKUP(startvn, path, retval);

	VOP_DECREF(startvn);
    return result;
}

int vfs_lookparent(char *path, struct vnode **retval, char *buf, size_t buflen) {
    struct vnode *startvn;
	int result;

    // idk this does; i think if console no path
	if(strlen(path) == 0){
        *retval = startvn;
        return 0;
    }


	result = VOP_LOOKPARENT(startvn, path, retval, buf, buflen);

	VOP_DECREF(startvn);
	return 0;
}

int vfs_open(char *path, int openflags, int mode, struct vnode **ret) {
    int how;
	int result;
	int canwrite;
	struct vnode *vn = NULL;

    
	result = vfs_lookup(path, &vn);

	if (result) {
		return result;
	}

	assert(vn != NULL);

	result = VOP_EACHOPEN(vn, openflags);
	if (result) {
		VOP_DECREF(vn);
		return result;
	}

	*ret = vn;

return 0;
}

void vfs_close(struct vnode *vn) {
    VOP_DECREF(vn);
}



struct serial * serial_port;
char *holdc;
ringBuffer_typedef(char, stream_buf);
stream_buf *sb_ptr;

void thishandler(struct serial *serial, char c) {
	bufferWrite(sb_ptr, c);

    if (c == '\n') {
       	resume(curr, NULL);
	}
}

static int console_open(int flags){
	serial_register_handler(serial_port, thishandler);
	return 0;
}

static int console_close(){
	return 0;
}

static int console_read(struct uio *u){
	char msg[u->len];
	int i = 0;
	while (!isBufferEmpty(sb_ptr) || i > u->len) {
		char c;
		bufferRead(sb_ptr, c);
		msg[i++] = c;
	}
	sos_copyout(msg, i);
	return i;
}

static int console_write(struct uio * u){
	serial_send(serial_port, shared_buf, u->len);
	return u->len;
}


void vfs_bootstrap(void) {
	device_list = malloc(sizeof(struct device_entry));
	
	strcpy(device_list->name, "console");
	device_list->next = NULL;
	struct device* dev = malloc(sizeof(struct device));
	dev->close = console_close;
	dev->open = console_open;
	dev->write = console_write;
	dev->read = console_read;
	device_list->vn = dev_create_vnode(dev);
	serial_port = serial_init();
	
	stream_buf sb;
    bufferInit(sb, 1024, char);
	sb_ptr = &sb;
}
