#ifndef _KIT_H_
#define _KIT_H_

#include "../image/image.h"
#include "../surface/surface.h"
#include "../world/world.h"
#include "../vehicle/vehicle.h"
#include "../world/world_viewer.h"
#include "../protocol/so_game_protocol.h"
#include "../packet/packet.h"
#include "../socket/socket.h"
#include "common.h"

/** COMMON **/
void clear(char* buf);
void update_info(World *world, int id, int flag);

/** SERVER **/
typedef struct thread_args {
	int id;
	int socket_desc;	
} thread_args;

void welcome_server(void);

/** CLIENT **/
typedef struct {
  volatile int run;
  int id;
  int tcp_desc;
  Image *texture;
  Vehicle *vehicle;
  World *world;
} UpdaterArgs;

void welcome_client(int id);
Image* get_vehicle_texture(void);
void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world);
void *updater_thread(void *arg);

#endif
