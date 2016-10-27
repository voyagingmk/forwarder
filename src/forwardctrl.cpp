#include "forwardctrl.h"

namespace spd = spdlog;
using namespace std;
using namespace rapidjson;

ForwardCtrl::ForwardCtrl() :
	poolForwardServer(sizeof(ForwardServer)),
	poolForwardClient(sizeof(ForwardClient)),
	serverNum(0),
	isExit(false)
{
	handleFuncs[1] = &ForwardCtrl::handlePacket_1;
	handleFuncs[2] = &ForwardCtrl::handlePacket_2;
	handleFuncs[3] = &ForwardCtrl::handlePacket_3;
	handleFuncs[4] = &ForwardCtrl::handlePacket_4;
}


ForwardCtrl::~ForwardCtrl() {
	while (servers.size() > 0) {
		ForwardServer* server = servers.back();
		servers.pop_back();
		enet_host_destroy(server->host);
	}
	poolForwardServer.clear();
	poolForwardClient.clear();
	handleFuncs.clear();
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
		server->admin = (serverConfig.HasMember("admin") ? serverConfig["admin"].GetBool() : false);
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
			serverDict[server->id] = server;
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



bool ForwardCtrl::getHeader(ForwardHeader * header, ENetPacket * packet) {
	memcpy(header, packet->data, sizeof(ForwardHeader));
	if (header->version != FORWARDER_VERSION)
		return FORWARDER_ERR;
	if (header->length != sizeof(ForwardHeader))
		return FORWARDER_ERR;
	return FORWARDER_OK;
}

// system command
bool ForwardCtrl::handlePacket_1(ForwardParam& param) {
	if(!param.server->admin)
		return FORWARDER_ERR;
	ForwardHeader outHeader;
	outHeader.protocol = 1;
	int subID = param.header->subID;
	if (subID == 1) {
		//stat
		const rapidjson::Document& d = stat();
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);
		const char* s = buffer.GetString();
		int totalLength = sizeof(ForwardHeader) + strlen(s) + 1;
		ENetPacket * outPacket = enet_packet_create(NULL, totalLength, ENET_PACKET_FLAG_RELIABLE);
		memset(outPacket->data, '\0', totalLength);
		memcpy(outPacket->data, &outHeader, sizeof(ForwardHeader));
		memcpy(outPacket->data + sizeof(ForwardHeader), s, strlen(s));
		enet_peer_send(param.client->peer, param.channelID, outPacket);
		logger()->info("response 1");
	}
	else if (subID == 2){
		//force disconnect
	}
	return FORWARDER_OK;
}

// has destHostID and has destCID
bool ForwardCtrl::handlePacket_2(ForwardParam& param) {
	ForwardHeader& inHeader = *param.header;
	ForwardHeader outHeader;
	outHeader.protocol = 2;
	if (inHeader.getFlag(FORWARDER_FLAG_WITH_ADDRESS)) {
		outHeader.hostID = param.server->id;
		outHeader.clientID = param.client->id;
	}
	
	ForwardServer* outHost = nullptr;
	if (param.server->dest) {
		// prior
		outHost = param.server->dest;
	}
	else {
		int destHostID = inHeader.hostID;
		if (!destHostID)
			return FORWARDER_ERR;
		auto it_server = serverDict.find(destHostID);
		if (it_server == serverDict.end())
			return FORWARDER_ERR;
		outHost = it_server->second;
	}
	int destClientID = 0;
	if (!param.server->dest) {
		destClientID = inHeader.clientID;
	}
	if (destClientID) {
		//single send
		auto it_client = outHost->clients.find(destClientID);
		if (it_client == outHost->clients.end())
			return FORWARDER_ERR;
		ForwardClient* outClient = it_client->second;
		ENetPacket * outPacket = param.packet;
		memcpy(outPacket->data, &outHeader, sizeof(ForwardHeader));
		enet_peer_send(outClient->peer, param.channelID, outPacket);
	}
	else {
		//broadcast
		ENetPacket * outPacket = param.packet;
		memcpy(outPacket->data, &outHeader, sizeof(ForwardHeader));
		// broadcast the incoming packet to dest host's peers
		enet_host_broadcast(outHost->host, param.channelID, outPacket);
	}
	logger()->info("forwarded 2");
	return FORWARDER_OK;
}

bool ForwardCtrl::handlePacket_3(ForwardParam& param) {

	return FORWARDER_OK;
}
bool ForwardCtrl::handlePacket_4(ForwardParam& param) {
	return FORWARDER_OK;
}

void  ForwardCtrl::onReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID) {
	logger()->info("[cli:{0}][c:{1}][len:{2}]",
		client->id,
		channelID,
		inPacket->dataLength);
	ForwardHeader header;
	bool err = getHeader(&header, inPacket);
	if (err) {
		logger()->warn("[onReceived] getHeader err");
		return;
	}
	const char * content = (const char*)(inPacket->data) + sizeof(header);
	logger()->info("[data]{0}", content);
	auto it = handleFuncs.find(header.getProtocol());
	if (it == handleFuncs.end()) {
		logger()->warn("[onReceived] wrong protocol:{0}", header.getProtocol());
		return;
	}
	ForwardParam param;
	param.header = &header;
	param.channelID = channelID;
	param.packet = inPacket;
	param.client = client;
	param.server = server;
	handlePacketFunc handleFunc = it->second;
	(this->*handleFunc)(param);
}

void ForwardCtrl::loop() {
	ENetEvent event;
	while (!isExit) {
		int ret;
		for (auto& server : servers) {
			ret = enet_host_service(server->host, &event, 5);
			while (ret > 0)
			{
				logger()->info("event.type = {}", event.type);
				switch (event.type) {
					case ENET_EVENT_TYPE_CONNECT: {
						UniqID id = server->idGenerator.getNewID();
						ForwardClient* client = poolForwardClient.add();
						client->id = id;
						client->peer = event.peer;
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
					case ENET_EVENT_TYPE_DISCONNECT: {
						ForwardClient* client = (ForwardClient*)event.peer->data;
						logger()->info("[c:{1}] disconnected.",
							client->id);
						event.peer->data = nullptr;
						auto it = server->clients.find(client->id);
						if (it != server->clients.end())
							server->clients.erase(it);
						poolForwardClient.del(client);
					}
					case ENET_EVENT_TYPE_NONE:
						break;
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
			Value id(server->id);
			add("id", id);
			Value destId(server->destId);
			add("destId", destId);
			Value desc;
			desc.SetString(server->desc.c_str(), server->desc.size(), d.GetAllocator());
			add("desc", desc);
			Value port(server->host->address.port);
			add("port", port);
			Value peerLimit(server->peerLimit);
			add("peerLimit", peerLimit);
			Value channelLimit(server->host->channelLimit);
			add("channels", channelLimit);
			addToServer("config", dConfig);
		}
		{
			Value dIdGenerator(kObjectType);
			auto add = [&](Value::StringRefType k, Value& v) {
				dIdGenerator.AddMember(k, v, d.GetAllocator());
			}; 
			Value maxCount(server->idGenerator.getCount());
			Value recyled(server->idGenerator.getPecycledLength());
			add("max", maxCount);
			add("recyled", recyled);
			addToServer("idGenerator", dIdGenerator);
		}
		Value peers(server->clients.size());
		addToServer("peers", peers);
		lstServers.PushBack(dServer.Move(), d.GetAllocator());
	}
	d.AddMember("servers", lstServers.Move(), d.GetAllocator());
	return d;
}