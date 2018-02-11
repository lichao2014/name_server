#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "comm.h"
#include "net.h"

static void
print_help_info(void) {
    printf("client opt:\n"
        "-h\tprint help info\n"
        "-a\tlocal address ip(default:127.0.0.1)\n"
        "-p\tlocal address port(default:0)\n"
        "-b\tremote address ip(default:127.0.0.1)\n"
        "-r\tremote address port\n");
}


int
main(int argc, char *argv[]) {
    const char *local_ip = "127.0.0.1";
    int local_port = 0;
    const char *remote_ip = "127.0.0.1";
    int remote_port = -1;

    int ch;
    while (-1 != (ch = getopt(argc, argv, "ha:p:b:r:"))) {
        switch (ch) {
        case 'h':
            print_help_info();
            return 0;
            break;
        case 'a':
            local_ip = optarg;
            break;
        case 'p':
            local_port = atoi(optarg);
            break;
        case 'b':
            remote_ip = optarg;
            break;
        case 'r':
            remote_port = atoi(optarg);
            break;
        case '?':
            printf("invalid arg '%c'\n", (char)(optopt));
            break;
        default:
            break;
        }
    }

    if (!local_ip || (local_ip < 0) || !remote_ip || (remote_port <= 0)) {
        print_help_info();
        return -1;
    }

    int c = create_tcp_socket(local_ip, local_port, 0);
    if (c < 0) {
        return -1;
    }

    struct sockaddr_in peer = make_address4(remote_ip, remote_port);
    int ret = connect(c, (struct sockaddr *)&peer, sizeof peer);
    if (ret < 0) {
        perror("connect");
        close(c);
        return -1;
    }

    char buf[1024];
    struct name_msg_t *msg = (struct name_msg_t *)buf;

    char line[1024];
    while (fgets(line, 1024, stdin)) {
        if (2 != sscanf(line, "%d %s", &msg->id, msg->data)) {
            continue;
        }

        buf[1023] = 0;
        msg->len = strlen(msg->data);

        int n = send(c, buf, sizeof(int) * 2 + msg->len, 0);
        if (n <= 0) {
            break;
        }
    }

    close(c);

    return 0;
}
