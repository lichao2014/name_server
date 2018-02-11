#include <assert.h>
#include "slice.h"

int main() {
    struct slice_t s1;
    assert(slice_init(&s1, sizeof(int), 0, 1) != -1);
    assert(0 == s1.len);
    assert(1 == s1.cap);

    *(int *)slice_append(&s1) = 0x123;
    int *p1 = slice_at(&s1, 0);
    assert(0x123 == *p1);

    assert(1 == s1.len);
    assert(1 == s1.cap);

    *(int *)slice_append(&s1) = 0x567;
    p1 = slice_at(&s1, 1);
    assert(0x567 == *p1);
    assert(2 == s1.len);
    assert(2 == s1.cap);

    *(int *)slice_append(&s1) = 0x567;
    p1 = slice_at(&s1, 2);
    assert(0x567 == *p1);
    assert(3 == s1.len);
    assert(4 == s1.cap);


    slice_free(&s1);
}