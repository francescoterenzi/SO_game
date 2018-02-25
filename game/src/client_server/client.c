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

WorldViewer viewer;
World world;
Vehicle* vehicle;


void *updater_thread(void *arg);

int main(int argc, char **argv) {

	if (argc<2) {
		printf("usage: %s <server_address>\n", argv[1]);
		exit(-1);
	}

	int ret;

	// setup
	int my_id = -1;
	Image* map_elevation = NULL;
	Image* map_texture = NULL;
	Image* vehicle_texture = NULL;
	char* buf = (char*)malloc(sizeof(char) * BUFLEN);
	int socket_desc = tcp_client_setup();
	
	// REQUEST AND GET AN ID
	clear(buf);
	IdPacket* id_packet = id_packet_init(GetId, my_id);	
	tcp_send(socket_desc , &id_packet->header);	     // Requesting id
	ret = tcp_receive(socket_desc , buf);          // Receiving id
	
	IdPacket* received_packet = (IdPacket*)Packet_deserialize(buf, ret); // Id received!
	my_id = received_packet->id;

	welcome_client(my_id);
	get_vehicle_texture(vehicle_texture);
	printf("\n\nJoin the game!! ***\n\n");

    // REQUEST AND GET ELEVATION MAP    
    clear(buf);
    ImagePacket* elevationImage_packet = image_packet_init(GetElevation, NULL, 0);
    tcp_send(socket_desc , &elevationImage_packet->header);
       
    ret = tcp_receive(socket_desc , buf);
    
    ImagePacket* elevation_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	/*
	if( (elevation_packet->header).type == PostElevation && elevation_packet->id == 0) {
		if(DEBUG) printf("%s ELEVATION MAP RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, elevation map not received!\n");
		exit(EXIT_FAILURE);
	}
	*/

	map_elevation = elevation_packet->image;
	
	
	
	// REQUEST AND GET SURFACE TEXTURE
	clear(buf);
	ImagePacket* surfaceTexture_packet = image_packet_init(GetTexture, NULL, 0);
    tcp_send(socket_desc , &surfaceTexture_packet->header);
	
    ret = tcp_receive(socket_desc , buf); 

    ImagePacket* texture_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	/*
	if( (texture_packet->header).type == PostTexture && texture_packet->id == 0) {
		if(DEBUG) printf("%s SURFACE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, surface texture not received!\n");
		exit(EXIT_FAILURE);
	}
	*/
    map_texture = texture_packet->image;



    // GET VEHICLE TEXTURE
	clear(buf);
	ImagePacket* vehicleTexture_packet = NULL;
	if(vehicle_texture) {
		vehicleTexture_packet = image_packet_init(PostTexture, vehicle_texture, my_id);    // client chose to use his own image
		tcp_send(socket_desc , &vehicleTexture_packet->header);
	}
	else {

		vehicleTexture_packet = image_packet_init(GetTexture, NULL, my_id);    // client chose to use his own image
		tcp_send(socket_desc , &vehicleTexture_packet->header);

		ret = tcp_receive(socket_desc , buf);	
		ImagePacket* vehicle_packet = (ImagePacket*)Packet_deserialize(buf, ret);
		
		/*
		if( (vehicle_packet->header).type == PostTexture && vehicle_packet->id > 0) {
			if(DEBUG) printf("%s VEHICLE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
		} else {
			if(DEBUG) printf(" ERROR, vehicle texture not received!\n");
			exit(EXIT_FAILURE);
		}
		*/
		vehicle_texture = vehicle_packet->image;
	}
	
	//if(DEBUG) printf("%s ALL TEXTURES RECEIVED\n", TCP_SOCKET_NAME);
	
	// construct the world
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	
	Vehicle_init(vehicle, &world, my_id, vehicle_texture);
	World_addVehicle(&world, vehicle);
	
	
	/*** UDP PART NOTIFICATION ***/
	pthread_t runner_thread;
	pthread_attr_t runner_attrs;
	UpdaterArgs runner_args={
		.texture = vehicle_texture,
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
	Packet_free(&vehicleTexture_packet->header);

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
		client_update(wu_packet, arg->tcp_desc, &world);
 		
		usleep(30000);
	}

	return 0;
}
