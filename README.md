# OPERATING SISTEMS PROJECT - VIDEOGAME
	
## Description
Implement a distributed videogame
    

### How to run
1. Aprire il terminale e spostarsi in `so_game/game`
2. Digitare da terminale il comando `make`
3. Digitare da terminale il comando `make test_server` per avviare la parte server side
4. Digitare in un secondo terminale il comando `make test_client` per avviare la parte client side

_Per avviare una modalità multiplayer sarà necessario effettuare il punto 4_
_tante volte quanti sono i giocatori partecipanti._


### Documentation
Server Side:
- the server operates in both TCP and UDP

TCP Part
- registerning a new client when it comes online
- deregistering a client when it goes offline
- sending the map, when the client requests it

  
UDP part
  - the server receives preiodic upates from the client
  in the form of
  <timestamp, translational acceleration, rotational acceleration>

Each "epoch" it integrates the messages from the clients,
and sends back a state update
    - the server sends to each connected client
      the position of the agents around him

Client side
- the client does the following
- connects to the server (TCP)
- requests the map, and gets an ID from the server (TCP)
- receives udates on the state from the server

periodically
- updates the viewer (provided)
- reads either keyboard or joystick
- sends the <UDP> packet of the control to the server

     
## Authors
- [Giorgio Grisetti](https://gitlab.com/grisetti)
- [Irvin Aloise](https://istinj.github.io/)

## Contributors
- [Valerio Nicolanti](https://github.com/valenico)
- [Francesco Terenzi](https://github.com/fratere)
