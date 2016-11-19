/* pcb.h : process control blocks
   See pcb.c for further documentation
 */

#ifndef PCB_H
#define PCB_H

// maximum number of processes
#define PCB_TABLE_SIZE 32

#define SIGNAL_TABLE_SIZE 32

// Kernel PCB structures defined in pcb.c
extern proc_ctrl_block_t g_pcb_table[PCB_TABLE_SIZE];
extern proc_ctrl_block_t *g_proc_queue_heads[2];
extern proc_ctrl_block_t *g_proc_queue_tails[2];



void pcb_table_init(void);

proc_ctrl_block_t* get_next_proc(void);
proc_ctrl_block_t* get_next_available_pcb(void);

void add_pcb_to_queue(proc_ctrl_block_t *proc, proc_state_enum_t new_state);
void remove_pcb_from_queue(proc_ctrl_block_t *proc);
void print_pcb_queue(proc_state_enum_t queue);

void add_proc_to_msgqueue(proc_ctrl_block_t *proc,
                          proc_ctrl_block_t *queue_owner,
                          blocking_queue_t queue);

int remove_proc_from_msgqueue(proc_ctrl_block_t *proc,
                              proc_ctrl_block_t *queue_owner,
                              blocking_queue_t queue);

proc_ctrl_block_t* pid_to_proc(int pid);
proc_ctrl_block_t* get_idleproc(void);

int terminate_proc_if_exists(int pid);
void cleanup_proc(proc_ctrl_block_t *proc);

#endif
