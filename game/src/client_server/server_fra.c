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

#include "common.h"
#include "kit.h"
#include "../image/image.h"
#include "../surface/surface.h"
#include "../world/world.h"
#include "../vehicle/vehicle.h"
#include "../world/world_viewer.h"
#include "../protocol/so_game_protocol.h"
#include "../packet/packet.h"
#include "../socket/socket.h"


void *tcp_handler(void *arg);
void *udp_handler(void *arg);
void *tcp_client_handler(void *arg);


World world;
Image* surface_texture;
Image* surface_elevation;
Image* vehicle_texture;

int id;

int main(int argc, char **argv) {
	
	if (argc < 4) {
	printf("usage: %s <elevation_image> <texture_image> <vehicle_texture> \n", argv[1]);
		exit(-1);
	}

	char* elevation_filename = argv[1];
	char* texture_filename = argv[2];
	char* vehicle_texture_filename = argv[3];
	
	// load the images
	surface_elevation = Image_load(elevation_filename);
	surface_texture = Image_load(texture_filename);
	vehicle_texture = Image_load(vehicle_texture_filename);

	if(!surface_elevation || !surface_texture || !vehicle_texture) {
		printf("Errore nel caricamento delle immagini di default\n");
		exit(1);
	}
	
	
	// creating the world
	World_init(&world, surface_elevation, surface_texture, 0.5, 0.5, 0.5);
	
	int ret;
	pthread_t tcp_thread;
	pthread_t udp_thread;
	
	//Creating tcp socket..
	ret = pthread_create(&tcp_thread, NULL, tcp_handler, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot create the tcp_thread!");
	
	//Creating udp socket..
	ret = pthread_create(&udp_thread, NULL, udp_handler, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot create the udp_thread!");
	
	//Joining udp and tcp sockets.
	ret = pthread_join(tcp_thread, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot join the tcp_thread!");
	ret = pthread_join(udp_thread, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot join the udp_thread!");
	
	exit(EXIT_SUCCESS); 
}

void *tcp_handler(void *arg) {
	int ret, client_desc;

	int socket_desc = tcp_server_setup();
	int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()


	// we allocate client_addr dynamically and initialize it to zero
	struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));

	id = 1;
	
	welcome_server();
	
	while (1) {		
		
		// accept incoming connection
		client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);
		if (client_desc == -1 && errno == EINTR)
			continue; // check for interruption by signals
		ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");
		
		pthread_t thread;
		thread_args* args = (thread_args*)malloc(sizeof(thread_args));
		args->socket_desc = client_desc;
		args->id = id; //here I set id for the client

		id++;
		
		ret = pthread_create(&thread, NULL, tcp_client_handler, (void*)args);
		PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

		ret = pthread_detach(thread); 
	    PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");

	}
	
	return NULL;

}

void *tcp_client_handler(void *arg){

	thread_args* args = (thread_args*)arg;
	
	int socket_desc = args->socket_desc;

    int ret, run = 1;
    char* buf = (char*) malloc(sizeof(char)*BUFLEN);
    
    PacketHeader* packet_from_client;
	IdPacket* id_packet;
	ImagePacket* elevation_packet;
	ImagePacket* texture_packet;
	ImagePacket* image_client_packet;
    
    while(run) {		
		clear(buf);		
		ret = tcp_receive(socket_desc , buf);

		packet_from_client = (PacketHeader*)Packet_deserialize(buf , ret);

		if(packet_from_client->type == GetId) {
			//client requested the id			
			id_packet = id_packet_init(GetId, args->id);
						
			tcp_send(socket_desc, &id_packet->header); 
			//if(DEBUG) printf("%s ASSIGNED ID TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
			free(id_packet);
		}
		
		else if(packet_from_client->type == GetElevation) {  
			//client requested the elevation map
			elevation_packet = image_packet_init(PostElevation, surface_elevation , 0);
			
			//if(DEBUG) printf("%s SENDING ELEVATION TEXTURE TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
			tcp_send(socket_desc, &elevation_packet->header);
			free(elevation_packet);
			
		}
		
		else if(packet_from_client->type == GetTexture) {
			ImagePacket* packet = (ImagePacket*)packet_from_client;
			
			if(packet->id == 0){  
				// Client requested surface texture
				texture_packet = image_packet_init(PostTexture, surface_texture , 0);
							
				//if(DEBUG) printf("%s SENDING SURFACE TEXTURE TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
				tcp_send(socket_desc, &texture_packet->header);
				free(texture_packet);
			}
			else if(packet->id > 0){ 
				// Client requested vehicle texture
				texture_packet = image_packet_init(PostTexture, vehicle_texture, packet->id);
				
				//if(DEBUG) printf("%s SENDING VECHICLE TEXTURE TO CLIENT %d\n", TCP_SOCKET_NAME, texture_packet->id);
				tcp_send(socket_desc, &texture_packet->header);
				free(texture_packet);
				run = 0;
			}			
		}
		
		else if(packet_from_client->type == PostTexture){

			image_client_packet = (ImagePacket*)packet_from_client;
			
			vehicle_texture = image_client_packet->image;
			free(image_client_packet);
			run = 0;		
		}
		
		free(packet_from_client);
	}

	//if(DEBUG) printf("%s ALL TEXTURES SENT TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
	
	
	// INSERISCO IL CLIENT NEL MONDO
	Vehicle *vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	Vehicle_init(vehicle, &world, args->id, vehicle_texture);
	World_addVehicle(&world, vehicle);

	update_info(&world, args->id, 1);
	


	//qui verifichiamo se il client chiude la connessione
	
	int connected = 1;
	while(connected) {
		ret = tcp_receive(socket_desc, NULL);
		if(ret == 0) connected = 0;
	}

	World_detachVehicle(&world, vehicle);
	update_info(&world, args->id, 0);

	ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket for incoming connection");    
	
    free(args);
    free(buf);
    pthread_exit(NULL);

}

void *udp_handler(void *arg) {
	
	struct sockaddr_in si_me, udp_client_addr;
	int udp_socket = udp_server_setup(&si_me);
	
	int res;
	char buffer[BUFLEN];

	while(1) {

		clear(buffer);
		res = udp_receive(udp_socket, &udp_client_addr, buffer);

		VehicleUpdatePacket* deserialized_vehicle_packet = (VehicleUpdatePacket*)Packet_deserialize(buffer, res);
		world_update(deserialized_vehicle_packet, &world);
		WorldUpdatePacket* world_packet = world_update_init(&world);	
		
		udp_send(udp_socket, &udp_client_addr, &world_packet->header);
	}
	return NULL;
}
