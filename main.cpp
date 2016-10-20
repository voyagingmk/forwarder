#include "base.h"
#include "utils.h"
#include "uniqid.h"

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


class ForwardClient {
public:
	ENetPeer * peer;
	UniqID id = 0;
};

class ForwardServer {
public:
	ForwardServer():
		id(0),
		destId(0),
		dest(nullptr),
		host(nullptr)
	{}
	~ForwardServer() {
		dest = nullptr;
		host = nullptr;
		clients.clear();
	}
public:	
	UniqID id;
	int destId;
	ForwardServer* dest;
	ENetHost * host;
	UniqIDGenerator idGenerator;
	map<UniqID, ForwardClient> clients;
};

void initServers(Value& serversConfig, Pool<ForwardServer> * poolForwardServer, vector<ForwardServer*> * servers) {
	auto logger = spdlog::get("my_logger");
	UniqIDGenerator idGenerator;
	for (Value& serverConfig : serversConfig.GetArray()) {
		ENetAddress address;
		ForwardServer* server = poolForwardServer->add();
		enet_address_set_host(&address, "0.0.0.0");
		//address.host = ENET_HOST_ANY;
		address.port = serverConfig["port"].GetInt();
		server->host = enet_host_create(&address,
			serverConfig["peers"].GetInt(),
			serverConfig["channels"].GetInt(),
			0      /* assume any amount of incoming bandwidth */,
			0      /* assume any amount of outgoing bandwidth */);
		if(serverConfig.HasMember("destId"))
			server->destId = serverConfig["destId"].GetInt();
		if (server->host == NULL) {
			logger->error("An error occurred while trying to create an ENet server host.");
		}
		else {
			server->id = idGenerator.getNewID();
			servers->push_back(server);
		}
	}
	for (auto it = servers->begin(); it != servers->end(); it++) {
		ForwardServer* server = *it;
		int destId = server->destId;
		if (!destId)
			continue;
		for (auto it2 = servers->begin(); it2 != servers->end(); it2++) {
			ForwardServer* _server = *it2;
			if (_server->id == destId) {
				server->dest = _server;
				break;
			}
		}
	}

}

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
	Pool<ForwardServer> poolForwardServer(sizeof(ForwardServer));
	Pool<ForwardClient> poolForwardClient(sizeof(ForwardClient));
	vector<ForwardServer*> servers;

	initServers(serversConfig, &poolForwardServer, &servers);

	ENetEvent event;
	while (!isExit) {
		int ret;
		for (auto& server: servers) {
			while (ret = enet_host_service(server->host, &event, 5) > 0)
			{
				logger->info("event.type = {}", event.type);
				switch (event.type)
				{
				case ENET_EVENT_TYPE_CONNECT: {
					UniqID id = server->idGenerator.getNewID();
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

					// forward the packet to dest host
					enet_host_broadcast(server->dest->host, event.channelID, event.packet);
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
				if (isExit)
					break;
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(20));
			if (isExit)
				break;
		}
	}
	while(servers.size() > 0) {
		ForwardServer* server = servers.back();
		servers.pop_back();
		enet_host_destroy(server->host);
	}

	poolForwardServer.clear();
	poolForwardClient.clear();
	atexit(enet_deinitialize);
	atexit(spdlog::drop_all);

}
