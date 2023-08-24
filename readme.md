# Explicit free list allocator features:

- Block headers and recycling freed nodes
- An explicit free list managed as a doubly-linked list, using the first 16 bytes of each free block's payload for next/prev pointers
- ***Malloc*** searches the explicit list of free blocks
- Freed blocks are coalesced with their neighboring block to the right if it is also free. Block coalesce operates in O(1) time
- ***Realloc*** resizes a block in-place whenever possible, e.g. if the client is resizing to a smaller size, or neighboring block(s) to its right are free and can be absorbed. When an in-place realloc is not possible, adjacent free blocks are absorbed as much as possible until realloc in place is possible, or can no longer absorb and must reallocate elsewhere.
