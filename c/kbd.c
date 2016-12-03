/* kbd.c: Keyboard device specific code
*/

#include <stdarg.h>
#include <xeroslib.h>
#include <kbd.h>
#include <pcb.h>
#include <i386.h>

#define KBD_DEFAULT_EOF ((char)0x04)

#define KEYBOARD_PORT_CONTROL_READY_MASK 0x01
#define KEYBOARD_PORT_DATA_SCANCODE_MASK 0x0FF

typedef struct kbd_task {
    proc_ctrl_block_t *pcb;
    void *buf;
    int buflen;
    int i;
} kbd_task_t;
// Circular buffer implementation, 1 index is "wasted" to indicate full buffer
#define KBD_TASK_QUEUE_SIZE (PCB_TABLE_SIZE + 1)
static kbd_task_t g_kbd_task_queue[KBD_TASK_QUEUE_SIZE];
static int g_kbd_task_queue_head = 0;
static int g_kbd_task_queue_tail = 0;

typedef struct kbd_dvioblk {
    int orig_echo_flag;
} kbd_dvioblk_t;

static int kbd_ioctl_set_eof(void *args);
// Only 1 keyboard type is allowed to be open at a time
static int g_kbd_refcount = 0;
static int g_kbd_current_type = 0;
static int g_kbd_done = 0;

static void keyboard_flush_buffer(void);
static char keyboard_process_scancode(int data);
static void keyboard_unblock_proc(proc_ctrl_block_t *pcb, int retval);
static void keyboard_process_char(char c);
static void keyboard_handle_eof(void);

#define KEYBOARD_STATE_SHIFT_BIT 0
#define KEYBOARD_STATE_CTRL_BIT 1
#define KEYBOARD_STATE_CAPLOCK_BIT 2
static int g_keyboard_keystate_flag = 0;

// Circular buffer implementation, 1 index is "wasted" to indicate full buffer
#define KEYBOARD_BUFFER_SIZE (4 + 1)
static char g_keyboard_buffer[KEYBOARD_BUFFER_SIZE] = {0};
static int g_keyboard_buffer_head = 0;
static int g_keyboard_buffer_tail = 0;
static char g_keyboard_eof;
static char g_keyboard_echo_flag; // 1 for on, 0 for off

/**
 * Fills in a device table entry with keyboard-device specific values
 * @param entry - device table entry to be modified
 * @param echo_flag - 1 if keyboard will echo the keystrokes 0 for silent
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
    entry->dvminor = echo_flag;
    // Note: this kmalloc will intentionally never be kfree'd
    entry->dvioblk = (kbd_dvioblk_t*)kmalloc(sizeof(kbd_dvioblk_t));
    ASSERT(entry->dvioblk != NULL);
    ((kbd_dvioblk_t*)entry->dvioblk)->orig_echo_flag = echo_flag;
}

/******************************************************************************
 * Implementations of devsw abstract functions
 ******************************************************************************/

int kbd_init(void) {
    g_kbd_refcount = 0;
    g_kbd_done = 0;
    g_keyboard_buffer_head = 0;
    g_keyboard_buffer_tail = 0;
    g_kbd_task_queue_head = 0;
    g_kbd_task_queue_tail = 0;
    
    // Read data from the ports, in case some interrupts triggered in the past
    inb(KEYBOARD_PORT_DATA);
    inb(KEYBOARD_PORT_CONTROL);
    return 0;
}

int kbd_open(proc_ctrl_block_t *proc, void *dvioblk) {
    // unused
    (void)proc;
    
    int echo_flag = ((kbd_dvioblk_t*)dvioblk)->orig_echo_flag;
    
    if (g_kbd_refcount > 0) {
        if (g_kbd_current_type != echo_flag) {
            return EBUSY;
        }
        
        g_kbd_refcount++;
        return 0;
    }
    
    g_kbd_refcount = 1;
    g_kbd_current_type = echo_flag;
    g_kbd_done = 0;
    g_keyboard_buffer_head = 0;
    g_keyboard_buffer_tail = 0;
    g_kbd_task_queue_head = 0;
    g_kbd_task_queue_tail = 0;
    g_keyboard_keystate_flag = 0;
    g_keyboard_eof = KBD_DEFAULT_EOF;
    g_keyboard_echo_flag = echo_flag;
    setEnabledKbd(1);
    return 0;
}

int kbd_close(proc_ctrl_block_t *proc, void *dvioblk) {
    // unused
    (void)dvioblk;
    int i;
    int j;
    int k;
    
    if (g_kbd_refcount <= 0) {
        return EBADF;
    }
    
    g_kbd_refcount--;
    if (g_kbd_refcount == 0) {
        setEnabledKbd(0);
    }
    
    // Reorganize our queue in case the proc was blocking us
    // This is somewhat expensive, but thankfully should only occur rarely
    for (i = g_kbd_task_queue_tail; i != g_kbd_task_queue_head; i = (i + 1) % KBD_TASK_QUEUE_SIZE) {
        if (g_kbd_task_queue[i].pcb == proc) {
            // shuffle the procs down
            for (j = i; j != g_kbd_task_queue_head; j = (j + 1) % KBD_TASK_QUEUE_SIZE) {
                k = (j + 1) % KBD_TASK_QUEUE_SIZE;
                g_kbd_task_queue[j] = g_kbd_task_queue[k];
            }
            
            // decrement our head in a way where we don't underflow
            g_kbd_task_queue_head = (g_kbd_task_queue_head + KBD_TASK_QUEUE_SIZE - 1) % KBD_TASK_QUEUE_SIZE;
        }            
    }
    
    return 0;
}

int kbd_read(proc_ctrl_block_t *proc, void *dvioblk, void* buf, int buflen) {
    // unused
    (void)proc;
    (void)dvioblk;
    
    kbd_task_t *task = &g_kbd_task_queue[g_kbd_task_queue_head];
    g_kbd_task_queue_head++;
    
    task->pcb = proc;
    task->buf = buf;
    task->i = 0;
    task->buflen = buflen;
    
    keyboard_flush_buffer();
    if (task->i == buflen) {
        return buflen;
    }
    
    if (g_kbd_done) {
        // EOF was encountered. We have to do this check
        // here in case our buffer has a couple of stray \n characters,
        // which would require multiple reads to fully flush
        return task->i;
    }
    
    return BLOCKERR;
}

int kbd_write(proc_ctrl_block_t *proc, void *dvioblk, void* buf, int buflen) {
    // unused
    (void)proc;
    (void)dvioblk;
    (void)buf;
    (void)buflen;
    // Cannot write to keyboard
    return -1;
}

int kbd_ioctl(proc_ctrl_block_t *proc, void *dvioblk, unsigned long command, void *args) {
    // unused
    (void)proc;
    (void)dvioblk;
    
    switch(command) {
        case KEYBOARD_IOCTL_SET_EOF:
            return kbd_ioctl_set_eof(args);
            
        case KEYBOARD_IOCTL_ENABLE_ECHO:
            g_keyboard_echo_flag = 1;
            return 0;
            
        case KEYBOARD_IOCTL_DISABLE_ECHO:
            g_keyboard_echo_flag = 0;
            return 0;
            
        case KEYBOARD_IOCTL_GET_EOF:
            return (int)g_keyboard_eof;
            
        case KEYBOARD_IOCTL_GET_ECHO:
            return g_keyboard_echo_flag;
            
        default:
            return SYSERR;
    }
}

int kbd_iint(void) {
    return -1;
}

int kbd_oint(void) {
    return -1;
}

/**
 * kbd_ioctl_set_eof
 * Helper function for setting the EOF character recognized
 * @param args - va_list passed from userspace
 * @return 0 on success, error code otherwise
 */
static int kbd_ioctl_set_eof(void *args) {
    va_list v;
    
    if (args == NULL) {
        return SYSERR;
    }
    
    v = (va_list)args;
    // the va_arg parameter requires int, compiler warns against using char
    g_keyboard_eof = (char)va_arg(v, int);
    
    return 0;
}

/******************************************************************************
 * Keyboard lower-half functions
 ******************************************************************************/
 
/**
 * keyboard_isr
 * Function called when a keyboard interrupt occurs.
 * Reads the data from the keyboard's registers via ports and handles the data
 *
 * The data will either be directly written to a waiting proc's buffer
 * or be buffered internally in g_keyboard_buffer to wait for future reads.
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
            if (g_keyboard_echo_flag) {
                kprintf("%c", c);
            }
            
            if (g_kbd_task_queue_tail != g_kbd_task_queue_head) {
                // If there is a task waiting, write to task
                keyboard_process_char(c);
            } else if (((g_keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE) != g_keyboard_buffer_tail) {
                // Make sure buffer has room
                g_keyboard_buffer[g_keyboard_buffer_head] = c;
                g_keyboard_buffer_head = (g_keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
            }
        }
    }
}

/**
 * keyboard_flush_buffer
 * Flush the entire keyboard buffer into the buffers held by blocked processes
 */
static void keyboard_flush_buffer(void) {
    char c;
    
    // While the buffer still has some contents in it
    while (g_kbd_task_queue_tail != g_kbd_task_queue_head &&
        g_keyboard_buffer_tail != g_keyboard_buffer_head) {
            
        c = g_keyboard_buffer[g_keyboard_buffer_tail];
        keyboard_process_char(c);
        g_keyboard_buffer_tail = (g_keyboard_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    }
}

/**
 * keyboard_process_char
 * Logic for handling a ascii character, namely handling EOFs and newlines
 * @param c - ascii character
 */
static void keyboard_process_char(char c) {
    kbd_task_t *task;
    
    if (c == g_keyboard_eof) {
        keyboard_handle_eof();
        return;
    }
    
    ASSERT(g_kbd_task_queue_tail != g_kbd_task_queue_head);
    task = &g_kbd_task_queue[g_kbd_task_queue_tail];
    ((char*)(task->buf))[task->i] = c;
    task->i++;
    
    // Unblock once we've filled the buffer, OR  we encounter \n
    if (task->i == task->buflen || c == '\n') {
        g_kbd_task_queue_tail = (g_kbd_task_queue_tail + 1) % KBD_TASK_QUEUE_SIZE;
        keyboard_unblock_proc(task->pcb, task->i);
    }
}

/**
 * keyboard_handle_eof
 * Logic for handling EOF character, namely waking all blocked processes.
 */
static void keyboard_handle_eof(void) {
    kbd_task_t *task;
    
    setEnabledKbd(0);
    g_kbd_done = 1;
    
    // Flush all queues
    while (g_kbd_task_queue_tail != g_kbd_task_queue_head) {
        task = &g_kbd_task_queue[g_kbd_task_queue_tail];
        g_kbd_task_queue_tail = (g_kbd_task_queue_tail + 1) % KBD_TASK_QUEUE_SIZE;
        keyboard_unblock_proc(task->pcb, task->i);
    }
    
    return;
}

/**
 * keyboard_unblock_proc
 * Helper method to unblock a process
 * @param pcb - proc to unblock
 * @param retval - return value to set in the pcb
 */
static void keyboard_unblock_proc(proc_ctrl_block_t *pcb, int retval) {
    pcb->ret = retval;
    // Edge case: Make sure we only unblock BLOCKED processes
    if (pcb->curr_state == PROC_STATE_BLOCKED) {
        add_pcb_to_queue(pcb, PROC_STATE_READY);
        pcb->blocking_queue_name = NO_BLOCKER;
    }
}

/**
 * Translate the scancodes
 * @param data - raw keycode
 * @return ascii char
 */
static char keyboard_process_scancode(int data) {
    static char lower[0x54] = {
        // 0x00 - 0x07
           0, 0x1B,  '1',  '2',  '3',  '4',  '5',  '6',
        // 0x08 - 0x0F
         '7',  '8',  '9',  '0',  '-',  '=', 0x08, '\t',
        // 0x10 - 0x17
         'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
        // 0x18 - 0x1F
         'o',  'p',  '[',  ']', '\n',    0,  'a',  's',
        // 0x20 - 0x27
         'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
        // 0x28 - 0x2F
        '\'',  '`',    0, '\\',  'z',  'x',  'c',  'v',
        // 0x30 - 0x37
         'b',  'n',  'm',  ',',  '.',  '/',    0, 0x2A,
        // 0x38 - 0x3F
           0,  ' ',    0,    0,    0,    0,    0,    0,
        // 0x40 - 0x47
           0,    0,    0,    0,    0,    0,    0,    0,
        // 0x48 - 0x4F
           0,    0, 0x2D,    0,    0,    0, 0x2B,    0,
        // 0x50 - 0x53
           0,    0,    0,    0
    };

    static char upper[0x54] = {
        // 0x00 - 0x07
           0, 0x1B,  '!',  '@',  '#',  '$',  '%',  '^',
        // 0x08 - 0x0F
         '&',  '*',  '(',  ')',  '_',  '+', 0x08, '\t',
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
           0,    0,    0,    0,    0,    0,    0, 0x37,
        // 0x48 - 0x4F
        0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
        // 0x50 - 0x53
        0x32, 0x33, 0x30, 0x2E
    };
    
    static char ctrl[0x54] = {
        // 0x00 - 0x07
           0, 0x1B,    0,    0,    0,    0,    0, 0x1E,
        // 0x08 - 0x0F
           0,    0,    0,    0, 0x1F,    0, 0x7F,    0,
        // 0x10 - 0x17
        0x11, 0x17, 0x05, 0x12, 0x14, 0x19, 0x15, 0x09,
        // 0X18 - 0X1F
        0x0F, 0x10, 0x1B, 0x1D, 0x0A,    0, 0x01, 0x13,
        // 0X20 - 0X27
        0x04, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,    0,
        // 0X28 - 0X2F
           0,    0,    0, 0x1C, 0x1A, 0x18, 0x03, 0x16,
        // 0X30 - 0X37
        0x02, 0x0E, 0x0D,    0,    0,    0,    0, 0x10,
        // 0x38 - 0x3F
           0,  ' ',    0,    0,    0,    0,    0,    0,
        // 0x40 - 0x47
           0,    0,    0,    0,    0,    0,    0,    0,
        // 0x48 - 0x4F
           0,    0,    0,    0,    0,    0,    0,    0,
        // 0x50 - 0x53
           0,    0,    0,    0
    };
    
    char c = 0;
    
    if (data < 0x54) {
        if (FLAG_BIT_CHECK(g_keyboard_keystate_flag, KEYBOARD_STATE_CTRL_BIT)) {
            c = ctrl[data];
        } else if (FLAG_BIT_CHECK(g_keyboard_keystate_flag, KEYBOARD_STATE_SHIFT_BIT) ^
            FLAG_BIT_CHECK(g_keyboard_keystate_flag, KEYBOARD_STATE_CAPLOCK_BIT)) {
            // XOR in condition: Either SHIFT is pressed, or CAPLOCK is on
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
                FLAG_BIT_SET(g_keyboard_keystate_flag, KEYBOARD_STATE_SHIFT_BIT);
                break;
            case 0xAA:
            case 0xB6:
                // shift key released
                FLAG_BIT_CLEAR(g_keyboard_keystate_flag, KEYBOARD_STATE_SHIFT_BIT);
                break;
            case 0x1D:
                // ctrl key pressed
                FLAG_BIT_SET(g_keyboard_keystate_flag, KEYBOARD_STATE_CTRL_BIT);
                break;
            case 0x9D:
                // ctrl key released
                FLAG_BIT_CLEAR(g_keyboard_keystate_flag, KEYBOARD_STATE_CTRL_BIT);
                g_keyboard_keystate_flag &= ~KEYBOARD_STATE_CTRL_BIT;
                break;
            case 0x3A:
                // caplock pressed
                FLAG_BIT_TOGGLE(g_keyboard_keystate_flag, KEYBOARD_STATE_CAPLOCK_BIT);
                break;
            default:
                break;
        }
    }
    return c;
}
