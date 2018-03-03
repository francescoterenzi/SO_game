#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "server_kit.h"

void welcome_server(void) {
	char *os_msg = "\nOPERATING SYSTEM PROJECT 2018 - SERVER SIDE ***\n\n";
	if(DEBUG) {
		fprintf(stdout ,"%s", os_msg);
		fflush(stdout);
		fprintf(stdout, "OS SERVER ip:[%s] port:[%d] ready to accept incoming connections! ***\n\n", 
					SERVER_ADDRESS , TCP_PORT);
		fflush(stdout);
	}
}

void world_update(VehicleUpdatePacket *vehicle_packet, World *world) {
	
	int id = vehicle_packet->id;
		
	Vehicle* v = World_getVehicle(world, id);
	v->rotational_force_update = vehicle_packet->rotational_force;
	v->translational_force_update = vehicle_packet->translational_force; 

	World_update(world);
}

ServerListItem* ServerListItem_init(int sock){
	ServerListItem* item = (ServerListItem*) malloc(sizeof(ServerListItem));
	item->info = sock;
	item->list.prev = NULL;
	item->list.next = NULL;
}

int Server_addSocket(ListHead* l, int sock){
	ServerListItem* item = ServerListItem_init(sock);	
	ListItem* result = List_insert(l, l->last, (ListItem*)item);
	return ((ServerListItem*)result)->info;
}

void Server_listFree(ListHead* l){
	ListItem *item = l->first;
	int size = l->size;
	int i;
	for(i=0; i < size; i++) {
		ServerListItem *v = (ServerListItem*) item;
		item = item->next;
		free(v);
		
	}
}

void Server_socketClose(ListHead* l){
	ListItem* item = l->first;
	int i;
	for(i = 0; i < l->size; i++) {

		ServerListItem* v = (ServerListItem*) item;
		int client_desc = v->info;
		
		//if(DEBUG) fprintf(stdout,"closing socket...%d\n", client_desc);
		//fflush(stdout);
		int ret = close(client_desc);
        ERROR_HELPER(ret, "Cannot close socket");
		item = item->next;
	}
}

