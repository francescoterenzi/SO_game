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
	fprintf(stdout ,"%s", os_msg);
	fprintf(stdout, "OS SERVER ip:[%s] port:[%d] ready to accept incoming connections! ***\n\n", 
				SERVER_ADDRESS , TCP_PORT);
	fflush(stdout);
}

void goodbye_server(void) {
	fprintf(stdout ,"\n\nServer closed.\n");
	fprintf(stdout ,"# Authors: \n");
	fprintf(stdout ,"- [Giorgio Grisetti](https://gitlab.com/grisetti) \n");
	fprintf(stdout ,"- [Irvin Aloise](https://istinj.github.io/) \n");
	fprintf(stdout ,"\n# Contributors: \n");
	fprintf(stdout ,"- [Valerio Nicolanti](https://github.com/valenico) \n");
	fprintf(stdout ,"- [Francesco Terenzi](https://github.com/fratere)\n");
	fprintf(stdout ,"\nGoodbye :D\n");
	fflush(stdout);
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

ServerListItem* Server_getSocket(ListHead* l, int sock){
	ListItem* item = l->first;
	while(item){
		ServerListItem* v=(ServerListItem*)item;
		if(v->info==sock) return v;
		item=item->next;
	}
	return NULL;
}

void Server_detachSocket(ListHead* l, int sock){
	ServerListItem* to_remove = Server_getSocket(l,sock);
	List_detach(l, (ListItem*)to_remove);
}

void Server_listFree(ListHead* l){
	if(l->first == NULL) return;
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
	if(l == NULL || l->first == NULL) return;
	ListItem* item = l->first;
	int i;
	for(i = 0; i < l->size; i++) {

		ServerListItem* v = (ServerListItem*) item;
		int client_desc = v->info;
		
		//if(DEBUG) fprintf(stdout,"closing socket...%d\n", client_desc);
		//fflush(stdout);
		closeSocket(client_desc);
		item = item->next;
	}
}

void closeSocket(int fd) {
	int ret;
		
	if (fd >= 0) {
	   if (shutdown(fd, SHUT_RDWR) < 0){ // terminate the 'reliable' delivery, SHUT_RDWR makes impossible to receive and send on socket
		 if (errno != ENOTCONN && errno != EINVAL){
			ERROR_HELPER(-1,"Error: shutdown faild");
		 }
	   }
			
	  if (close(fd) < 0) // finally call close()
         ERROR_HELPER( -1,"Error: close socket failed");
   }
}