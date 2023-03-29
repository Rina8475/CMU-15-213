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

/* word and header/footer size (byte) */
#define WSIZE   4

/* double word size (byte) */
#define DSIZE   8

/* the min size of a block */
#define MIN_BLOCK_SIZE      (4*WSIZE)

/* extend heap by this amount (byte) */
#define CHUNKSIZE   (1 << 12)

#define MAX(x, y)   ((x) >= (y) ? (x) : (y))

/* define flags */
#define ALLOC   1

/* pack a size and a flag into a word */
#define PACK(size, flag)    ((size) | (flag))

/* read and write a word in address p */
#define GET(p)          (*(unsigned *) (p))
#define PUT(p, val)     (*(unsigned *) (p) = (unsigned) (val))

/* read size or allocated field from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* give ptr bp, compute the address of header, footer and NEXT field */
#define HDRP(bp)        ((char *) (bp) - WSIZE)
#define FTRP(bp)        ((char *) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXTP(bp)       ((char *) (bp))

/* give block ptr bp, compute address of next block */
#define NEXT_BLKP(bp)       ((char *) GET(NEXTP(bp)))

/* give block ptr bp, compute address of predecessor or successor */
#define PDCR_BLKP(bp)       ((char *) (bp) - GET_SIZE((char *) (bp) - DSIZE))
#define SUCR_BLKP(bp)       ((char *) (bp) + GET_SIZE(HDRP(bp)))

/* length of the root list */
#define ROOT_LIST_LEN       (9*WSIZE)

char *root_base;
char *heap_listp;

static void *extend_heap(unsigned size);
static void *coalesce(void *bp);
static void insert(void *bp);
static void delete(void *bp);
static char *get_root(unsigned size);
static void *find_fit(unsigned size);
static void place(char *bp, unsigned size);
static void mm_check(char *str);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((root_base = mem_sbrk(12*WSIZE)) == (void *) -1) {
        return -1;
    }
    PUT(root_base + 0*WSIZE, 0);    /* size < 32    */
    PUT(root_base + 1*WSIZE, 0);    /* size < 64    */
    PUT(root_base + 2*WSIZE, 0);    /* size < 128   */
    PUT(root_base + 3*WSIZE, 0);    /* size < 256   */
    PUT(root_base + 4*WSIZE, 0);    /* size < 512   */
    PUT(root_base + 5*WSIZE, 0);    /* size < 1024  */
    PUT(root_base + 6*WSIZE, 0);    /* size < 2048  */
    PUT(root_base + 7*WSIZE, 0);    /* size < 4096  */
    PUT(root_base + 8*WSIZE, 0);    /* size >= 4096 */

    PUT(root_base + 9*WSIZE, PACK(DSIZE, ALLOC));       /* the start block */
    PUT(root_base + 10*WSIZE, PACK(DSIZE, ALLOC));
    PUT(root_base + 11*WSIZE, PACK(0, ALLOC));          /* the end block   */
    heap_listp = root_base + 12*WSIZE;
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
    char *bp;

    if (size == 0) {
        return NULL;
    }

    size = ALIGN(size + DSIZE);
    /* find or extend a block */
    if ((bp = find_fit(size)) == NULL) {
        if ((bp = extend_heap(MAX(size, CHUNKSIZE))) == NULL) {
            return NULL;
        }
    }
    /* remove this block from free list */
    delete(bp);             
    /* split this block */
    place(bp, size);
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
    if (bsize >= asize) {       /* case 3 - dont need to alloc a new block */
        place(ptr, asize);
        return ptr;
    }
    /* case 4 - need to alloc a new block */
    if ((bp = mm_malloc(size)) == NULL) {
        return NULL;
    }
    memcpy(bp, ptr, bsize-DSIZE);
    mm_free(ptr);
    return bp;
}

/* extend_heap - Extend the heap SIZE bytes, the input SIZE must be aligned
 * return address after coalesce, if error then return NULL */
static void *extend_heap(unsigned size) {
    char *bp;

    if ((bp = (char *) mem_sbrk(size)) == (char *) -1) {
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    /* set the end block */
    PUT(HDRP(SUCR_BLKP(bp)), PACK(0, ALLOC));
    return coalesce(bp);
}

/* coalesce - coalesce the block and return address point to the payload */
static void *coalesce(void *bp) {
    unsigned pdcr_alloc, sucr_alloc, size;
    char *pdcrp, *sucrp;

    pdcrp = PDCR_BLKP(bp);
    sucrp = SUCR_BLKP(bp);
    pdcr_alloc = GET_ALLOC(HDRP(pdcrp));
    sucr_alloc = GET_ALLOC(HDRP(sucrp));
    if (!pdcr_alloc) {          /* the predecessor is free */
        size = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(pdcrp));
        delete(pdcrp);      /* remove the predecessor from free list */
        bp = pdcrp;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } 
    if (!sucr_alloc) {          /* the successor is free */
        size = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(sucrp));
        delete(sucrp);      /* remove the successor from free list */
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* insert this block to the free list */
    insert(bp);
    return bp;
}

/* insert - insert this block to the begin of appropriate free list  */
static void insert(void *bp) {
    char *root;

    root = get_root(GET_SIZE(HDRP(bp)));
    PUT(NEXTP(bp), GET(root));
    PUT(root, bp);
}

/* delete - remove this block from free list */
static void delete(void *bp) {
    char *walk;

    walk = get_root(GET_SIZE(HDRP(bp)));
    while ((void *) GET(walk) != bp) {
        walk = (char *) NEXTP(GET(walk));
        // TODO assert the range of WALK
    }
    PUT(walk, NEXT_BLKP(GET(walk)));
}

/* get_root - Inpute the size of a free block, then return address of root 
 * of free list */
static char *get_root(unsigned size) {
    unsigned count;

    count = (size >= 32) + (size >= 64) + (size >= 128) + (size >= 256) + 
            (size >= 512) + (size >= 1024) + (size >= 2048) + (size >= 4096);
    return root_base + count*WSIZE;  
}

/* find_fit - find a block in free list whose size is bigger or equal to
 * SIZE, return address of the payload of the block
 * if not find such block, then return NULL */
static void *find_fit(unsigned size) {
    /* use first fit */
    char *root, *walk;

    root = (char *) get_root(size);
    for (; root < root_base + ROOT_LIST_LEN; root += WSIZE) {
        walk = (char *) GET(root);      /* point to the first block */
        for (; walk != NULL; walk = NEXT_BLKP(walk)) {
            if (GET_SIZE(HDRP(walk)) >= size) {
                return walk;
            }
        }
    }
    return NULL;
}

/* place - place a block in BP whose size is SIZE, and BP is a freed block 
 * has been removed from free list */
static void place(char *bp, unsigned size) {
    unsigned asize;

    asize = GET_SIZE(HDRP(bp));
    size = (asize >= size + MIN_BLOCK_SIZE) ? size : asize;
    PUT(HDRP(bp), PACK(size, ALLOC));
    PUT(FTRP(bp), PACK(size, ALLOC));
    if (size != asize) {        /* need split this block */
        bp = SUCR_BLKP(bp);
        PUT(HDRP(bp), PACK(asize-size, 0));
        PUT(FTRP(bp), PACK(asize-size, 0));
        insert(bp);         /* insert the successor to the free list */
    }
}

/* mm_check - scans the heap and checks it for consistency */
static void mm_check(char *str) {
    char *bp, *walk;
    unsigned size, pdcr_alloc, alloc;

    bp = root_base;
    for (; bp < root_base + ROOT_LIST_LEN; bp += WSIZE) {
        walk = (char *) GET(bp);
        for (; walk != NULL; walk = NEXT_BLKP(walk)) {
            /* assert the address is legal */
            assert(mem_heap_lo() < (void *) walk && 
                mem_heap_hi() > (void *) walk);
            size = GET_SIZE(HDRP(walk));
            assert(GET_ALLOC(HDRP(walk)) == 0);             /* assert each block in the free list is freed */
            // printf("#Check: heap_listp=%p, root=%p, walk=%p, walk.next=%p, "
                // "size=%u\n", \
                // heap_listp, bp, walk, NEXT_BLKP(walk), size);
            assert(GET(HDRP(walk)) == GET(FTRP(walk)));     /* assert the footer equal to header */
            PUT(HDRP(walk), PACK(size, 0x2));
        }
    }

    pdcr_alloc = ALLOC;
    bp = heap_listp;
    for (; GET_SIZE(HDRP(bp)) != 0; bp = SUCR_BLKP(bp)) {
        alloc = GET_ALLOC(HDRP(bp));
        assert(!(alloc == 0 && pdcr_alloc == 0));           /* assert there has no contiguous free blocks */
        if (alloc == 0) {
            assert((GET(HDRP(bp)) & 0x2) == 0x2);           /* assert all the free blocks in free list */
            PUT(HDRP(bp), GET(FTRP(bp)));                   /* make the header eaual to footer */
        }
        assert(GET(HDRP(bp)) == GET(FTRP(bp)));             /* assert the footer eauql to header */
        pdcr_alloc = alloc;
    }
}