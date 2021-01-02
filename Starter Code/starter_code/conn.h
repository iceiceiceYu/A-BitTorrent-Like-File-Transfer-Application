//
// Created by Yu Zhexuan on 2020/12/15.
//

#ifndef STARTER_CODE_CONN_H
#define STARTER_CODE_CONN_H

#include "bt_parse.h"
#include "sha.h"
#include "packet.h"

#define CHUNK_SIZE     512

typedef struct chunk_buffer_s {
    char chunk_hash[SHA1_HASH_SIZE];
    char *data_buf;
} chunk_buffer_t;

typedef struct rcv_conn_s {
    int from_here; // use when caching data into chunk_buf
    int next_ack; // next ack expected
    bt_peer_t *sender;
    chunk_buffer_t *chunk_buf; // use to cache data
} rcv_conn_t;

typedef struct snd_conn_s {
    int last_ack; // last ack received
    int to_send; // next to send
    int available; // index of next packet after the send window
    int dup_times; // duplicate times of last_ack
    int cwnd; // window size
    // int ssthresh; // threshold of slow start
    // int rtt_flag; // represent the end of a rtt
    long begin_time;
    packet_t **pkts; // cache all packets to send
    bt_peer_t *receiver;
} snd_conn_t;

typedef struct rcv_pool_s {
    rcv_conn_t **conns;
    int conn_num;
    int max_conn;
} rcv_pool_t;

typedef struct snd_pool_s {
    snd_conn_t **conns;
    int conn_num;
    int max_conn;
} snd_pool_t;

chunk_buffer_t *init_chunk_buffer(char *hash);

void free_chunk_buffer(chunk_buffer_t *chunk_buf);

rcv_conn_t *init_rcv_conn(bt_peer_t *peer, chunk_buffer_t *chunk_buf);

rcv_conn_t *add_to_rcv_pool(rcv_pool_t *pool, bt_peer_t *peer, chunk_buffer_t *chunk_buf);

void remove_from_rcv_pool(rcv_pool_t *pool, bt_peer_t *peer);

void init_rcv_pool(rcv_pool_t *pool, int max_conn);

rcv_conn_t *get_rcv_conn(rcv_pool_t *pool, bt_peer_t *peer);

snd_conn_t *init_snd_conn(bt_peer_t *peer, packet_t **pkts);

snd_conn_t *add_to_snd_pool(snd_pool_t *pool, bt_peer_t *peer, packet_t **pkts);

void remove_from_snd_pool(snd_pool_t *pool, bt_peer_t *peer);

void init_snd_pool(snd_pool_t *pool, int max_conn);

snd_conn_t *get_snd_conn(snd_pool_t *pool, bt_peer_t *peer);

#endif //STARTER_CODE_CONN_H
