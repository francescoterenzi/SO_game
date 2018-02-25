#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "kit.h"
#include "common.h"
#include "../packet/packet.h"
#include "../world/world.h"
#include "../socket/socket.h"

/** SERVER **/
void welcome_server(void) {
	char *os_msg = "\nOPERATING SYSTEM PROJECT 2018 - SERVER SIDE ***\n\n";
	if(DEBUG) {
		fprintf(stdout ,"%s", os_msg);
		fflush(stdout);
		fprintf(stdout, "OS SERVER ip:[%s] port:[%d] ready to accept incoming connections! ***\n", 
					SERVER_ADDRESS , TCP_PORT);
		fflush(stdout);
	}
}

void world_update(VehicleUpdatePacket *deserialized_vehicle_packet, World *world) {
	
	int vehicle_id = deserialized_vehicle_packet->id;
		
	Vehicle* v = World_getVehicle(world, vehicle_id);
	v->rotational_force_update = deserialized_vehicle_packet->rotational_force;
	v->translational_force_update = deserialized_vehicle_packet->translational_force; 

	World_update(world);

}

/** CLIENT **/
void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world) {

	int numb_of_vehicles = deserialized_wu_packet->num_vehicles;
	int world_size = world->vehicles.size;
	
	if(numb_of_vehicles > world_size) {
		int i;
		for(i=0; i<numb_of_vehicles; i++) {
			int v_id = deserialized_wu_packet->updates[i].id;
			if(World_getVehicle(world, v_id) == NULL) {
	
				char buffer[BUFLEN];

				ImagePacket* vehicle_packet = image_packet_init(GetTexture, NULL, v_id);
    			tcp_send(socket_desc , &vehicle_packet->header);
    			
				int ret = tcp_receive(socket_desc , buffer);
    			vehicle_packet = (ImagePacket*) Packet_deserialize(buffer, ret);

				Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle));
				Vehicle_init(v,world, v_id, vehicle_packet->image);
				
				World_addVehicle(world, v);
				printf("%s new vehicle added with id: %d\n", UDP_SOCKET_NAME, v_id);
			} 
		}
	}
	
	else if(numb_of_vehicles < world_size) {
		ListItem* item=world->vehicles.first;
		int i, find = 0;
		while(item){
			Vehicle* v = (Vehicle*)item;
			int vehicle_id = v->id;
			for(i=0; i<numb_of_vehicles; i++){
				if(deserialized_wu_packet->updates[i].id == vehicle_id)
					find = 1;
			}

			if (find == 0) {
				printf("client %d disconnected\n", v->id);
				World_detachVehicle(world, v);
			}

			find = 0;
			item=item->next;
		}
	}

	int i;
	for(i=0; i<world_size; i++) {
		Vehicle *v = World_getVehicle(world, deserialized_wu_packet->updates[i].id);
		
		v->x = deserialized_wu_packet->updates[i].x;
		v->y = deserialized_wu_packet->updates[i].y;
		v->theta = deserialized_wu_packet->updates[i].theta;
	}
}