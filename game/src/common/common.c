#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "common.h"

void clear(char* buf){
	memset(buf , 0 , BUFLEN * sizeof(char));
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
