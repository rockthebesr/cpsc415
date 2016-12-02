/* xeroskernel.h - disable, enable, halt, restore, isodd, min, max */

#ifndef XEROSKERNEL_H
#define XEROSKERNEL_H

/* Symbolic constants used throughout Xinu */

typedef	char    Bool;        /* Boolean type                  */
typedef unsigned int size_t; /* Something that can hold the value of
                              * theoretical maximum number of bytes 
                              * addressable in this architecture.
                              */
#define	FALSE   0       /* Boolean constants             */
#define	TRUE    1
#define	EMPTY   (-1)    /* an illegal gpq                */
#define	NULL    0       /* Null pointer for linked lists */
#define	NULLCH '\0'     /* The null character            */


/* Universal return constants */

#define	OK            1         /* system call ok               */
#define	SYSERR       -1         /* system call failed           */
#define	EOF          -2         /* End-of-file (usu. from read)	*/
#define	TIMEOUT      -3         /* time out  (usu. recvtim)     */
#define	INTRMSG      -4         /* keyboard "intr" key pressed	*/
                                /*  (usu. defined as ^B)        */
#define	BLOCKERR     -5         /* non-blocking op would block  */
#define	EINVAL       -6         /* invalid parameter */
#define	ENOMEM       -7         /* out of memory */
#define EPROCLIMIT   -8         /* process limit reached */
#define ENODEV       -9         /* device does not exist */
#define EMFILE       -10        /* too many open files (no available FDs) */
#define EBADF        -11        /* bad FD number */
#define ENOIOCTLCMD  -12        /* no such IOCTL command for this device */
#define EBUSY        -13        /* device busy */

#define DEFAULT_STACK_SIZE 8192
#define TICK_LENGTH_IN_MS 10

/* Functions defined by startup code */


void           bzero(void *base, int cnt);
void           bcopy(const void *src, void *dest, unsigned int n);
void           disable(void);
unsigned short getCS(void);
unsigned char  inb(unsigned int);
void           init8259(void);
int            kprintf(char * fmt, ...);
void           lidt(void);
void           outb(unsigned int, unsigned char);
void           set_evec(unsigned int xnum, unsigned long handler);

/* Helpful macros - why aren't these in lib? */
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* Helpful debug macros */
#ifdef TESTING
#define DEBUG(...) kprintf("[%s:%d %s] ", __FILE__, __LINE__, __FUNCTION__); kprintf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif
#define ASSERT(x) if (!(x)) { DEBUG("Assertion failed!"); while(1); }
#define ASSERT_EQUAL(x, y) if (x != y) { DEBUG("Assertion failed. %d != %d", x, y); while(1); }

typedef void (*funcptr)(void);
typedef void (*funcptr_args1)(void*);

/* Memory Manager */
void  kmeminit(void);
long  kmem_maxaddr(void);
long  kmem_freemem(void);
void* kmalloc(size_t size);
void  kfree(void *ptr);
void  kmem_dump_free_list(void);
int kmem_get_free_list_length(void);

/* Forward declarations */
typedef struct proc_ctrl_block proc_ctrl_block_t;

/* Devices */
typedef enum device_id_enum {
    DEVICE_ID_KEYBOARD_NO_ECHO = 0,
    DEVICE_ID_KEYBOARD,
    NUM_DEVICES_ID_ENUMS
} device_id_enum_t;

#define KEYBOARD_PORT_DATA 0x60
#define KEYBOARD_PORT_CONTROL 0x64

#define KEYBOARD_IOCTL_SET_EOF 53
#define KEYBOARD_IOCTL_ENABLE_ECHO 55
#define KEYBOARD_IOCTL_DISABLE_ECHO 56
// These aren't required by asst3, but we've implemented them for testing
#define KEYBOARD_IOCTL_GET_EOF 57
#define KEYBOARD_IOCTL_GET_ECHO 58

typedef struct devsw {
    int dvnum;
    char dvname[20];
    int (*dvinit)(void);
    int (*dvopen)(void *dvioblk);
    int (*dvclose)(void *dvioblk);
    int (*dvread)(proc_ctrl_block_t *proc, void *dvioblk, void *buf, int buflen);
    int (*dvwrite)(proc_ctrl_block_t *proc, void *dvioblk, void *buf, int buflen);
    int (*dvioctl)(void *dvioblk, unsigned long command, void *args);
    // input available interrupt
    int (*dviint)(void);
    // output available interrupt
    int (*dvoint)(void);
    // device specific data (usually pointer to another struct)
    void *dvioblk;
} devsw_t;

void di_init_devtable(void);

/* Process Manager */

// maximum number of processes
#define PCB_TABLE_SIZE 32
#define PCB_NUM_FDS 4

typedef enum {
    PROC_STATE_READY = 0,
    PROC_STATE_STOPPED = 1,
    PROC_STATE_BLOCKED,
    PROC_STATE_RUNNING
} proc_state_enum_t;

typedef enum {
    SENDER = 0,
    RECEIVER = 1,
    WAITING = 2,
    RECEIVE_ANY,
    SLEEP,
    NO_BLOCKER
} blocking_queue_t;

typedef struct proc_ctrl_block {
    int pid;
    proc_state_enum_t curr_state;
    struct proc_ctrl_block *next_proc;
    struct proc_ctrl_block *prev_proc;
    int cpu_time;

    void *memory_region;
    void *esp;
    unsigned long *args;
    int ret;

    funcptr_args1 *signal_table;
    int signals_fired;
    int signals_enabled;
    
    devsw_t *fd_table[PCB_NUM_FDS];

    struct proc_ctrl_block *blocking_proc;
    blocking_queue_t blocking_queue_name;
    struct proc_ctrl_block *blocking_queue_heads[3];
    struct proc_ctrl_block *blocking_queue_tails[3];
} proc_ctrl_block_t;


/* disp */
#define TIMER_INTERRUPT_VALUE 32
#define KEYBOARD_INTERRUPT_VALUE 33
#define SYSCALL_INTERRUPT_VALUE 50

typedef enum {
    TIMER_INT,
    KEYBOARD_INT,
    SYSCALL_CREATE,
    SYSCALL_YIELD,
    SYSCALL_STOP,
    SYSCALL_GETPID,
    SYSCALL_KILL,
    SYSCALL_WAIT,
    SYSCALL_PUTS,
    SYSCALL_SEND,
    SYSCALL_RECV,
    SYSCALL_SLEEP,
    SYSCALL_CPUTIMES,
    SYSCALL_SIGHANDLER,
    SYSCALL_SIGRETURN,
    SYSCALL_OPEN,
    SYSCALL_CLOSE,
    SYSCALL_WRITE,
    SYSCALL_READ,
    SYSCALL_IOCTL
} syscall_request_id_t;

void dispinit(void);
void dispatch(funcptr root_proc);

/* syscall return constants */
#define SYSPID_OK       0
#define SYSPID_DNE     -1
#define SYSPID_ME      -2
#define SYSERR_OTHER   -3
#define SYSKILL_TARGET_DNE -712
#define SYSKILL_INVALID_SIGNAL -651
#define SYSWAIT_SIGNALLED -2
#define SYSHANDLER_INVALID_SIGNAL -1
#define SYSHANDLER_INVALID_FUNCPTR -2
#define PROC_SIGNALLED -362

/* ctsw */
void ctsw_init_evec(void);
syscall_request_id_t ctsw_contextswitch(proc_ctrl_block_t *proc);

/* syscall */
typedef struct struct_ps {
  int pid[PCB_TABLE_SIZE];
  int status[PCB_TABLE_SIZE];
  long cpuTime[PCB_TABLE_SIZE];
} processStatuses;

extern unsigned int syscreate(funcptr func, int stack);
extern void sysyield(void);
extern void sysstop(void);
extern int sysgetpid(void);
extern int syskill(int pid, int signalNumber);
extern int syswait(int pid);
extern void sysputs(char *str);
extern int syssendbuf(int dest_pid, void *buffer, unsigned long len);
extern int sysrecvbuf(int *from_pid, void *buffer, unsigned long len);
extern int syssend(int dest_pid, unsigned long num);
extern int sysrecv(int *from_pid, unsigned long *num);
extern unsigned int syssleep(unsigned int milliseconds);
extern int sysgetcputimes(processStatuses *ps);
extern int syssighandler(int signal, funcptr_args1 newhandler,
                         funcptr_args1 *oldHandler);
extern void syssigreturn(void *old_sp);
extern int sysopen(int device_no);
extern int sysclose(int fd);
extern int syswrite(int fd, void *buf, int buflen);
extern int sysread(int fd, void *buf, int buflen);
extern int sysioctl(int fd, unsigned long command, ...);

typedef struct context_frame {
    unsigned long edi;
    unsigned long esi;
    unsigned long ebp;
    unsigned long esp;
    unsigned long ebx;
    unsigned long edx;
    unsigned long ecx;
    unsigned long eax;
    unsigned long iret_eip;
    unsigned long iret_cs;
    unsigned long eflags;

    // context_frame appears on the stack following a syscall.
    // This location is where the arguments to the call lie
    // Adding syscallargs to this struct allows us to avoid ugly pointer math
    // syscallargs takes no space.
    unsigned long syscallargs[];
} context_frame_t;

/* disp calls for devices */

extern int di_open(proc_ctrl_block_t *proc, int device_no);
extern int di_close(proc_ctrl_block_t *proc, int fd);
extern int di_write(proc_ctrl_block_t *proc, int fd, void *buf, int buflen);
extern int di_read(proc_ctrl_block_t *proc, int fd, void *buf, int buflen);
extern int di_ioctl(proc_ctrl_block_t *proc, int fd,
                    unsigned long command_code, void *args);

/* kernel services */
extern void init_idle_proc(proc_ctrl_block_t *idle_proc);

extern int create(funcptr func, int stack);
extern void setup_context_frame(context_frame_t *cf, funcptr func);

extern int send(proc_ctrl_block_t *srcproc, proc_ctrl_block_t *destproc,
                void *buffer, unsigned long len);

extern int recv(proc_ctrl_block_t *srcproc, proc_ctrl_block_t *destproc,
                void *buffer, unsigned long len);

extern int recv_any(proc_ctrl_block_t *destproc,
                    void *buffer, unsigned long len);

extern void sleep(proc_ctrl_block_t *proc, unsigned int time);
extern void wake(proc_ctrl_block_t *proc);
extern void tick(void);

extern void sigtramp(funcptr_args1 handler, void *cntx);
extern int signal(int pid, int sig_no);

/* user programs */
extern void root(void);
extern void producer(void);
extern void consumer(void);
extern void child(void);
extern void parent(void);
extern void login_proc(void);

#endif
