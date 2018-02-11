#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "comm.h"
#include "net.h"
#include "list.h"
#include "slice.h"
#include "log.h"

#define kEpollSize 1

struct name_worker_t;

struct name_client_t {
    struct list_t q;
    int fd;
    int buf_offset;
    struct name_worker_t *worker;
    struct slice_t buf;
};

struct name_worker_t {
    struct io_context io_ctx;
    int listenfd;
    int id;
    int fd;
    struct list_t clients;
    size_t nclients : (sizeof(size_t) * 8 - 1);
    int stopped : 1;
};

struct name_server_t;

struct name_worker_ctx_t {
    struct list_t q;
    struct name_server_t *server;
    pid_t pid;
    int id;
    int fd;
};

struct name_server_t {
    struct io_context io_ctx;
    int listenfd;
    struct list_t workers;
    size_t nworkers : (sizeof(size_t) * 8 - 1);
    int stopped : 1;
};

static void
worker_close_client(struct name_client_t *client) {
    DEBUG_LOG("client close #%p\n", client);

    if (client->worker) {
        if (io_context_del(&client->worker->io_ctx, client->fd) < 0) {
            perror("io_context_del");
        }

        client->worker->nclients--;
    }

    slice_free(&client->buf);
    list_del(&client->q);

    if (client->fd >= 0) {
        close(client->fd);
        client->fd = -1;
    }

    free(client);
}

static void *
client_alloc_msg(struct name_client_t *client, int len) {
    if (client->buf_offset + len > client->buf.len) {
        slice_append_n(&client->buf, client->buf_offset + len - client->buf.len);
    }

    return slice_at(&client->buf, client->buf_offset);
}

static void
client_on_read(struct io_object *o) {
    struct name_client_t *client = o->arg;

    int read_body = 0;
    int nread = 0;
    struct name_msg_t *msg;
    if (client->buf_offset < sizeof(int)) {
        // read id
        nread = sizeof(int) - client->buf_offset;
    } else if (client->buf_offset < 2 * sizeof(int)) {
        // read len
        nread = 2 * sizeof(int) - client->buf_offset;
    } else {
        // read body
        msg = (struct name_msg_t *)client->buf.data;
        nread = msg->len + 2 * sizeof(int) - client->buf_offset;
        read_body = 1;
    }

    int n = recv(client->fd, client_alloc_msg(client, nread), nread, 0);

    DEBUG_LOG("client %d recv n=%d\n", client->fd, n);
    if (n <= 0) {
        perror("recv");
        worker_close_client(client);
        return ;
    }

    client->buf_offset += n;

    msg = (struct name_msg_t *)client->buf.data;
    if (read_body && n == nread) {
        INFO_LOG("client %d recv a msg, len:%d\n", client->fd, msg->len);
        client->buf_offset = 0;
        return ;
    }

    if (sizeof(int) != client->buf_offset) {
        return ;
    }

    DEBUG_LOG("client %d recv a id:%d\n", client->fd, msg->id);

    if (client->worker->id == msg->id) {
        return ;
    }

    // check id. relay fd msg if id is not right
    if (send_with_fd(client->worker->fd, &msg->id, sizeof(int), client->fd) < 0) {
        perror("sendmsg");
    }

    worker_close_client(client);
}

static void
client_on_error(struct io_object *o) {
    struct name_client_t *client = o->arg;
    ERROR_LOG("client %d on error:%d\n", client->fd, errno);
    worker_close_client(client);
}

static int
worker_add_client(struct name_worker_t *worker, int fd, int id) {
    struct name_client_t *client = malloc(sizeof(struct name_client_t));
    if (io_context_add(&worker->io_ctx,
                        fd,
                        &client_on_read,
                        NULL,
                        &client_on_error,
                        client) < 0) {
        ERROR_LOG("io_context_add failed");
        free(client);
        return -1;
    }

    client->fd = fd;
    client->worker = worker;
    list_init(&client->q);
    
    if (id >= 0) {
        slice_init(&client->buf, 1, 4, 8);
        memcpy(client->buf.data, &id, sizeof id);
        client->buf_offset = sizeof id;
    } else {
        slice_init(&client->buf, 1, 0, 8);
        client->buf_offset = 0;
    }

    list_insert_after(&worker->clients, &client->q);
    worker->nclients++;

    return 0;
}

static void
worker_on_accept(struct io_object *o) {
    struct name_worker_t *worker = o->arg;
    int c = accept(worker->listenfd, NULL, NULL);
    if (c <= 0) {
        return ;
    }

    DEBUG_LOG("worker %d accept fd:%d\n",  worker->id, c);

    if (worker_add_client(worker, c, -1) < 0) {
        close(c);
    }
}

static void
worker_on_read(struct io_object *o) {
    struct name_worker_t *worker = o->arg;
    int id;
    int fd;
    if (recv_with_fd(worker->fd, &id, sizeof(int), &fd) < 0) {
        return ;
    }

    DEBUG_LOG("worker %d recv fd:%d msg id:%d\n",  worker->id, fd, id);

    if (worker->id != id) {
        return ;
    }

    if (worker_add_client(worker, fd, id) < 0) {
        close(fd);
    }
}

static void
worker_on_error(struct io_object *o) {
    struct name_worker_t *worker = o->arg;

    ERROR_LOG("worker %d on error:%d\n", worker->id, errno);
    worker->stopped = 1;
}

static int
worker_init(struct name_worker_t *worker,
                int listenfd,
                int id,
                int fd) {
    if (io_context_init(&worker->io_ctx, kEpollSize) < 0) {
        return -1;
    }

    worker->listenfd = listenfd;
    worker->id = id;
    worker->fd = fd;
    list_init(&worker->clients);
    worker->nclients = 0;
    worker->stopped = 0;

    return 0;
}

static void
worker_close(struct name_worker_t *worker) {
    if (worker->fd >= 0) {
        close(worker->fd);
    }

    io_context_close(&worker->io_ctx);
}

static int
worker_run(struct name_worker_t *worker) {
    if (io_context_add(&worker->io_ctx,
                        worker->listenfd,
                        &worker_on_accept,
                        NULL,
                        NULL,
                        worker) < 0) {
        return -1;
    }

    if (io_context_add(&worker->io_ctx,
                        worker->fd,
                        &worker_on_read,
                        NULL,
                        &worker_on_error,
                        worker) < 0) {
        return -1;
    }

    while (!worker->stopped) {
        io_context_run(&worker->io_ctx, 10000);
    }

    return 0;
}

static int
do_worker(int listenfd, int id, int fd) {
    struct name_worker_t worker;
    if (worker_init(&worker, listenfd, id, fd) < 0) {
        ERROR_LOG("worker init failed.id:%d,fd:%d\n", id, fd);
        return -1;
    }

    INFO_LOG("worker %d run with fd:%d\n", id, fd);

    worker_run(&worker);
    worker_close(&worker);

    return 0;
}

static int
server_init(struct name_server_t *server, const char *ip, int port) {
    int listenfd = create_tcp_socket(ip, port, 1);
    if (listenfd < 0) {
        ERROR_LOG("create socket failed.server:%s,port:%d\n", ip, port);
        return -1;
    }

    if (io_context_init(&server->io_ctx, kEpollSize) < 0) {
        ERROR_LOG("io_context_init failed");
        close(listenfd);
        return -1;
    }

    server->listenfd = listenfd;
    list_init(&server->workers);
    server->nworkers = 0;
    server->stopped = 0;

    return 0;
}

static void
server_close(struct name_server_t *server) {
    for (struct list_t *i = server->workers.next; i; i = i->next) {
        struct name_worker_ctx_t *wc = list_of(i, struct name_worker_ctx_t, q);
        close(wc->fd);
        free(wc);
    }

    server->nworkers = 0;

    if (server->listenfd) {
        close(server->listenfd);
    }

    io_context_close(&server->io_ctx);
}

static struct name_worker_ctx_t *
server_find_worker(struct name_server_t *server, int id) {
    for (struct list_t *i = server->workers.next; i; i = i->next) {
        struct name_worker_ctx_t *wc = list_of(i, struct name_worker_ctx_t, q);
        if (wc->id == id) {
            return wc;
        }
    }

    return NULL;
}

static void
close_socketpair(int fds[2]) {
    if (fds[0] > 0) {
        close(fds[0]);
    }

    if (fds[1] > 0) {
        close(fds[1]);
    }
}

static void
server_on_read(struct io_object *o) {
    struct name_worker_ctx_t *wc = o->arg;

    int id;
    int fd;
    if (recv_with_fd(wc->fd, &id, sizeof(int), &fd) < 0) {
        perror("recvmsg");
        return ;
    }

    DEBUG_LOG("server recv fd:%d msg id:%d\n", fd, id);

    struct name_worker_ctx_t *other = server_find_worker(wc->server, id);
    if (!other) {
        ERROR_LOG("server can not find worker by id:%d\n", id);
        close(fd);
        return ;
    }

    // relay fd msg
    send_with_fd(other->fd, &id, sizeof(id), fd);
    close(fd);
}

static void
server_on_error(struct io_object *o) {
    struct name_worker_ctx_t *wc = o->arg;
    
    ERROR_LOG("server'` worker %d on error:%d\n", wc->id, errno);
}

static int
server_add_worker(struct name_server_t *server, int id) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        perror("socketpair");
        return -1;
    }

    int pid = fork();
    if (pid < 0) {
        perror("fork");
        close_socketpair(fds);
        return -1;
    }

    if (0 == pid) {
        if (do_worker(server->listenfd, id, fds[0]) < 0) {
            close(fds[0]);
        }

        return 1;
    }

    struct name_worker_ctx_t *wc = malloc(sizeof(struct name_worker_ctx_t));
    wc->pid = pid;
    wc->id = id;
    wc->server = server;
    wc->fd = fds[1];
    list_init(&wc->q);
    if (io_context_add(&server->io_ctx,
                        fds[1],
                        &server_on_read,
                        NULL,
                        &server_on_error,
                        wc) < 0) {
        free(wc);
        close(fds[1]);
        return -1;
    }

    INFO_LOG("add worker #%d fd:%d\n", pid, fds[1]);

    list_insert_after(&server->workers, &wc->q);
    server->nworkers++;

    return 0;
}

static int
server_run(struct name_server_t *server, int nworkers) {
    int ret;
    for (int i = 0; i < nworkers; ++i) {
        ret = server_add_worker(server, i);
        if (0 != ret) {
            ERROR_LOG("server add worker failed.i:%d\n", i);
            return ret;
        }
    }

    // only master process run
    while (!server->stopped) {
        io_context_run(&server->io_ctx, 1000);
    }

    return 0;
}

static int
do_server(const char *ip, int port, int nworkers) {
    struct name_server_t server;
    if (server_init(&server, ip, port) < 0) {
        return -1;
    }

    INFO_LOG("server run on %s:%d with %d workers\n", ip, port, nworkers);

    server_run(&server, nworkers);
    server_close(&server);

    return 0;
}

int
main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("invalid args");
        return -1;
    }

    return do_server(argv[1], atoi(argv[2]), atoi(argv[3]));
}
