#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "server_kit.h"

void welcome_server(void) {
	char *os_msg = "\nOPERATING SYSTEM PROJECT 2018 - SERVER SIDE ***\n\n";
	if(DEBUG) {
		fprintf(stdout ,"%s", os_msg);
		fflush(stdout);
		fprintf(stdout, "OS SERVER ip:[%s] port:[%d] ready to accept incoming connections! ***\n\n", 
					SERVER_ADDRESS , TCP_PORT);
		fflush(stdout);
	}
}

void world_update(VehicleUpdatePacket *vehicle_packet, World *world) {
	
	int id = vehicle_packet->id;
		
	Vehicle* v = World_getVehicle(world, id);
	v->rotational_force_update = vehicle_packet->rotational_force;
	v->translational_force_update = vehicle_packet->translational_force; 

	World_update(world);
}
