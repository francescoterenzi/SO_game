#ifndef _UPDATE_PACKET_H_
#define _UPDATE_PACKET_H_

#include "so_game_protocol.h"
#include "world.h"

ImagePacket* image_packet_init(Type type, Image *image, int id);

IdPacket* id_packet_init(Type header_type, int id);

VehicleUpdatePacket* vehicle_update_init(World *world, int arg_id, float rotational_force, float translational_force);

WorldUpdatePacket* world_update_init(World *world);

void clear(char* buf);

#endif
