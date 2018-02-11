#include "slice.h"

int
slice_init(struct slice_t *slice, size_t size, size_t len, size_t cap) {
    if (len > cap) {
        return -1;
    }

    slice->data = calloc(cap, size);
    slice->size = size;
    slice->len = len;
    slice->cap = cap;

    return 0;
}

void
slice_free(struct slice_t *slice) {
    if (slice && slice->data) {
        free(slice->data);
    }
}

void *
slice_append_n(struct slice_t *slice, size_t n) {
    void *p;

    if (slice->cap < slice->len + n) {
        size_t cap = slice->cap;
        while (cap < slice->len + n) {
            cap <<= 1;
        }

        p = realloc(slice->data, cap * slice->size);
        if (!p) {
            return NULL;
        }

        slice->data = p;
        slice->cap = cap;
    }

    p = (char *)slice->data + slice->len * slice->size;
    slice->len += n;

    return p;
}

void *
slice_append(struct slice_t *slice) {
    return slice_append_n(slice, 1);
}

void *
slice_at(struct slice_t *slice, size_t pos) {
    if (!slice || slice->len <= pos) {
        return NULL;
    }

    return (char *)slice->data + pos * slice->size;
}
