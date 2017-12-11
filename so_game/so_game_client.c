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

// MACRO
#define BUFLEN 1024
#define UDP_SOCKET_NAME "[UDP]"

// GLOBALI
int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle

// STRUTTURE DATI
typedef struct {
  volatile int run;
} UpdaterArgs;
 
// PROTOTIPI
int connectToServer(void);
void *updater_thread(void *arg);
void sendToServer(int socket_desc, PacketHeader* header);

// MAIN
int main(int argc, char **argv) {
	if (argc<3) {
	printf("usage: %s <server_address> <player texture>\n", argv[1]);
	exit(-1);
	}

	/** ASK TO GRISETTI **/
	printf("loading texture image from %s ... ", argv[2]);
	Image* my_texture = Image_load(argv[2]);
	if (my_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}
	
	/** ASK TO GRISETTI **/
	Image* my_texture_for_server = NULL;
	// todo: connect to the server
	//   -get an id
	//   -send your texture to the server (so that all can see you)
	//   -get an elevation map
	//   -get the texture of the surface

	// these come from the server
	int my_id = -1;
	
	/** VALERIO NON ROMPERE, FINO A QUANDO NON LE USI NON LE VOGLIO COME WARNING NEL MAKE
	Image* map_elevation;
	Image* map_texture;
	Image* my_texture_from_server; //vehicle texture (maybe)
	**/
	
	// get an id
	PacketHeader* id_header = (PacketHeader*)malloc(sizeof(PacketHeader));
	id_header->type = GetId;
	id_header->size = sizeof(*id_header);
	
	IdPacket* id_packet = (IdPacket*)malloc(sizeof(IdPacket));
	id_packet->header = (*id_header);
	id_packet->id = my_id;
	
	//initiate a connection on the socket
	int socket;
	socket = connectToServer();
	
	sendToServer(socket , id_header);
	
	my_id = -1/** id received from server**/ ;
	
	
	// send your texture to the server (so that all can see you)
	// server response should assign the surface texture, the surface elevation and the texture to vehicle
	PacketHeader* image_header = (PacketHeader*)malloc(sizeof(PacketHeader));
	image_header->type = PostTexture;
	image_header->size = sizeof(*image_header);
	
	ImagePacket* image_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	image_packet->header = (*image_header);
	image_packet->id = my_id;
	image_packet->image = my_texture_for_server;
	

	
	// construct the world
	/**
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
	World_addVehicle(&world, vehicle);
	**/
	
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

int connectToServer(void){
	/** Connection to the server **/
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
	
		char *vup = "hello server";
		
		ret = sendto(s, vup, strlen(vup) , 0 , (struct sockaddr *) &si_other, slen);
		ERROR_HELPER(ret, "Cannot send to server");

		
		char buf[1024];
		ret = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *) &si_other, (socklen_t *) &slen);
		ERROR_HELPER(ret, "Cannot recv from server");
		
		sleep(2);
		//usleep(30000);
	}
	return 0;
}

void sendToServer(int socket_desc, PacketHeader* header){
	// converts a well formed packet into a string in dest.
	// returns the written bytes
	// h is the packet to write
	//int Packet_serialize(char* dest, const PacketHeader* h);
	int ret;
	char to_send[1024];
	int len =  Packet_serialize(to_send, header);

	while ((ret = send(socket_desc, to_send, len, 0)) < 0){
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot send msg to the server");
    }
    
    //if(DEBUG) printf("Message sent\n");
}

/** SE SERVE LA RIMETTIAMO SU
void keyPressed(unsigned char key, int x, int y){
	switch(key){ 
	  case 27: 
		glutDestroyWindow(window); 
		exit(0); 
	  case ' ': 
		vehicle->translational_force_update = 0; 
		vehicle->rotational_force_update = 0; 
		break; 
	  case '+': 
		viewer.zoom *= 1.1f; 
		break; 
	  case '-': 
		viewer.zoom /= 1.1f; 
		break; 
	  case '1': 
		viewer.view_type = Inside; 
		break; 
	  case '2': 
		viewer.view_type = Outside; 
		break; 
	  case '3': 
		viewer.view_type = Global; 
		break; 
	  } 
}


void specialInput(int key, int x, int y) {
  switch(key){
  case GLUT_KEY_UP:
    vehicle->translational_force_update += 0.1;
    break;
  case GLUT_KEY_DOWN:
    vehicle->translational_force_update -= 0.1;
    break;
  case GLUT_KEY_LEFT:
    vehicle->rotational_force_update += 0.1;
    break;
  case GLUT_KEY_RIGHT:
    vehicle->rotational_force_update -= 0.1;
    break;
  case GLUT_KEY_PAGE_UP:
    viewer.camera_z+=0.1;
    break;
  case GLUT_KEY_PAGE_DOWN:
    viewer.camera_z-=0.1;
    break;
  }
}


void display(void) {
  WorldViewer_draw(&viewer);
}


void reshape(int width, int height) {
	WorldViewer_reshapeViewport(&viewer, width, height);
}

void idle(void) {
	World_update(&world);
	usleep(30000);
	glutPostRedisplay();

	// decay the commands
	vehicle->translational_force_update *= 0.999;
	vehicle->rotational_force_update *= 0.7;
}**/
