#include "forwardctrl.h"

namespace spd = spdlog;
using namespace std;
using namespace rapidjson;

ForwardCtrl::ForwardCtrl() :
	poolForwardServer(sizeof(ForwardServer)),
	poolForwardClient(sizeof(ForwardClient)),
	serverNum(0),
	isExit(false)
{}


ForwardCtrl::~ForwardCtrl() {
	while (servers.size() > 0) {
		ForwardServer* server = servers.back();
		servers.pop_back();
		enet_host_destroy(server->host);
	}
	poolForwardServer.clear();
	poolForwardClient.clear();
}


void ForwardCtrl::initServers(rapidjson::Value& serversConfig) {
	serverNum = serversConfig.GetArray().Size();
	auto logger = spdlog::get("my_logger");
	UniqIDGenerator idGenerator;
	for (rapidjson::Value& serverConfig : serversConfig.GetArray()) {
		ENetAddress address;
		ForwardServer* server = poolForwardServer.add();
		enet_address_set_host(&address, "0.0.0.0");
		//address.host = ENET_HOST_ANY;
		address.port = serverConfig["port"].GetInt();
		server->desc = serverConfig["desc"].GetString();
		server->peerLimit = serverConfig["peers"].GetInt();
		server->host = enet_host_create(&address,
			server->peerLimit,
			serverConfig["channels"].GetInt(),
			0      /* assume any amount of incoming bandwidth */,
			0      /* assume any amount of outgoing bandwidth */);
		if (serverConfig.HasMember("destId"))
			server->destId = serverConfig["destId"].GetInt();
		if (server->host == NULL) {
			logger->error("An error occurred while trying to create an ENet server host.");
		}
		else {
			server->id = idGenerator.getNewID();
			servers.push_back(server);
		}
	}
	for (auto it = servers.begin(); it != servers.end(); it++) {
		ForwardServer* server = *it;
		int destId = server->destId;
		if (!destId)
			continue;
		for (auto it2 = servers.begin(); it2 != servers.end(); it2++) {
			ForwardServer* _server = *it2;
			if (_server->id == destId) {
				server->dest = _server;
				break;
			}
		}
	}
}

void  ForwardCtrl::onReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID) {
	logger()->info("[cli:{0}][c:{1}][len:{2}] {3}",
		client->id,
		channelID,
		inPacket->dataLength,
		inPacket->data);
	ENetPacket * outPacket = enet_packet_create(inPacket->data, inPacket->dataLength, ENET_PACKET_FLAG_RELIABLE);
	enet_packet_destroy(inPacket);
	// broadcast the incoming packet to dest host's peers
	enet_host_broadcast(server->dest->host, channelID, outPacket);
	logger()->info("forwarded");
}

void ForwardCtrl::loop() {
	ENetEvent event;
	while (!isExit) {
		int ret;
		for (auto& server : servers) {
			while (ret = enet_host_service(server->host, &event, 5) > 0)
			{
				logger()->info("event.type = {}", event.type);
				switch (event.type)
				{
				case ENET_EVENT_TYPE_CONNECT: {
					UniqID id = server->idGenerator.getNewID();
					ForwardClient* client = poolForwardClient.add();
					client->id = id;
					event.peer->data = client;
					server->clients[id] = client;
					logger()->info("[c:{1}] connected, from {1}:{2}.",
						client->id,
						event.peer->address.host,
						event.peer->address.port);
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE: {
					ForwardClient* client = (ForwardClient*)event.peer->data;
					ENetPacket * inPacket = event.packet;
					onReceived(server, client, inPacket, event.channelID);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
					ForwardClient* client = (ForwardClient*)event.peer->data;
					logger()->info("[c:{1}] disconnected.",
						client->id);
					event.peer->data = nullptr;
					auto it = server->clients.find(client->id);
					if(it != server->clients.end())
						server->clients.erase(it);
					poolForwardClient.del(client);
				}
				if (isExit)
					break;
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(20));
			if (isExit)
				break;
		}
	}
}

/*
{
	servers:[
		{
			config:{
				id: int,
				destId: int,
				port: int,
				peerLimit: int,
				peerCount: int,
				channels: int,
				desc: str,
				encrypt: bool
			},
			peers: int,		
			idGenerator: {
				max: int,
				recyled: int
			}
		},
	]
}
*/

Document ForwardCtrl::stat() {
	Document d(kObjectType);
	Value lstServers(kArrayType);
	for (auto it = servers.begin(); it != servers.end(); it++) {
		ForwardServer * server = *it;
		Value dServer(kObjectType);	
		auto addToServer = [&](Value::StringRefType k, Value& v) {
			dServer.AddMember(k, v, d.GetAllocator());
		}; 
		{
			Value dConfig(kObjectType);
			auto add = [&](Value::StringRefType k, Value& v) {
				dConfig.AddMember(k, v, d.GetAllocator());
			};
			//auto add = bind(&dServer.AddMember, dServer, placeholders::_1, placeholders::_2, d.GetAllocator());
			add("id", Value(server->id));
			add("destId", Value(server->destId));
			Value desc;
			desc.SetString(server->desc.c_str(), server->desc.size(), d.GetAllocator());
			add("desc", desc);
			add("port", Value(server->host->address.port));
			add("peerLimit", Value(server->peerLimit));
			add("channels", Value(server->host->channelLimit));
			addToServer("config", dConfig.Move());
		}
		{
			Value dIdGenerator(kObjectType);
			auto add = [&](Value::StringRefType k, Value& v) {
				dIdGenerator.AddMember(k, v, d.GetAllocator());
			}; 
			add("max", Value(server->idGenerator.getCount()));
			add("recyled", Value(server->idGenerator.getPecycledLength()));
			addToServer("idGenerator", dIdGenerator.Move());
		}
		addToServer("peers", Value(server->clients.size()));
		lstServers.PushBack(dServer.Move(), d.GetAllocator());
	}
	d.AddMember("servers", lstServers.Move(), d.GetAllocator());
	return d;
}