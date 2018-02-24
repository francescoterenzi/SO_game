#ifndef _UPDATE_PACKET_H_
#define _UPDATE_PACKET_H_

#include "so_game_protocol.h"
#include "world.h"

ImagePacket* image_packet_init(Type type, Image *image, int id);
VehicleUpdatePacket* vehicle_update_init(World *world, int arg_id, float rotational_force, float translational_force);
WorldUpdatePacket* world_update_init(World *world);

#endif