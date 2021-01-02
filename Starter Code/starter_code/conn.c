//
// Created by Yu Zhexuan on 2020/12/15.
//

// construct a reliable connection for data transfer

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "conn.h"
#include "chunk.h"

chunk_buffer_t *init_chunk_buffer(char *hash) {
    chunk_buffer_t *chunk_buf = malloc(sizeof(chunk_buffer_t));
    memcpy(chunk_buf->chunk_hash, hash, SHA1_HASH_SIZE);
    chunk_buf->data_buf = malloc(BT_CHUNK_SIZE);
    return chunk_buf;
}

void free_chunk_buffer(chunk_buffer_t *chunk_buf) {
    free(chunk_buf->data_buf);
    free(chunk_buf);
}

rcv_conn_t *init_rcv_conn(bt_peer_t *peer, chunk_buffer_t *chunk_buf) {
    rcv_conn_t *conn = malloc(sizeof(rcv_conn_t));
    conn->from_here = 0;
    conn->next_ack = 1;
    conn->chunk_buf = chunk_buf;
    conn->sender = peer;
    return conn;
}

rcv_conn_t *add_to_rcv_pool(rcv_pool_t *pool, bt_peer_t *peer, chunk_buffer_t *chunk_buf) {
    rcv_conn_t *conn = init_rcv_conn(peer, chunk_buf);
    for (int i = 0; i < pool->max_conn; i++) {
        if (pool->conns[i] == NULL) {
            pool->conns[i] = conn;
            break;
        }
    }
    pool->conn_num++;
    return conn;
}

void remove_from_rcv_pool(rcv_pool_t *pool, bt_peer_t *peer) {
    rcv_conn_t **conns = pool->conns;
    for (int i = 0; i < pool->max_conn; i++) {
        if (conns[i] != NULL && conns[i]->sender->id == peer->id) {
            free_chunk_buffer(conns[i]->chunk_buf);
            free(conns[i]);
            conns[i] = NULL;
            pool->conn_num--;
            break;
        }
    }
}

void init_rcv_pool(rcv_pool_t *pool, int max_conn) {
    pool->conn_num = 0;
    pool->max_conn = max_conn;
    pool->conns = malloc(max_conn * sizeof(rcv_conn_t *));
    for (int i = 0; i < max_conn; i++) {
        pool->conns[i] = NULL;
    }
}

rcv_conn_t *get_rcv_conn(rcv_pool_t *pool, bt_peer_t *peer) {
    rcv_conn_t **conns = pool->conns;
    for (int i = 0; i < pool->max_conn; i++) {
        rcv_conn_t *rcv_conn = conns[i];
        if (rcv_conn != NULL && rcv_conn->sender->id == peer->id) {
            return rcv_conn;
        }
    }
    return NULL;
}

snd_conn_t *init_snd_conn(bt_peer_t *peer, packet_t **pkts) {
    snd_conn_t *conn = malloc(sizeof(snd_conn_t));
    conn->last_ack = 0;
    conn->to_send = 0;
    conn->dup_times = 0;
    // conn->cwnd = 1; // initial congestion control window size
    // conn->ssthresh = 64; // initial slow start threshold
    // conn->available = 1;
    // conn->rtt_flag = 1;
    conn->cwnd = 8; // fixed window size
    int available = conn->to_send + conn->cwnd;
    conn->available = available < CHUNK_SIZE ? available : CHUNK_SIZE;
    conn->begin_time = clock();
    conn->receiver = peer;
    conn->pkts = pkts;
    return conn;
}

snd_conn_t *add_to_snd_pool(snd_pool_t *pool, bt_peer_t *peer, packet_t **pkts) {
    snd_conn_t *conn = init_snd_conn(peer, pkts);
    for (int i = 0; i < pool->max_conn; i++) {
        if (pool->conns[i] == NULL) {
            pool->conns[i] = conn;
            break;
        }
    }
    pool->conn_num++;
    return conn;
}

void remove_from_snd_pool(snd_pool_t *pool, bt_peer_t *peer) {
    snd_conn_t **conns = pool->conns;
    for (int i = 0; i < pool->max_conn; i++) {
        if (conns[i] != NULL && conns[i]->receiver->id == peer->id) {
            for (int j = 0; j < CHUNK_SIZE; j++) {
                free(conns[i]->pkts[j]);
            }
            free(conns[i]->pkts);
            free(conns[i]);
            conns[i] = NULL;
            pool->conn_num--;
            break;
        }
    }
}

void init_snd_pool(snd_pool_t *pool, int max_conn) {
    pool->conn_num = 0;
    pool->max_conn = max_conn;
    pool->conns = malloc(max_conn * sizeof(snd_conn_t *));
    for (int i = 0; i < max_conn; i++) {
        pool->conns[i] = NULL;
    }
}

snd_conn_t *get_snd_conn(snd_pool_t *pool, bt_peer_t *peer) {
    snd_conn_t **conns = pool->conns;
    for (int i = 0; i < pool->max_conn; i++) {
        snd_conn_t *snd_conn = conns[i];
        if (snd_conn != NULL && snd_conn->receiver->id == peer->id) {
            return snd_conn;
        }
    }
    return NULL;
}