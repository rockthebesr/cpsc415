/* kbd.h : keyboard driver
   See kbd.c for further documentation
 */

#include <xeroskernel.h>

void kbd_devsw_create(devsw_t *entry, int echo_flag);
int kbd_init(void);
int kbd_read(void *dvioblk, void* buf, int buflen);
int kbd_write(void *dvioblk, void* buf, int buflen);
int kbd_ioctl(void *dvioblk, unsigned long command, ...);
int kbd_iint(void);
int kbd_oint(void);