#ifndef _SERVER_KIT_H_
#define _SERVER_KIT_H_

#include "../image/image.h"
#include "../surface/surface.h"
#include "../world/world.h"
#include "../vehicle/vehicle.h"
#include "../world/world_viewer.h"
#include "../protocol/so_game_protocol.h"
#include "../packet/packet.h"
#include "../socket/socket.h"
#include "common.h"

typedef struct thread_args {
	int id;
	int socket_desc;	
} thread_args;

typedef struct ServerListItem{
  ListItem list;
  int info;
} ServerListItem;


void welcome_server(void);
void world_update(VehicleUpdatePacket *vehicle_packet, World *world);

ServerListItem* ServerListItem_init(int sock);
int Server_addSocket(ListHead* l, int sock);
void Server_listFree(ListHead* l);
void Server_socketClose(ListHead* socket_list);


#endif
