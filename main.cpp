#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <stdio.h>
#include<stdlib.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>

bool isExit = false;
namespace spd = spdlog;
using namespace std;
using namespace rapidjson;

void cs(int n)
{
	if (n == SIGINT)
	{
		isExit = true;
	}
}


string readFile(const string &fileName)
{
	ifstream ifs(fileName.c_str(), ios::in | ios::binary | ios::ate);

	ifstream::pos_type fileSize = ifs.tellg();
	ifs.seekg(0, ios::beg);

	vector<char> bytes(fileSize);
	ifs.read(&bytes[0], fileSize);

	return string(&bytes[0], fileSize);
}

bool is_file_exist(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

int main(int argc, char ** argv)
{
	printf("forwarder started.\n");
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(make_shared<spdlog::sinks::daily_file_sink_st>("daily","txt", 0, 0));
#ifdef _MSC_VER
	sinks.push_back(make_shared<spdlog::sinks::wincolor_stdout_sink_st>());
#else
	sinks.push_back(make_shared<spdlog::sinks::stdout_sink_st>());
#endif
	auto logger = make_shared<spdlog::logger>("my_logger", begin(sinks), end(sinks));
	spdlog::register_logger(logger);
	logger->flush_on(spd::level::err);
	spd::set_pattern("[%D %H:%M:%S:%e][%l] %v");
	spd::set_level(spd::level::info);
	logger->set_level(spd::level::debug);
	logger->info("logger created successfully.");

	const char * configPath = "./../config.json";
	if(!is_file_exist(configPath)){	
		logger->error("config.json not found!");
		return EXIT_FAILURE;
	}
	string json = readFile(configPath);
	Document config;
	config.Parse(json.c_str());
	/*
	// 2. Modify it by DOM.
	Value& s = d["stars"];
	s.SetInt(s.GetInt() + 1);

	// 3. Stringify the DOM
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	d.Accept(writer);

	// Output {"project":"rapidjson","stars":11}
	std::cout << buffer.GetString() << std::endl;
	*/


	signal(SIGINT, cs);
	if (enet_initialize() != 0)
	{
		fprintf(stderr, "An error occurred while initializing ENet.\n");
		return EXIT_FAILURE;
	}
	Value& hostData = config["host"];

	ENetAddress address;
	ENetHost * server;
	enet_address_set_host (&address, "0.0.0.0");
	//address.host = ENET_HOST_ANY;
	address.port = hostData["port"].GetInt();
	server = enet_host_create(&address,
		hostData["peers"].GetInt(),    
		hostData["channels"].GetInt(),
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
	spdlog::drop_all();

}
