/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* extend heap by this amount (byte) */
#define CHUNKSIZE   (1 << 12)

/* word and header/footer size (byte) */
#define WSIZE   4

/* double word size (byte) */
#define DSIZE   8

#define MAX(x, y)   ((x) >= (y) ? (x) : (y))
#define MIN(x, y)   ((x) <= (y) ? (x) : (y))

/* define flags */
#define ALLOC       1
#define PREV_ALLOC  2

/* pack a size and flag int into a word */
#define PACK(size, flag)    ((size) | (flag))

/* read and write a word at address p */
#define GET(p)      *((unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *) (p) = (val))

/* read the size, allocated and previous allocated field from address p */
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)     
#define GET_PREV_ALLOC(p)   (GET(p) & 0x2)

/* given block ptr bp, compute address of header and footer */
#define HDRP(bp)    ((char *) (bp) - WSIZE)
#define FTRP(bp)    ((char *) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* give block ptr bp, compute address of next and previous blocks and previous footer */
#define NEXT_BLKP(bp)   ((char *) (bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)   ((char *) (bp) - GET_SIZE((char *) (bp) - DSIZE))

/* point to the base address of the heap */
static char *heap_listp;

static void *extend_heap(size_t words);     /* the function prototype */
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void mm_check(char *str);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    heap_listp = (char *) mem_sbrk(WSIZE * 4);
    if (heap_listp == (char *) -1) {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, PREV_ALLOC));
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, PREV_ALLOC));
    PUT(heap_listp + 3*WSIZE, PACK(0, ALLOC));
    heap_listp += 2 * WSIZE;

    if (extend_heap(CHUNKSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t block_size;
    void *bp;

    if (size == 0) {
        return NULL;
    }
    
    block_size = ALIGN(size + WSIZE);
    bp = find_fit(block_size);
    if (bp == NULL) {
    /* can't find a such block, need extend heap */
        bp = extend_heap(MAX(CHUNKSIZE, block_size));
        if (bp == NULL) {
            return NULL;
        }
    }
    place(bp, block_size);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    unsigned int prev_alloc, size, alloc;
    char *bp;

    /* change this block to free state */
    prev_alloc = GET_PREV_ALLOC(HDRP(ptr));
    size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, prev_alloc));
    PUT(FTRP(ptr), PACK(size, prev_alloc));
    /* change the PREV_ALLOC flag of the next block */
    bp = NEXT_BLKP(ptr);
    alloc = GET_ALLOC(HDRP(bp));
    if (alloc) {
        PUT(HDRP(bp), GET(HDRP(bp)) & ~PREV_ALLOC);
    }       /* if the next block is not allocated, then don't modify it, 
               in the next step, coalesce, will modify it */
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    char *bp;
    unsigned int prev_alloc;
    size_t asize;
    
    if (ptr == NULL) {      /* case 1 */
        bp = mm_malloc(size);
        return bp;
    }
    if (size == 0) {        /* case 2 */
        mm_free(ptr);
        return NULL;
    }
    asize = GET_SIZE(HDRP(ptr)) - WSIZE;    /* get the payload of the current block */
    if (asize >= size) {        /* case 3 - not need to alloc a new block */
        prev_alloc = GET_PREV_ALLOC(HDRP(ptr));
        size = ALIGN(size+WSIZE);           /* change the size to the size of block */
        asize += WSIZE;
        PUT(HDRP(ptr), PACK(size, prev_alloc | ALLOC));
        PUT(HDRP(ptr), PACK(size, prev_alloc | ALLOC));
        if (asize != size) {
            bp = NEXT_BLKP(ptr);
            PUT(HDRP(bp), PACK(asize-size, PREV_ALLOC | ALLOC));
            PUT(HDRP(bp), PACK(asize-size, PREV_ALLOC | ALLOC));
            mm_free(bp);        /* need free the rest */
        }
        /* if the size of old block equal to the new size, then dont need change the block */
        return ptr;             
    }      
    /* case 4 - need to alloc a new block */
    if ((bp = (char *) mm_malloc(size)) == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < asize; i += 1) {
        bp[i] = *((char *) ptr + i);
    }
    mm_free(ptr);
    return bp;
}

/* 
 * extend_heap - Extend the heap WORDS words, and return address after the 
 * block coalesced, if error, then return NULL
 * The input size need be aligned
 */
static void *extend_heap(size_t size) {
    char *bp;
    unsigned int prev_alloc;

    bp = mem_sbrk(size);
    if (bp == (char *) -1) {
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    PUT(HDRP(bp), PACK(size, prev_alloc));
    PUT(FTRP(bp), PACK(size, prev_alloc));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, ALLOC));
    return coalesce(bp);
}

/* coalesce - coalesce the block and return an address point to the payload */
static void *coalesce(void *bp) {
    unsigned int prev_alloc, next_alloc, size, asize;

    size = GET_SIZE(HDRP(bp));
    prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    if (!prev_alloc) {      /* if prevous block is not allocated */
        bp = PREV_BLKP(bp);
        asize = GET_SIZE(HDRP(bp));
        size += asize;
        PUT(HDRP(bp), PACK(size, PREV_ALLOC));
        PUT(FTRP(bp), PACK(size, PREV_ALLOC));
    }
    if (!next_alloc) {      /* if next block is not allocated */
        asize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += asize;
        PUT(HDRP(bp), PACK(size, PREV_ALLOC));
        PUT(FTRP(bp), PACK(size, PREV_ALLOC));
    }
    return bp;
}

/* find_fit - find a block in free block list and its size is SIZE
 * Return an address point to the head of payload 
 * If can't find such block, then return NULL */
static void *find_fit(size_t size) {
    /* use first fit */
    char *bp;
    size_t asize;

    for (bp = heap_listp; (asize = GET_SIZE(HDRP(bp))) != 0; bp = NEXT_BLKP(bp)) {
        if (asize >= size && !GET_ALLOC(HDRP(bp))) {
            return bp;
        }
    }
    return NULL;
}

/* place - place a block whose size is SIZE in BP */
static void place(void *bp, size_t size) {
    size_t rest;
    unsigned int val;

    rest = GET_SIZE(HDRP(bp)) - size;
    PUT(HDRP(bp), PACK(size, PREV_ALLOC | ALLOC));
    PUT(FTRP(bp), PACK(size, PREV_ALLOC | ALLOC));
    if (rest != 0) {        /* need split this block */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(rest, PREV_ALLOC));
        PUT(FTRP(bp), PACK(rest, PREV_ALLOC));
    } else {                /* else chenge the flags of next block */
        bp = NEXT_BLKP(bp);
        val = GET(HDRP(bp));
        val |= PREV_ALLOC;
        PUT(HDRP(bp), val);
        PUT(FTRP(bp), val);
    }
}

/* mm_check - Check whether some */
static void mm_check(char *str) {
    char *bp;
    unsigned int size, prev_alloc, alloc;

    /* the start block's alloc flag is TRUE */
    prev_alloc = ALLOC;
    for (bp = heap_listp; (size = GET_SIZE(HDRP(bp))) != 0; bp = NEXT_BLKP(bp)) {
        alloc = GET_ALLOC(HDRP(bp));
        assert(bp == (char *) (((unsigned int) bp >> 3) << 3));     /* check the address alignment */
        assert(bp > (char *) mem_heap_lo());                        /* assert the address is legal */
        assert(bp < (char *) mem_heap_hi());    
        assert(prev_alloc == (GET_PREV_ALLOC(HDRP(bp)) >> 1));      /* check the PREV_ALLOC flag */
        assert((!prev_alloc & !alloc) == 0);                        /* check whether there has two contiguous free block */
        if (!alloc) {
            assert(GET(HDRP(bp)) == GET(FTRP(bp)));                 /* assert the header equal footer in the free block */
        }
        prev_alloc = alloc;
    }
}


