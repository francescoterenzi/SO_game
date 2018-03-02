#include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

// for the udp_socket
#include <arpa/inet.h>
#include <sys/socket.h>

#include "kit.h"

void *udp_handler(void *arg);
void *tcp_client_handler(void *arg);


World world;
Image* surface_texture;
Image* surface_elevation;
Image* vehicle_texture;

int *run_server;

int main(int argc, char **argv) {
	
	if (argc<4) {
	printf("usage: %s <elevation_image> <texture_image> <vehicle_texture>\n", argv[1]);
		exit(-1);
	}

	char* elevation_filename=argv[1];
	char* texture_filename=argv[2];
	char* vehicle_texture_filename=argv[3];
	
	// load the images
	surface_elevation = Image_load(elevation_filename);
	surface_texture = Image_load(texture_filename);
	vehicle_texture = Image_load(vehicle_texture_filename);
	
	if(!surface_elevation || !surface_texture || !vehicle_texture) {
		fprintf(stderr,"Errore nel caricamento delle immagini di default\n");
		exit(EXIT_FAILURE);
	}
	
	// creating the world
	World_init(&world, surface_elevation, surface_texture, 0.5, 0.5, 0.5);
	
	*run_server = 1;
	
	int id = 1, ret, client_desc;
	pthread_t udp_thread;
	
	//Creating udp thread
	ret = pthread_create(&udp_thread, NULL, udp_handler, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot create the udp_thread!");
	
	
	int socket_desc = tcp_server_setup();
	int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()


	// we allocate client_addr dynamically and initialize it to zero
	struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));
	
	welcome_server();
	
	while (*run_server) {		
		
		// accept incoming connection
		client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);
		if (client_desc == -1 && errno == EINTR)
			continue; // check for interruption by signals
		ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");
				
		Vehicle *vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, id, vehicle_texture);
		World_addVehicle(&world, vehicle);

		pthread_t thread;
		thread_args* args = (thread_args*)malloc(sizeof(thread_args));
		args->socket_desc = client_desc;
		args->id = id; //here I set id for the client

		update_info(&world, id, 1);
		
		id++;
		
		ret = pthread_create(&thread, NULL, tcp_client_handler, (void*)args);
		PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

		ret = pthread_detach(thread); 
	    PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");

	}
	
	//Joining udp thread
	ret = pthread_join(udp_thread, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot join the udp_thread!");
	
	exit(EXIT_SUCCESS); 
}



void *tcp_client_handler(void *arg){

	thread_args* args = (thread_args*)arg;
	
	int socket_desc = args->socket_desc;
	char buf[BUFLEN];
    int run = 1;

    while(run) {
		
		int ret = tcp_receive(socket_desc , buf);
		
		if(!ret) run = 0;
		
		else {

			PacketHeader* packet = (PacketHeader*) Packet_deserialize(buf , ret);
			
			switch(packet->type) {
				
				case GetId: { 
					IdPacket* id_packet = id_packet_init(GetId, args->id);
					tcp_send(socket_desc, &id_packet->header); 
					free(id_packet);
					break;
				}
				
				case GetElevation: {  
					ImagePacket* elevation_packet = image_packet_init(PostElevation, surface_elevation , 0);
					tcp_send(socket_desc, &elevation_packet->header);
					free(elevation_packet);
					break;
				}
				
				case GetTexture: {
					
					if(!((ImagePacket*) packet)->id){  
						ImagePacket* texture_packet = image_packet_init(PostTexture, surface_texture , 0);
						tcp_send(socket_desc, &texture_packet->header);
					}
					else { 
						Vehicle *v = World_getVehicle(&world, ((ImagePacket*) packet)->id);
						ImagePacket* texture_packet = image_packet_init(PostTexture, v->texture, ((ImagePacket*) packet)->id);
						tcp_send(socket_desc, &texture_packet->header);
					}
					break;			
				}
				
				case PostTexture: {
					Vehicle *v = World_getVehicle(&world, ((ImagePacket*) packet)->id);
					v->texture = ((ImagePacket*) packet)->image;	
					break;
				}

				default: break;
			}
		}
	}	

	Vehicle *v = World_getVehicle(&world, args->id);

	World_detachVehicle(&world, v);
	update_info(&world, args->id, 0);

	int ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket");
    
    free(args);
    pthread_exit(NULL);

}

void *udp_handler(void *arg) {
	
	struct sockaddr_in si_me, udp_client_addr;
	int udp_socket = udp_server_setup(&si_me);
	
	int res;
	char buffer[BUFLEN];

	while(*run_server) {

		res = udp_receive(udp_socket, &udp_client_addr, buffer);
		VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*)Packet_deserialize(buffer, res);
		
		world_update(vehicle_packet, &world);

		WorldUpdatePacket* world_packet = world_update_init(&world);		
		udp_send(udp_socket, &udp_client_addr, &world_packet->header);
	}
	return NULL;
}
