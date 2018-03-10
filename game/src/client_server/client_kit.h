#pragma once

#include "../image/image.h"
#include "../surface/surface.h"
#include "../world/world.h"
#include "../vehicle/vehicle.h"
#include "../world/world_viewer.h"
#include "../protocol/so_game_protocol.h"
#include "../packet/packet.h"
#include "../socket/socket.h"
#include "common.h"

typedef struct {
  volatile int run;
  int id;
  int tcp_desc;
  Image *texture;
  Vehicle *vehicle;
  World *world;
} UpdaterArgs;

void welcome_client(int id);
void Client_siglePlayerNotification(void);
Image* get_vehicle_texture(void);
void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world);

// thread functions
void *updater_thread(void *args);
void *connection_checker_thread(void* args);
