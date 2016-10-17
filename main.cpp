#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <pool/pool.h>
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
#include <list>
#include <vector>
#include <map>

namespace spd = spdlog;
using namespace std;
using namespace rapidjson;


bool isExit = false;
void onSIGINT(int n)
{
	if (n == SIGINT){
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

bool isFileExist(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

bool setupLogger() {
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(make_shared<spdlog::sinks::daily_file_sink_st>("daily", "txt", 0, 0));
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
	return true;
}

typedef unsigned int UniqID;

class UniqIDGenerator
{
public:
	UniqIDGenerator():count(0){};
	UniqID getNewID() noexcept {
		if (count > 10000) {
			if (recycled.front() > 0) {
				UniqID id = recycled.front();
				recycled.pop_front();
				return id;
			}
		}
		count++;
		return count;
	}
	void recycleID(UniqID id) noexcept {
		recycled.push_back(id);
	}
private:
	std::list<UniqID> recycled;
	UniqID count;
};

class ForwardClient {
public:
	ENetPeer * peer;
	UniqID id = 0;
};

class ForwardServer {
public:	
	UniqID id = 0;
	UniqIDGenerator idGenerator;
	ENetHost * host = nullptr;
	map<UniqID, ForwardClient> clients;
};

int main(int argc, char ** argv)
{
	printf("forwarder started.\n");
	setupLogger();
	auto logger = spdlog::get("my_logger");
	const char * configPath = "./../config.json";
	if(!isFileExist(configPath)){	
		logger->error("config.json not found!");
		return EXIT_FAILURE;
	}
	string json = readFile(configPath);
	Document config;
	config.Parse(json.c_str());
	signal(SIGINT, onSIGINT);
	if (enet_initialize() != 0)
	{
		logger->error("An error occurred while initializing ENet");
		return EXIT_FAILURE;
	}

	Value& serversConfig = config["servers"];
	int serverNum = serversConfig.GetArray().Size();
	vector<ForwardServer> servers;
	UniqIDGenerator idGenerator;
	for (Value& serverConfig : serversConfig.GetArray()){
		ENetAddress address;
		ForwardServer server;
		enet_address_set_host(&address, "0.0.0.0");
		//address.host = ENET_HOST_ANY;
		address.port = serverConfig["port"].GetInt();
		server.host = enet_host_create(&address,
			serverConfig["peers"].GetInt(),
			serverConfig["channels"].GetInt(),
			0      /* assume any amount of incoming bandwidth */,
			0      /* assume any amount of outgoing bandwidth */);
		if (server.host == NULL) {
			logger->error("An error occurred while trying to create an ENet server host.");
		}
		else {
			server.id = idGenerator.getNewID();
			servers.push_back(server);
		}
	}
	Pool<ForwardClient> poolForwardClient;
	ENetEvent event;
	while (!isExit) {
		int ret;
		for (auto& server: servers) {
			while (ret = enet_host_service(server.host, &event, 5) > 0)
			{
				logger->info("event.type = {}", event.type);
				switch (event.type)
				{
				case ENET_EVENT_TYPE_CONNECT: {
					UniqID id = server.idGenerator.getNewID();
					ForwardClient* client = poolForwardClient.add();
					client->id = id;
					event.peer->data = client;
					logger->info("[c:{1}] connected, from {1}:{2}.", 
						client->id, 
						event.peer->address.host,
						event.peer->address.port);
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE: {
					ForwardClient* client = (ForwardClient*)event.peer->data;
					logger->info("[c:{1}][len:{2}] {3}", 
						client->id, 
						event.packet->dataLength, 
						event.packet->data);
					enet_packet_destroy(event.packet);
					//ENetPacket * packet = enet_packet_create("world", strlen("world") + 1, ENET_PACKET_FLAG_RELIABLE);
					//enet_peer_send(event.peer, 0, packet);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
					ForwardClient* client = (ForwardClient*)event.peer->data;
					logger->info("[c:{1}] disconnected.",
						client->id);
					poolForwardClient.del(client);
					event.peer->data = nullptr;
				}
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	}
	while(servers.size() > 0) {
		ForwardServer& server = servers.back();
		servers.pop_back();
		enet_host_destroy(server.host);
	}

	atexit(enet_deinitialize);
	atexit(spdlog::drop_all);

}
