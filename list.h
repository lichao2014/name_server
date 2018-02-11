#ifndef _LIST_H_INCLUDED
#define _LIST_H_INCLUDED

#include <stdlib.h>
#include "comm.h"

struct list_t {
    struct list_t *prev;
    struct list_t *next;
};

#define list_init(l)                        \
    do {                                    \
        (l)->prev = (l)->next = NULL;       \
    } while (0)

#define list_empty(l)                       \
    (l)->prev == (l)->next

#define list_insert_after(l, i)             \
    do {                                    \
        (i)->prev = (l);                    \
        (i)->next = (l)->next;              \
        (l)->next = (i);                    \
    } while(0)

#define list_insert_before(l, i)            \
    do {                                    \
        (i)->prev = (l)->prev;              \
        (i)->next = (l);                    \
        (l)->prev = (i);                    \
    } while(0)


#define list_del(i)                         \
    do {                                    \
        if ((i)->next) {                    \
            (i)->next->prev = (i)->prev;    \
        }                                   \
        if ((i)->prev) {                    \
            (i)->prev->next = (i)->next;    \
        }                                   \
    } while (0)

#define list_of(i, T, m)                    \
    container_of(i, T, m)

#endif //!_LIST_H_INCLUDED