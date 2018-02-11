#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "comm.h"
#include "net.h"

const char* localipaddr = "0.0.0.0";

int
main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("invalid args");
        return -1;
    }

    int c = create_tcp_socket(localipaddr, atoi(argv[2]), 0);
    if (c < 0) {
        return -1;
    }

    struct sockaddr_in peer = make_address4(argv[1], atoi(argv[3]));
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

        send(c, buf, sizeof(int) * 2 + msg->len, 0);
    }

    close(c);

    return 0;
}
