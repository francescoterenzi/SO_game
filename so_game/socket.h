#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "so_game_protocol.h"

#define SERVER_ADDRESS    "127.0.0.1"
#define UDP_SOCKET_NAME   "[UDP]"
#define TCP_SOCKET_NAME   "[TCP]"
#define UDP_PORT        3000
#define TCP_PORT        4000
#define MAX_CONN_QUEUE  20
#define UDP_BUFLEN      512


int tcp_server_setup(void);

int tcp_client_setup(void);

int udp_server_setup(struct sockaddr_in *si_me);

int udp_client_setup(struct sockaddr_in *si_other);

void tcp_send(int socket_desc, PacketHeader* packet);

int tcp_receive(int socket_desc , char* msg);

void udp_send(int socket, struct sockaddr_in *si, const PacketHeader* h);

int udp_receive(int socket, struct sockaddr_in *si, char *buffer);

#endif
