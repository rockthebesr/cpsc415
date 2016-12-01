/* kbd.c: Keyboard device specific code
*/

#include <xeroslib.h>
#include <kbd.h>

// TODO: Default EOF should be ctrl-D
#define KBD_DEFAULT_EOF 0

typedef struct kbd_dvioblk {
    char eof;
    int echo_flag; // 1 for on, 0 for off
} kbd_dvioblk_t;

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
    DEBUG("\n");
    return -1;
}

int kbd_write(void *dvioblk, void* buf, int buflen) {
    DEBUG("\n");
    return -1;
}

int kbd_ioctl(void *dvioblk, unsigned long command, ...) {
    DEBUG("\n");
    return -1;
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
