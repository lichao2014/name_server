#ifndef _COMM_H_INCLUDED
#define _COMM_H_INCLUDED

#ifndef offsetof
    #define offsetof(T, m) (size_t)(&((T *)0)->m)
#endif


#ifndef container_of
    #define container_of(ptr, T, m) (T *)((char *)ptr - offsetof(T, m))
#endif

struct name_msg_t {
    int id;
    int len;
    char data[1];
};

#endif //! _COMM_H_INCLUDED
