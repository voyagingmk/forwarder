#include <enet/enet.h>
#include <stdio.h>
#include<stdlib.h>
#include <signal.h>
#include <string.h>

bool isExit = false;

void cs(int n)
{
	if (n == SIGINT)
	{
		isExit = true;
	}
}

int main(int argc, char ** argv)
{
	signal(SIGINT, cs);
	if (enet_initialize() != 0)
	{
		fprintf(stderr, "An error occurred while initializing ENet.\n");
		return EXIT_FAILURE;
	}
	ENetAddress address;
	ENetHost * server;
	/* Bind the server to the default localhost.     */
	/* A specific host address can be specified by   */
	/* enet_address_set_host (& address, "x.x.x.x"); */
	enet_address_set_host (&address, "0.0.0.0");
	//address.host = ENET_HOST_ANY;
	/* Bind the server to port 1234. */
	address.port = 18888;
	server = enet_host_create(&address /* the address to bind the server host to */,
		32      /* allow up to 32 clients and/or outgoing connections */,
		2      /* allow up to 2 channels to be used, 0 and 1 */,
		0      /* assume any amount of incoming bandwidth */,
		0      /* assume any amount of outgoing bandwidth */);
	if (server == NULL)
	{
		fprintf(stderr,
			"An error occurred while trying to create an ENet server host.\n");
		exit(EXIT_FAILURE);
	}
	ENetHost * client;
	client = enet_host_create(NULL /* create a client host */,
		1 /* only allow 1 outgoing connection */,
		2 /* allow up 2 channels to be used, 0 and 1 */,
		57600 / 8 /* 56K modem with 56 Kbps downstream bandwidth */,
		14400 / 8 /* 56K modem with 14 Kbps upstream bandwidth */);
	if (client == NULL)
	{
		fprintf(stderr,
			"An error occurred while trying to create an ENet client host.\n");
		exit(EXIT_FAILURE);
	}

	ENetEvent event;
	/* Wait up to 1000 milliseconds for an event. */
	while (!isExit) {
		int ret;
		while (ret = enet_host_service(server, &event, 10) > 0)
		{
			printf("event.type = %d\n", event.type);
			switch (event.type)
			{
			case ENET_EVENT_TYPE_CONNECT:{
				printf("A new client connected from %x:%u.\n",
					event.peer->address.host,
					event.peer->address.port);
				/* Store any relevant client information here. */
				event.peer->data = (char*)"Client information";
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE:{
				printf("A packet of length %u containing %s was received from %s on channel %u.\n",
					event.packet->dataLength,
					event.packet->data,
					event.peer->data,
					event.channelID);
				/* Clean up the packet now that we're done using it. */
				enet_packet_destroy(event.packet);
				ENetPacket * packet = enet_packet_create("world", strlen("world")+1, ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(event.peer, 0, packet);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
				printf("%s disconnected.\n", event.peer->data);
				/* Reset the peer's client information. */
				event.peer->data = NULL;
			}
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(20));
		
	}

	enet_host_destroy(client);
	enet_host_destroy(server);
	atexit(enet_deinitialize);
}