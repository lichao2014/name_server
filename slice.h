#ifndef _SLICE_H_INCLUDED
#define _SLICE_H_INCLUDED

#include <stdlib.h>

struct slice_t {
    void *data;
    size_t len;
    size_t cap;
    size_t size;
};

int slice_init(struct slice_t *slice, size_t size, size_t len, size_t cap);
void slice_free(struct slice_t *slice);
void *slice_append_n(struct slice_t *slice, size_t n);
void *slice_append(struct slice_t *slice);
void *slice_at(struct slice_t *slice, size_t pos);

#endif //!_SLICE_H_INCLUDED