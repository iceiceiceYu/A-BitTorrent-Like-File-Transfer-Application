//
// Created by Yu Zhexuan on 2020/12/15.
//

#ifndef STARTER_CODE_PACKET_H
#define STARTER_CODE_PACKET_H

// Package Header
#define HEADER_LEN         16
#define PACKET_LEN         1500
#define DATA_LEN           (PACKET_LEN - HEADER_LEN)
#define MAGIC_NUM          15441
#define MAX_CHUNK_NUM      74

// Packet Type & Code
#define WHOHAS             0
#define IHAVE              1
#define GET                2
#define DATA               3
#define ACK                4
#define DENIED             5

typedef struct header_s {
    unsigned short magic;
    unsigned char version;
    unsigned char type;
    unsigned short header_len;
    unsigned short packet_len;
    unsigned int seq_num;
    unsigned int ack_num;
} header_t;

typedef struct packet_s {
    header_t header;
    char data[DATA_LEN];
} packet_t;

#endif //STARTER_CODE_PACKET_H
