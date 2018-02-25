#ifndef _KIT_H_
#define _KIT_H_

#include "../protocol/so_game_protocol.h"
#include "../world/world.h"

/** SERVER **/
typedef struct thread_args {
	int id;
	int socket_desc;	
} thread_args;

void welcome_server(void);
void world_update(VehicleUpdatePacket *deserialized_vehicle_packet, World *world);


/** CLIENT **/
typedef struct {
  Image *texture;
  volatile int run;
  int id;
  int tcp_desc;
} UpdaterArgs;

void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world);

#endif