#ifndef _NET_H_INCLUDED
#define _NET_H_INCLUDED

#include "list.h"
#include "slice.h"

struct io_object;

typedef void (*read_callback_t)(struct io_object *);
typedef void (*write_callback_t)(struct io_object *);
typedef void (*error_callback_t)(struct io_object *);

struct io_context {
    struct slice_t events;
    struct list_t objects;
    size_t nobjects;
    int epollfd;
};

struct io_object {
    struct list_t q;
    struct io_context *ctx;
    read_callback_t on_read;
    write_callback_t on_write;
    error_callback_t on_error;
    void *arg;
    int fd;
    int deleted;
};

int io_context_init(struct io_context *ctx, size_t hint);
void io_context_close(struct io_context *ctx);
// return >0 success; 0 timeout; < 0 failed
int io_context_run(struct io_context *ctx, int timeout);
int io_context_add(struct io_context *ctx,
                        int fd,
                        read_callback_t on_read,
                        write_callback_t on_write,
                        error_callback_t on_error,
                        void *arg);
// add signal notify. return signalfd
int io_context_add_signal(struct io_context *ctx,
                                   read_callback_t on_signal,
                                   void *arg,
                                   ...);
int io_context_del(struct io_context *ctx, int fd);

struct sockaddr_in make_address4(const char *ip, int port);
int create_tcp_socket(const char *ip, int port, int is_server);
int send_with_fd(int s, void *buf, size_t len, int fd);
int recv_with_fd(int s, void *buf, size_t len, int *fd);

#endif //!_NET_H_INCLUDED
