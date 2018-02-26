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

void clear(char* buf);

void welcome_server(void);

void update_info(World *world, int id, int flag);

void welcome_client(int id);

Image* get_vehicle_texture(void);

#endif
