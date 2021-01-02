//
// Created by Yu Zhexuan on 2020/12/15.
//

#ifndef STARTER_CODE_HANDLER_H
#define STARTER_CODE_HANDLER_H

#include "sha.h"
#include "list.h"
#include "init.h"
#include "bt_parse.h"
#include "packet.h"
#include "conn.h"

typedef struct get_s {
    int get_num;
    int *status; // 0: not start; 1: done; 2: processing.
    bt_peer_t **providers;
    chunk_t *get_chunks;
    char **chunk_data;
    char output[BT_FILENAME_LEN];
} get_t;

// handle user's GET command
void manage_user_input(char *chunk_file, char *out_file);

// add next chunk sender
char *add_sender(list_t *chunk_hash_list, bt_peer_t *peer);

// check the chunk data by the hash
void check_and_add_data(char *hash, char *data);

int is_task_finish();

void write_data();

void pkt_ntoh(packet_t *packet);

// return corresponding packet type if packet is valid, -1 otherwise
int parse_type(packet_t *packet);

// return a IHAVE packet if this peer has the chunk
packet_t *manage_whohas(packet_t *pkt_whohas);

// whether i have this chunk or not, has -> return 1, otherwise return 0.
int has_chunk(char *chunk_hash);

packet_t *new_pkt(unsigned char type, unsigned short packet_len,
                  unsigned int seq_num, unsigned int ack_num, char *payload);

// create WHOHAS packets according to the GET command
list_t *new_whohas_pkt();

// assemble hashes to torrent file
void chunk_hash_to_torrent(char *payload, int chunk_num, chunk_t *chunks);

// split torrent file into chunks
list_t *split_into_chunks(void *payload);

// manage IHAVE
packet_t *manage_ihave(packet_t *pkt, bt_peer_t *peer);

// manage GET
void manage_get(int sock, packet_t *pkt, bt_peer_t *peer);

// send data
void send_data_pkts(snd_conn_t *conn, int sock, struct sockaddr *to);

// receive data
packet_t **get_data_pkts(char *chunk_hash);

// manage DATA
void manage_data(int sock, packet_t *pkt, bt_peer_t *peer);

// manage ACK
void manage_ack(int sock, packet_t *pkt, bt_peer_t *peer);

// manage TIMEOUT
void manage_timeout();

#endif //STARTER_CODE_HANDLER_H
