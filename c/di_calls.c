/* di_calls.c: Device calls

Called from outside:
    di_open() - opens device
    di_close() - close fd
    di_write() - write to device
    di_read() - read from device
    di_ioctl() - special control
    
Further details can be found in the documentation above the function headers.
 */

#include <xeroskernel.h>
#include <kbd.h>

// Device table
static devsw_t g_device_table[NUM_DEVICES_ID_ENUMS];

void di_init_devtable(void) {
    kbd_devsw_create(&g_device_table[DEVICE_ID_KEYBOARD], 1);
    kbd_devsw_create(&g_device_table[DEVICE_ID_KEYBOARD_NO_ECHO], 0);
}


/**
 * Handler for sysopen
 */
int di_open(proc_ctrl_block_t *proc, int device_no) {
    int i;
    
    ASSERT(proc != NULL);
    
    if (device_no < 0 || device_no >= NUM_DEVICES_ID_ENUMS) {
        return ENODEV;
    }
    
    for (i = 0; i < PCB_NUM_FDS; i++) {
        if (proc->fd_table[i] == NULL) {
            break;
        }
    }
    
    if (i >= PCB_NUM_FDS) {
        return EMFILE;
    }
    
    proc->fd_table[i] = &g_device_table[device_no];
    return 0;
}

/**
 * Handler for sysclose
 */
int di_close(proc_ctrl_block_t *proc, int fd) {
    ASSERT(proc != NULL);
    
    if (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL) {
        return EBADF;
    }
    
    proc->fd_table[fd] = NULL;
    return 0;
}

/**
 * Handler for syswrite
 */
int di_write(proc_ctrl_block_t *proc, int fd, void *buf, int buflen) {
    ASSERT(proc != NULL);
    
    if (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL) {
        return EBADF;
    }
    
    return proc->fd_table[fd]->dvwrite(proc->fd_table[fd]->dvioblk, buf, buflen);
}

/**
 * Handler for sysread
 */
int di_read(proc_ctrl_block_t *proc, int fd, void *buf, int buflen) {
    ASSERT(proc != NULL);
    
    if (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL) {
        return EBADF;
    }
    
    return proc->fd_table[fd]->dvread(proc->fd_table[fd]->dvioblk, buf, buflen);
}

/**
 * Handler for sysioctl
 */
int di_ioctl(proc_ctrl_block_t *proc) {
    DEBUG("\n");
    // TODO: Implement me!
    return -1;
}
