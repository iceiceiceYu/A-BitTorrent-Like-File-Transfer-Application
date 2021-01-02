/*
 * peer.c
 * 
 * Author: Yi Lu <19212010040@fudan.edu.cn>,
 *
 * Modified from CMU 15-441,
 * Original Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *                   Dave Andersen
 * 
 * Class: Networks (Spring 2015)
 *
 */

/*
 * Student Name: 俞哲轩
 * Student ID: 18302010018
 * Class: Computer Network (Autumn 2020)
 */


// main function of the project

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "packet.h"
#include "util.h"
#include "init.h"

bt_config_t config;
get_t get;
list_t *chunk_tracker;
list_t *chunk_ihave;
snd_pool_t snd_pool;
rcv_pool_t rcv_pool;

void peer_run(bt_config_t *config);

void handler(int signal);

int main(int argc, char **argv) {
    bt_init(&config, argc, argv);

    DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
    config.identity = 1; // 18302010018 俞哲轩
    strcpy(config.chunk_file, "chunkfile");
    strcpy(config.has_chunk_file, "haschunks");
#endif

    bt_parse_command_line(&config);

#ifdef DEBUG
    if (debug & DEBUG_INIT) {
      bt_dump_config(&config);
    }
#endif

    signal(SIGALRM, handler);
    chunk_tracker = init_list();
    chunk_ihave = init_list();
    init_tracker();
    init_chunks_ihave();
    init_rcv_pool(&rcv_pool, config.max_conn);
    init_snd_pool(&snd_pool, config.max_conn);
    peer_run(&config);
    return 0;
}

void handler(int signal) { //handle signal of alarm: timeout and retransmission
    manage_timeout();
}

void process_inbound_udp(int sock) {
#define BUF_LEN 1500
    struct sockaddr_in from;
    socklen_t fromlen;
    char buf[BUF_LEN];

    fromlen = sizeof(from);
    spiffy_recvfrom(sock, buf, BUF_LEN, 0, (struct sockaddr *) &from, &fromlen);

//    printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
//           "Incoming message from %s:%d\n%s\n\n",
//           inet_ntoa(from.sin_addr),
//           ntohs(from.sin_port),
//           buf);

    packet_t *pkt_rcv = (packet_t *) buf;
    pkt_ntoh(pkt_rcv);
    int pkt_type = parse_type(pkt_rcv);

    //find the peer who sends the packet
    bt_peer_t *peer;
    for (peer = config.peers; peer != NULL; peer = peer->next) {
        if (peer->addr.sin_port == from.sin_port) {
            break;
        }
    }

    switch (pkt_type) {
        case WHOHAS: {
            packet_t *reply = manage_whohas(pkt_rcv);
            if (reply != NULL) {
                spiffy_sendto(sock, reply, reply->header.packet_len, 0, (struct sockaddr *) &from, fromlen);
                free(reply);
            }
            break;
        }
        case IHAVE: {
            packet_t *get_pkt = manage_ihave(pkt_rcv, peer);
            if (get_pkt != NULL) {
                spiffy_sendto(sock, get_pkt, get_pkt->header.packet_len, 0, (struct sockaddr *) &from, fromlen);
                free(get_pkt);
            }
            break;
        }
        case GET: {
            manage_get(sock, pkt_rcv, peer);
            break;
        }
        case DATA: {
            manage_data(sock, pkt_rcv, peer);
            break;
        }
        case ACK: {
            manage_ack(sock, pkt_rcv, peer);
            break;
        }
        case DENIED:
            //do nothing
            break;
        default:
            printf("Invalid Packet Type: %d\n", pkt_type);
    }
}

void process_get(char *chunkfile, char *outputfile) {
//    printf("PROCESS GET SKELETON CODE CALLED.  Fill me in!  (%s, %s)\n",
//           chunkfile, outputfile);

    manage_user_input(chunkfile, outputfile);
    list_t *ask_whohas = new_whohas_pkt();
    node_t *node = ask_whohas->head;
    while (node != NULL) {
        packet_t *packet = (packet_t *) (node->data);
        bt_peer_t *curr_peer = config.peers;
        while (curr_peer != NULL) {
            if (curr_peer->id != config.identity) {
                struct sockaddr *to = (struct sockaddr *) &(curr_peer->addr);
                spiffy_sendto(config.sock, packet, packet->header.packet_len, 0, to, sizeof(*to));
            }
            curr_peer = curr_peer->next;
        }
        free(packet);
        node = node->next;
    }
}

void handle_user_input(char *line, void *cbdata) {
    char chunkf[128], outf[128];

    bzero(chunkf, sizeof(chunkf));
    bzero(outf, sizeof(outf));

    if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
        if (strlen(outf) > 0) {
            process_get(chunkf, outf);
        }
    }
}


void peer_run(bt_config_t *config) {
    int sock;
    struct sockaddr_in myaddr;
    fd_set readfds;
    struct user_iobuf *userbuf;

    if ((userbuf = create_userbuf()) == NULL) {
        perror("peer_run could not allocate userbuf");
        exit(-1);
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
        perror("peer_run could not create socket");
        exit(-1);
    }

    bzero(&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(config->myport);

    if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
        perror("peer_run could not bind socket");
        exit(-1);
    }

    spiffy_init(config->identity, (struct sockaddr *) &myaddr, sizeof(myaddr));
    config->sock = sock;

    while (1) {
        int nfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);
        nfds = select(sock + 1, &readfds, NULL, NULL, NULL);

        if (nfds > 0) {
            if (FD_ISSET(sock, &readfds)) {
                process_inbound_udp(sock);
            }

            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                process_user_input(STDIN_FILENO, userbuf, handle_user_input,
                                   "Currently unused");
            }
        }
    }
}
