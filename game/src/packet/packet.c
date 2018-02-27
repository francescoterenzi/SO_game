#include <stdlib.h>
#include <string.h>

#include "packet.h"
#include "../client_server/common.h"

ImagePacket* image_packet_init(Type type, Image *image, int id) {

	ImagePacket *packet = (ImagePacket*) malloc(sizeof(ImagePacket));

	PacketHeader header;
	header.type = type;

	packet->header = header;
	packet->id = id;
	packet->image = image;

	return packet;
}

IdPacket* id_packet_init(Type header_type, int id){
	PacketHeader id_header;
	id_header.type = header_type;
	
	IdPacket* id_packet = (IdPacket*)malloc(sizeof(IdPacket));
	id_packet->header = id_header;
	id_packet->id = id;	
	return id_packet;
}

VehicleUpdatePacket* vehicle_update_init(World *world,int arg_id, float rotational_force, float translational_force) {
    
    VehicleUpdatePacket *vehicle_packet = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));
	PacketHeader v_head;
	v_head.type = VehicleUpdate;

	vehicle_packet->header = v_head;
	vehicle_packet->id = arg_id;
	vehicle_packet->rotational_force = (World_getVehicle(world, arg_id))->rotational_force_update;
	vehicle_packet->translational_force = (World_getVehicle(world, arg_id))->translational_force_update;

	return vehicle_packet;
}

WorldUpdatePacket* world_update_init(World *world) {
    
    WorldUpdatePacket* world_packet = (WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
	PacketHeader w_head;
	w_head.type = WorldUpdate;
	world_packet->header = w_head;
	
	
	world_packet->num_vehicles = world->vehicles.size;
	
	ClientUpdate* update_block = (ClientUpdate*)malloc(world_packet->num_vehicles*sizeof(ClientUpdate));
	
	ListItem *item = world->vehicles.first;

	int i;
	for(i=0; i<world->vehicles.size; i++) {

		Vehicle *v = (Vehicle*) item;
		update_block[i].id = v->id;
		update_block[i].x = v->x;
		update_block[i].y = v->y;			
		update_block[i].theta = v->theta;

		item = item->next;
	}
		
	world_packet->updates = update_block;
	return world_packet;
}
