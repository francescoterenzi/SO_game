#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "client_kit.h"

void welcome_client(int id) {
	fprintf(stdout, "\n\nConnect to OS SEREVR ip:[%s] port:[%d] with id:[%d] ***\n", 
				SERVER_ADDRESS , TCP_PORT, id);
	fprintf(stdout,"\nJoin the game!! ***\n\n");
	fflush(stdout);
}

Image* get_vehicle_texture() {

	Image* my_texture;
	char image_path[256];
	fprintf(stdout, "\nOPERATING SYSTEM PROJECT 2018 - CLIENT SIDE ***\n\n");
	fflush(stdout);
	fprintf(stdout, "You will be soon connected to the game server.\n");
	fprintf(stdout, "First, you can choose to use your own image. Only .ppm images are supported.\n");
	fprintf(stdout, "Insert path ('no' for default vehicle image) :\n");

	if(scanf("%s",image_path) < 0){
		fprintf(stderr, "Error occured!\n");
		exit(EXIT_FAILURE);
	}

	if(strcmp(image_path, "no") == 0) return NULL;
	else {
		char *dot = strrchr(image_path, '.');
		if (dot == NULL || strcmp(dot, ".ppm")!=0){
			fprintf(stderr,"Sorry! Image not found or not supported... \n");
			exit(EXIT_FAILURE);
		}
		my_texture = Image_load(image_path);
		if (my_texture) {
			printf("Done! \n");
			return my_texture;
		} else {
			fprintf(stderr,"Sorry! Chose image cannot be loaded... \n");
			exit(EXIT_FAILURE);
		}
	}
	return NULL; // will never be reached
}



void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world) {

	int numb_of_vehicles = deserialized_wu_packet->num_vehicles;
	
	if(numb_of_vehicles > world->vehicles.size) {
		int i;
		for(i=0; i<numb_of_vehicles; i++) {
			int v_id = deserialized_wu_packet->updates[i].id;
			if(World_getVehicle(world, v_id) == NULL) {
	
				char buffer[BUFLEN];

				ImagePacket* vehicle_packet = image_packet_init(GetTexture, NULL, v_id);
    			tcp_send(socket_desc , &vehicle_packet->header);
    			
				char msg[2048];
				sprintf(msg, "./image_%d", v_id);

				int ret = tcp_receive(socket_desc , buffer);
    			vehicle_packet = (ImagePacket*) Packet_deserialize(buffer, ret);

				Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle));
				Vehicle_init(v,world, v_id, vehicle_packet->image);
				World_addVehicle(world, v);

				/*
				Image_save(vehicle_packet->image, msg);
				update_info(world, v_id, 1);
				*/
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
	for(i = 0 ; i < world->vehicles.size ; i++) {
		Vehicle *v = World_getVehicle(world, deserialized_wu_packet->updates[i].id);
		
		v->x = deserialized_wu_packet->updates[i].x;
		v->y = deserialized_wu_packet->updates[i].y;
		v->theta = deserialized_wu_packet->updates[i].theta;
	}
}

void *updater_thread(void *args) {
	
	UpdaterArgs* arg = (UpdaterArgs*) args;

	// variables
	int id = arg->id;
	World *world = arg->world;
	Vehicle *vehicle = arg->vehicle;

	// creo socket udp
	struct sockaddr_in si_other;
	int udp_socket = udp_client_setup(&si_other);

    int ret;

	char buffer[BUFLEN];
    
	while(arg->run) {

		float r_f_update = vehicle->rotational_force_update;
		float t_f_update = vehicle->translational_force_update;

		// create vehicle_packet
		VehicleUpdatePacket* vehicle_packet = vehicle_update_init(world, id, r_f_update, t_f_update);
		udp_send(udp_socket, &si_other, &vehicle_packet->header);
		
        clear(buffer); 	// possiamo sempre usare lo stesso per ricevere
		
		ret = udp_receive(udp_socket, &si_other, buffer);
		WorldUpdatePacket* wu_packet = (WorldUpdatePacket*)Packet_deserialize(buffer, ret);		
		client_update(wu_packet, arg->tcp_desc, world);
 		
		usleep(30000);
	}

	return 0;
}

