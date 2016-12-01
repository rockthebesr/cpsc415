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
int di_open(int device_no) {
    DEBUG("device_no: %d\n", device_no);
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for sysclose
 */
int di_close(int fd) {
    DEBUG("fd: %d\n", fd);
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for syswrite
 */
int di_write(int fd, void *buf, int buflen) {
    DEBUG("fd: %d, buf: 0x%08x, buflen: %d\n", fd, buf, buflen);
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for sysread
 */
int di_read(int fd, void *buf, int buflen) {
    DEBUG("fd: %d, buf: 0x%08x, buflen: %d\n", fd, buf, buflen);
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for sysioctl
 */
int di_ioctl(void) {
    DEBUG("\n");
    // TODO: Implement me!
    return -1;
}
