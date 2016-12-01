/* kbd.h : keyboard driver
   See kbd.c for further documentation
 */

#include <xeroskernel.h>

// Upper half
void kbd_devsw_create(devsw_t *entry, int echo_flag);
int kbd_init(void);
int kbd_open(void *dvioblk);
int kbd_close(void *dvioblk);
int kbd_read(void *dvioblk, void* buf, int buflen);
int kbd_write(void *dvioblk, void* buf, int buflen);
int kbd_ioctl(void *dvioblk, unsigned long command, void *args);
int kbd_iint(void);
int kbd_oint(void);

// Lower half
void keyboard_isr(void);