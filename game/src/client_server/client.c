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

#include "kit.h"

int main(int argc, char **argv) {

	if (argc<2) {
		printf("usage: %s <server_address>\n", argv[1]);
		exit(-1);
	}

	// for the world
	World world;
	Vehicle* vehicle;

	int ret;
	
	Image* map_elevation;
	Image* map_texture;
	Image* vehicle_texture; //vehicle texture
	int my_id = -1;
	
	vehicle_texture = get_vehicle_texture(); 
	char* buf = (char*)malloc(sizeof(char) * BUFLEN);	
	int socket_desc = tcp_client_setup();	
	
	
	// REQUEST AND GET AN ID
	clear(buf);
	IdPacket* id_packet = id_packet_init(GetId, my_id);	

	tcp_send(socket_desc , &id_packet->header);	     // Requesting id
	ret = tcp_receive(socket_desc , buf);          // Receiving id
	
	IdPacket* received_packet = (IdPacket*)Packet_deserialize(buf, ret); // Id received!
	my_id = received_packet->id;
	
	///if(DEBUG) printf("Id received : %d\n", my_id);
	
	welcome_client(my_id);


    // REQUEST AND GET ELEVATION MAP    
    clear(buf);
    ImagePacket* elevationImage_packet = image_packet_init(GetElevation, NULL, 0);
    tcp_send(socket_desc , &elevationImage_packet->header);
       
    ret = tcp_receive(socket_desc , buf);
    
    ImagePacket* elevation_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	if( (elevation_packet->header).type != PostElevation || elevation_packet->id != 0) {
		fprintf(stderr,"Error: Image corrupted");
		exit(EXIT_FAILURE);		
	}
	///if(DEBUG) printf("%s ELEVATION MAP RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	
	map_elevation = elevation_packet->image;
	
	
	
	// REQUEST AND GET SURFACE TEXTURE
	clear(buf);
	ImagePacket* surfaceTexture_packet = image_packet_init(GetTexture, NULL, 0);
    tcp_send(socket_desc , &surfaceTexture_packet->header);
	
    ret = tcp_receive(socket_desc , buf); 

    ImagePacket* texture_packet = (ImagePacket*)Packet_deserialize(buf, ret);
	
	if( (texture_packet->header).type != PostTexture || texture_packet->id != 0) {
		fprintf(stderr,"Error: Image corrupted");
		exit(EXIT_FAILURE);		
	}
	///if(DEBUG) printf("%s SURFACE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
	
    map_texture = texture_packet->image;



    // GET VEHICLE TEXTURE
	clear(buf);
	ImagePacket* vehicleTexture_packet;
	
	if(vehicle_texture) {
		vehicleTexture_packet = image_packet_init(PostTexture, vehicle_texture, my_id);    // client chose to use his own image
		tcp_send(socket_desc , &vehicleTexture_packet->header);
	} else {
		vehicleTexture_packet = image_packet_init(GetTexture, NULL, my_id); // client chose default vehicle image
		
		tcp_send(socket_desc , &vehicleTexture_packet->header);	
		ret = tcp_receive(socket_desc , buf);
		
		ImagePacket* vehicle_packet = (ImagePacket*)Packet_deserialize(buf, ret);
		
		if( (vehicle_packet->header).type != PostTexture || vehicle_packet->id <= 0) {
			fprintf(stderr,"Error: Image corrupted");
			exit(EXIT_FAILURE);			
		}
		///if(DEBUG) printf("%s VEHICLE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
		
		vehicle_texture = vehicle_packet->image;
		free(vehicle_packet);
    }
	
	// construct the world
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	
	Vehicle_init(vehicle, &world, my_id, vehicle_texture);
	World_addVehicle(&world, vehicle);
	
	
	/*** UDP PART NOTIFICATION ***/
	pthread_t runner_thread;
	pthread_attr_t runner_attrs;
	UpdaterArgs runner_args={
		.run=1,
		.id = my_id,
		.tcp_desc = socket_desc,
		.texture = vehicle_texture,
		.vehicle = vehicle,
		.world = &world
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
	Packet_free(&received_packet->header);
	
	Packet_free(&surfaceTexture_packet->header);
	Packet_free(&texture_packet->header);
	
	Packet_free(&elevationImage_packet->header);
	Packet_free(&elevation_packet->header);
	
	Packet_free(&vehicleTexture_packet->header);
	free(buf);
	
	return 0;             
}
