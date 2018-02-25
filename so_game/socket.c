#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "socket.h"
#include "common.h"
#include "so_game_protocol.h"

int tcp_server_setup(void) {
	
	// some fields are required to be filled with 0
	struct sockaddr_in server_addr = {0};

	int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

	// initialize socket for listening
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(TCP_PORT); // network byte order!

	// We enable SO_REUSEADDR to quickly restart our server after a crash
	int reuseaddr_opt = 1;
	int ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
	ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

	// bind address to socket
	ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);
	ERROR_HELPER(ret, "Cannot bind address to socket");
	
	// start listening
	ret = listen(socket_desc, MAX_CONN_QUEUE);
	ERROR_HELPER(ret, "Cannot listen on socket");

	return socket_desc;
}

int tcp_client_setup(void){
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

int udp_server_setup(struct sockaddr_in *si_me) {

	// create the socket
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_HELPER(sock, "Could not create the udp_socket");

	// zero the memory
	memset((char *) si_me, 0, sizeof(*si_me));

	si_me->sin_family = AF_INET;
	si_me->sin_port = htons(UDP_PORT);
	si_me->sin_addr.s_addr = htonl(INADDR_ANY);


	//bind the socket to port
	int res = bind(sock , (struct sockaddr*) si_me, sizeof(*si_me));
	ERROR_HELPER(res, "Could not bind the udp_socket");

	return sock;
}

int udp_client_setup(struct sockaddr_in *si_other) {
	
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    si_other->sin_family = AF_INET;
    si_other->sin_port = htons(UDP_PORT);
    si_other->sin_addr.s_addr = inet_addr(SERVER_ADDRESS);	

	return sock;
}

void tcp_send(int socket_desc, PacketHeader* packet){
	int ret;	
	char to_send[BUFLEN];
	char len_to_send[BUFLEN];
	
	int len =  Packet_serialize(to_send, packet);
	snprintf(len_to_send, BUFLEN , "%d", len);
	
	if(DEBUG) printf("*** Bytes to send : %s\n" , len_to_send);
	
	ret = send(socket_desc, len_to_send, sizeof(long int) , 0);
	ERROR_HELPER(ret, "Cannot send msg to the client");  
	
	ret = send(socket_desc, to_send, len , 0);
	ERROR_HELPER(ret, "Cannot send msg to the client");  

}

int tcp_receive(int socket_desc , char* msg) {
	
	int ret;
	char len_to_receive[BUFLEN];
	
	ret = recv(socket_desc , len_to_receive , sizeof(long int) , 0);
	ERROR_HELPER(ret, "Cannot receive from client");
	
	int received_bytes = 0;
	int to_receive = atoi(len_to_receive);
	if(DEBUG) printf("*** Bytes to_received : %d \n" , to_receive);

	
	while(received_bytes < to_receive){
		ret = recv(socket_desc , msg + received_bytes , to_receive - received_bytes , 0);
	    ERROR_HELPER(ret, "Cannot receive from client");
	    received_bytes += ret;
	    
	    if(ret==0) break;
	}
	if(DEBUG) printf("*** Bytes received : %d \n" , ret);

	return received_bytes;
}

void udp_send(int socket, struct sockaddr_in *si, const PacketHeader* h) {

	char buffer[BUFLEN];
	char size = 0;

	switch(h->type) {
		case VehicleUpdate:
		{
			VehicleUpdatePacket *vup = (VehicleUpdatePacket*) h;
			size = Packet_serialize(buffer, &vup->header);
			break;
		}
		case WorldUpdate: 
		{
			WorldUpdatePacket *wup = (WorldUpdatePacket*) h;
			size = Packet_serialize(buffer, &wup->header);
			break;
		}
	}
	
	int ret = sendto(socket, buffer, size , 0 , (struct sockaddr *) si, sizeof(*si));
	ERROR_HELPER(ret, "Cannot send to server");
}

int udp_receive(int socket, struct sockaddr_in *si, char *buffer) {

	ssize_t slen = sizeof(*si);

	int ret = recvfrom(socket, buffer, BUFLEN, 0, (struct sockaddr *) si, (socklen_t*) &slen);
	ERROR_HELPER(ret, "Cannot recv from server");

	return ret;
}
