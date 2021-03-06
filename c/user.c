/* user.c : User processes
 */

#include <xeroskernel.h>
#include <xeroslib.h>

#define USER_KILL_SIGNAL 25

static void filter_newline(char *str);
static void shell(void);
static int get_command(char *str, char *command, char* arg);
static void setup_kill_handler(void);
static void command_ps(void);
static void command_k(void);
static void command_a(void);
static void command_t(void);
static void command_sudo(void);

static int g_pid_to_kill;

static char *detailed_states[] = {
    "READY",
    "STOPPED",
    "RUNNING",
    "BLOCKED: SENDING",
    "BLOCKED: RECEIVING",
    "BLOCKED: WAITING",
    "BLOCKED: RECEIVE ANY",
    "BLOCKED: SLEEPING",
    "BLOCKED: IO"
};

static char *g_arg;

/**
 * Authenticates the user, starts the shell process
 */
void login_proc(void) {
    char* valid_user = "cpsc415";
    char* valid_pass = "EveryoneGetsAnA";

    while(1) {
        char user_buf[80];
        char pass_buf[80];
        memset(user_buf, '\0', sizeof(user_buf));
        memset(pass_buf, '\0', sizeof(pass_buf));

        sysputs("Welcome to Xeros - an experimental OS\n");
        int fd = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);

        sysioctl(fd, KEYBOARD_IOCTL_ENABLE_ECHO);
        sysputs("Username: ");
        sysread(fd, user_buf, 20);

        sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO);
        sysputs("\nPassword: ");
        sysread(fd, pass_buf, 20);
        sysclose(fd);

        filter_newline(user_buf);
        filter_newline(pass_buf);

        if (strcmp(user_buf, valid_user) == 0 &&
            strcmp(pass_buf, valid_pass) == 0) {
            int shell_pid = syscreate(&shell, DEFAULT_STACK_SIZE);
            syswait(shell_pid);
        }

    }
}

/**
 * Converts first newline in a string to a null terminator
 * @param str - null terminated string
 */
static void filter_newline(char *str) {
    while(*str != '\0') {
        if (*str == '\n') {
            *str = '\0';
            break;
        }
        str++;
    }
}

/**
 * User's shell. Processes various commands
 */
static void shell(void) {
    setup_kill_handler();
    sysputs("\n");
    int fd = sysopen(DEVICE_ID_KEYBOARD);

    char buf[100];

    while(1) {
        memset(buf, '\0', sizeof(buf));

        sysputs("> ");
        int bytes = sysread(fd, buf, 80);
        if (bytes == 0) {
            // EOF encountered
            break;
        }

        filter_newline(buf);
        char command[50];
        char arg[50];
        char printbuf[80];
        int ampersand = get_command(buf, command, arg);

        int wait = 1;
        int pid = 0;

        if(!strcmp("t", command)) {
            pid = syscreate(&command_t, DEFAULT_STACK_SIZE);
            wait = !ampersand;

        } else if(!strcmp("ps", command)) {
            pid = syscreate(&command_ps, DEFAULT_STACK_SIZE);

        } else if(!strcmp("a", command)) {
            g_arg = arg;
            pid = syscreate(&command_a, DEFAULT_STACK_SIZE);

        } else if(!strcmp("k", command)) {
            g_pid_to_kill = atoi(arg);
            pid = syscreate(&command_k, DEFAULT_STACK_SIZE);

        } else if(!strcmp("ex", command)) {           
            break;

        } else if(!strcmp("sudo", command)) {
            wait = !ampersand;
            g_arg = arg;
            pid = syscreate(&command_sudo, DEFAULT_STACK_SIZE);
        
        } else {
            sprintf(printbuf, "The program %s is currently not installed. You can install it by typing:\n", command);
            sysputs(printbuf);
            sprintf(printbuf, "sudo apt-get install %s\n", command);
            sysputs(printbuf);
        }

        if (wait) {
            syswait(pid);
        }
    }

    sysputs("Goodbye.\n");
    sysclose(fd);
}

/**
 * Parses user input, determines command, if & was passed
 * @param str - user input to parse
 * @param command - buffer to place the command
 * @param arg - buffer to place argument in
 * @return 0 if & was not passed, 1 if it was
 */
static int get_command(char* str, char* command, char* arg) {
    int ampersand = 0;
    int len = 0;

    while (*str != '\0' && *str != ' ' && *str != '&') {
        *command = *str;
        command++;
        str++;
        len++;
    }

    *command = '\0';

    // get to first argument
    while ((*str != '\0') && (*str == ' ' || *str == '&')) {
        str++;
        len++;
    }

    // read argument
    while (*str != '\0' && *str != ' ' && *str != '&') {
        *arg = *str;
        arg++;
        str++;
        len++;
    }

    *arg = '\0';

    // go to the end of the command line, then we'll read to see if we hit a &
    while(*str != '\0') {
        str++;
        len++;
    }

    len--;
    str--; 
    while((len > 0) && (*str == ' ' || *str == '&')) {
        if (*str == '&') {
            ampersand = 1;
        }
        if (*str != ' ') {
            break;
        }
        str--;
        len--;
    }

    return ampersand;
}

static void command_ps(void) {
    setup_kill_handler();
    processStatuses ps;
    char str[80];

    int num = sysgetcputimes(&ps);

    sysputs("PID | State           | Time\n");
    for (int i = 0; i <= num; i++) {
        sprintf(str, "%4d  %16s  %8d\n", ps.pid[i],
                detailed_states[ps.status[i]], ps.cpuTime[i]);
        sysputs(str);
    }
}

/**
 * Kills the process with pid g_pid_to_kill
 */
static void command_k(void) {
    setup_kill_handler();
    int ret = syskill(g_pid_to_kill, USER_KILL_SIGNAL);
    if (ret) {
        sysputs("No such process.\n");
    }
}

static void command_a_handler(void) {
    funcptr_args1 oldHandler;
    
    sysputs("ALARM ALARM ALARM\n");
    syssighandler(15, NULL, &oldHandler);
}

static void command_a(void) {
    setup_kill_handler();
    
    funcptr_args1 oldHandler;
    int sleeparg = atoi(g_arg);
    
    if (sleeparg <= 0) {
        sysputs("Usage: a SLEEP_MILLIS\n");
        return;
    }
    
    syssighandler(15, (funcptr_args1)&command_a_handler, &oldHandler);
    syssleep(TICK_LENGTH_IN_MS * sleeparg);
    syskill(sysgetpid(), 15);
}

static void command_t(void) {
    setup_kill_handler();
    while(1) {
        sysputs("t\n");
        syssleep(10000);
    }
}

static void command_sudo(void) {
    setup_kill_handler();
    
    char buf[20] = {0};
    char printbuf[80];
    
    kprintf("[sudo] password for cpsc415:");
    int fd = sysopen(DEVICE_ID_KEYBOARD);
    int prev_echo = sysioctl(fd, KEYBOARD_IOCTL_GET_ECHO);
    sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO);
    sysread(fd, buf, sizeof(buf) - 1);
    
    if (!strcmp(buf, "EveryoneGetsAnA\n")) {
        sysputs("\nReading package lists...");
        syssleep(1000);
        sysputs("Done\n");
        
        sysputs("Building dependency tree...");
        syssleep(2000);
        sysputs("Done\n");
        
        sysputs("Reading state information...");
        syssleep(3000);
        sysputs("Done\n");
        
        sprintf(printbuf, "E: '%s' not found.\n", g_arg);
        sysputs(printbuf);
    } else {
        kprintf("Invalid password.\n");
    }
    
    if (prev_echo) {
        sysioctl(fd, KEYBOARD_IOCTL_ENABLE_ECHO);
    }
    
    sysclose(DEVICE_ID_KEYBOARD);
}

static void setup_kill_handler(void) {
    funcptr_args1 oldHandler;
    syssighandler(USER_KILL_SIGNAL,(funcptr_args1)&sysstop, &oldHandler);
}
