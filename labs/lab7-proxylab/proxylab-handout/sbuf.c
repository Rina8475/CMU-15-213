/****************
 *  SBUF packet
 ****************/
#include "sbuf.h"

/* sbuf_init - Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n) {
    sp->buf   = (int *) Malloc(n * sizeof(int));
    sp->n     = n;
    sp->front = 0;
    sp->rear  = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n); 
    Sem_init(&sp->items, 0, 0);
}

/* sbuf_deinit - Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp) {
    free(sp->buf);
}

/* sbuf_insert - Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item) {
    sem_wait(&sp->slots);
    sem_wait(&sp->mutex);
    sp->rear = (sp->rear+1) % sp->n;
    sp->buf[sp->rear] = item;
    sem_post(&sp->mutex);
    sem_post(&sp->items);
}

/* sbuf_remove - Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp) {
    int item;

    sem_wait(&sp->items);
    sem_wait(&sp->mutex);
    sp->front = (sp->front+1) % sp->n;      /* point to the first item */
    item = sp->buf[sp->front];
    sem_post(&sp->mutex);
    sem_post(&sp->slots);
    return item;
}