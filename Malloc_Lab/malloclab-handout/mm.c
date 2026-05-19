/*
 * mm.c - A malloc package based on segregated explicit free lists.
 *
 * Each block has a header containing the block size, allocation bit, and
 * previous-block allocation bit. Allocated blocks do not need a footer; free
 * blocks keep a footer so the next block can coalesce backward. Free blocks
 * reuse the first two payload words as prev/next pointers in a circular
 * doubly-linked free list. The allocator keeps several size classes and
 * searches from the appropriate class upward.
 *
 * Small and medium payload requests are rounded into coarse payload classes
 * to improve later reuse. Free blocks are coalesced immediately. Realloc
 * first tries to resize in place by using adjacent free blocks, and falls
 * back to malloc-copy-free only when in-place growth is not possible.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "A NOBODY",
    /* First member's full name */
    "Xiang Li",
    /* First member's email address */
    "leexsean.code@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* Basic constants and macros */
#define WSIZE               4           /* Word and header/footer size (bytes) */
#define DSIZE               8           /* Double word size (bytes) */
#define CHUNKSIZE           (1<<6)     /* Extend Heap by this amount (bytes) */
#define CLASSNUM            12

#define MAX(x, y)           ((x) > (y)? (x) : (y))

/* Pack a size and flag bits into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)              (*(unsigned int *)(p))
#define PUT(p, val)         (*(unsigned int *)(p) = (val))

/* Read and write free-list pointers stored inside free block payloads */
#define GET_PTR(p)          ((char *)(uintptr_t)GET(p))
#define PUT_PTR(p, val)     (PUT((p), (unsigned int)(uintptr_t)(val)))

/* Read the size and allocation fields from address p */
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)
#define GET_PREV_ALLOC(p)   (GET(p) & 0x2)

/* Free block payload layout: previous pointer followed by next pointer */
#define PREV(bp)            ((char *)(bp))
#define NEXT(bp)            ((char *)(bp) + WSIZE)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)            ((char *)(bp) - WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define NEXT_FBLKP(bp)      (GET_PTR(NEXT(bp)))
#define PREV_FBLKP(bp)      (GET_PTR(PREV(bp)))

#define SEG_HEAD(index)             (GET_PTR(seg_free_lists + ((index) * WSIZE)))
#define SET_SEG_HEAD(index, val)    (PUT_PTR(seg_free_lists + ((index) * WSIZE), (val)))

/* Static global variables */
static char *heap_listp;
static char *seg_free_lists;

/*
 * round_payload - Round small and medium payload requests into reusable
 * payload classes.
 *
 * This rounds the user payload only. The real block size is still computed
 * later by adding header overhead and aligning to DSIZE.
 */
static size_t round_payload(size_t size)
{
    size_t rounded = 16;

    if (size <= 16)
        return size;

    while (rounded < size && rounded < 512)
        rounded <<= 1;

    if (rounded >= size && rounded <= 512)
        return rounded;

    return size;
}

/*
 * find_index - Return the segregated-list index for an adjusted block size.
 *
 * The index is based on the payload-like part of the block size
 * (asize - WSIZE), so blocks produced by round_payload land in classes that
 * match the request sizes they are meant to serve.
 */
static size_t find_index(size_t asize)
{
    size_t index;
    size_t count = 0;

    if (asize <= 2*DSIZE) 
        return 0;

    index = (asize-WSIZE-1) >> 4;
    while (index && count < (CLASSNUM-1)) {
        index >>= 1;
        count += 1;
    }
    index = count;

    return index;
}

/*
 * insert_node - Insert a free block at the front of its size-class list.
 */
static void insert_node(void *bp)
{
    size_t index = find_index(GET_SIZE(HDRP(bp)));
    if (SEG_HEAD(index) == NULL) {
        SET_SEG_HEAD(index, bp);
        PUT_PTR(PREV(bp), bp);
        PUT_PTR(NEXT(bp), bp);
        return;
    }

    PUT_PTR(NEXT(bp), SEG_HEAD(index));
    PUT_PTR(PREV(bp), PREV_FBLKP(SEG_HEAD(index)));
    PUT_PTR(NEXT(PREV_FBLKP(SEG_HEAD(index))), bp);
    PUT_PTR(PREV(SEG_HEAD(index)), bp);

    SET_SEG_HEAD(index, bp);
}

/*
 * remove_node - Remove a free block from its size-class list.
 */
static void remove_node(void *bp)
{
    size_t index = find_index(GET_SIZE(HDRP(bp)));
    
    char *cur_prev = PREV_FBLKP(bp);
    char *cur_next = NEXT_FBLKP(bp);

    PUT_PTR(NEXT(cur_prev), cur_next);
    PUT_PTR(PREV(cur_next), cur_prev);

    if (SEG_HEAD(index) == bp) {
        SET_SEG_HEAD(index, cur_next == bp ? NULL : cur_next);
    }
}

/*
 * place - Mark a free block as allocated and split it if the remainder
 * can hold a minimum-sized free block.
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        remove_node(bp);
        PUT(HDRP(bp), PACK(asize, GET_PREV_ALLOC(HDRP(bp)) | 0x1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 0x2));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 0x2));
        insert_node(NEXT_BLKP(bp));
    }
    else {
        remove_node(bp);
        PUT(HDRP(bp), PACK(csize, GET_PREV_ALLOC(HDRP(bp)) | 0x1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) | 0x2));
    }
}

/*
 * coalesce - Merge a newly freed block with adjacent free blocks, then
 * insert the resulting block into the appropriate free list.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        insert_node(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) {
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0x2));
        PUT(FTRP(bp), PACK(size, 0x2));
    }

    else if (!prev_alloc && next_alloc) {
        remove_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(PREV_BLKP(bp)))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, GET_PREV_ALLOC(HDRP(PREV_BLKP(bp)))));
        bp = PREV_BLKP(bp);
    }

    else {
        remove_node(PREV_BLKP(bp));
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, GET_PREV_ALLOC(HDRP(PREV_BLKP(bp)))));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, GET_PREV_ALLOC(HDRP(PREV_BLKP(bp)))));
        bp = PREV_BLKP(bp);
    }
    
    insert_node(bp);
    return bp;
}

/*
 * extend_heap - Request more heap space, build a new free block, and
 * coalesce it with the previous heap block if possible.
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2)? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0x1));

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * find_fit - Search from the matching size class upward and choose the
 * smallest usable block in the first class that contains a fit.
 */
static void *find_fit(size_t asize)
{
    size_t minSize;
    size_t index = find_index(asize);
    char *ptr = NULL;

    while (index < CLASSNUM && ptr == NULL) {
        char *bp = SEG_HEAD(index);
        if (bp == NULL) {
            index++;
            continue;
        }
        do {
            if (asize <= GET_SIZE(HDRP(bp)) && ptr == NULL) {
                ptr = bp;
                minSize = GET_SIZE(HDRP(ptr));
            }
            else if (asize <= GET_SIZE(HDRP(bp)) && ptr != NULL) {
                if (GET_SIZE(HDRP(bp)) < minSize) {
                    ptr = bp;
                    minSize = GET_SIZE(HDRP(ptr));
                }
            }
            bp = NEXT_FBLKP(bp);
        } while (bp != SEG_HEAD(index));
        index++;
    }
    if (ptr != NULL)
        return ptr;

    /* No fit */
    return NULL;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *bp;
    int i;

    if ((seg_free_lists = mem_sbrk(CLASSNUM * WSIZE)) == (void *)-1)
        return -1;
    for (i = 0; i < CLASSNUM; i++) {
        SET_SEG_HEAD(i, NULL);
    }

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 0x3));      /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate an aligned block with at least size bytes of payload.
 * Search the segregated free lists first, extending the heap if needed.
 */
void *mm_malloc(size_t size)
{
    size_t asize;           /* Adjusted block size */
    size_t extendsize;      /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Round the payload class, then add header overhead and alignment. */
    size = round_payload(size);
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + WSIZE + (DSIZE - 1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Mark a block free and immediately coalesce it.
 */
void mm_free(void *bp)
{
    size_t size;

    if (bp == NULL)
        return;

    size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(FTRP(bp), PACK(size, GET_PREV_ALLOC(HDRP(bp))));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), GET_ALLOC(HDRP(NEXT_BLKP(bp)))));

    coalesce(bp);
}

/*
 * mm_realloc - Resize a block while preserving its old payload contents.
 *
 * Realloc grows requests by a small margin before rounding, which reduces
 * repeated realloc-copy-free cycles. It then tries, in order: shrink in
 * place, extend at the heap end, merge with the next free block, extend past
 * a next free block that reaches the heap end, merge with the previous free
 * block, merge with both neighbors, and finally allocate a replacement block.
 */
void *mm_realloc(void *bp, size_t size)
{
    char *ptr;
    size_t asize;
    size_t bsize;
    size_t copySize;
    size_t prevSize;
    size_t nextSize;

    if (bp == NULL && size == 0)
        return bp;

    if (bp == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    bsize = GET_SIZE(HDRP(bp));
    /* Leave modest slack for future growth before payload-class rounding. */
    size += size >> 4;
    size = round_payload(size);
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + WSIZE + (DSIZE - 1)) / DSIZE);

    if (asize <= bsize) {
        if (bsize - asize >= (2*DSIZE)) {
            PUT(HDRP(bp), PACK(asize, GET_PREV_ALLOC(HDRP(bp)) | 0x1));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(bsize-asize, 0x2));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(bsize-asize, 0x2));
            PUT(HDRP(NEXT_BLKP(NEXT_BLKP(bp))), PACK(GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(bp)))), GET_ALLOC(HDRP(NEXT_BLKP(NEXT_BLKP(bp))))));
            if (!GET_ALLOC(HDRP(NEXT_BLKP(NEXT_BLKP(bp))))) {
                PUT(FTRP(NEXT_BLKP(NEXT_BLKP(bp))), PACK(GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(bp)))), GET_ALLOC(HDRP(NEXT_BLKP(NEXT_BLKP(bp))))));
            }
            coalesce(NEXT_BLKP(bp));
            return bp;
        }
        else {
            return bp;
        }
    }
    
    else {
        copySize = GET_SIZE(HDRP(bp)) - WSIZE;

        /* If this is the last real block, grow it directly with mem_sbrk. */
        if (GET_SIZE(HDRP(NEXT_BLKP(bp))) == 0) {
            if (mem_sbrk(asize-bsize) == (void *)-1)
                return NULL;
            PUT(HDRP(bp), PACK(asize, GET_PREV_ALLOC(HDRP(bp)) | 0x1));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0x3));
            return bp;
        }

        /* Prefer expanding into the next block because the payload stays put. */
        if (!GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            ptr = NEXT_BLKP(bp);
            nextSize = GET_SIZE(HDRP(ptr));

            /* If the next free block reaches the heap end, absorb it and
             * extend only the remaining shortage.
             */
            if (GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0 && bsize + nextSize < asize) {
                if (mem_sbrk(asize - bsize - nextSize) == (void *)-1)
                    return NULL;
                remove_node(ptr);
                PUT(HDRP(bp), PACK(asize, GET_PREV_ALLOC(HDRP(bp)) | 0x1));
                PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0x3));
                return bp;
            }

            if (nextSize >= (asize-bsize) + (4*DSIZE)) {
                remove_node(NEXT_BLKP(bp));
                PUT(HDRP(bp), PACK(asize, GET_PREV_ALLOC(HDRP(bp)) | 0x1));
                PUT(HDRP(NEXT_BLKP(bp)), PACK(nextSize-(asize-bsize), 0x2));
                PUT(FTRP(NEXT_BLKP(bp)), PACK(nextSize-(asize-bsize), 0x2));
                insert_node(NEXT_BLKP(bp));
                return bp;
            }
            else if (nextSize >= (asize-bsize)) {
                remove_node(NEXT_BLKP(bp));
                PUT(HDRP(bp), PACK(bsize+nextSize, GET_PREV_ALLOC(HDRP(bp)) | 0x1));
                PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), GET_ALLOC(HDRP(NEXT_BLKP(bp))) | 0x2));
                if (!GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
                    PUT(FTRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), GET_ALLOC(HDRP(NEXT_BLKP(bp))) | 0x2));
                }
                return bp;
            }
        }
        
        /* Expanding backward requires moving the payload into the previous block. */
        if (!GET_PREV_ALLOC(HDRP(bp))) {
            ptr = PREV_BLKP(bp);
            prevSize = GET_SIZE(HDRP(ptr));
            if (prevSize >= (asize-bsize) + (4*DSIZE)) {
                remove_node(ptr);
                memmove(ptr, bp, copySize);
                PUT(HDRP(ptr), PACK(asize, GET_PREV_ALLOC(HDRP(ptr)) | 0x1));
                bp = NEXT_BLKP(ptr);
                PUT(HDRP(bp), PACK(prevSize-(asize-bsize), 0x2));
                PUT(FTRP(bp), PACK(prevSize-(asize-bsize), 0x2));
                PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), GET_ALLOC(HDRP(NEXT_BLKP(bp)))));
                if (!GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
                    PUT(FTRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), GET_ALLOC(HDRP(NEXT_BLKP(bp)))));
                }
                coalesce(bp);
                return ptr;
            }
            else if (prevSize >= (asize-bsize)) {
                remove_node(ptr);
                memmove(ptr, bp, copySize);
                PUT(HDRP(ptr), PACK(bsize+prevSize, GET_PREV_ALLOC(HDRP(ptr)) | 0x1));
                bp = NEXT_BLKP(ptr);
                PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) | 0x2));
                if (!GET_ALLOC(HDRP(bp))) {
                    PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) | 0x2));
                }
                return ptr;
            }
        }

        /* If either neighbor alone is not enough, try using both sides. */
        if (!GET_PREV_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            nextSize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
            ptr = PREV_BLKP(bp);
            prevSize = GET_SIZE(HDRP(ptr));
            if (prevSize + nextSize >= (asize-bsize) + (4*DSIZE)) {
                remove_node(NEXT_BLKP(bp));
                remove_node(ptr);
                memmove(ptr, bp, copySize);
                PUT(HDRP(ptr), PACK(asize, GET_PREV_ALLOC(HDRP(ptr)) | 0x1));
                bp = NEXT_BLKP(ptr);
                PUT(HDRP(bp), PACK(prevSize+nextSize-(asize-bsize), 0x2));
                PUT(FTRP(bp), PACK(prevSize+nextSize-(asize-bsize), 0x2));
                insert_node(bp);
                return ptr;
            }
            else if (prevSize + nextSize >= (asize-bsize)) {
                remove_node(NEXT_BLKP(bp));
                remove_node(ptr);
                memmove(ptr, bp, copySize);
                PUT(HDRP(ptr), PACK(bsize+prevSize+nextSize, GET_PREV_ALLOC(HDRP(ptr)) | 0x1));
                bp = NEXT_BLKP(ptr);
                PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) | 0x2));
                if (!GET_ALLOC(HDRP(bp))) {
                    PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) | 0x2));
                }
                return ptr;
            }
        }
    
        ptr = mm_malloc(size);
        if (ptr == NULL)
            return NULL;
        if (size < copySize)
            copySize = size;
        memcpy(ptr, bp, copySize);
        mm_free(bp);
        return ptr;
    }
}
