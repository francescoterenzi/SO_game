#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <time.h>

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
		fprintf(stdout, "OS SERVER ip:[%s] port:[%d] ready to accept incoming connections! ***\n\n", 
					SERVER_ADDRESS , TCP_PORT);
		fflush(stdout);
	}
}

void update_info(World *world, int id, int flag) {

	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	char buffer[1024];
	if(flag) {
		sprintf(buffer, "- new client id:[%d] connected", id);
	} 
	else {
		sprintf(buffer, "- client id:[%d] closed the game", id);
	}

	if(DEBUG) {
	fprintf(stdout, "update %s%s\n- number of vehicles connected %d ***\n\n", 
			asctime(timeinfo), buffer, world->vehicles.size);
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
void welcome_client(int id) {
    	char *os_msg = "\nOPERATING SYSTEM PROJECT 2018 - CLIENT SIDE ***\n\n";
	if(DEBUG) {
		fprintf(stdout ,"%s", os_msg);
		fflush(stdout);
		fprintf(stdout, "Connect to OS SEREVR ip:[%s] port:[%d] with id:[%d] ***\n\n", 
					SERVER_ADDRESS , TCP_PORT, id);
		fflush(stdout);
	}
}

void get_vehicle_texture(Image *vehicle_texture) {

	Image* my_texture = NULL;
	
	int vehicle_texture_flag;
	char image_path[256];

	fprintf(stdout, "You can use your own image. Only .ppm images are supported.\n");
	fprintf(stdout, "Insert path ('no' for default vehicle image) :\n");

	if(scanf("%s",image_path) < 0){
		fprintf(stderr, "fgets error occured!\n");
		exit(EXIT_FAILURE);
	}

	if(strcmp(image_path, "no") == 0) return;
	else {
		char *dot = strrchr(image_path, '.');
		if (dot == NULL || strcmp(dot, ".ppm")!=0){
			fprintf(stderr,"Sorry! Image not found or not supported... \n");
			exit(EXIT_FAILURE);
		}
		my_texture = Image_load(image_path);
		if (my_texture) {
			printf("Done! \n");
			vehicle_texture = my_texture;
		} else {
			fprintf(stderr,"Sorry! Chose image cannot be loaded... \n");
			exit(EXIT_FAILURE);
		}
	}
}

void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world) {

	int numb_of_vehicles = deserialized_wu_packet->num_vehicles;
	
	if(numb_of_vehicles > world->vehicles.size) {
		int i;
		for(i=0; i<numb_of_vehicles; i++) {
			int v_id = deserialized_wu_packet->updates[i].id;
			if(World_getVehicle(world, v_id) == NULL) {
				
				/*
				char buffer[BUFLEN];

				ImagePacket* vehicle_packet = image_packet_init(GetTexture, NULL, v_id);
    			tcp_send(socket_desc , &vehicle_packet->header);
    			
				int ret = tcp_receive(socket_desc , buffer);
    			vehicle_packet = (ImagePacket*) Packet_deserialize(buffer, ret);

				Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle));
			
				Vehicle_init(v,world, v_id, vehicle_packet->image);
				*/

				Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle));
				Vehicle_init(v,world, v_id, Image_load("./arow-right.ppm"));

				World_addVehicle(world, v);
				update_info(world, v_id, 1);
			}
		}
	}
	
	else if(numb_of_vehicles < world->vehicles.size) {
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
				World_detachVehicle(world, v);
				update_info(world, vehicle_id, 0);
			}

			find = 0;
			item=item->next;
		}
	}

	int i;
	for(i=0; i<world->vehicles.size; i++) {
		Vehicle *v = World_getVehicle(world, deserialized_wu_packet->updates[i].id);
		
		v->x = deserialized_wu_packet->updates[i].x;
		v->y = deserialized_wu_packet->updates[i].y;
		v->theta = deserialized_wu_packet->updates[i].theta;
	}
}