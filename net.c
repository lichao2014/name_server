#include "net.h"
#include "log.h"
#include <unistd.h>
#include <stdio.h>

int
io_context_init(struct io_context *ctx, size_t hint) {
    int epollfd = epoll_create(hint);
    if (epollfd < 0) {
        perror("epoll_create");
        return -1;
    }

    if (slice_init(&ctx->events, sizeof(struct epoll_event), 0, 2) < 0) {
        close(epollfd);
        return -1;
    }

    ctx->epollfd = epollfd;
    list_init(&ctx->objects);
    ctx->nobjects = 0;

    return 0;
}

void
io_context_close(struct io_context *ctx) {
    for (struct list_t *i = ctx->objects.next; i; i = i->next) {
        struct io_object *o = list_of(i, struct io_object, q);
        close(o->fd);
        free(o);
    }

    ctx->nobjects = 0;

    slice_free(&ctx->events);

    if (ctx->epollfd >= 0) {
        close(ctx->epollfd);
    }
}

int
io_context_run(struct io_context *ctx, int timeout) {
    if (ctx->nobjects > ctx->events.len) {
        slice_append_n(&ctx->events, ctx->nobjects - ctx->events.len);
    }

    struct epoll_event *events = (struct epoll_event *)ctx->events.data;
    int ret = epoll_wait(ctx->epollfd, events, ctx->nobjects, timeout);
    if (ret <= 0) {
        return ret;
    }

    DEBUG_LOG("epoll_wait ret:%d\n", ret);

    for (int i = 0; i < ret; ++i) {
        struct io_object *o = events[i].data.ptr;

        if (events[i].events & EPOLLOUT) {
            o->on_write(o);
        }

        if (events[i].events & EPOLLIN) {
            o->on_read(o);
        }

        if (events[i].events & EPOLLERR) {
            o->on_error(o);
        }

        if (o->deleted) {
            free(o);
        }
    }

    return ret;
}

int
io_context_add(struct io_context *ctx,
                    int fd,
                    read_callback_t on_read,
                    write_callback_t on_write,
                    error_callback_t on_error,
                    void *arg) {
    struct io_object *o = malloc(sizeof(struct io_object));
    if (!o) {
        return -1;
    }

    list_init(&o->q);
    o->ctx = ctx;
    o->arg = arg;
    o->fd = fd;
    o->deleted = 0;
    o->on_read = on_read;
    o->on_write = on_write;
    o->on_error = on_error;

    struct epoll_event e;
    e.data.ptr = o;
    e.events = 0;

    if (on_read) {
        e.events |= EPOLLIN;
    }

    if (on_write) {
        e.events |= EPOLLOUT;
    }

    if (on_error) {
        e.events |= EPOLLERR;
    }

    if (epoll_ctl(ctx->epollfd, EPOLL_CTL_ADD, fd, &e) < 0) {
        free(o);
        return -1;
    }

    list_insert_after(&ctx->objects, &o->q);
    ctx->nobjects++;

    return 0;
}

int
io_context_del(struct io_context *ctx, int fd) {
    struct io_object *o = NULL;

    for (struct list_t *i = ctx->objects.next; i; i = i->next) {
        o = list_of(i, struct io_object, q);
        if (o->fd == fd) {
            list_del(&o->q);
            ctx->nobjects--;
            break;
        }
    }

    if (!o) {
        return -1;
    }

    o->deleted = 1;

    return epoll_ctl(ctx->epollfd, EPOLL_CTL_DEL, fd, NULL);
}

struct sockaddr_in
make_address4(const char *ip, int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    return addr;
}

int
create_tcp_socket(const char *ip, int port, int is_server) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return fd;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr = make_address4(ip, port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (is_server) {
        if (listen(fd, 64) < 0) {
            perror("listen");
            close(fd);
            return -1;
        }
    }

    return fd;
}

int
send_with_fd(int s, void *buf, size_t len, int fd) {
    char control[CMSG_SPACE(sizeof(int))];

    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = len;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof control;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int *)CMSG_DATA(cmsg) = fd;

    return sendmsg(s, &msg, 0);
}

int
recv_with_fd(int s, void *buf, size_t len, int *fd) {
    char control[CMSG_SPACE(sizeof(int))];

    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = len;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof control;

    if (recvmsg(s, &msg, 0) <= 0) {
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    if (!cmsg
        || (CMSG_LEN(sizeof(int)) != cmsg->cmsg_len)
        || (SOL_SOCKET != cmsg->cmsg_level)
        || (SCM_RIGHTS != cmsg->cmsg_type)) {
        return -1;
    }

    *fd = *(int *)CMSG_DATA(cmsg);

    return 0;
}
