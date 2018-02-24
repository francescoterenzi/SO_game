#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "so_game_protocol.h"

int tcp_client_setup(void);
void tcp_send(int socket_desc, PacketHeader* packet);
int tcp_receive(int socket_desc , char* msg);

#endif