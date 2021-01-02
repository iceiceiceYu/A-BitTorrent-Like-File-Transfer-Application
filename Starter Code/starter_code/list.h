//
// Created by Yu Zhexuan on 2020/12/15.
//

#ifndef STARTER_CODE_LIST_H
#define STARTER_CODE_LIST_H

typedef struct node_s {
    void *data;
    struct node_s *next;
} node_t;

typedef struct list_s {
    int node_num;
    node_t *head;
    node_t *tail;
} list_t;

list_t *init_list();

void add_node(list_t *list, void *data);

void *remove_node(list_t *list);

#endif //STARTER_CODE_LIST_H
