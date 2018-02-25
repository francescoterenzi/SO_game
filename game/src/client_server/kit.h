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
void update_info(World *world, int id, int flag);
void world_update(VehicleUpdatePacket *deserialized_vehicle_packet, World *world);


/** CLIENT **/
typedef struct {
  Image *texture;
  volatile int run;
  int id;
  int tcp_desc;
} UpdaterArgs;

void welcome_client(int id);
void get_vehicle_texture(Image *vehicle_texture);
void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world);

#endif