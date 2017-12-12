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
	Image* my_texture = Image_load(argv[2]);
	if (my_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}
	
	
	///Image* my_texture_for_server = NULL; *BHO*
	
	// todo: connect to the server
	//   -get an id
	//   -send your texture to the server (so that all can see you)
	//   -get an elevation map
	//   -get the texture of the surface

	// these come from the server
	int my_id = -1;
	Image* map_elevation;
	Image* map_texture;
	Image* my_texture_from_server; //vehicle texture (maybe)
	
	int ret;
	
	
	// GET AN ID
	PacketHeader* id_header = (PacketHeader*)malloc(sizeof(PacketHeader));
	id_header->type = GetId;
	
	IdPacket* id_packet = (IdPacket*)malloc(sizeof(IdPacket));
	id_packet->header = (*id_header);
	id_packet->id = my_id;
	
	
	int socket;
	socket = connectToServer(); //initiate a connection on the socket
	char buf[BUFLEN];
	size_t buf_len = sizeof(buf);
	
	sendToServer(socket , &id_packet->header);	// Send id request
	
	ret = receiveFromServer(socket , buf , buf_len);  // Receive id response
	
	Packet_free(&id_packet->header);
	
	IdPacket* received_packet = (IdPacket*)Packet_deserialize(buf, ret); // Id received!
	my_id = received_packet->id;
	
	if(DEBUG) printf("Id received : %d\n", my_id);
	
	
	// SEND YOUR TEXTURE to the server (so that all can see you)
	// server response should assign the surface texture, the surface elevation and the texture to vehicle
	PacketHeader* image_header = (PacketHeader*)malloc(sizeof(PacketHeader));
	image_header->type = PostTexture;
	
	ImagePacket* image_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	image_packet->header = (*image_header);
	image_packet->id = my_id;
	image_packet->image = my_texture;
	
	sendToServer(socket , &image_packet->header);
	Packet_free(image_header);
	
	
	
	
	
	// GET SURFACE TEXTURE
	memset(buf , '\0', sizeof(char)*BUFLEN);
	
    ret = receiveFromServer(socket , buf , buf_len);  
	
	ImagePacket* texture_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	if( (texture_packet->header).type == PostTexture && texture_packet->id == 0) {
		if(DEBUG) printf(" OK, surface texture received!\n");
	} else {
		if(DEBUG) printf(" ERROR, surface texture not received!\n");
	}
    map_texture = texture_packet->image;
    
    ///if(DEBUG) printf(" map_texture: %s\n", map_texture->data);
    
    
    
    // GET ELEVATION MAP    
    memset(buf , 0, sizeof(char)*BUFLEN);
    ret = receiveFromServer(socket , buf , buf_len);  // ***SI BLOCCA SU QUESTA RECEIVE*** //
	
	ImagePacket* elevation_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	if( (elevation_packet->header).type == PostElevation && elevation_packet->id == 0) {
		if(DEBUG) printf(" OK, elevation map received!\n");
	} else {
		if(DEBUG) printf(" ERROR, elevation map not received!\n");
	}
	map_elevation = elevation_packet->image;
    
    
    
    // GET VEHICLE TEXTURE
	memset(buf , '\0', sizeof(char)*BUFLEN);
    ret = receiveFromServer(socket , buf , buf_len);  
	
	ImagePacket* vehicle_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	if( (vehicle_packet->header).type == PostTexture && vehicle_packet->id > 0) {
		if(DEBUG) printf(" OK, vehicle texture received!\n");
	} else {
		if(DEBUG) printf(" ERROR, vehicle texture not received!\n");
	}
	my_texture_from_server = vehicle_packet->image;
	
	
	
	
	
	if(DEBUG) printf(" map_texture: %s\n", map_texture->data);
	if(DEBUG) printf(" map_elevation: %s\n", map_elevation->data);
	
	
	
	
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
	/*FILLME*/
	
	
	/*** UDP PART NOTIFICATION ***/
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
    si_other.sin_port = htons(SERVER_PORT);
    
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
		
		WorldUpdatePacket* deserialized_wu_packet = (WorldUpdatePacket*)Packet_deserialize(world_buffer, sizeof(world_buffer));
		
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
	server_addr.sin_port        = htons(SERVER_PORT); // network byte order!

	// initiate a connection on the socket
	ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret, "Could not create connection");

	if (DEBUG) fprintf(stderr, "Connection established!\n");  
	
	return socket_desc;
}



void sendToServer(int socket_desc, PacketHeader* packet){
	int ret;
	char to_send[BUFLEN];
	int len =  Packet_serialize(to_send, packet);

	while ((ret = send(socket_desc, to_send, len , 0)) < 0){
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot send msg to the server");
    }
    //if(DEBUG) printf("Message sent\n");
}


size_t receiveFromServer(int socket_desc, char* msg , size_t buf_len){
	int ret;
	
	while( (ret = recv(socket_desc, msg , buf_len - 1, 0)) < 0 ) {
		if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot receive from server");
	}
	msg[ret] = '\0';
	return ret; //number of bytes received
}
