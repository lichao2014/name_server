#include <stdio.h>
#include "list.h"

struct list_node_t {
    struct list_t q;
    int data;
};

static void
list_print(const char *msg, struct list_t *lst) {
    printf("\nlist:%s\n", msg);

    while ((lst = lst->next)) {
        struct list_node_t *node = container_of(lst, struct list_node_t, q);
        printf("%d-", node->data);
    }
}

int main() {
    struct list_t lst;

    list_init(&lst);
    list_print("1", &lst);

    struct list_node_t *node = malloc(sizeof(struct list_node_t));
    node->data = 1;
    list_init(&node->q);
    list_insert_after(&lst, &node->q);

    node = malloc(sizeof(struct list_node_t));
    node->data = 2;
    list_init(&node->q);
    list_insert_after(&lst, &node->q);

    node = malloc(sizeof(struct list_node_t));
    node->data = 3;
    list_init(&node->q);
    list_insert_after(&lst, &node->q);

    list_print("2", &lst);


    list_del(&node->q);
    list_print("3", &lst);
}
