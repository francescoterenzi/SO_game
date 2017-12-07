
// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// for the udp_socket
#include <arpa/inet.h>
#include <sys/socket.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"

#define PORT 3000
#define UDP_BUFLEN 512

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
  Image* surface_elevation = Image_load(elevation_filename);
  if (surface_elevation) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }


  printf("loading texture image from %s ... ", texture_filename);
  Image* surface_texture = Image_load(texture_filename);
  if (surface_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
  Image* vehicle_texture = Image_load(vehicle_texture_filename);
  if (vehicle_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  // not needed here
  //   // construct the world
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
  
  
  /*** TCP SERVER ***/
  
  
  /** UDP SERVER
   * Single thread implementation, if needed I can change it in a multi-thread implemetation.
   * A error handler is required!
   **/
  
  struct sockaddr_in si_me, client_addr;
  char buf[UDP_BUFLEN];
  int udp_socket, res, sockaddr_len = sizeof(client_addr);
  
  // create the socket
  printf("Creating the udp_socket...\n");
  udp_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(udp_socket >= 0) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  
  // zero the memory
  memset((char *) &si_me, 0, sizeof(si_me));
     
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  
  
  //bind the socket to port
  printf("Binding the socket to port %d\n", PORT);
  res = bind(udp_socket , (struct sockaddr*)&si_me, sizeof(si_me));
  if(res >= 0) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  
  //Listening on port 3000
  while(1) {
	  
	  printf("Waiting for data...\n");
	  res = recvfrom(udp_socket, buf, UDP_BUFLEN, 0, (struct sockaddr *) &client_addr, (socklen_t *) &sockaddr_len);
	  
	  if(res >= 0) {
		  // it should be in this form <timestamp, translational acceleration, rotational acceleration>
		  printf("Received packet from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
		  printf("Data: %s\n" , buf);
	  }
	  else {
		printf("recv failed\n");
		continue;
	  }

  }
  
 
  return 0;             
}
