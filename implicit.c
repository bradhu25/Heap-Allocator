/* File: implicit.c
 * Bradley Hu
 * CS107
 * Assignment 6
 * ---------------------------------------------------------------------
 * This file handles heap operations. It supports allocating, freeing, and
 * reallocating memory.
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
// total heap size
#define HEAP_SIZE 4294967296

/* type block_header to denote an 8 byte block of memory containing the header
 * which stores the size of the block with the LSB indicating the status.
 */
typedef size_t block_header; 

static block_header *curr;
static void *segment_start;
static void *segment_end;
static void *heap_end;

/* Function: myinit
 * ----------------
 * This function initializes the heap with the correct header size and
 * status as well as setting values for the static variable used later on.
 */
bool myinit(void *heap_start, size_t heap_size) {
    segment_start = heap_start;
    segment_end = heap_start;
    curr = heap_start;
    // initialize size of block
    *curr = (heap_size - HEADER_SIZE);
    // initialize status to free 
    *curr |= 1UL; 
    heap_end = (char *)heap_start + heap_size;
    
    return true;
}

/* Function: get_block_size
 * ------------------------
 * This function returns the size of the block by temporarily turning off the 
 * LSB of the current header.
 */
size_t get_block_size(void *ptr) {
    size_t block_size = *(block_header *)ptr & ~1UL;
    return block_size;
}

/* Function: next_header
 * ---------------------
 * This function jumps to the next header.
 */
void next_header() {
    void *curr_temp = (char *)curr + get_block_size(curr) + HEADER_SIZE;
    curr = curr_temp;
}

/* Function: is_free
 * -----------------
 * This function return true if the current block is free and false if it is used.
 */
bool is_free() {
    return (*curr & 1UL) == 1 ? true : false;
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
 * block, the remaining block size, and the status of the block (used or free).
 */
void dump_heap(size_t requested_size) {
    size_t needed = roundup(requested_size, ALIGNMENT);
    size_t header_size = get_block_size(curr);
    size_t status = (*curr & 1UL);
    printf("Needed: %zu Address: %p Remaining block size: %zu Status: %zu \n", needed,
           curr, header_size, status);
}

/* Function: mymalloc
 * -------------------
 * This function completes the client's allocation request. It searches the headers
 * for a free block of viable size and returns the pointer to the location at which the 
 * client's request should be allocated. It then splits the block if possible to accomodate
 * following requests, initializing header information as well as updating header information
 * for the newly allocated block.
 */
void *mymalloc(size_t requested_size) {
    if ((requested_size > MAX_REQUEST_SIZE) || (requested_size == 0)) {
        return NULL;
    }
    size_t needed = roundup(requested_size, ALIGNMENT);
    curr = segment_start;
    while (curr != heap_end) {
        // check if status is free and block size >= needed size
        if (is_free() && get_block_size(curr) >= needed) {
            size_t remaining = get_block_size(curr);
            void *payload_ptr = (char *)curr + HEADER_SIZE;
            // move pointer to location after payload
            segment_end = (char *)curr + HEADER_SIZE + needed;
            if (remaining - needed >= HEADER_SIZE) {
                // initialize size of new block
                *(block_header *)segment_end = (remaining - needed - HEADER_SIZE);
                // initialize status to free
                *(block_header *)segment_end |= 1UL;
            }
            // update size of current block
            *curr = needed;
            // update status of current block to used
            *curr &= ~1;
            curr = segment_end;
            
            return payload_ptr;
        }
        next_header();
    }
    printf("Heap space exhausted.\n");
    return NULL;
}

/* Function: myfree
 * ----------------
 * This function frees a previously allocated block by updating the header status to
 * free, allowing the space to be overwritten by subsequent allocation requests.
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        ptr = (char *)ptr - HEADER_SIZE;
        *(block_header *)ptr |= 1UL; // update status to free
    }
}

/* Function: myrealloc
 * -------------------
 * This function reallocates previously used memory. It frees the previously allocated
 * memory and returns the new location of the allocation, accounting for the new size.
 * If the old pointer doesn't exist, the function returns the malloc of the new request.
 * If the new requested size is 0, the function frees the memory at the location of the 
 * old pointer and returns NULL. Otherwise, it copies memory to a new location, frees the
 * old pointer, and returns the location of the new pointer.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    void *new_ptr = mymalloc(new_size);
    if (old_ptr == NULL) {
        return new_ptr;
    }
    if (new_size == 0) {
        myfree(old_ptr);
        return NULL;
    }
    if (new_ptr != NULL) {
        memcpy(new_ptr, old_ptr, new_size);
        myfree(old_ptr);
        return new_ptr;
    }
    return NULL;
}

/* Function validate_heap
 * ----------------------
 * This function is called periodically to check for corruption in the heap space. It performs
 * a series of safety checks, such as ensuring that the block size if a multiple of the alignment
 * and that the total used size does not exceed the total heap size. 
 */
bool validate_heap() {
    // reset purposes
    void *curr_temp = curr;
    
    // block sizes should always be a multiple of the HEADER_SIZE
    curr = segment_start;
    while (curr != heap_end) {
        if (get_block_size(curr) % HEADER_SIZE != 0) {
            printf("Something went wrong - this block size is not a multiple of the alignment!");
            return false;
        }
        next_header();
    } 
    // total used size should not exceed heap size
    curr = segment_start;
    size_t total_size = 0;
    while (curr != heap_end) {
        if (!is_free(curr)) {
            total_size += get_block_size(curr);
        }
        if (total_size > HEAP_SIZE) {
            printf("Oh no! Used more heap than total available...");
            return false;
        }
        next_header();
    }
    // reset purposes
    curr = curr_temp;                            
    return true;
}
