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
    
    for (int i = 0; i < NUM_DEVICES_ID_ENUMS; i++) {
        g_device_table[i].dvinit();
    }
}


/**
 * Handler for sysopen
 */
int di_open(proc_ctrl_block_t *proc, int device_no) {
    int i;
    int result;
    
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
    result = proc->fd_table[i]->dvopen(proc->fd_table[i]->dvioblk);
    if (result != 0) {
        proc->fd_table[i] = NULL;
        return result;
    }
    
    return 0;
}

/**
 * Handler for sysclose
 */
int di_close(proc_ctrl_block_t *proc, int fd) {
    devsw_t *entry;
    ASSERT(proc != NULL);
    
    if (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL) {
        return EBADF;
    }
    
    entry = proc->fd_table[fd];
    proc->fd_table[fd] = NULL;
    return entry->dvclose(entry->dvioblk);
}

/**
 * Handler for syswrite
 */
int di_write(proc_ctrl_block_t *proc, int fd, void *buf, int buflen) {
    ASSERT(proc != NULL);
    
    if (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL) {
        return EBADF;
    }
    
    return proc->fd_table[fd]->dvwrite(proc, proc->fd_table[fd]->dvioblk, buf, buflen);
}

/**
 * Handler for sysread
 */
int di_read(proc_ctrl_block_t *proc, int fd, void *buf, int buflen) {
    ASSERT(proc != NULL);
    
    if (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL) {
        return EBADF;
    }
    
    return proc->fd_table[fd]->dvread(proc, proc->fd_table[fd]->dvioblk, buf, buflen);
}

/**
 * Handler for sysioctl
 */
int di_ioctl(proc_ctrl_block_t *proc, int fd, unsigned long command_code, void *args) {
    ASSERT(proc != NULL);
    
    if (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL) {
        return EBADF;
    }
    
    return proc->fd_table[fd]->dvioctl(proc->fd_table[fd]->dvioblk,
        command_code, args);
}
