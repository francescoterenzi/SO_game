#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>  // htons() and inet_addr()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "common.h"
#include "so_game_protocol.h"
#include "packet.h"
#include "socket.h"


WorldViewer viewer;
World world;
Vehicle* vehicle;


typedef struct {
  Image *texture;
  volatile int run;
  int id;
  int tcp_desc;
} UpdaterArgs;

int connectToServer(void);
void *updater_thread(void *arg);
void clear(char* buf);
void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc);

int main(int argc, char **argv) {
	if (argc<2) {
		printf("usage: %s <server_address>\n", argv[1]);
		exit(-1);
	}
	Image* my_texture;
	Image* my_texture_for_server;
	int vehicle_texture_flag;
	char image_path[256];
	int ret;
	
	fprintf(stdout, "You can use your own image. Only .ppm images are supported.\n");
	fprintf(stdout, "Insert path ('no' for default vehicle image) :\n");
	
	if(scanf("%s",image_path) < 0){
		fprintf(stderr, "fgets error occured!\n");
		exit(EXIT_FAILURE);
	}
	
	if(strcmp(image_path, "no") == 0) vehicle_texture_flag = 0;
	else {
		char *dot = strrchr(image_path, '.');
		if (dot == NULL || strcmp(dot, ".ppm")!=0){
			fprintf(stderr,"Sorry! Image not found or not supported... \n");
			exit(EXIT_FAILURE);
		}
		my_texture = Image_load(image_path);
		if (my_texture) {
			printf("Done! \n");
			my_texture_for_server = my_texture;
			vehicle_texture_flag = 1;
		} else {
			fprintf(stderr,"Sorry! Chose image cannot be loaded... \n");
			exit(EXIT_FAILURE);
		}
	}

	// these come from the server
	int my_id = -1;
	Image* map_elevation;
	Image* map_texture;
	Image* my_texture_from_server; //vehicle texture
	
	char* buf = (char*)malloc(sizeof(char) * BUFLEN);
	
	int socket_desc = tcp_client_setup();	//initiate a connection on the socket	
	
	// REQUEST AND GET AN ID
	clear(buf);
	IdPacket* id_packet = id_packet_init(GetId, my_id);	

	tcp_send(socket_desc , &id_packet->header);	     // Requesting id
	ret = tcp_receive(socket_desc , buf);          // Receiving id
	
	IdPacket* received_packet = (IdPacket*)Packet_deserialize(buf, ret); // Id received!
	my_id = received_packet->id;
	
	if(DEBUG) printf("Id received : %d\n", my_id);



    // REQUEST AND GET ELEVATION MAP    
    clear(buf);
    ImagePacket* elevationImage_packet = image_packet_init(GetElevation, NULL, 0);
    tcp_send(socket_desc , &elevationImage_packet->header);
       
    ret = tcp_receive(socket_desc , buf);
    
    ImagePacket* elevation_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	if( (elevation_packet->header).type == PostElevation && elevation_packet->id == 0) {
		if(DEBUG) printf("%s ELEVATION MAP RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, elevation map not received!\n");
		exit(EXIT_FAILURE);
	}
	map_elevation = elevation_packet->image;
	
	
	
	// REQUEST AND GET SURFACE TEXTURE
	clear(buf);
	ImagePacket* surfaceTexture_packet = image_packet_init(GetTexture, NULL, 0);
    tcp_send(socket_desc , &surfaceTexture_packet->header);
	
    ret = tcp_receive(socket_desc , buf); 

    ImagePacket* texture_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	if( (texture_packet->header).type == PostTexture && texture_packet->id == 0) {
		if(DEBUG) printf("%s SURFACE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, surface texture not received!\n");
		exit(EXIT_FAILURE);
	}
    map_texture = texture_packet->image;



    // GET VEHICLE TEXTURE
	clear(buf);
	ImagePacket* vehicleTexture_packet;
	
	if(!vehicle_texture_flag) vehicleTexture_packet = image_packet_init(GetTexture, NULL, my_id); // client chose default vehicle image
	else vehicleTexture_packet = image_packet_init(PostTexture, my_texture_for_server, my_id);    // client chose to use his own image
	tcp_send(socket_desc , &vehicleTexture_packet->header);
	
	ret = tcp_receive(socket_desc , buf);
	
	ImagePacket* vehicle_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	if( (vehicle_packet->header).type == PostTexture && vehicle_packet->id > 0) {
		if(DEBUG) printf("%s VEHICLE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, vehicle texture not received!\n");
		exit(EXIT_FAILURE);
	}
	my_texture_from_server = vehicle_packet->image;
	
	if(DEBUG) printf("%s ALL TEXTURES RECEIVED\n", TCP_SOCKET_NAME);
	
	// construct the world
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	
	Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
	World_addVehicle(&world, vehicle);
	
	
	/*** UDP PART NOTIFICATION ***/
	pthread_t runner_thread;
	pthread_attr_t runner_attrs;
	UpdaterArgs runner_args={
		.texture = my_texture_from_server,
		.run=1,
		.id = my_id,
		.tcp_desc = socket_desc
	};
	  
	pthread_attr_init(&runner_attrs);
	pthread_create(&runner_thread, &runner_attrs, updater_thread, &runner_args);
	
	WorldViewer_runGlobal(&world, vehicle, &argc, argv);
	runner_args.run=0;
	
	void* retval;
	pthread_join(runner_thread, &retval);
	

	World_destroy(&world);

	//free allocated memory
	Packet_free(&id_packet->header);
	Packet_free(&received_packet->header);
	
	Packet_free(&surfaceTexture_packet->header);
	Packet_free(&texture_packet->header);
	
	Packet_free(&elevationImage_packet->header);
	Packet_free(&elevation_packet->header);
	
	Packet_free(&vehicleTexture_packet->header);
	Packet_free(&vehicle_packet->header);
	free(buf);
	
	return 0;             
}

void *updater_thread(void *args) {
	
	UpdaterArgs* arg = (UpdaterArgs*) args;

	// creo socket udp
	struct sockaddr_in si_other;
	int udp_socket = udp_client_setup(&si_other);

    int ret;

	char buffer[BUFLEN];
    
	while(arg->run) {

		// create vehicle_packet
		VehicleUpdatePacket* vehicle_packet = vehicle_update_init(&world, arg->id, vehicle->rotational_force_update, vehicle->translational_force_update);
		udp_send(udp_socket, &si_other, &vehicle_packet->header);
		
        clear(buffer); 	// possiamo sempre usare lo stesso per ricevere
		
		ret = udp_receive(udp_socket, &si_other, buffer);
		WorldUpdatePacket* wu_packet = (WorldUpdatePacket*)Packet_deserialize(buffer, ret);		
		client_update(wu_packet, arg->tcp_desc);
 		
		usleep(30000);
	}

	return 0;
}

void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc) {

	int numb_of_vehicles = deserialized_wu_packet->num_vehicles;
	int world_size = world.vehicles.size;
	
	if(numb_of_vehicles > world_size) {
		int i;
		for(i=0; i<numb_of_vehicles; i++) {
			int v_id = deserialized_wu_packet->updates[i].id;
			if(World_getVehicle(&world, v_id) == NULL) {
	
				char buffer[BUFLEN];

				ImagePacket* vehicle_packet = image_packet_init(GetTexture, NULL, v_id);
    			tcp_send(socket_desc , &vehicle_packet->header);
    			
				int ret = tcp_receive(socket_desc , buffer);
    			vehicle_packet = (ImagePacket*) Packet_deserialize(buffer, ret);

				Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle));
				Vehicle_init(v,&world, v_id, vehicle_packet->image);
				
				World_addVehicle(&world, v);
				printf("%s new vehicle added with id: %d\n", UDP_SOCKET_NAME, v_id);
			} 
		}
	}
	
	else if(numb_of_vehicles < world_size) {
		ListItem* item=world.vehicles.first;
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
				World_detachVehicle(&world, v);
			}

			find = 0;
			item=item->next;
		}
	}

	int i;
	for(i=0; i<world_size; i++) {
		Vehicle *v = World_getVehicle(&world, deserialized_wu_packet->updates[i].id);
		
		v->x = deserialized_wu_packet->updates[i].x;
		v->y = deserialized_wu_packet->updates[i].y;
		v->theta = deserialized_wu_packet->updates[i].theta;
	}
}
