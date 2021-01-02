//
// Created by Yu Zhexuan on 2020/12/15.
//

// read master-chunk-file & has-chunk-file and store the information

#include <stdio.h>
#include <stdlib.h>
#include "init.h"
#include "bt_parse.h"
#include "list.h"
#include "debug.h"
#include "chunk.h"

extern bt_config_t config;
extern list_t *chunk_tracker;
extern list_t *chunk_ihave;
char master_file_name[256];

void init_tracker() {
    FILE *fd = fopen(config.chunk_file, "r");
    char info_buf[256], line_buf[256];
    fgets(line_buf, 256, fd);
    sscanf(line_buf, "%s %s", info_buf, master_file_name);

    int chunk_id;
    fgets(line_buf, 256, fd);
    while (fgets(line_buf, 256, fd)) {
        if (sscanf(line_buf, "%d %s", &chunk_id, info_buf) != 2) {
            continue;
        }
        chunk_t *chunk = malloc(sizeof(chunk_t));
        chunk->id = chunk_id;
        hex2binary(info_buf, 2 * SHA1_HASH_SIZE, (unsigned char *)chunk->chunk_hash);
        add_node(chunk_tracker, chunk);
    }
    fclose(fd);
}

void init_chunks_ihave() {
    FILE *fd = fopen(config.has_chunk_file, "r");
    char hash_buf[256], line_buf[256];

    int chunk_id;
    while (fgets(line_buf, 256, fd)) {
        if (sscanf(line_buf, "%d %s", &chunk_id, hash_buf) != 2) {
            continue;
        }
        chunk_t *chunk = malloc(sizeof(chunk_t));
        chunk->id = chunk_id;
        hex2binary(hash_buf, 2 * SHA1_HASH_SIZE, (unsigned char *)chunk->chunk_hash);
        add_node(chunk_ihave, chunk);
    }
    fclose(fd);
}