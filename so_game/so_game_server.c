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
void sendToClient(int socket_desc, char* to_send, PacketHeader* packet);
size_t receiveFromClient(int socket_desc, char* msg , size_t buf_len);
void clear(char* buf, size_t len);


typedef struct thread_args {
	int id;
	int socket_desc;	
} thread_args;

typedef struct Client {
	int id;	
} Client;



World world;
Image* surface_texture;
Image* surface_elevation;
Image* vehicle_texture;

Client** clients;
int id;



/**  TO REMEMBER	
 * Alla fine, prima di chiudere il server bisogna rilasciare la memoria
 *   - void Packet_free(PacketHeader* h);
 *   - void Image_free(Image* img);
 *   - free(...) varie
 * **/



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
	surface_elevation = Image_load(elevation_filename);
	if (surface_elevation) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}


	printf("loading texture image from %s ... ", texture_filename);
	surface_texture = Image_load(texture_filename);
	if (surface_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}

	printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
	vehicle_texture = Image_load(vehicle_texture_filename);
	if (vehicle_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}
	
	// Qui creo un array di clients, dove la size massima è MAX_CONN_QUEUE
	clients = (Client**) malloc(MAX_CONN_QUEUE*sizeof(Client*));
	
	int ret;
	pthread_t tcp_thread;
	//pthread_t udp_thread;
	
	//Creating tcp socket..
	ret = pthread_create(&tcp_thread, NULL, tcp_handler, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot create the tcp_thread!");
	
	/**
	//Creating udp socket..
	ret = pthread_create(&udp_thread, NULL, udp_handler, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot create the udp_thread!");
	**/
	
	//Joining udp and tcp sockets.
	ret = pthread_join(tcp_thread, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot join the tcp_thread!");
	//ret = pthread_join(udp_thread, NULL);
	//PTHREAD_ERROR_HELPER(ret, "Cannot join the udp_thread!");
	
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
		
		
		// La connessione è stata accettata, ora creo una struct con tutte le informazioni utili per il client e me la salvo
		// OSS. Il thread_handler si può anche omettere, possiamo ricavare ciò che ci serve grazie al client_id
		Client* client = (Client*) malloc(sizeof(Client));
		client->id = id;
		clients[id] = client;
		printf("Creato nuovo client: %d\n",id);
		
		pthread_t thread;
		thread_args* args = (thread_args*)malloc(sizeof(thread_args));
		args->socket_desc = client_desc;
		args->id = id; //here I set id for the client
		id = id + 1;
		
		ret = pthread_create(&thread, NULL, tcp_client_handler, (void*)args);
		PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

		//if (DEBUG) fprintf(stderr, "New thread created to handle the request!\n");

		ret = pthread_detach(thread); // I won't phtread_join() on this thread
	    PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");
	    
        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));

	}
	
	return NULL;

}





void *tcp_client_handler(void *arg){

	thread_args* args = (thread_args*)arg;
	int id = args->id;
	int socket = args->socket_desc;


	int ret = 0;
	
	char id_request_buf[BUFLEN];
	while( (ret = recv(socket , id_request_buf , sizeof(id_request_buf), 0)) < 0) { // Receiving id request
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from socket");
	}
	IdPacket* id_request = (IdPacket*)Packet_deserialize(id_request_buf , ret);
	
	IdPacket* id_response = (IdPacket*)malloc(sizeof(IdPacket));
	id_response->header = id_request->header;
	id_response->id = id;
	

	printf("%s Assignment id to the client: %d\n", TCP_SOCKET_NAME, id);
	
	char id_response_buf[BUFLEN];
	int id_response_buf_size = Packet_serialize(id_response_buf, &id_response->header);
	while( (ret =  send(socket, id_response_buf, id_response_buf_size, 0)) < 0) { // Send to the client the assigned id
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
	}
	printf("id_size: %d\n", ret);
	
	
    // RECEIVING CLIENT TEXTURE
    char image_packet_buf[BUFLEN];
    ret = 0;
    while( (ret = recv(socket, image_packet_buf , sizeof(image_packet_buf), 0) < 0)) { 
            if (errno == EINTR)
                continue;
            ERROR_HELPER(-1, "Cannot read from socket");
    }
	printf("client_texture_size: %d\n", ret);
	
	ImagePacket* image_packet = (ImagePacket*)Packet_deserialize(image_packet_buf , ret);

	int client_id = image_packet->id;
	//Image* client_image = image_packet->image;

	
	// send surface elevation
	ImagePacket * elevation_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	
	PacketHeader elevation_header;
	elevation_header.type = PostElevation;
	
	elevation_packet->header = elevation_header;
	elevation_packet->id = 0;
	elevation_packet->image = surface_elevation;
	
    char elevation_packet_buf[BUFLEN];
    int elevation_packet_buf_size = Packet_serialize(elevation_packet_buf, &elevation_packet->header);
    
    ret = 0;
	while( (ret =  send(socket, elevation_packet_buf, elevation_packet_buf_size, 0)) < 0) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
	}
	printf("elevation_size: %d\n", ret);

	// send surface texture	
	PacketHeader texture_header;
	texture_header.type = PostTexture;
	
	ImagePacket * texture_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	texture_packet->header = texture_header;
	texture_packet->id = 0;
	texture_packet->image = surface_texture;
	
    char texture_packet_buf[BUFLEN];
    int texture_packet_buf_size = Packet_serialize(texture_packet_buf, &texture_packet->header);
    
    ret = 0;
	while( (ret =  send(socket, texture_packet_buf, texture_packet_buf_size, 0)) < 0) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
	}
	printf("texture_size: %d\n", ret);
	 
	
	// send vehicle texture of the client client_id	
	PacketHeader vehicle_header;
	vehicle_header.type = PostTexture;
	
	ImagePacket * vehicle_packet = (ImagePacket*)malloc(sizeof(ImagePacket));
	vehicle_packet->header = vehicle_header;
	vehicle_packet->id = client_id;
	vehicle_packet->image = vehicle_texture;
	

    char vehicle_packet_buf[BUFLEN];
    int vehicle_packet_buf_size = Packet_serialize(vehicle_packet_buf, &vehicle_packet->header);
    
    ret = 0;
	while( (ret =  send(socket, vehicle_packet_buf, vehicle_packet_buf_size, 0)) < 0) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
	}
	printf("vehicle_size: %d\n", ret);
	
	printf("all textures sent\n");
	
	
	// free allocated memory
	Packet_free(&texture_packet->header);
	Packet_free(&elevation_packet->header);
	Packet_free(&vehicle_packet->header);
	
    ret = close(socket);
    ERROR_HELPER(ret, "Cannot close socket for incoming connection");


    free(args);
    pthread_exit(NULL);
}




void *udp_handler(void *arg) {
	
	struct sockaddr_in si_me, udp_client_addr;
	int udp_socket, res, udp_sockaddr_len = sizeof(udp_client_addr);

	// create the socket
	udp_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	ERROR_HELPER(udp_socket, "Could not create the udp_socket");

	// zero the memory
	memset((char *) &si_me, 0, sizeof(si_me));

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(UDP_PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);


	//bind the socket to port
	res = bind(udp_socket , (struct sockaddr*)&si_me, sizeof(si_me));
	ERROR_HELPER(udp_socket, "Could not bind the udp_socket");

	//Listening on port 3000
	while(1) {
		
		// udp riceve il pacchetto dal client, aggiorna i suoi dati e lo rimanda
		 
		char vehicle_buffer[BUFLEN];
		res = recvfrom(udp_socket, vehicle_buffer, sizeof(vehicle_buffer), 0, (struct sockaddr *) &udp_client_addr, (socklen_t *) &udp_sockaddr_len);
		ERROR_HELPER(res, "Cannot recieve from the client");
		//VehicleUpdatePacket* deserialized_vehicle_packet = (VehicleUpdatePacket*)Packet_deserialize(vehicle_buffer, sizeof(vehicle_buffer));
		printf("Recived VehicleUpdate\n");


		ClientUpdate* update_block = (ClientUpdate*)malloc(sizeof(ClientUpdate));
		update_block->id = 10;
		update_block->x = 4.4;
	    update_block->y = 6.4;
        update_block->theta = 90;
  
		WorldUpdatePacket* world_packet = (WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
		PacketHeader w_head;
		w_head.type = WorldUpdate;

		world_packet->header = w_head;
		world_packet->num_vehicles = 2;
		world_packet->updates = update_block;


		char world_buffer[BUFLEN];
		int world_buffer_size = Packet_serialize(world_buffer, &world_packet->header);

		sendto(udp_socket, world_buffer, world_buffer_size , 0 , (struct sockaddr *) &udp_client_addr, udp_sockaddr_len);
		printf("%s send to %s:%d\n", UDP_SOCKET_NAME, inet_ntoa(udp_client_addr.sin_addr), ntohs(udp_client_addr.sin_port));
	}
	return NULL;
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
