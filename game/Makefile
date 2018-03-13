CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
LIBS= -lglut -lGLU -lGL -lm -lpthread
CC=gcc
AR=ar


BINS=libso_game.a\
     server\
     client

OBJS = src/vehicle/vec3.o\
       src/linked_list/linked_list.o\
       src/surface/surface.o\
       src/image/image.o\
       src/vehicle/vehicle.o\
       src/world/world.o\
       src/world/world_viewer.o\
       src/protocol/so_game_protocol.o\
	   src/packet/packet.o\
	   src/socket/socket.o\
	   src/common/common.o\
	   src/server/server_kit.o\
	   src/client/client_kit.o

HEADERS=helpers.h\
	src/image/image.h\
	src/linked_list/linked_list.h\
	src/protocol/so_game_protocol.h\
	src/surface/surface.h\
	src/vehicle/vec3.h\
	src/vehicle/vehicle.h\
	src/world/world.h\
	src/world/world_viewer.h\
	src/packet/packet.h\
	src/socket/socket.h\
	src/common/common.h\
	src/server/server_kit.h\
	src/client/client_kit.h

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS) 


libso_game.a: $(OBJS) 
	$(AR) -rcs ./bin/$@ $^
	$(RM) $(OBJS)
	
server: ./src/server/server.c ./bin/libso_game.a 
	$(CC) $(CCOPTS) -Ofast -o ./bin/$@ $^ $(LIBS)

client: ./src/client/client.c ./bin/libso_game.a 
	$(CC) $(CCOPTS) -Ofast -o ./bin/$@ $^ $(LIBS) 

test_server:
	./bin/server ./images/test.pgm ./images/test.ppm ./images/arrow-right.ppm

test_client:
	./bin/client 127.0.0.1

clean:
	rm -rf *.o *~  ./bin/* 
