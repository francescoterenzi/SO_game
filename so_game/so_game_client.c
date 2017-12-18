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
#include "common.h"


int window;
WorldViewer viewer;
World world;
Vehicle* vehicle;


typedef struct {
  volatile int run;
} UpdaterArgs;

int connectToServer(void);
void *updater_thread(void *arg);
void sendToServer(int socket_desc, PacketHeader* header);
void clear(char* buf, size_t len);
size_t receiveFromServer(int socket_desc, char* msg , size_t buf_len);


/**  TO REMEMBER	
 * Alla fine, prima di chiudere il server bisogna rilasciare la memoria
 *   - void Packet_free(PacketHeader* h);
 *   - void Image_free(Image* img);
 *   - free(...) varie
 * **/


int main(int argc, char **argv) {
	if (argc<3) {
	printf("usage: %s <server_address> <player texture>\n", argv[1]);
	exit(-1);
	}

	printf("loading texture image from %s ... \n", argv[2]);
	Image* my_texture_for_server = Image_load(argv[2]);
	if (my_texture_for_server) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}
	
	int socket = connectToServer(); //initiate a connection on the socket
	
	
	// todo: connect to the server
	//   -get an id
	//   -send your texture to the server (so that all can see you)
	//   -get an elevation map
	//   -get the texture of the surface
	
		
	int ret = 0;	
	
	// GET AN ID	
    IdPacket* id_packet = (IdPacket*)malloc(sizeof(IdPacket));

    PacketHeader id_head;
    id_head.type = GetId;

    id_packet->header = id_head;
    id_packet->id = -1;
    
	char id_packet_buffer[BUFLEN];
	int id_packet_buffer_size = Packet_serialize(id_packet_buffer, &id_packet->header);
	
	while( (ret = send(socket, id_packet_buffer, id_packet_buffer_size, 0)) < 0) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
	}
	
	char id_response[BUFLEN];
	ret = 0;
	
	while( (ret = recv(socket , id_response , sizeof(id_response), 0)) < 0) {  // Receive id response
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read to socket");
	}
	printf("id_size: %d\n", ret);
	
	IdPacket* received_packet = (IdPacket*)Packet_deserialize(id_response, ret); // Id received!
	int my_id = received_packet->id;
	printf("Id received: %d\n", my_id);
	
	
	// SEND YOUR TEXTURE to the server (so that all can see you)
	// server response should assign the surface texture, the surface elevation and the texture to vehicle
	ImagePacket* image_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	
	PacketHeader image_head;
	image_head.type = PostTexture;
	
	image_packet->header = image_head;
	image_packet->id = my_id;
	image_packet->image = my_texture_for_server;
	
	char image_packet_buffer[BUFLEN];
	int image_packet_buffer_size = Packet_serialize(image_packet_buffer, &image_packet->header);
	
	ret = 0;
	while( (ret = send(socket, image_packet_buffer, image_packet_buffer_size, 0)) < 0) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
	}
	printf("client_texture_size: %d\n", ret);
	
	
	/*
	Image* map_elevation;
	Image* map_texture;
	Image* vehicle_texture;
    */
    
    // GET ELEVATION TEXTURE
    ret = 0;  
    char elevation_packet_buf[BUFLEN];
    
    while( (ret = recv(socket, elevation_packet_buf, sizeof(elevation_packet_buf), 0)) < 0) {
            if (errno == EINTR)
                continue;
            ERROR_HELPER(-1, "Cannot read from socket");
    }
	
	printf("elevation_size: %d\n", ret);
	
    ImagePacket* elevation_packet = (ImagePacket*)Packet_deserialize(elevation_packet_buf, ret);
	//Image_save(elevation_packet->image, "./images_test/elevation_packet.pgm");
	
	/*
	if( (elevation_packet->header).type == PostElevation && elevation_packet->id == 0) {
		if(DEBUG) printf(" OK, elevation map received!\n");
	} else {
		if(DEBUG) printf(" ERROR, elevation map not received!\n");
	}
	map_elevation = elevation_packet->image;
	*/
	
	// GET SURFACE TEXTURE
	ret = 0;
	char texture_packet_buf[BUFLEN];	
    while( (ret = recv(socket, texture_packet_buf, sizeof(texture_packet_buf), 0)) < 0) {
            if (errno == EINTR)
                continue;
            ERROR_HELPER(-1, "Cannot read from socket");
    }

	
	printf("surface_size: %d\n", ret);
    
    
    ImagePacket* texture_packet = (ImagePacket*)Packet_deserialize(texture_packet_buf, ret);
	//Image_save(texture_packet->image, "./images_test/texture_packet.pgm");
	/*
	if( (texture_packet->header).type == PostTexture && texture_packet->id == 0) {
		if(DEBUG) printf(" OK, surface texture received!\n");
	} else {
		if(DEBUG) printf(" ERROR, surface texture not received!\n");
	}
    map_texture = texture_packet->image;
    */
    
    // GET VEHICLE TEXTURE
    ret = 0;
	char vehicle_packet_buf[BUFLEN];
	
    while( (ret = recv(socket, vehicle_packet_buf, sizeof(vehicle_packet_buf), 0)) < 0) {
            if (errno == EINTR)
                continue;
            ERROR_HELPER(-1, "Cannot read from socket");
    }
    
    printf("vehicle_size: %d\n", ret);
    
    ImagePacket* vehicle_packet = (ImagePacket*)Packet_deserialize(vehicle_packet_buf, ret);
    //Image_save(vehicle_packet->image, "./images_test/vehicle_packet.pgm");
    
    printf("All packets received\n");
    
    // close the socket
    ret = close(socket);
    ERROR_HELPER(ret, "Cannot close socket");
    
    /*
	if( (vehicle_packet->header).type == PostTexture && vehicle_packet->id > 0) {
		if(DEBUG) printf(" OK, vehicle texture received!\n");
	} else {
		if(DEBUG) printf(" ERROR, vehicle texture not received!\n");
	}
	my_texture_from_server = vehicle_packet->image;
	*/

	
	/**
	//free allocated memory
	Packet_free(&id_packet->header);
	Packet_free(&image_packet->header);
	Packet_free(&texture_packet->header);
	Packet_free(&elevation_packet->header);
	Packet_free(&vehicle_packet->header);
	
	// construct the world
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
	World_addVehicle(&world, vehicle);
	
	
	// spawn a thread that will listen the update messages from
	// the server, and sends back the controls
	// the update for yourself are written in the desired_*_force
	// fields of the vehicle variable
	// when the server notifies a new player has joined the game
	// request the texture and add the player to the pool

	pthread_t runner_thread;
	pthread_attr_t runner_attrs;
	UpdaterArgs runner_args={
		.run=1,
	};
	  
	pthread_attr_init(&runner_attrs);
	pthread_create(&runner_thread, &runner_attrs, updater_thread, &runner_args);
	//WorldViewer_runGlobal(&world, vehicle, &argc, argv);
	//runner_args.run=0;
	
	void* retval;
	pthread_join(runner_thread, &retval);
	
	// cleanup
	//World_destroy(&world);
	*/
	return 0;             
}




void *updater_thread(void *arg) {
	
	UpdaterArgs* _arg = (UpdaterArgs*) arg;

	// creo socket udp
	struct sockaddr_in si_other;
    int ret, s, slen=sizeof(si_other);
    
    s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(UDP_PORT);
    
    inet_aton(SERVER_ADDRESS , &si_other.sin_addr);	

	while(_arg->run) {
		
		VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*)malloc(sizeof(VehicleUpdatePacket));
		PacketHeader v_head;
		v_head.type = VehicleUpdate;

		vehicle_packet->header = v_head;
		vehicle_packet->id = 24;
		vehicle_packet->rotational_force = 9.0;
		vehicle_packet->translational_force = 9.0;
		
		char vehicle_buffer[BUFLEN];
		int vehicle_buffer_size = Packet_serialize(vehicle_buffer, &vehicle_packet->header);
		
		ret = sendto(s, vehicle_buffer, vehicle_buffer_size , 0 , (struct sockaddr *) &si_other, slen);
		ERROR_HELPER(ret, "Cannot send to server");

		
        char world_buffer[BUFLEN];
		ret = recvfrom(s, world_buffer, sizeof(world_buffer), 0, (struct sockaddr *) &si_other, (socklen_t *) &slen);
		ERROR_HELPER(ret, "Cannot recv from server");
		
		//WorldUpdatePacket* deserialized_wu_packet = (WorldUpdatePacket*)Packet_deserialize(world_buffer, sizeof(world_buffer));
		
		sleep(2);
		//usleep(30000);
	}
	return 0;
}




int connectToServer(void){
	int ret;

	// variables for handling a socket
	int socket_desc;
	struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

	// create a socket
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	// set up parameters for the connection
	server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
	server_addr.sin_family      = AF_INET;
	server_addr.sin_port        = htons(TCP_PORT); // network byte order!

	// initiate a connection on the socket
	ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret, "Could not create connection");

	if (DEBUG) fprintf(stderr, "Connection established!\n");  
	
	return socket_desc;
}
