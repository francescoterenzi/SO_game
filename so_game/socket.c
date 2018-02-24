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

void tcp_send(int socket_desc, PacketHeader* packet){
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

int tcp_receive(int socket_desc , char* msg){
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