/* kbd.c: Keyboard device specific code
*/

#include <xeroslib.h>
#include <kbd.h>
#include <stdarg.h>

// TODO: Default EOF should be ctrl-D
#define KBD_DEFAULT_EOF 0

typedef struct kbd_dvioblk {
    char eof;
    int echo_flag; // 1 for on, 0 for off
} kbd_dvioblk_t;

static int kbd_ioctl_set_eof(kbd_dvioblk_t *dvioblk, void *args);
static int kbd_ioctl_enable_echo(kbd_dvioblk_t *dvioblk);
static int kbd_ioctl_disable_echo(kbd_dvioblk_t *dvioblk);
static int kbd_ioctl_get_eof(kbd_dvioblk_t *dvioblk);
static int kbd_ioctl_get_echo_flag(kbd_dvioblk_t *dvioblk);

/**
 * Fills in a device table entry with keyboard-device specific values
 * @param entry - device table entry to be modified
 */
void kbd_devsw_create(devsw_t *entry, int echo_flag) {
    ASSERT(entry != NULL);
    
    sprintf(entry->dvname, "keyboard");
    entry->dvinit = &kbd_init;
    entry->dvread = &kbd_read;
    entry->dvwrite = &kbd_write;
    entry->dvioctl = &kbd_ioctl;
    entry->dviint = &kbd_iint;
    entry->dvoint = &kbd_oint;
    // Note: this kmalloc will intentionally never be kfree'd
    entry->dvioblk = (kbd_dvioblk_t*)kmalloc(sizeof(kbd_dvioblk_t));
    ASSERT(entry->dvioblk != NULL);
    ((kbd_dvioblk_t*)entry->dvioblk)->eof = (char)KBD_DEFAULT_EOF;
    ((kbd_dvioblk_t*)entry->dvioblk)->echo_flag = echo_flag;
}

/**
 * Implementations of devsw abstract functions
 */

int kbd_init(void) {
    DEBUG("\n");
    return -1;
}

int kbd_read(void *dvioblk, void* buf, int buflen) {
    DEBUG("buf: 0x%08x, buflen: %d\n", buf, buflen);
    // TODO: implement me!
    return 0;
}

int kbd_write(void *dvioblk, void* buf, int buflen) {
    DEBUG("buf: 0x%08x, buflen: %d\n", buf, buflen);
    // TODO: implement me!
    return 0;
}

int kbd_ioctl(void *dvioblk, unsigned long command, void *args) {
    switch(command) {
        case KEYBOARD_IOCTL_SET_EOF:
            return kbd_ioctl_set_eof((kbd_dvioblk_t*)dvioblk, args);
        case KEYBOARD_IOCTL_ENABLE_ECHO:
            return kbd_ioctl_enable_echo((kbd_dvioblk_t*)dvioblk);
        case KEYBOARD_IOCTL_DISABLE_ECHO:
            return kbd_ioctl_disable_echo((kbd_dvioblk_t*)dvioblk);
        case KEYBOARD_IOCTL_GET_EOF:
            return kbd_ioctl_get_eof((kbd_dvioblk_t*)dvioblk);
        case KEYBOARD_IOCTL_GET_ECHO:
            return kbd_ioctl_get_echo_flag((kbd_dvioblk_t*)dvioblk);
        default:
            return ENOIOCTLCMD;
    }
}

// input available interrupt
int kbd_iint(void) {
    DEBUG("\n");
    return -1;
}

// output available interrupt
int kbd_oint(void) {
    DEBUG("\n");
    return -1;
}

/**
 * ioctl commands
 */
static int kbd_ioctl_set_eof(kbd_dvioblk_t *dvioblk, void *args) {
    ASSERT(dvioblk != NULL);
    va_list v;
    
    if (args == NULL) {
        return EINVAL;
    }
    
    v = (va_list)args;
    // the va_arg parameter requires int, compiler warns against using char
    ((kbd_dvioblk_t*)dvioblk)->eof = (char)va_arg(v, int);
    
    return 0;
}

static int kbd_ioctl_enable_echo(kbd_dvioblk_t *dvioblk) {
    ASSERT(dvioblk != NULL);
    ((kbd_dvioblk_t*)dvioblk)->echo_flag = 1;
    return 0;
}

static int kbd_ioctl_disable_echo(kbd_dvioblk_t *dvioblk) {
    ASSERT(dvioblk != NULL);
    ((kbd_dvioblk_t*)dvioblk)->echo_flag = 0;
    return 0;
}

static int kbd_ioctl_get_eof(kbd_dvioblk_t *dvioblk) {
    ASSERT(dvioblk != NULL);
    return (int)((kbd_dvioblk_t*)dvioblk)->eof;
}

static int kbd_ioctl_get_echo_flag(kbd_dvioblk_t *dvioblk) {
    ASSERT(dvioblk != NULL);
    return (int)((kbd_dvioblk_t*)dvioblk)->echo_flag;
}