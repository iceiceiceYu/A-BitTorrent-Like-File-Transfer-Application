//
// Created by Yu Zhexuan on 2020/12/15.
//

// handle situation and reply, such as GET chunk、snd_packet、rcv_ACK and timeout

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include "util.h"
#include "sha.h"
#include "list.h"
#include "init.h"
#include "spiffy.h"
#include "chunk.h"

extern get_t get;
extern list_t *chunk_tracker;
extern list_t *chunk_ihave;
extern get_t get;
extern snd_pool_t snd_pool;
extern rcv_pool_t rcv_pool;
extern char master_file_name[256];
extern bt_config_t config;
static snd_conn_t *this_snd_conn;

void manage_user_input(char *chunk_file, char *out_file) {
    FILE *fd = fopen(chunk_file, "r");

    int line_num = 0;
    char read_buf[256];
    int id_buf;
    char hash_buf[2 * SHA1_HASH_SIZE];
    while (fgets(read_buf, 256, fd)) {
        line_num++;
    }
    get.get_num = line_num;
    get.get_chunks = malloc(line_num * sizeof(chunk_t));
    get.chunk_data = malloc(line_num * sizeof(char *));
    strcpy(get.output, out_file);
    get.status = malloc(line_num * sizeof(int));
    get.providers = malloc(line_num * sizeof(bt_peer_t *));
    for (int i = 0; i < line_num; i++) {
        get.status[i] = 0;
        get.providers[i] = NULL;
    }

    fseek(fd, 0, SEEK_SET);
    int index = 0;
    while (fgets(read_buf, 256, fd)) {
        if (sscanf(read_buf, "%d %s", &id_buf, hash_buf) != 2) {
            continue;
        }
        get.get_chunks[index].id = id_buf;
        hex2binary(hash_buf, 2 * SHA1_HASH_SIZE, (unsigned char *)get.get_chunks[index].chunk_hash);
        index++;
    }
}

char *add_sender(list_t *chunk_hash_list, bt_peer_t *peer) {
    int num = get.get_num;
    char *to_download = NULL;
    int is_first = 1;
    for (node_t *node = chunk_hash_list->head; node != NULL; node = node->next) {
        char *chunk_hash = (char *) (node->data);
        for (int i = 0; i < num; i++) {
            char *this_chunk_hash = get.get_chunks[i].chunk_hash;
            if (memcmp(this_chunk_hash, chunk_hash, SHA1_HASH_SIZE) == 0) {
                if (get.status[i] == 0) {
                    get.providers[i] = peer;

                    //in consideration of simultaneous transmission
                    if (is_first == 1) {
                        get.status[i] = 2;
                        to_download = get.get_chunks[i].chunk_hash;
                        is_first++;
                    }
                }
                break;
            }
        }
    }
    return to_download;
}

void check_and_add_data(char *hash, char *data) {
    int i;
    for (i = 0; i < get.get_num; i++) {
        char *this_hash = get.get_chunks[i].chunk_hash;
        if (memcmp(this_hash, hash, SHA1_HASH_SIZE) == 0) {
            get.chunk_data[i] = malloc(BT_CHUNK_SIZE);
            memcpy(get.chunk_data[i], data, BT_CHUNK_SIZE);
            break;
        }
    }

    uint8_t hash_my_data[SHA1_HASH_SIZE];
    char *my_data = get.chunk_data[i];
    shahash((uint8_t *) my_data, BT_CHUNK_SIZE, hash_my_data);
    get.status[i] = memcmp(hash_my_data, hash, SHA1_HASH_SIZE) == 0 ? 1 : 0;
}

int is_task_finish() {
    for (int i = 0; i < get.get_num; i++) {
        if (get.status[i] != 1) {
            return 0;
        }
    }
    return 1;
}

void write_data() {
    FILE *fd = fopen(get.output, "wb+");
    for (int i = 0; i < get.get_num; i++) {
        fwrite(get.chunk_data[i], 1024, 512, fd);
    }
    fclose(fd);
    for (int i = 0; i < get.get_num; i++) {
        free(get.chunk_data[i]);
    }
    free(get.chunk_data);
    free(get.status);
    free(get.providers);
    free(get.get_chunks);
}


void pkt_ntoh(packet_t *packet) {
    packet->header.magic = ntohs(packet->header.magic);
    packet->header.header_len = ntohs(packet->header.header_len);
    packet->header.packet_len = ntohs(packet->header.packet_len);
    packet->header.seq_num = ntohl(packet->header.seq_num);
    packet->header.ack_num = ntohl(packet->header.ack_num);
}

int parse_type(packet_t *packet) {
    header_t header = packet->header;
    if (header.magic != MAGIC_NUM || header.version != 1 || header.type < 0 || header.type > 5) {
        return -1;
    } else {
        return header.type;
    }
}

packet_t *manage_whohas(packet_t *pkt_whohas) {
    int req_num = pkt_whohas->data[0];
    int has_num = 0;
    int from_here = 4;
    char payload[DATA_LEN];
    char *chunk_hash = pkt_whohas->data + from_here;

    for (int i = 0; i < req_num; i++) {
        if (has_chunk(chunk_hash)) {
            has_num++;
            memcpy(payload + from_here, chunk_hash, SHA1_HASH_SIZE);
            from_here += SHA1_HASH_SIZE;
        }
        chunk_hash += SHA1_HASH_SIZE;
    }

    if (has_num == 0) {
        return NULL;
    } else {
        memset(payload, 0, 4);
        payload[0] = has_num;
        return new_pkt(IHAVE, HEADER_LEN + from_here, 0, 0, payload);
    }
}

packet_t *new_pkt(unsigned char type, unsigned short packet_len,
                  unsigned int seq_num, unsigned int ack_num, char *payload) {
    packet_t *packet = (packet_t *) malloc(sizeof(packet_t));
    packet->header.magic = htons(MAGIC_NUM);
    packet->header.version = 1;
    packet->header.type = type;
    packet->header.header_len = htons(HEADER_LEN);
    packet->header.packet_len = htons(packet_len);
    packet->header.seq_num = htonl(seq_num);
    packet->header.ack_num = htonl(ack_num);
    if (payload != NULL) {
        memcpy(packet->data, payload, packet_len - HEADER_LEN);
    }
    return packet;
}

int has_chunk(char *chunk_hash) {
    if (chunk_ihave->node_num == 0) {
        return 0;
    } else {
        node_t *curr_node = chunk_ihave->head;
        chunk_t *curr_chunk;
        while (curr_node != NULL) {
            curr_chunk = (chunk_t *) (curr_node->data);
            if (memcmp(curr_chunk->chunk_hash, chunk_hash, SHA1_HASH_SIZE) == 0) {
                return 1;
            }
            curr_node = curr_node->next;
        }
        return 0;
    }
}

list_t *new_whohas_pkt() {
    list_t *list = init_list();
    char payload[DATA_LEN];
    unsigned short pkt_len;

    if (get.get_num > MAX_CHUNK_NUM) { //need more than one packet to send WHOHAS data
        int quotient = get.get_num / MAX_CHUNK_NUM;
        int remainder = get.get_num - quotient * MAX_CHUNK_NUM;
        for (int i = 0; i < quotient; i++) {
            pkt_len = HEADER_LEN + 4 + MAX_CHUNK_NUM * SHA1_HASH_SIZE;
            chunk_hash_to_torrent(payload, MAX_CHUNK_NUM, get.get_chunks + i * MAX_CHUNK_NUM);
            add_node(list, new_pkt(WHOHAS, pkt_len, 0, 0, payload));
        }

        pkt_len = HEADER_LEN + 4 + remainder * SHA1_HASH_SIZE;
        chunk_hash_to_torrent(payload, remainder, get.get_chunks + quotient * MAX_CHUNK_NUM);
        add_node(list, new_pkt(WHOHAS, pkt_len, 0, 0, payload));
    } else {
        pkt_len = HEADER_LEN + 4 + get.get_num * SHA1_HASH_SIZE;
        chunk_hash_to_torrent(payload, get.get_num, get.get_chunks);
        add_node(list, new_pkt(WHOHAS, pkt_len, 0, 0, payload));
    }
    return list;
}

void chunk_hash_to_torrent(char *payload, int chunk_num, chunk_t *chunks) {
    int need = 0;
    char *hash_stater_p = payload + 4;
    for (int i = 0; i < chunk_num; i++) {
        if (get.providers[i] != NULL) { //there exists a peer to send the chunk data
            continue;
        }
        memcpy(hash_stater_p + i * SHA1_HASH_SIZE, chunks[i].chunk_hash, SHA1_HASH_SIZE);
        need++;
    }

    memset(payload, 0, 4);
    payload[0] = need;
}

list_t *split_into_chunks(void *payload) {
    list_t *chunks = init_list();
    int num = ((char *) payload)[0];
    char *from_here = payload + 4;
    for (int i = 0; i < num; i++) {
        char *chunk = malloc(SHA1_HASH_SIZE);
        memcpy(chunk, from_here + i * SHA1_HASH_SIZE, SHA1_HASH_SIZE);
        add_node(chunks, chunk);
    }
    return chunks;
}

packet_t *manage_ihave(packet_t *pkt, bt_peer_t *peer) {
    if (get_rcv_conn(&rcv_pool, peer) != NULL) {
        return NULL;
    } else {
        list_t *chunks = split_into_chunks(pkt->data);
        if (rcv_pool.conn_num >= rcv_pool.max_conn) {
            fprintf(stderr, "download pool is full, please wait!");
            return NULL;
        } else {
            char *to_download = add_sender(chunks, peer);
            chunk_buffer_t *chunk_buf = init_chunk_buffer(to_download);
            add_to_rcv_pool(&rcv_pool, peer, chunk_buf);
            packet_t *get_pkt = new_pkt(GET, HEADER_LEN + SHA1_HASH_SIZE, 0, 0, to_download);
            return get_pkt;
        }
    }
}

void manage_get(int sock, packet_t *pkt, bt_peer_t *peer) {
    snd_conn_t *snd_conn = get_snd_conn(&snd_pool, peer);
    //consider the situation of duplicate GET packets
    if (snd_conn != NULL) {
        remove_from_snd_pool(&snd_pool, peer);
        snd_conn = NULL;
    }

    if (snd_pool.conn_num >= snd_pool.max_conn) {
        fprintf(stderr, "upload pool is full!");
    } else {
        char to_upload[SHA1_HASH_SIZE];
        memcpy(to_upload, pkt->data, SHA1_HASH_SIZE);
        packet_t **data_pkt = get_data_pkts(to_upload);
        snd_conn = add_to_snd_pool(&snd_pool, peer, data_pkt);
        this_snd_conn = snd_conn;
        send_data_pkts(snd_conn, sock, (struct sockaddr *) (&(peer->addr)));
        alarm(2); //start timer: 2s
    }
}

packet_t **get_data_pkts(char *chunk_hash) {
    int id;
    for (node_t *node = chunk_tracker->head; node != NULL; node = node->next) {
        char *this_chunk_hash = ((chunk_t *) (node->data))->chunk_hash;
        if (memcmp(this_chunk_hash, chunk_hash, SHA1_HASH_SIZE) == 0) {
            id = ((chunk_t *) (node->data))->id;
            break;
        }
    }

    FILE *fd = fopen(master_file_name, "r");
    fseek(fd, id * BT_CHUNK_SIZE, SEEK_SET);
    char data[1024];
    packet_t **data_pkts = malloc(CHUNK_SIZE * sizeof(packet_t *));
    for (unsigned int i = 0; i < CHUNK_SIZE; i++) {
        fread(data, 1024, 1, fd);
        data_pkts[i] = new_pkt(DATA, HEADER_LEN + 1024, i + 1, 0, data);
    }
    fclose(fd);
    return data_pkts;
}

void send_data_pkts(snd_conn_t *conn, int sock, struct sockaddr *to) {
    int id_sender = config.identity;
    int id_receiver = conn->receiver->id;
    long now_time = clock();
    FILE *fd = fopen("problem2-peer.txt", "at");
    fprintf(fd, "%s%d-%d    %ld    %d\n", "conn", id_sender, id_receiver, now_time - conn->begin_time, conn->cwnd);
    fclose(fd);

    while (conn->to_send < conn->available) {
        spiffy_sendto(sock, conn->pkts[conn->to_send],
                      conn->pkts[conn->to_send]->header.packet_len, 0, to, sizeof(*to));
        conn->to_send++;
    }
}

void manage_data(int sock, packet_t *pkt, bt_peer_t *peer) {
    rcv_conn_t *rcv_conn = get_rcv_conn(&rcv_pool, peer);
    unsigned int seq = pkt->header.seq_num;
    int data_len = pkt->header.packet_len - HEADER_LEN;
    if (rcv_conn == NULL) {
        return;
    }

    packet_t *ack_pkt;
    if (seq == rcv_conn->next_ack) {
        memcpy(rcv_conn->chunk_buf->data_buf + rcv_conn->from_here, pkt->data, data_len);
        rcv_conn->from_here += data_len;
        rcv_conn->next_ack += 1;
        ack_pkt = new_pkt(ACK, HEADER_LEN, 0, seq, NULL);
    } else {
        ack_pkt = new_pkt(ACK, HEADER_LEN, 0, rcv_conn->next_ack - 1, NULL);
    }

    struct sockaddr *to = (struct sockaddr *) (&(peer->addr));
    spiffy_sendto(sock, ack_pkt, ack_pkt->header.packet_len, 0, to, sizeof(*to));
    free(ack_pkt);

    if (rcv_conn->from_here == BT_CHUNK_SIZE) {
        check_and_add_data(rcv_conn->chunk_buf->chunk_hash, rcv_conn->chunk_buf->data_buf);
        remove_from_rcv_pool(&rcv_pool, peer);
        if (is_task_finish()) {
            printf("write data to the target file\n");
            write_data();
        } else {
            printf("continue GET task\n");
            for (int i = 0; i < get.get_num; i++) {
                if (get.status[i] == 0 && get.providers[i] != NULL) {
                    char *to_download = get.get_chunks[i].chunk_hash;
                    chunk_buffer_t *chunk_buf = init_chunk_buffer(to_download);
                    add_to_rcv_pool(&rcv_pool, peer, chunk_buf);
                    packet_t *get_pkt = new_pkt(GET, HEADER_LEN + SHA1_HASH_SIZE, 0, 0, to_download);
                    struct sockaddr *get_data_from = (struct sockaddr *) (&(get.providers[i]->addr));
                    spiffy_sendto(sock, get_pkt, get_pkt->header.packet_len, 0, get_data_from, sizeof(*get_data_from));
                    get.status[i] = 2;
                    break;
                }
            }
        }
    }
}

void manage_ack(int sock, packet_t *pkt, bt_peer_t *peer) {
    int ack_num = pkt->header.ack_num;
    snd_conn_t *snd_conn = get_snd_conn(&snd_pool, peer);
    if (snd_conn == NULL) {
        return;
    }

    if (ack_num == CHUNK_SIZE) {
        alarm(0); //stop timer
        remove_from_snd_pool(&snd_pool, peer);
    } else if (ack_num > snd_conn->last_ack) {
        alarm(0);
        snd_conn->last_ack = ack_num;
        snd_conn->dup_times = 0;

//        if (snd_conn->cwnd < snd_conn->ssthresh) { // slow start
//            snd_conn->cwnd += add_wnd;
//            snd_conn->rtt_flag = ack_num + snd_conn->cwnd;
//        } else { // congestion avoidance
//            if (ack_num >= snd_conn->rtt_flag) {
//                snd_conn->cwnd += 1;
//                snd_conn->rtt_flag += snd_conn->cwnd;
//            }
//        }
//        snd_conn->to_send = snd_conn->to_send > ack_num ? snd_conn->to_send : ack_num;
        int next_available = ack_num + snd_conn->cwnd;
        snd_conn->available = next_available <= CHUNK_SIZE ? next_available : CHUNK_SIZE;
        send_data_pkts(snd_conn, sock, (struct sockaddr *) (&(peer->addr)));
        alarm(2);
    } else if (ack_num == snd_conn->last_ack) {
        snd_conn->dup_times++;
        if (snd_conn->dup_times >= 3) {
            alarm(0);
            snd_conn->dup_times = 0;
            snd_conn->to_send = ack_num;

//            int snd_ssthresh = (int) (snd_conn->cwnd / 2);
//            snd_conn->ssthresh = snd_ssthresh >= 2 ? snd_ssthresh : 2;
//            snd_conn->cwnd = 1;

            int available = snd_conn->to_send + snd_conn->cwnd;
            snd_conn->available = available < CHUNK_SIZE ? available : CHUNK_SIZE;
            send_data_pkts(snd_conn, sock, (struct sockaddr *) (&(peer->addr)));
            alarm(2);
        }
    }
}

void manage_timeout() {
    this_snd_conn->to_send = this_snd_conn->last_ack;
    this_snd_conn->dup_times = 0;

//    int snd_ssthresh = (int) (this_snd_conn->cwnd / 2);
//    this_snd_conn->ssthresh = snd_ssthresh >= 2 ? snd_ssthresh : 2;
//    this_snd_conn->cwnd = 1;

    int next_available = this_snd_conn->to_send + this_snd_conn->cwnd;
    this_snd_conn->available = next_available <= CHUNK_SIZE ? next_available : CHUNK_SIZE;
    send_data_pkts(this_snd_conn, config.sock, (struct sockaddr *) (&(this_snd_conn->receiver->addr)));
    alarm(2);
}