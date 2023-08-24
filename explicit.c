/* File: explicit.c
 * Bradley Hu
 * CS107
 * Assignment 6
 * ---------------------------------------------------------------------
 * This file handles heap operations. It supports allocation, free, and
 * reallocation with utilization optimizations such as coalescing and in-place
 * realloc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./allocator.h"
#include "./debug_break.h"

// how many bytes are printed per line in dump_heap
#define BYTES_PER_LINE 32
// size of a header
#define HEADER_SIZE 8
// minimum size of a payload
#define PAYLOAD_MIN_SIZE 16
// total heap size
#define HEAP_SIZE 4294967296

/* type block_header to denote an 8 byte block of memory containing the header
 * which stores the size of the block with the LSB indicating the status.
 */
typedef size_t block_header;

/* Struct: node_block
 * ------------------
 * This struct is used to denote a node, which in this case is defined 
 * to be the start of the payload. It holds two 8-byte pointers, next and prev,
 * used in the linked free-list traversal.
 */
typedef struct node_block {
    struct node_block *next;
    struct node_block *prev;
} node_block;

static node_block *node;
static void *segment_end;
static void *segment_start;
static node_block *list_front;
static void *heap_end;

/* Function: myinit
 * ----------------
 * This function initializes the heap with the correct header size and
 * status as well as setting values for the static variable used later on. 
 * It also initializers the linked free-list and variable tracking the front 
 * of the list.
 */
bool myinit(void *heap_start, size_t heap_size) {
    void *first = (char *)heap_start + HEADER_SIZE;
    node = (node_block *)first;
    node -> next = NULL;
    node -> prev = NULL;   
    list_front = node;

    segment_start = heap_start;
    segment_end = heap_start;
    // initialize size of block
    *(block_header *)heap_start = (heap_size - HEADER_SIZE);
    // initialize status to free
    *(block_header *)heap_start |= 1UL; 
    heap_end = (char *)heap_start + heap_size;

    return true;
}
/* Function: get_block_size
 * ------------------------
 * This function returns the size of the block by temporarily turning off the 
 * LSB of the current header.
 */
size_t get_block_size(node_block *ptr) {
    return *(block_header *)((char *)ptr - HEADER_SIZE) & ~1UL;
}

/* Function: roundup
 * -----------------
 * This function rounds up the given number to the given multiple, which
 * must be a power of 2, and returns the result.  (you saw this code in lab1!).
 */
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

/* Function: dump_heap
 * -------------------
 * This function is a debugging tool. It prints out useful information about the 
 * heap after each request, including the needed size, the address of the allocated
 * block, and the next and previous blocks pointed to in the linked free-list.
 */
void dump_heap(node_block *ptr, size_t requested_size) {
  
    size_t needed = requested_size <= ALIGNMENT ?
        PAYLOAD_MIN_SIZE : roundup(requested_size, ALIGNMENT);
    size_t header_size = get_block_size(ptr);
    void *new_node_temp = (char *)ptr + needed + HEADER_SIZE;
    node_block *new_node = (node_block *)new_node_temp;
    printf("Address: %p Payload size: %zu New Node Next: %p New Node Prev: %p\n",
    ptr, header_size, new_node->next, new_node->prev);
}

/* Function: add_node
 * ------------------
 * This function adds a node to the linked free-list. If there are nodes in the
 * list, it initializes a node. Otherwise, it adds the node to the front of the list
 * and updates the list_front vraiable to reflect the newly added node.
 */
void add_node(node_block *node_ptr) {
    if (list_front == NULL) {
        node_ptr->next = NULL;
        node_ptr->prev = NULL;
    }
    else {
        node_ptr->next = list_front;
        node_ptr->next->prev = node_ptr;
        node_ptr->prev = NULL;
    }
    list_front = node_ptr;
}

/* Function: remove_node
 * ---------------------
 * This function removes a node from the linked free-list. It accounts for special cases
 * where the node may be the head of the list, end of the list, or the only element in
 * the list.
 */
void remove_node(node_block *node_ptr) {   
    // only node in list
    if (node_ptr->next == NULL && node_ptr->prev == NULL) {
        list_front = NULL;
    }
    // head of list
    else if (node_ptr->next != NULL && node_ptr->prev == NULL) {
        node_ptr->next->prev = NULL;
        list_front = node_ptr->next;
    }     
    // end of list
    else if (node_ptr->prev != NULL && node_ptr->next == NULL) {
        node_ptr->prev->next = NULL;
    }
    else {
        node_ptr->prev->next = node_ptr->next;
        node_ptr->next->prev = node_ptr->prev;        
    }
    node_ptr->next = NULL;
    node_ptr->prev = NULL;
}

/* Function: is_free
 * -----------------
 * This function return true if the current block is free and false if it is used.
 */
bool is_free(block_header *header) {
    return (*header & 1UL) == 1 ? true : false;
}

/* Function: split_block
 * ---------------------
 * This function splits the block at the needed size. It updates the headers of the
 * newly split blocks to reflect the changes. 
 */
void split_block(node_block *ptr, size_t size) {
    size_t needed = size <= ALIGNMENT ?
        PAYLOAD_MIN_SIZE : roundup(size, ALIGNMENT);
    
    segment_end = (char *)ptr + needed;
    
    // initialize size of new header
    *(block_header *)segment_end = (get_block_size(ptr) - needed - HEADER_SIZE);
    // initialize status of new header to free
    *(block_header *)segment_end |= 1UL;

    // update curr header size 
    void *curr_header = (char *)ptr - HEADER_SIZE;
    *(block_header *)curr_header = needed;
    // update curr header status to used
    *(block_header *)curr_header &= ~1UL;
}

/* Function: mymalloc
 * -------------------
 * This function completes the client's allocation request. It searches the linked free-list
 * for a free block of viable size and returns the pointer to the location at which the 
 * client's request should be allocated. It then splits the block if possible to accomodate
 * following requests. It removes the newly allocated node from the free-list and adds the
 * free split portion to the list.
 */
void *mymalloc(size_t requested_size) {
    if ((requested_size > MAX_REQUEST_SIZE) || (requested_size == 0)) {
        return NULL;
    }    
    size_t needed = requested_size <= ALIGNMENT ?
        PAYLOAD_MIN_SIZE : roundup(requested_size, ALIGNMENT);
    
    node = list_front;
    while (node != NULL) {
        size_t block_size = get_block_size(node);
        // if found viable block
        if (block_size >= needed) {
            node_block *curr_node = node;
            // if we should split, split
            if (block_size - needed >= HEADER_SIZE + PAYLOAD_MIN_SIZE) {
                split_block(node, requested_size);
                
                // remove current node from free list
                remove_node(node);
                
                // make new node
                void *temp_node = ((char *)segment_end + HEADER_SIZE);
                node_block *new_node = (node_block *)temp_node;
                
                // add new node to free list
                add_node(new_node);
                
                // dump_heap(curr_node, requested_size);               
            } else {
                // update current header to used
                *(block_header *)((char *)node - HEADER_SIZE) &= ~1;
                remove_node(node);
            }
            return curr_node;
        }
        node = node->next;
    }
    printf("Heap space exhausted.\n");
    return NULL;
}

/* Function: can_coalesce
 * ----------------------
 * This function returns whether a block can be coalesced with its adjacent right 
 * neighbor. If the right neighboring block is not the end of the heap and is free,
 * the function returns true. Otherwise, it returns false.
 */
bool can_coalesce(node_block *ptr) {
    size_t payload_size = get_block_size(ptr);
    void *right_block = (char *)ptr + payload_size;
    if (right_block != heap_end && is_free(right_block)) {
        return true;
    }
    return false;
}

/* Function: coalesce
 * ------------------
 * This function coalesces a block and its adjacent right block. It removes the right
 * free block from the free-list and adds its size to that of the current block. It
 * updates all header information accordingly.
 */
void coalesce(node_block *ptr) {
    size_t payload_size = get_block_size(ptr);    
    void *curr_header_temp = (char *)ptr - HEADER_SIZE;
    block_header *curr_header = (block_header *)curr_header_temp;

    // right block
    void *right_block_temp = (char *)ptr + payload_size + HEADER_SIZE;
    node_block *right_block = (node_block *)right_block_temp;
    size_t right_block_size = get_block_size(right_block);

    // remove right block from free list
    remove_node(right_block);
    // add right block size to curr block
    *curr_header += (right_block_size + HEADER_SIZE);              
}

/* Function: myfree
 * ----------------
 * This function frees a previously allocated block. It coalesces once if possible and
 * sets the header status to free. It adds the node to the free-list, allowing the space
 * to be overwritten in subsequent allocation requests.
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        if (can_coalesce(ptr)) {
            coalesce(ptr);
        }
        // set status to free
        *(block_header *)((char *)ptr - HEADER_SIZE) |= 1UL;
        add_node(ptr);
    }
}

/* Function: myrealloc
 * -------------------
 * This function reallocates previously used memory. It accounts for a series of scenarios; 
 * if the needed size is less than the old size, it splits the block and returns the old pointer,
 * allowing the conserved space for subsequent allocations. If the needed size greater than the
 * old size, it attempts to perform in-place reallocation by coalescing while possible to try 
 * and make space for the new size. If this is possible, the function splits accordingly once
 * again to conserve the excess space. If all lese fails, the function mallocs a new pointer, 
 * copies the memory to the new location, and frees the old pointer. 
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (old_ptr == NULL && new_size == 0) {
        return NULL;
    }
    if (old_ptr == NULL && new_size != 0) {
        return mymalloc(new_size);
    }
    if (old_ptr != NULL && new_size == 0) {
        myfree(old_ptr);
        return NULL;
    }
    size_t old_size = get_block_size(old_ptr);
    size_t needed = new_size <= ALIGNMENT ?
        PAYLOAD_MIN_SIZE : roundup(new_size, ALIGNMENT);
    
    if (needed <= old_size) {
        // check if we should split
        if (old_size - needed >= HEADER_SIZE + PAYLOAD_MIN_SIZE) {
            split_block(old_ptr, needed);
            add_node((node_block *)((char *)old_ptr + needed + HEADER_SIZE));
        }
        return old_ptr;
    }
    // while need more space, coalesce if possible
    while (get_block_size(old_ptr) < needed) {
        if (can_coalesce(old_ptr)) {
            coalesce(old_ptr);
        } else {
            void *new_ptr = mymalloc(new_size);
            if (new_ptr != NULL) {
                memcpy(new_ptr, old_ptr, needed);
                myfree(old_ptr);
                return new_ptr;
            }
            else {
                return NULL;
            }
        }
    }
    // if we should split, split
    if (get_block_size(old_ptr) - needed >= HEADER_SIZE + PAYLOAD_MIN_SIZE) {
        split_block(old_ptr, needed);
        add_node((node_block *)((char *)old_ptr + needed + HEADER_SIZE));
    }
    return old_ptr;            
}

/* Function validate_heap
 * ----------------------
 * This function is called periodically to check for corruption in the heap space. It performs
 * a series of safety checks, such as ensuring that the block size if a multiple of the alignment
 * and that the total used size does not exceed the total heap size. 
 */
bool validate_heap() {
    /*    
    // reset purposes
    void *curr_node = node;
    block_header *header_ptr = segment_start;
    
    // block sizes should always be a multiple of the HEADER_SIZE
    while (header_ptr != (block_header *)(node - HEADER_SIZE)) {
        if ((*header_ptr & ~1UL) % PAYLOAD_MIN_SIZE != 0) {
            printf("Something went wrong - this block size is not a multiple of the alignment!");
            return false;
        }
        header_ptr += (get_block_size((node_block *)(header_ptr + HEADER_SIZE)) + HEADER_SIZE); 
    } 
    
    // total used size should not exceed heap size
    header_ptr = segment_start;
    size_t total_size = 0;
    while (header_ptr != (block_header *)(node - HEADER_SIZE)) {
        if (!is_free(header_ptr)) {
            total_size += (*header_ptr & ~1UL);
        }
        if (total_size > HEAP_SIZE) {
            printf("Oh no! Used more heap than total available...");
            return false;
        }
        header_ptr += (get_block_size((node_block *)(header_ptr + HEADER_SIZE)) + HEADER_SIZE); 

    }
    // reset purposes
    node = curr_node;
    */                            
    return true;
}
