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

/* define flags */
#define ALLOC   1

/* the min size of a block */
#define MIN_BLOCK_SIZE      (4*WSIZE)  

#define MAX(x, y)       ((x) >= (y) ? (x) : (y))
#define MIN(x, y)       ((x) <= (y) ? (x) : (y))

/* pack a size and flag into a word */
#define PACK(size, flag)    ((size) | (flag))

/* read and write a word at address p */
#define GET(p)          (*(unsigned *) (p))
#define PUT(p, val)     (*(unsigned *) (p) = (unsigned) (val))

/* read size and allocated filed from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* give block ptr bp, compute address of header, footer, prev and next */
#define HDRP(bp)        ((char *) (bp) - WSIZE)    
#define FTRP(bp)        ((char *) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define PREVP(bp)       ((char *) (bp))
#define NEXTP(bp)       ((char *) (bp) + WSIZE)

/* give block ptr bp, compute address of next and previous block */
#define SUCR_BLKP(bp)   ((char *) (bp) + GET_SIZE(HDRP(bp)))
#define PDCR_BLKP(bp)   ((char *) (bp) - GET_SIZE((char *) (bp) - DSIZE))
#define NEXT_BLKP(bp)   ((char *) (*(unsigned *) NEXTP(bp)))
#define PREV_BLKP(bp)   ((char *) (*(unsigned *) PREVP(bp)))

static char *heap_listp;        /* point to the start of the heap */
static char *heap_start;        /* point to the start of the free list */

static void *extend_heap(unsigned size);        /* the function prototype */
static void *coalesce(void *bp);
static void *find_fit(unsigned size);
static void place(void *bp, unsigned size);
static void insert2start(char *bp);
static void mm_check(char *str);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *bp;

    if ((heap_listp = (char *) mem_sbrk(7*WSIZE)) == (char *) -1) {
        return -1;
    }
    heap_listp += 2*WSIZE;
    /* the start block, set the footer and header */
    PUT(HDRP(heap_listp), PACK(MIN_BLOCK_SIZE, ALLOC));
    PUT(FTRP(heap_listp), PACK(MIN_BLOCK_SIZE, ALLOC));
    bp = SUCR_BLKP(heap_listp);             /* point to the end block */
    /* the end block, the end block just has header and PREV field */
    PUT(HDRP(bp), PACK(0, ALLOC)); 
    /* set the PREV and NEXT field of start block and end block */
    PUT(PREVP(heap_listp), NULL);
    PUT(NEXTP(heap_listp), bp);
    PUT(PREVP(bp), heap_listp);
    heap_start = NEXTP(heap_listp);
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
    unsigned asize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    asize = ALIGN(size + DSIZE);
    /* find or get a block */
    if ((bp = find_fit(asize)) == NULL) {
        if ((bp = extend_heap(MAX(CHUNKSIZE, asize))) == NULL) {
            return NULL;
        }
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    unsigned size;

    size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    unsigned asize, bsize;
    char *bp;

    if (ptr == NULL) {      /* case 1 */
        return mm_malloc(size);
    }
    if (size == 0) {        /* case 2 */
        mm_free(ptr);
        return NULL;
    }

    asize = ALIGN(size + DSIZE);
    bsize = GET_SIZE(HDRP(ptr));
    if (bsize >= asize) {   /* case 3 - dont need to alloc a new block */
        asize = (bsize - asize >= MIN_BLOCK_SIZE) ? asize : bsize;
        PUT(HDRP(ptr), PACK(asize, ALLOC));
        PUT(FTRP(ptr), PACK(asize, ALLOC));
        if (asize != bsize) {
            bp = SUCR_BLKP(ptr);
            PUT(HDRP(bp), PACK(bsize-asize, 0));
            PUT(FTRP(bp), PACK(bsize-asize, 0));
            insert2start(bp);
        }
        return ptr;
    }
    /* case 4 - need to alloc a new block */
    if ((bp = (char *) mm_malloc(size)) == NULL) {
        return NULL;
    }
    for (int i = 0; i < size; i += 1) { 
        bp[i] = *((char *) ptr + i);
    }
    mm_free(ptr);
    return bp;
}

/* extend_heap - Extend the heap SIZE byte
 * the input SIZE must be aligned
 * return address after the block coalesced, if error then return NULL */
static void *extend_heap(unsigned size) {
    char *bp, *endbp, *prevp;

    if ((bp = (char *) mem_sbrk(size)) == (char *) -1) {
        return NULL;
    }
    bp -= WSIZE;            /* move pointer to the payload */
    PUT(HDRP(bp), PACK(size, 0));           /* set header field */                       
    PUT(FTRP(bp), PACK(size, 0));           /* set footer field */
    endbp = SUCR_BLKP(bp);
    prevp = PREV_BLKP(bp);
    /* move the end block to another place */
    PUT(HDRP(endbp), PACK(0, ALLOC));       /* set the end block */
    PUT(PREVP(endbp), prevp);                 
    PUT(NEXTP(prevp), endbp);
    return coalesce(bp);  
}

/* coalesce - coalesce the block and return address point to the payload */
static void *coalesce(void *bp) {
    unsigned pdcr_alloc, sucr_alloc, asize;
    char *pdcrp, *sucrp;

    pdcrp = PDCR_BLKP(bp);
    sucrp = SUCR_BLKP(bp);
    pdcr_alloc = GET_ALLOC(HDRP(pdcrp));
    sucr_alloc = GET_ALLOC(HDRP(sucrp));
    if (!pdcr_alloc) {          /* the predecessor is free */
        /* delete predecessor from free list */
        PUT(NEXTP(PREV_BLKP(pdcrp)), NEXT_BLKP(pdcrp));
        PUT(PREVP(NEXT_BLKP(pdcrp)), PREV_BLKP(pdcrp));
        /* modify the size of this block */
        asize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(pdcrp));
        bp = pdcrp;
        PUT(HDRP(bp), PACK(asize, 0));
        PUT(FTRP(bp), PACK(asize, 0));
    }
    if (!sucr_alloc) {          /* the successor is free */
        /* delete successor from free list */
        PUT(NEXTP(PREV_BLKP(sucrp)), NEXT_BLKP(sucrp));
        PUT(PREVP(NEXT_BLKP(sucrp)), PREV_BLKP(sucrp));
        /* modify the size of this block */
        asize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(sucrp));
        PUT(HDRP(bp), PACK(asize, 0));
        PUT(FTRP(bp), PACK(asize, 0));
    }
    /* insert this block to the begin of free list */
    insert2start(bp);
    return bp;
}

/* find_fit - find a block in free list whose size is bigger than or 
 * equal to SIZE. Return an address of the payload of the block
 * if dont find it, then return NULL */
static void *find_fit(unsigned size) {
    /* use first fit */
    char *bp;
    unsigned asize;

    bp = (char *) GET(heap_start);
    for (; (asize = GET_SIZE(HDRP(bp))) != 0; bp = NEXT_BLKP(bp)) {
        if (asize >= size) {
            return bp;
        }
    }
    return NULL;
}

/* place - place a block in bp whose size is SIZE */
static void place(void *bp, unsigned size) {
    unsigned asize;

    /* remove this block from free list */
    PUT(NEXTP(PREV_BLKP(bp)), NEXT_BLKP(bp));
    PUT(PREVP(NEXT_BLKP(bp)), PREV_BLKP(bp));
    /* split the block */
    asize = GET_SIZE(HDRP(bp));
    size = (asize - size >= MIN_BLOCK_SIZE) ? size : asize;
    PUT(HDRP(bp), PACK(size, ALLOC));
    PUT(FTRP(bp), PACK(size, ALLOC));
    if (asize != size) {        /* in this case, need modify the successor */
        bp = SUCR_BLKP(bp);
        asize -= size;
        /* modify the size of successor */
        PUT(HDRP(bp), PACK(asize, 0));
        PUT(FTRP(bp), PACK(asize, 0));
        /* insert the successor to the begin of free list */
        insert2start(bp);
    }
}

/* insert2start - insert a block to the begin of free list */
static void insert2start(char *bp) {
    char *ap, *start;

    ap = (char *) GET(heap_start);  /* point to the next block of starter */  
    start = heap_start - WSIZE;     /* point to the starter */
    PUT(PREVP(bp), start);
    PUT(NEXTP(bp), ap);
    PUT(NEXTP(start), bp);
    PUT(PREVP(ap), bp);
}

/* mm_check - scans the heap and checks it for consistency */
static void mm_check(char *str) {
    unsigned pdcr_alloc, alloc, size;
    char *bp, *prev_bp;

    /* check all the blocks in free list */
    prev_bp = heap_start - WSIZE;       /* point to the starter */
    bp = (char *) GET(heap_start);      /* point to the next block of starter */
    for (; (size = GET_SIZE(HDRP(bp))) != 0; bp = NEXT_BLKP(bp)) {
        alloc = GET_ALLOC(HDRP(bp));
        assert(alloc == 0);                         /* assert all the block is freed */
        // printf("#Check: heap_listp=%p, heap_start=%p, bp=%p, bp.next=%p, "
            // "size=%u\n", \
            // heap_listp, heap_start, bp, NEXT_BLKP(bp), size);
        /* assert the address in NEXTP field is legal */
        assert(mem_heap_lo() < (void *) NEXT_BLKP(bp) &&
               mem_heap_hi() > (void *) NEXT_BLKP(bp));
        assert(PREV_BLKP(bp) == prev_bp);           /* assert the PREV field is valid */
        assert(GET(HDRP(bp)) == GET(FTRP(bp)));     /* assert the footer equal to header */
        PUT(HDRP(bp), PACK(size, 0x2));             /* mark each blocks in free list */
        prev_bp = bp;
    }

    /* check all the blocks */
    pdcr_alloc = 1;         /* the start block is allocated */
    for (bp = heap_listp; (size = GET_SIZE(HDRP(bp))) != 0; bp = SUCR_BLKP(bp)) {
        alloc = GET_ALLOC(HDRP(bp));
        assert(!(pdcr_alloc == 0 && alloc ==0));        /* assert there has no contiguous free blocks */
        if (alloc == 0) {
            assert((GET(HDRP(bp)) & 0x2) == 0x2);       /* assert all the free blocks in free list */
            PUT(HDRP(bp), GET(FTRP(bp)));               /* make the header equal to footer */
        }
        assert(GET(HDRP(bp)) == GET(FTRP(bp)));         /* assert the footer equal to header */
        pdcr_alloc = alloc;
    }
}