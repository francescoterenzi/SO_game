#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <time.h>

#include "kit.h"

void clear(char* buf){
	memset(buf , 0 , BUFLEN * sizeof(char));
}


/** SERVER **/
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


/** CLIENT **/
void welcome_client(int id) {
	fprintf(stdout, "Connect to OS SEREVR ip:[%s] port:[%d] with id:[%d] ***\n\n", 
				SERVER_ADDRESS , TCP_PORT, id);
	fprintf(stdout,"\n\nJoin the game!! ***\n\n");
	fflush(stdout);
}

Image* get_vehicle_texture() {

	Image* my_texture;
	char image_path[256];
	fprintf(stdout, "\nOPERATING SYSTEM PROJECT 2018 - CLIENT SIDE ***\n\n");
	fflush(stdout);
	fprintf(stdout, "You will be soon connected to the game server.\n");
	fprintf(stdout, "First, you can choose to use your own image. Only .ppm images are supported.\n");
	fprintf(stdout, "Insert path ('no' for default vehicle image) :\n");

	if(scanf("%s",image_path) < 0){
		fprintf(stderr, "Error occured!\n");
		exit(EXIT_FAILURE);
	}

	if(strcmp(image_path, "no") == 0) return NULL;
	else {
		char *dot = strrchr(image_path, '.');
		if (dot == NULL || strcmp(dot, ".ppm")!=0){
			fprintf(stderr,"Sorry! Image not found or not supported... \n");
			exit(EXIT_FAILURE);
		}
		my_texture = Image_load(image_path);
		if (my_texture) {
			printf("Done! \n");
			return my_texture;
		} else {
			fprintf(stderr,"Sorry! Chose image cannot be loaded... \n");
			exit(EXIT_FAILURE);
		}
	}
	return NULL; // will never be reached
}

void update_info(World *world, int id, int flag) {

	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	char buffer[1024];
	if(flag) {
		sprintf(buffer, "- new client id:[%d] connected", id);
	} 
	else {
		sprintf(buffer, "- client id:[%d] closed the game", id);
	}

	if(DEBUG) {
	fprintf(stdout, "update %s%s\n- number of vehicles connected %d ***\n\n", 
			asctime(timeinfo), buffer, world->vehicles.size);
	fflush(stdout);
	}
}
