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

#define SERVER_PORT 3000
#define UDP_BUFLEN 512
#define  MAX_CONN_QUEUE 20

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "common.h"
#include "linked_list.h"

World world;
int id;

void *client_handler(void *arg){
	return NULL;
}

int main(int argc, char **argv) {
	if (argc<3) {
	printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
		exit(-1);
	}
	char* elevation_filename=argv[1];
	char* texture_filename=argv[2];
	char* vehicle_texture_filename="./images/arrow-right.ppm";
	printf("loading elevation image from %s ... ", elevation_filename);

	// load the images
	Image* surface_elevation = Image_load(elevation_filename);
	if (surface_elevation) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}


	printf("loading texture image from %s ... ", texture_filename);
	Image* surface_texture = Image_load(texture_filename);
	if (surface_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}

	printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
	Image* vehicle_texture = Image_load(vehicle_texture_filename);
	if (vehicle_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}



	/*** TCP SERVER ***/
	int socket_desc, client_desc;
	int ret;
	
	// some fields are required to be filled with 0
	struct sockaddr_in server_addr = {0};

	int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

	// initialize socket for listening
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	//ERROR_HELPER(socket_desc, "Could not create socket");

	server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT); // network byte order!

	// We enable SO_REUSEADDR to quickly restart our server after a crash
	int reuseaddr_opt = 1;
	ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
	
	//ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

	// bind address to socket
	ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);
	//ERROR_HELPER(ret, "Cannot bind address to socket");
	if(ret >= 0) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}

	// start listening
	ret = listen(socket_desc, MAX_CONN_QUEUE);
	//ERROR_HELPER(ret, "Cannot listen on socket");

	// we allocate client_addr dynamically and initialize it to zero
	struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));

	id = 0;

	while (1) {
		// accept incoming connection
		client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);
		if (client_desc == -1 && errno == EINTR)
			continue; // check for interruption by signals
		//ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

		//if (DEBUG)
			//fprintf(stderr, "Incoming connection accepted...\n");

		pthread_t thread;

		ret = pthread_create(&thread, NULL, client_handler, NULL);
		//PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

		//if (DEBUG)
			//fprintf(stderr, "New thread created to handle the request!\n");

		ret = pthread_detach(thread); // I won't phtread_join() on this thread
		//PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");

	}

	/** UDP SERVER
	* Single thread implementation, if needed I can change it in a multi-thread implemetation.
	* A error handler is required!
	**/

	struct sockaddr_in si_me, udp_client_addr;
	char buf[UDP_BUFLEN];
	int udp_socket, res, udp_sockaddr_len = sizeof(udp_client_addr);

	// create the socket
	printf("Creating the udp_socket...\n");
	udp_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(udp_socket >= 0) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}

	// zero the memory
	memset((char *) &si_me, 0, sizeof(si_me));

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(SERVER_PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);


	//bind the socket to port
	printf("Binding the socket to port %d\n", SERVER_PORT);
	res = bind(udp_socket , (struct sockaddr*)&si_me, sizeof(si_me));
	if(res >= 0) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}

	//Listening on port 3000
	while(1) {

		printf("Waiting for data...\n");
		res = recvfrom(udp_socket, buf, UDP_BUFLEN, 0, (struct sockaddr *) &udp_client_addr, (socklen_t *) &udp_sockaddr_len);

		if(res >= 0) {
			// it should be in this form <timestamp, translational acceleration, rotational acceleration>
			printf("Received packet from %s:%d\n", inet_ntoa(udp_client_addr.sin_addr), ntohs(udp_client_addr.sin_port));
			printf("Data: %s\n" , buf);
		}
		else {
			printf("recv failed\n");
			continue;
		}
	}

	exit(EXIT_SUCCESS); // this will never be executed
}



	// not needed here
	// construct the world
	// World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

	// // create a vehicle
	// vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	// Vehicle_init(vehicle, &world, 0, vehicle_texture);

	// // add it to the world
	// World_addVehicle(&world, vehicle);



	// // initialize GL
	// glutInit(&argc, argv);
	// glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	// glutCreateWindow("main");

	// // set the callbacks
	// glutDisplayFunc(display);
	// glutIdleFunc(idle);
	// glutSpecialFunc(specialInput);
	// glutKeyboardFunc(keyPressed);
	// glutReshapeFunc(reshape);

	// WorldViewer_init(&viewer, &world, vehicle);


	// // run the main GL loop
	// glutMainLoop();

	// // check out the images not needed anymore
	// Image_free(vehicle_texture);
	// Image_free(surface_texture);
	// Image_free(surface_elevation);

	// // cleanup
	// World_destroy(&world);

