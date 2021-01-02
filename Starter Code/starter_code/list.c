//
// Created by Yu Zhexuan on 2020/12/15.
//

// define a single linked list data structure

#include <stdio.h>
#include <stdlib.h>
#include "list.h"

list_t *init_list() {
    list_t *list = malloc(sizeof(list_t));
    list->node_num = 0;
    list->head = NULL;
    list->tail = NULL;
    return list;
}

void add_node(list_t *list, void *data) {
    node_t *node = malloc(sizeof(node_t));
    node->data = data;
    node->next = NULL;

    list->node_num++;
    if (list->head == NULL) {
        list->head = node;
    }
    if (list->tail != NULL) {
        list->tail->next = node;
    }
    list->tail = node;
}

void *remove_node(list_t *list) {
    if (list->node_num == 0) {
        return NULL;
    }
    node_t *node = list->head;
    void *data = node->data;
    list->head = node->next;
    list->node_num--;
    if (list->node_num == 0) {
        list->tail = NULL;
    }
    return data;
}