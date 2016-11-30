/* msg.c : messaging system 

Called from outside:
  send() - sends a message to a specific proc
  recv() - receives a message from a specific proc
  recv_any() - receives a message from any proc

Further details can be found in the documentation above the function headers.
*/

#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

/**
 * Sends a message to another proc
 * @param srcproc - the process to send from
 * @param destproc - the process to receive the data
 * @param buffer - buffer containing the data
 * @param len - length of the data in buffer
 * @return SYSPID return codes
 */
int send(proc_ctrl_block_t *srcproc, proc_ctrl_block_t *destproc,
         void *buffer, unsigned long len) {
    ASSERT(srcproc != NULL && destproc != NULL && buffer != NULL && len > 0);

    if (destproc->blocking_queue_name == RECEIVE_ANY ||
        remove_proc_from_blocking_queue(destproc, srcproc, RECEIVER)) {

        // Case 1: Receiver is waiting for us to send
        DEBUG("Receiver has been waiting for us!\n");

        if (destproc->blocking_queue_name == RECEIVE_ANY) {
            destproc->blocking_queue_name = NO_BLOCKER;
            int *from_pid = (int*)destproc->args[0];
            *from_pid = srcproc->pid;
        }

        // Copy message into receiver's buffer
        char *receiver_buf = (char*)destproc->args[1];
        unsigned long receiver_len = (unsigned long)destproc->args[2];
        unsigned long tocopy_len = MIN(receiver_len, len);
        strncpy(receiver_buf, buffer, tocopy_len);

        // Unblock the receiver
        ASSERT_EQUAL(destproc->curr_state, PROC_STATE_BLOCKED);
        destproc->ret = SYSPID_OK;
        add_pcb_to_queue(destproc, PROC_STATE_READY);
        return SYSPID_OK;
    } else {
        // Case 2: We have to block until the receiver is ready
        DEBUG("Waiting for receiver...\n");
        
        add_proc_to_blocking_queue(srcproc, destproc, SENDER);
        return SYSMSG_BLOCKED;
    }
}

/**
 * Receives a message from a specific proc
 * @param srcproc - the process to send from
 * @param destproc - the process to receive the data
 * @param buffer - buffer to store received data
 * @param len - length of buffer
 * @return SYSPID return codes
 */
int recv(proc_ctrl_block_t *srcproc, proc_ctrl_block_t *destproc,
         void *buffer, unsigned long len) {
    ASSERT(srcproc != NULL && destproc != NULL && buffer != NULL && len > 0);
    
    if (remove_proc_from_blocking_queue(srcproc, destproc, SENDER)) {
        // Case 1: Sender has been waiting for us
        DEBUG("Sender has been waiting for us!\n");

        // Copy message into receiver's buffer
        void *sender_buf = (void*)srcproc->args[1];
        unsigned long sender_len = (unsigned long)srcproc->args[2];
        unsigned long tocopy_len = MIN(sender_len, len);
        strncpy(buffer, sender_buf, tocopy_len);
        
        // Unblock the sender
        ASSERT_EQUAL(srcproc->curr_state, PROC_STATE_BLOCKED);
        srcproc->ret = SYSPID_OK;
        add_pcb_to_queue(srcproc, PROC_STATE_READY);
        return SYSPID_OK;
    } else {
        // Case 2: Wait for sender
        DEBUG("Waiting for receiver...\n");
        
        add_proc_to_blocking_queue(destproc, srcproc, RECEIVER);
        return SYSMSG_BLOCKED;
    }
}

/**
 * Receives a message from any proc
 * @param destproc - the process to receive the data
 * @param buffer - buffer to store received data
 * @param len - length of buffer
 * @return SYSPID return codes
 */
int recv_any(proc_ctrl_block_t *destproc, void *buffer, unsigned long len) {
    ASSERT(destproc != NULL && buffer != NULL && len > 0);

    proc_ctrl_block_t *srcproc = destproc->blocking_queue_heads[SENDER];

    if (srcproc != NULL) {
        // Case 1: A sender has been waiting for us
        DEBUG("Sender has been waiting for us!\n");

        int ret = remove_proc_from_blocking_queue(srcproc, destproc, SENDER);
        if (!ret) {
            return SYSERR_OTHER;
        }

        int *from_pid = (int*)destproc->args[0];
        *from_pid = srcproc->pid;

        // Copy message into receiver's buffer
        void *sender_buf = (void*)srcproc->args[1];
        unsigned long sender_len = (unsigned long)srcproc->args[2];
        unsigned long tocopy_len = MIN(sender_len, len);
        strncpy(buffer, sender_buf, tocopy_len);

        // Unblock the sender
        ASSERT_EQUAL(srcproc->curr_state, PROC_STATE_BLOCKED);
        srcproc->ret = SYSPID_OK;
        add_pcb_to_queue(srcproc, PROC_STATE_READY);
        return SYSPID_OK;
    } else {
        // Case 2: Wait for sender
        DEBUG("Waiting for receiver...\n");

        destproc->blocking_queue_name = RECEIVE_ANY;

        return SYSMSG_BLOCKED;
    }
}
