/* kbd.c: Keyboard device specific code
*/

#include <xeroslib.h>
#include <kbd.h>
#include <stdarg.h>
#include <i386.h>

// TODO: Default EOF should be ctrl-D
#define KBD_DEFAULT_EOF 0

#define KEYBOARD_PORT_CONTROL_READY_MASK 0x01
#define KEYBOARD_PORT_DATA_SCANCODE_MASK 0x0FF

typedef struct kbd_dvioblk {
    char eof;
    int echo_flag; // 1 for on, 0 for off
} kbd_dvioblk_t;

static int kbd_ioctl_set_eof(kbd_dvioblk_t *dvioblk, void *args);
static int kbd_ioctl_enable_echo(kbd_dvioblk_t *dvioblk);
static int kbd_ioctl_disable_echo(kbd_dvioblk_t *dvioblk);
static int kbd_ioctl_get_eof(kbd_dvioblk_t *dvioblk);
static int kbd_ioctl_get_echo_flag(kbd_dvioblk_t *dvioblk);
// Only 1 keyboard is allowed to be open at a time
static int g_kbd_in_use = 0;

static char keyboard_process_scancode(int data);
#define KEYBOARD_STATE_SHIFT_FLAG (0x01)
#define KEYBOARD_STATE_CTRL_FLAG (0x02)
static int g_keyboard_keystate_flag = 0;


/**
 * Fills in a device table entry with keyboard-device specific values
 * @param entry - device table entry to be modified
 */
void kbd_devsw_create(devsw_t *entry, int echo_flag) {
    ASSERT(entry != NULL);
    
    sprintf(entry->dvname, "keyboard");
    entry->dvinit = &kbd_init;
    entry->dvopen = &kbd_open;
    entry->dvclose = &kbd_close;
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
    g_kbd_in_use = 0;
    // Read data from the ports, in case some interrupts triggered in the past
    inb(KEYBOARD_PORT_DATA);
    inb(KEYBOARD_PORT_CONTROL);
    return 0;
}

int kbd_open(void *dvioblk) {
    // unused
    (void)dvioblk;
    
    if (g_kbd_in_use) {
        return EBUSY;
    }
    
    g_kbd_in_use = 1;
    g_keyboard_keystate_flag = 0;
    setEnabledKbd(1);
    return 0;
}

int kbd_close(void *dvioblk) {
    // unused
    (void)dvioblk;
    
    if (!g_kbd_in_use) {
        return EBADF;
    }
    
    g_kbd_in_use = 0;
    setEnabledKbd(0);
    return 0;
}

int kbd_read(void *dvioblk, void* buf, int buflen) {
    DEBUG("buf: 0x%08x, buflen: %d\n", buf, buflen);
    // TODO: implement me!
    return 0;
}

int kbd_write(void *dvioblk, void* buf, int buflen) {
    (void)dvioblk;
    (void)buf;
    (void)buflen;
    // Cannot write to keyboard
    return -1;
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

/**
 * Keyboard lower-half functions
 * For naming, kbd_* is upper half
 *             keyboard_* is lower half
 */
void keyboard_isr(void) {
    int isDataPresent;
    int data;
    char c = 0;
    
    isDataPresent = KEYBOARD_PORT_CONTROL_READY_MASK & inb(KEYBOARD_PORT_CONTROL);
    data = KEYBOARD_PORT_DATA_SCANCODE_MASK & inb(KEYBOARD_PORT_DATA);
    if (isDataPresent) {
        c = keyboard_process_scancode(data);
        if (c != 0) {
            kprintf("%c", c);
        }
    }
}

/**
 * Translate the scancodes
 * @param data - raw keycode
 * @return char if a character was input, 0 otherwise
 */
static char keyboard_process_scancode(int data) {
    static char lower[0x54] = {
        // 0x00 - 0x07
           0,    0,  '1',  '2',  '3',  '4',  '5',  '6',
        // 0x08 - 0x0F
         '7',  '8',  '9',  '0',  '-',  '=',    0, '\t',
        // 0x10 - 0x17
         'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
        // 0x18 - 0x1F
         'o',  'p',  '[',  ']', '\n',    0,  'a',  's',
        // 0x20 - 0x27
         'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
        // 0x28 - 0x2F
        '\'',  '`',    0, '\\',  'z',  'x',  'c',  'v',
        // 0x30 - 0x37
         'b',  'n',  'm',  ',',  '.',  '/',    0,    0,
        // 0x38 - 0x3F
           0,  ' ',    0,    0,    0,    0,    0,    0,
        // 0x40 - 0x47
           0,    0,    0,    0,    0,    0,    0,    0,
        // 0x48 - 0x4F
           0,    0,  '-',    0,    0,    0,    0,   '+',
        // 0x50 - 0x53
           0,    0,    0,    0
    };

    static char upper[0x54] = {
        // 0x00 - 0x07
           0,    0,  '!',  '@',  '#',  '$',  '%',  '^',
        // 0x08 - 0x0F
         '&',  '*',  '(',  ')',  '_',  '+',    0, '\t',
        // 0x10 - 0x17
         'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
        // 0X18 - 0X1F
         'O',  'P',  '{',  '}', '\n',    0,  'A',  'S',
        // 0X20 - 0X27
         'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
        // 0X28 - 0X2F
         '"',  '~',    0,  '|',  'Z',  'X',  'C',  'V',
        // 0X30 - 0X37
         'B',  'N',  'M',  '<',  '>',  '?',    0,    0,
        // 0x38 - 0x3F
           0,  ' ',    0,    0,    0,    0,    0,    0,
        // 0x40 - 0x47
           0,    0,    0,    0,    0,    0,    0,    0,
        // 0x48 - 0x4F
           0,    0,  '-',    0,    0,    0,    0,   '+',
        // 0x50 - 0x53
           0,    0,    0,    0
    };
    
    char c = 0;
    if (data < 0x54) {
        if (g_keyboard_keystate_flag & KEYBOARD_STATE_CTRL_FLAG) {
            c = '^';
        } else if (g_keyboard_keystate_flag & KEYBOARD_STATE_SHIFT_FLAG) {
            c = upper[data];
        } else {
            c = lower[data];
        }
    }
    
    if (c == 0) {
        switch(data) {
            case 0x2A:
            case 0x36:
                // shfit key pressed
                g_keyboard_keystate_flag |= KEYBOARD_STATE_SHIFT_FLAG;
                break;
            case 0xAA:
            case 0xB6:
                // shift key released
                g_keyboard_keystate_flag &= ~KEYBOARD_STATE_SHIFT_FLAG;
                break;
            case 0x1D:
                g_keyboard_keystate_flag |= KEYBOARD_STATE_CTRL_FLAG;
                break;
            case 0x9D:
                g_keyboard_keystate_flag &= ~KEYBOARD_STATE_CTRL_FLAG;
                break;
            default:
                break;
        }
    }
    return c;
}