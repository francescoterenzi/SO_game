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
#include "update_packet.h"
#include "socket.h"


WorldViewer viewer;
World world;
Vehicle* vehicle;


typedef struct {
  Image *texture;
  volatile int run;
  int id;
} UpdaterArgs;

int connectToServer(void);
void *updater_thread(void *arg);
void clear(char* buf);




int main(int argc, char **argv) {
	if (argc<3) {
	printf("usage: %s <server_address> <player texture>\n", argv[1]);
	exit(-1);
	}

	printf("loading texture image from %s ... \n", argv[2]);
	Image* my_texture = Image_load(argv[2]);
	if (my_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}
	
	
	///Image* my_texture_for_server = my_texture;

	// these come from the server
	int my_id = -1;
	Image* map_elevation;
	Image* map_texture;
	Image* my_texture_from_server; //vehicle texture
	
	char* buf = (char*)malloc(sizeof(char) * BUFLEN);
	
	
	
	int socket_desc = tcp_client_setup();	//initiate a connection on the socket	
	int ret;
	
	// REQUEST AND GET AN ID
	clear(buf);
	IdPacket* id_packet = IdPacket_build(my_id , GetId);	
	tcp_send(socket_desc , &id_packet->header);	     // Requesting id
	ret = tcp_receive(socket_desc , buf);          // Receiving id
	
	IdPacket* received_packet = (IdPacket*)Packet_deserialize(buf, ret); // Id received!
	my_id = received_packet->id;
	
	if(DEBUG) printf("Id received : %d\n", my_id);



    // REQUEST AND GET ELEVATION MAP    
    clear(buf);
    ImagePacket* elevationImage_packet = ImagePacket_build(NULL, 0 , GetElevation);
    tcp_send(socket_desc , &elevationImage_packet->header);
       
    ret = tcp_receive(socket_desc , buf);
    
    ImagePacket* elevation_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	if( (elevation_packet->header).type == PostElevation && elevation_packet->id == 0) {
		if(DEBUG) printf("%s ELEVATION MAP RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, elevation map not received!\n");
	}
	map_elevation = elevation_packet->image;
	
	
	
	// REQUEST AND GET SURFACE TEXTURE
	clear(buf);
	ImagePacket* surfaceTexture_packet = ImagePacket_build(NULL, 0 , GetTexture);
    tcp_send(socket_desc , &surfaceTexture_packet->header);
	
    ret = tcp_receive(socket_desc , buf); 

    ImagePacket* texture_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	if( (texture_packet->header).type == PostTexture && texture_packet->id == 0) {
		if(DEBUG) printf("%s SURFACE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, surface texture not received!\n");
	}
    map_texture = texture_packet->image;



    // REQUEST AND GET VEHICLE TEXTURE
	clear(buf);
	ImagePacket* vehicleTexture_packet = ImagePacket_build(NULL, my_id , GetTexture);
    tcp_send(socket_desc , &vehicleTexture_packet->header);
	
    ret = tcp_receive(socket_desc , buf);

    ImagePacket* vehicle_packet = (ImagePacket*)Packet_deserialize(buf, ret);
    
	if( (vehicle_packet->header).type == PostTexture && vehicle_packet->id > 0) {
		if(DEBUG) printf("%s VEHICLE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	} else {
		if(DEBUG) printf(" ERROR, vehicle texture not received!\n");
	}
	my_texture_from_server = vehicle_packet->image;
	
	if(DEBUG) printf("%s ALL TEXTURES RECEIVED\n", TCP_SOCKET_NAME);
	
	
	// construct the world
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	
	Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
	World_addVehicle(&world, vehicle);
	
	
	/*** UDP PART NOTIFICATION ***/
	pthread_t runner_thread;
	pthread_attr_t runner_attrs;
	UpdaterArgs runner_args={
		.texture = my_texture_from_server,
		.run=1,
		.id = my_id
	};
	  
	pthread_attr_init(&runner_attrs);
	pthread_create(&runner_thread, &runner_attrs, updater_thread, &runner_args);
	
	WorldViewer_runGlobal(&world, vehicle, &argc, argv);
	runner_args.run=0;
	
	void* retval;
	pthread_join(runner_thread, &retval);
	

	World_destroy(&world);

	//free allocated memory
	Packet_free(&id_packet->header);
	Packet_free(&surfaceTexture_packet->header);
	Packet_free(&elevationImage_packet->header);
	Packet_free(&vehicleTexture_packet->header);
	free(buf);
	
	return 0;             
}




void *updater_thread(void *arg) {
	
	UpdaterArgs* _arg = (UpdaterArgs*) arg;

	// creo socket udp
	struct sockaddr_in si_other;
    int ret, s, slen=sizeof(si_other);
    
    s=socket(AF_INET, SOCK_DGRAM, 0);
    
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(UDP_PORT);
    si_other.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);	
    


	while(_arg->run) {
	
		
		VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*)malloc(sizeof(VehicleUpdatePacket));
		PacketHeader v_head;
		v_head.type = VehicleUpdate;

		vehicle_packet->header = v_head;
		vehicle_packet->id = _arg->id;
		vehicle_packet->rotational_force = vehicle->rotational_force_update;
		vehicle_packet->translational_force = vehicle->translational_force_update;
		
		char vehicle_buffer[BUFLEN];
		int vehicle_buffer_size = Packet_serialize(vehicle_buffer, &vehicle_packet->header);
		
		ret = sendto(s, vehicle_buffer, vehicle_buffer_size , 0 , (struct sockaddr *) &si_other, slen);
		ERROR_HELPER(ret, "Cannot send to server");

		
        char world_buffer[BUFLEN];
		ret = recvfrom(s, world_buffer, sizeof(world_buffer), 0, (struct sockaddr *) &si_other, (socklen_t*) &slen);
		ERROR_HELPER(ret, "Cannot recv from server");
		
		WorldUpdatePacket* deserialized_wu_packet = (WorldUpdatePacket*)Packet_deserialize(world_buffer, ret);		

		int numb_of_vehicles = deserialized_wu_packet->num_vehicles;

		if(numb_of_vehicles > world.vehicles.size) {
			int i;
			for(i=0; i<numb_of_vehicles; i++) {
				int id = deserialized_wu_packet->updates[i].id;
				if(World_getVehicle(&world, id) == NULL) {

					Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle)); 
					Vehicle_init(v,&world, id, _arg->texture);
					World_addVehicle(&world, v);
					printf("%s new vehicle added with id: %d\n", UDP_SOCKET_NAME, id);
				} 
			}
		}
		else if(numb_of_vehicles < world.vehicles.size) {
			ListItem *item = world.vehicles.first;
			int i, j;
			for(i=0; i<world.vehicles.size; i++) {
				Vehicle *v = (Vehicle*) item;
				int found = 0;
				for(j=0; j<deserialized_wu_packet->num_vehicles; i++) {
					if(v->id == deserialized_wu_packet->updates[i].id) {
						found = 1;
						break;
					}
				}
				if(!found) World_detachVehicle(&world, v);
				printf("%svehicle with id: %d removed\n", UDP_SOCKET_NAME, v->id);
				item = item->next;	
			}
		}

		int i;
		for(i=0; i<world.vehicles.size; i++) {
			Vehicle *v = World_getVehicle(&world, deserialized_wu_packet->updates[i].id);
			
			v->x = deserialized_wu_packet->updates[i].x;
			v->y = deserialized_wu_packet->updates[i].y;
			v->theta = deserialized_wu_packet->updates[i].theta;
		}

		usleep(300000);
	}
	return 0;
}


void clear(char* buf){
	memset(buf , 0 , BUFLEN * sizeof(char));
}



void client_update(WorldUpdatePacket *deserialized_wu_packet) {

	int numb_of_vehicles = deserialized_wu_packet->num_vehicles;
	
	if(numb_of_vehicles > world.vehicles.size) {
		int i;
		for(i=0; i<numb_of_vehicles; i++) {
			int id = deserialized_wu_packet->updates[i].id;
			if(World_getVehicle(&world, id) == NULL) {

				Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle)); 
				Vehicle_init(v,&world, id, Image_load("./images/arrow-right.ppm"));
				World_addVehicle(&world, v);
				printf("%s new vehicle added with id: %d\n", UDP_SOCKET_NAME, id);
			} 
		}
	}
	
	else if(numb_of_vehicles < world.vehicles.size) {
		ListItem* item=world.vehicles.first;
		int i, find = 0;
		while(item){
			Vehicle* v=(Vehicle*)item;
			for(i=0; i<numb_of_vehicles; i++){
				if(deserialized_wu_packet->updates[i].id == v->id)
					find = 1;
			}

			if (find == 0) {
				printf("client %d disconnected\n", v->id);
				World_detachVehicle(&world, v);
			}

			find = 0;
			item=item->next;
		}
	}

	int i;
	for(i=0; i<world.vehicles.size; i++) {
		Vehicle *v = World_getVehicle(&world, deserialized_wu_packet->updates[i].id);
		
		v->x = deserialized_wu_packet->updates[i].x;
		v->y = deserialized_wu_packet->updates[i].y;
		v->theta = deserialized_wu_packet->updates[i].theta;
	}
}

