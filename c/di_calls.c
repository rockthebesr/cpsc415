/* di_calls.c: Device calls

Called from outside:
    di_init_devtable() - initializes device table, all devices within it
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

static int check_fd(proc_ctrl_block_t* proc, int fd);

/**
 * Initializes device table, all devices within it
 */
void di_init_devtable(void) {
    kbd_devsw_create(&g_device_table[DEVICE_ID_KEYBOARD_NO_ECHO], 0);
    kbd_devsw_create(&g_device_table[DEVICE_ID_KEYBOARD], 1);
    
    for (int i = 0; i < NUM_DEVICES_ID_ENUMS; i++) {
        g_device_table[i].dvinit();
    }
}


/**
 * Opens a device
 * @param proc - process opening the device
 * @param device_no - device identifier from device table
 * @return file descriptor on success, -1 on failure
 */
int di_open(proc_ctrl_block_t *proc, int device_no) {
    int fd;

    ASSERT(proc != NULL);
    
    if (device_no < 0 || device_no >= NUM_DEVICES_ID_ENUMS) {
        return SYSERR;
    }
    
    for (fd = 0; fd < PCB_NUM_FDS; fd++) {
        if (proc->fd_table[fd] == NULL) {
            break;
        }
    }
    
    if (fd >= PCB_NUM_FDS) {
        return SYSERR;
    }
    
    devsw_t *entry = &g_device_table[device_no];
    int result = entry->dvopen(entry->dvioblk);
    if (result) {
        return SYSERR;
    }

    proc->fd_table[fd] = entry;
    return fd;
}

/**
 * Closes a device
 * @param proc - process owning the fd
 * @param fd - process's file descriptor for the open device
 * @return 0 on success, -1 on failure
 */
int di_close(proc_ctrl_block_t *proc, int fd) {
    devsw_t *entry;
    ASSERT(proc != NULL);
    
    if (check_fd(proc, fd)) {
        return SYSERR;
    }
    
    entry = proc->fd_table[fd];
    int result = entry->dvclose(entry->dvioblk);
    if (result) {
        return SYSERR;
    }

    proc->fd_table[fd] = NULL;
    return 0;
}

/**
 * Writes to a device
 * @param proc - process owning the fd
 * @param fd - process's file descriptor for the open device
 * @param buf - buffer to write data from
 * @param buflen - length of data to write
 * @return number of bytes written, or -1 on failure
 */
int di_write(proc_ctrl_block_t *proc, int fd, void *buf, int buflen) {
    ASSERT(proc != NULL && buf != NULL);

    if (check_fd(proc, fd)) {
        return SYSERR;
    }

    return proc->fd_table[fd]->dvwrite(proc, proc->fd_table[fd]->dvioblk,
                                       buf, buflen);
}

/**
 * Reads from a device
 * @param proc - process owning the fd
 * @param fd - process's file descriptor for the open device
 * @param buf - buffer to read data into
 * @param buflen - length of data to read
 * @return number of bytes read, or -1 on failure
 */
int di_read(proc_ctrl_block_t *proc, int fd, void *buf, int buflen) {
    ASSERT(proc != NULL);
    
    if (check_fd(proc, fd)) {
        return SYSERR;
    }
    
    return proc->fd_table[fd]->dvread(proc, proc->fd_table[fd]->dvioblk,
                                      buf, buflen);
}

/**
 * Device specific control
 * @param proc - process owning the fd
 * @param fd - process's file descriptor for the open device
 * @param command_code - device specific command
 * @param args - variable number of arguments for command
 * @return 0 on success, or -1 on failure
 */
int di_ioctl(proc_ctrl_block_t *proc, int fd,
             unsigned long command_code, void *args) {
    ASSERT(proc != NULL);

    if (check_fd(proc, fd)) {
        return SYSERR;
    }
    
    return proc->fd_table[fd]->dvioctl(proc->fd_table[fd]->dvioblk,
                                       command_code, args);
}

/**
 * Ensures a file descriptor is valid
 * @param proc - proc who owns fd
 * @param fd - file descriptor to check
 * @return 0 on success, 1 on failure
 */
static int check_fd(proc_ctrl_block_t* proc, int fd) {
    return (fd < 0 || fd >= PCB_NUM_FDS || proc->fd_table[fd] == NULL);
}
