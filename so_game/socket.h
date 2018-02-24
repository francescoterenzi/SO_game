#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "so_game_protocol.h"

int tcp_server_setup(void);

int tcp_client_setup(void);

int udp_server_setup(struct sockaddr_in *si_me);

int udp_client_setup(struct sockaddr_in *si_other);

void tcp_send(int socket_desc, PacketHeader* packet);

int tcp_receive(int socket_desc , char* msg);

void udp_send(int socket, struct sockaddr_in *si, const PacketHeader* h);

int udp_receive(int socket, struct sockaddr_in *si, char *buffer);

#endif