/* mem.c : memory manager

Called from outside:
  kmeminit() - initializes memory mangament system

  kmalloc() - allocates memory, returns a pointer to its start
  kfree() - frees allocated memory for future use

  kmem_maxaddr() - return the max memory address
  kmem_freemem() - return the start of free memory / top of the kernel's stack

  kmem_dump_free_list() - prints free list, testing purposes only
  kmem_get_free_list_length() - returns length of free list, testing only

Further details can be found in the documentation above the function headers.
*/

#include <xeroskernel.h>
#include <i386.h>

extern long	freemem; 	/* start of free memory (set in i386.c) */
extern char	*maxaddr;	/* max memory address (set in i386.c)	*/

typedef struct memory_header {
    /* The size of the memory region + this memory_header */
    size_t size;
    
    struct memory_header *prev;
    struct memory_header *next;
    
    /* sanity_check and data_start should be equal */
    struct memory_header *sanity_check;
    unsigned char data_start[];
} memory_header_t;

static memory_header_t *g_free_list;

static void split_free_block(memory_header_t *block, size_t size);
static int coalesce_blocks(memory_header_t *block1, memory_header_t *block2);
static void kmem_dump_block(memory_header_t *ptr);
static size_t round_to_paragraph(size_t val);

/**
 * Initalizes free list
 */
void kmeminit(void) {
    // Assumed throughout this file
    ASSERT_EQUAL(sizeof(memory_header_t), 16);

    DEBUG("Initializing memory manager...\n");
    DEBUG("Freemem:    0x%x\n", freemem);
    DEBUG("Hole start: 0x%x\n", HOLESTART);
    DEBUG("Hole end:   0x%x\n", HOLEEND);
    DEBUG("Max addr:   0x%x\n", maxaddr);
  
    /* Create the free list, consisting of two regions surrounding the HOLE */
    memory_header_t *post_hole_region = (memory_header_t*) HOLEEND;
    post_hole_region->size = (size_t)(maxaddr - HOLEEND);
    post_hole_region->sanity_check = NULL;
  
    size_t memstart = round_to_paragraph(freemem);

    g_free_list = (memory_header_t *)memstart;
    g_free_list->size = (size_t)(HOLESTART - memstart);
    g_free_list->sanity_check = NULL;
  
    g_free_list->prev = NULL;
    g_free_list->next = post_hole_region;
    post_hole_region->prev = g_free_list;
    post_hole_region->next = NULL;

    ASSERT_EQUAL(kmem_get_free_list_length(), 2);
}

/**
 * Returns the max memory address
 * @return maxaddr
 */
long kmem_maxaddr(void) {
    return (long)maxaddr;
}

/**
 * Returns the start of free memory
 * @return freemem
 */
long kmem_freemem(void) {
    return freemem;
}

/**
 * Rounds the value given up to the nearest 16 bit boundary.
 * @param val - the number to round up
 * @return val, rounded up
 */
static size_t round_to_paragraph(size_t val) {
    if (val & 0xf) {
        val = (val + 0x10) & ~(0xf);
    }
    return val;
}

/**
 * Allocates a contiguous block of memory for the kernel to use,
 * which can later be freed by kfree. Returns NULL if no allocation is done.
 * @param size - number of bytes to allocate
 * @return pointer to start of allocated block
 */
void* kmalloc(size_t size) {
    if (size <= 0 || size >= kmem_maxaddr()) {
        return NULL;
    }

    size_t size_required = round_to_paragraph(size) + sizeof(memory_header_t);

    /* Using first fit. Donald said in class best fit wasn't necessary */
    memory_header_t *curr = g_free_list;
    while (curr != NULL) {
        if (size_required <= curr->size) {
            split_free_block(curr, size_required);

            if (curr->prev) {
                curr->prev->next = curr->next;
            } else {
                g_free_list = curr->next;
            }

            if (curr->next) {
                curr->next->prev = curr->prev;
            }

            curr->size = size_required;
            curr->sanity_check = (memory_header_t *)curr->data_start;

            // only allocate on 16 byte boundaries, last 4 bits must always be 0
            ASSERT_EQUAL(((size_t)curr->data_start & 0xf), 0);
            return curr->data_start;
        }

        curr = curr->next;
    }

    DEBUG("kmalloc could not allocate sufficient memory\n");
    return NULL;
}

/**
 * Frees block of memory previous allocated with kmalloc, for future use.
 * @param ptr - start of block returned by kmalloc, to be freed
 */
void kfree(void *ptr) {
    /* Make sure we were given a valid ptr */
    if (ptr == NULL) {
        DEBUG("Error: Invalid address 0x%x\n", ptr);
        return;
    }

    // We only allocate on 16 byte boundaries
    ASSERT_EQUAL(((size_t)ptr & 0xf), 0);

    memory_header_t *to_free = (memory_header_t*)(ptr - sizeof(memory_header_t));
    ASSERT_EQUAL(to_free->sanity_check, ptr);
    
    /* Search the free list for a place to put our new memory segment */
    memory_header_t *prev = NULL;
    memory_header_t *curr = g_free_list;
    while (curr != NULL && curr < to_free) {
        prev = curr;
        curr = curr->next;
    }
    
    /* Insert into free list */
    if (prev == NULL) {
        g_free_list = to_free;
    } else {
        prev->next = to_free;
    }
    
    to_free->prev = prev;
    to_free->next = curr;
    
    if (curr != NULL) {
        curr->prev = to_free;
    }
    
    // Coalesce if needed. Notice the order here matters to simplify the logic
    coalesce_blocks(to_free, to_free->next);
    coalesce_blocks(to_free->prev, to_free);
}

/**
 * Splits a free block into two free blocks,
 * with the first block having the size provided.
 * @param block - the block to split_free_block
 * @param size - the new size for block
 */
static void split_free_block(memory_header_t *block, size_t size) {
    ASSERT(block != NULL);
    ASSERT(block->size >= size);

    // perfect fit, no split necessary
    if (block->size == size) {
        return;
    }

    memory_header_t *other_half = (memory_header_t*)((size_t)block + size);
    other_half->size = block->size - size;
    other_half->prev = block;
    other_half->next = block->next;
    other_half->sanity_check = NULL;
    if (other_half->next != NULL) {
        other_half->next->prev = other_half;
    }

    block->next = other_half;
    block->size = size;
}

/**
 * If block1 and block2 are adjacent blocks, they are combined into block1.
 * Otherwise they are both unchanged.
 *
 * block1 is assumed to be a lower address value than block2
 * @param[in] block1 - lower address block to try and coalesce
 * @param[in] block2 - higher address block to try and coalesce
 * @return - 1 if blocks were coalesced, 0 otherwise
 */
static int coalesce_blocks(memory_header_t *block1, memory_header_t *block2) {
    if (block1 == NULL || block2 == NULL) {
        return 0;
    }
    
    /* If there is no space between the blocks, combine them */
    if ((size_t)block1 + block1->size == (size_t)block2) {
        block1->next = block2->next;
        if (block1->next != NULL) {
            block1->next->prev = block1;
        }
        
        block1->size += block2->size;
        return 1;
    }
    
    return 0;
}

/**
 * Dumps the entire free list into stdout
 */
void kmem_dump_free_list(void) {
    memory_header_t *ptr = g_free_list;
    int count = 0;

    while (ptr != NULL) {
        kmem_dump_block(ptr);
        count++;
        ptr = ptr->next;
    }

    DEBUG("%d blocks in free list\n", count);
}

/**
 * Returns number of blocks in the free list.
 * Purposely inefficient, should only be used for testing.
 * @return - number of blocks in our free list
 */
int kmem_get_free_list_length(void) {
    // While we could keep track of length through a static, global variable,
    // this simpler approach is taken to avoid being incorrect while debugging

    memory_header_t *ptr = g_free_list;
    int count = 0;

    while (ptr != NULL) {
        count++;
        ptr = ptr->next;
    }

    return count;
}

/**
 * Helper method for dumping a memory_header_t node into stdout
 * @param ptr - memory_header to print
 */
static void kmem_dump_block(memory_header_t *ptr) {
    DEBUG("================\n");
    DEBUG("addr: 0x%x       | size: 0x%x\n", ptr, ptr->size);
    DEBUG("data_start: 0x%x | sanity: 0x%x\n",
          ptr->data_start, ptr->sanity_check);
    DEBUG("prev: 0x%x       | next: 0x%x\n", ptr->prev, ptr->next);
}
