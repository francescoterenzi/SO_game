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

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "common.h"
#include "linked_list.h"
#include "so_game_protocol.h"


void *tcp_handler(void *arg);
void *udp_handler(void *arg);
void *tcp_client_handler(void *arg);
void sendToClient(int socket_desc, PacketHeader* packet);
int receiveFromClient(int socket_desc , char* msg);
void clear(char* buf);


typedef struct thread_args {
	int id;
	int socket_desc;	
} thread_args;

World world;
Image* surface_texture;
Image* surface_elevation;
Image* vehicle_texture;

int id;

int main(int argc, char **argv) {
	
	if (argc<3) {
	printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
		exit(-1);
	}

	char* elevation_filename=argv[1];
	char* texture_filename=argv[2];
	char* vehicle_texture_filename="./images/arrow-right.ppm";
	
	// load the images
	surface_elevation = Image_load(elevation_filename);
	surface_texture = Image_load(texture_filename);
	vehicle_texture = Image_load(vehicle_texture_filename);
	
	
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
	
	int ret;
	int socket_desc, client_desc;
	
	// some fields are required to be filled with 0
	struct sockaddr_in server_addr = {0};

	int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

	// initialize socket for listening
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(TCP_PORT); // network byte order!

	// We enable SO_REUSEADDR to quickly restart our server after a crash
	int reuseaddr_opt = 1;
	ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
	ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

	// bind address to socket
	ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);
	ERROR_HELPER(ret, "Cannot bind address to socket");
	
	// start listening
	ret = listen(socket_desc, MAX_CONN_QUEUE);
	ERROR_HELPER(ret, "Cannot listen on socket");

	// we allocate client_addr dynamically and initialize it to zero
	struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));

	id = 1;

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

    int ret;
    char* buf = (char*) malloc(BUFLEN);
    clear(buf);
	
	// receiving id request
	ret = receiveFromClient(socket_desc , buf);
	IdPacket* packet_from_client = (IdPacket*)Packet_deserialize(buf , ret);
	
	// Send to the client the assigned id
	IdPacket* to_send = (IdPacket*)malloc(sizeof(IdPacket));
	to_send->header = packet_from_client->header;
	to_send->id = args->id;	
	
	sendToClient(socket_desc, &to_send->header); 
	if(DEBUG) printf("%s ASSIGNED ID TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
	
	
    // AFTER THE CLIENT REQUESTED THE WORLD MAP, WE SEND THE NEEDED TEXTURES	
	clear(buf);
	
	// receiving textures request
	ret = receiveFromClient(socket_desc , buf);
	ImagePacket* image_packet = (ImagePacket*)Packet_deserialize(buf , ret);

	// client ha scelto un immagine, la sostituiamo a quella standard
	if(image_packet->image) vehicle_texture = image_packet->image; 
		
	
	// send surface elevation	
	PacketHeader elevation_header;
	elevation_header.type = PostElevation;
	
	ImagePacket * elevation_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	elevation_packet->header = elevation_header;
	elevation_packet->id = args->id;
	elevation_packet->image = surface_elevation;
	
	if(DEBUG) printf("%s SENDING SURFACE ELEVATION TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
	
	sendToClient(socket_desc, &elevation_packet->header);
	

	// send surface texture
	PacketHeader texture_header;
	texture_header.type = PostTexture;
	
	ImagePacket * texture_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	texture_packet->header = texture_header;
	texture_packet->id = args->id;
	texture_packet->image = surface_texture;
	
	if(DEBUG) printf("%s SENDING SURFACE TEXTURE TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);

	sendToClient(socket_desc, &texture_packet->header);
	
	
	 
	
	// send vehicle texture of the client client_id
	PacketHeader vehicle_header;
	vehicle_header.type = PostTexture;
	
	ImagePacket * vehicle_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	vehicle_packet->header = vehicle_header;
	vehicle_packet->id = args->id;
	vehicle_packet->image = vehicle_texture;
	
	if(DEBUG) printf("%s SENDING VECHICLE TEXTURE TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
	
	sendToClient(socket_desc, &vehicle_packet->header);
	
	
	if(DEBUG) printf("%s ALL TEXTURES SENT TO CLIENT %d\n", TCP_SOCKET_NAME, args->id);
	
	
	// INSERISCO IL CLIENT NEL MONDO
	Vehicle *vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	Vehicle_init(vehicle, &world, args->id, vehicle_texture);
	World_addVehicle(&world, vehicle);
	
	/**
	Packet_free(&texture_packet->header);
	Packet_free(&elevation_packet->header);
	Packet_free(&vehicle_packet->header);
	**/

	//qui verifichiamo se il client chiude la connessione
	while(1) {
		ret = receiveFromClient(socket_desc, NULL);
		if(ret == 0) break;
	}
	
	
	printf("%s CLIENT %d CLOSED THE GAME\n", TCP_SOCKET_NAME, args->id);
	
	World_detachVehicle(&world, vehicle);

	ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket for incoming connection");
	
    free(args);
    pthread_exit(NULL);

}




void *udp_handler(void *arg) {
	
	struct sockaddr_in si_me, udp_client_addr;
	int udp_socket, res, udp_sockaddr_len = sizeof(udp_client_addr);

	// create the socket
	udp_socket=socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_HELPER(udp_socket, "Could not create the udp_socket");

	// zero the memory
	memset((char *) &si_me, 0, sizeof(si_me));

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(UDP_PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);


	//bind the socket to port
	res = bind(udp_socket , (struct sockaddr*)&si_me, sizeof(si_me));
	ERROR_HELPER(res, "Could not bind the udp_socket");

	//Listening on port 3000
	while(1) {
		
		// udp riceve il pacchetto dal client, aggiorna i suoi dati e lo rimanda
		 
		char vehicle_buffer[BUFLEN];
		res = recvfrom(udp_socket, vehicle_buffer, sizeof(vehicle_buffer), 0, (struct sockaddr *) &udp_client_addr, (socklen_t *) &udp_sockaddr_len);
		ERROR_HELPER(res, "Cannot recieve from the client");
		VehicleUpdatePacket* deserialized_vehicle_packet = (VehicleUpdatePacket*)Packet_deserialize(vehicle_buffer, sizeof(vehicle_buffer));
		
		int vehicle_id = deserialized_vehicle_packet->id;
		
		Vehicle* v = World_getVehicle(&world, vehicle_id);
		v->rotational_force_update = deserialized_vehicle_packet->rotational_force;
		v->translational_force_update = deserialized_vehicle_packet->translational_force; 

		World_update(&world);

  
		WorldUpdatePacket* world_packet = (WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
		PacketHeader w_head;
		w_head.type = WorldUpdate;
		world_packet->header = w_head;
		
		
		world_packet->num_vehicles = world.vehicles.size;
		
		ClientUpdate* update_block = (ClientUpdate*)malloc(world_packet->num_vehicles*sizeof(ClientUpdate));
		
		ListItem *item = world.vehicles.first;

		int i;
		for(i=0; i<world.vehicles.size; i++) {

			Vehicle *v = (Vehicle*) item;
			update_block[i].id = v->id;
			update_block[i].x = v->x;
			update_block[i].y = v->y;			
			update_block[i].theta = v->theta;

			item = item->next;
		}
		
		world_packet->updates = update_block;    	

		char world_buffer[BUFLEN];
		int world_buffer_size = Packet_serialize(world_buffer, &world_packet->header);
		
		//OSSERVAZIONE, DOVREI MANDARLA A TUTTI I CLIENT!
		sendto(udp_socket, world_buffer, world_buffer_size , 0 , (struct sockaddr *) &udp_client_addr, (socklen_t) udp_sockaddr_len);
	}
	return NULL;
}



void sendToClient(int socket_desc, PacketHeader* packet){
	int ret;	
	char to_send[BUFLEN];
	char len_to_send[BUFLEN];
	
	int len =  Packet_serialize(to_send, packet);
	snprintf(len_to_send, BUFLEN , "%d", len);
	
	//if(DEBUG) printf("*** Bytes to send : %s ***\n" , len_to_send);
	
	ret = send(socket_desc, len_to_send, sizeof(long int) , 0);
	ERROR_HELPER(ret, "Cannot send msg to the client");  
	
	ret = send(socket_desc, to_send, len , 0);
	ERROR_HELPER(ret, "Cannot send msg to the client");  

}

int receiveFromClient(int socket_desc , char* msg){
	int ret;
	char len_to_receive[BUFLEN];
	
	ret = recv(socket_desc , len_to_receive , sizeof(long int) , 0);
	ERROR_HELPER(ret, "Cannot receive from client");
	
	int received_bytes = 0;
	int to_receive = atoi(len_to_receive);

	
	while(received_bytes < to_receive){
		ret = recv(socket_desc , msg + received_bytes , to_receive - received_bytes , 0);
	    ERROR_HELPER(ret, "Cannot receive from client");
	    received_bytes += ret;
	    //if(DEBUG) printf("*** Bytes received : %d ***\n" , ret);
	    if(ret==0) break;
	}

	return received_bytes;
}


void clear(char* buf){
	memset(buf , 0 , BUFLEN * sizeof(char));
}



/**
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
**/
