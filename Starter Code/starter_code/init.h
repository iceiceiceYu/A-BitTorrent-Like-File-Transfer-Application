//
// Created by Yu Zhexuan on 2020/12/15.
//

#ifndef STARTER_CODE_INIT_H
#define STARTER_CODE_INIT_H

#include "sha.h"

typedef struct chunk_s {
    int id;
    char chunk_hash[SHA1_HASH_SIZE];
} chunk_t;

// read master-chunk-file
void init_tracker();

// read has-chunk-file
void init_chunks_ihave();

#endif //STARTER_CODE_INIT_H
