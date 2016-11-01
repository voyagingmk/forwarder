#include "forwardctrl.h"
#include "utils.h"

namespace spd = spdlog;
using namespace std;
using namespace rapidjson;

ForwardCtrl::ForwardCtrl() :
	poolForwardServerENet(sizeof(ForwardServerENet)),
	poolForwardClientENet(sizeof(ForwardClientENet)),
	poolForwardServerWS(sizeof(ForwardServerWS)),
	poolForwardClientWS(sizeof(ForwardClientWS)),
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
	}
	poolForwardServerENet.clear();
	poolForwardClientENet.clear();
	poolForwardServerWS.clear();
	poolForwardClientWS.clear();
	handleFuncs.clear();
}

ForwardServer* ForwardCtrl::createForwardServer(int protocol) {
	if (protocol == Protocol::ENet) {
		return static_cast<ForwardServer*>(poolForwardServerENet.add());
	}
	else if (protocol == Protocol::WS) {
		return static_cast<ForwardServer*>(poolForwardServerWS.add());
	}
	return nullptr;
}

ForwardClient* ForwardCtrl::createForwardClient(int protocol) {
	if (protocol == Protocol::ENet) {
		return static_cast<ForwardClient*>(poolForwardClientENet.add());
	}
	else if (protocol == Protocol::WS) {
		return static_cast<ForwardClient*>(poolForwardClientWS.add());
	}
	return nullptr;
}

void ForwardCtrl::initServers(rapidjson::Value& serversConfig) {
	serverNum = serversConfig.GetArray().Size();
	auto logger = spdlog::get("my_logger");
	UniqIDGenerator idGenerator;
	for (rapidjson::Value& serverConfig : serversConfig.GetArray()) {
		int protocol = serverConfig["protocol"].GetString() == "enet" ? Protocol::ENet : Protocol::WS;
		ForwardServer* server = createForwardServer(protocol);
		server->desc = serverConfig["desc"].GetString();
		server->peerLimit = serverConfig["peers"].GetInt();
		server->admin = (serverConfig.HasMember("admin") ? serverConfig["admin"].GetBool() : false);
		if (serverConfig.HasMember("destId"))
			server->destId = serverConfig["destId"].GetInt();

		server->id = idGenerator.getNewID();
		servers.push_back(server);
		serverDict[server->id] = server;

		if (server->protocol == Protocol::WS) {
			ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(server);
			auto on_message = [&](websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg) {
				std::cout << msg->get_payload() << std::endl;
				onReceived(server);
			};
			auto logger = getLogger();
			auto on_open = [=](websocketpp::connection_hdl hdl) {
				logger->info("on_open");
				UniqID id = wsServer->idGenerator.getNewID();
				ForwardClientWS* client = poolForwardClientWS.add();
				client->id = id;
				client->hdl = hdl;
				wsServer->clients[id] = static_cast<ForwardClient*>(client);
				wsServer->hdlToClientId[hdl] = id;
				logger->info("[c:{0}] connected.", id);
			};

			auto on_close = [=](websocketpp::connection_hdl hdl) {
				logger->info("on_close");
				auto it = wsServer->hdlToClientId.find(hdl);
				if (it != wsServer->hdlToClientId.end()) {
					UniqID id = it->second;
					logger->info("[c:{0}] disconnected.", id);
					wsServer->hdlToClientId.erase(it);
					auto it = wsServer->clients.find(id);
					if (it != wsServer->clients.end()) {
						ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it->second);
						wsServer->clients.erase(it);
						poolForwardClientWS.del(client);
					}
				}
			};
			wsServer->server.set_message_handler(on_message);
			wsServer->server.set_open_handler(on_open);
			wsServer->server.set_close_handler(on_close);
		}

		server->init(serverConfig);
	}

	// init dest host
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


void ForwardCtrl::sendPacket(ForwardParam& param) {
	if (param.server->protocol == Protocol::ENet) {
		ForwardClientENet* client = dynamic_cast<ForwardClientENet*>(param.client);
		enet_peer_send(client->peer, param.channelID, param.packet);
	}
}

void ForwardCtrl::broadcastPacket(ForwardParam& param) {
	if (param.server->protocol == Protocol::ENet) {
		ForwardServerENet* server = dynamic_cast<ForwardServerENet*>(param.server);
		enet_host_broadcast(server->host, param.channelID, param.packet);
	}
}

void ForwardCtrl::onReceived(ForwardServer* server) {

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
		param.packet = outPacket;
		sendPacket(param);
		getLogger()->info("response 1");
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
	
	ForwardServer* outServer = nullptr;
	if (param.server->dest) {
		// prior
		outServer = param.server->dest;
	}
	else {
		int destHostID = inHeader.hostID;
		if (!destHostID)
			return FORWARDER_ERR;
		auto it_server = serverDict.find(destHostID);
		if (it_server == serverDict.end())
			return FORWARDER_ERR;
		outServer = it_server->second;
	}
	int destClientID = 0;
	if (!param.server->dest) {
		destClientID = inHeader.clientID;
	}
	if (destClientID) {
		//single send
		auto it_client = outServer->clients.find(destClientID);
		if (it_client == outServer->clients.end())
			return FORWARDER_ERR;
		ForwardClient* outClient = it_client->second;
		ENetPacket * outPacket = param.packet;
		memcpy(outPacket->data, &outHeader, sizeof(ForwardHeader));
		param.client = outClient;
		sendPacket(param);
	}
	else {
		//broadcast
		ENetPacket * outPacket = param.packet;
		memcpy(outPacket->data, &outHeader, sizeof(ForwardHeader));
		// broadcast the incoming packet to dest host's peers
		param.server = outServer;
		broadcastPacket(param);
	}
	getLogger()->info("forwarded 2");
	return FORWARDER_OK;
}

bool ForwardCtrl::handlePacket_3(ForwardParam& param) {

	return FORWARDER_OK;
}
bool ForwardCtrl::handlePacket_4(ForwardParam& param) {
	return FORWARDER_OK;
}

void  ForwardCtrl::onReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID) {
	getLogger()->info("[cli:{0}][c:{1}][len:{2}]",
		client->id,
		channelID,
		inPacket->dataLength);
	ForwardHeader header;
	bool err = getHeader(&header, inPacket);
	if (err) {
		getLogger()->warn("[onReceived] getHeader err");
		return;
	}
	const char * content = (const char*)(inPacket->data) + sizeof(header);
	getLogger()->info("[data]{0}", content);
	auto it = handleFuncs.find(header.getProtocol());
	if (it == handleFuncs.end()) {
		getLogger()->warn("[onReceived] wrong protocol:{0}", header.getProtocol());
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
		for (ForwardServer* it_server : servers) {
			if (it_server->protocol == Protocol::ENet) {
				ForwardServerENet* server = dynamic_cast<ForwardServerENet*>(it_server);
				ret = enet_host_service(server->host, &event, 5);
				while (ret > 0)
				{
					getLogger()->info("event.type = {}", event.type);
					switch (event.type) {
					case ENET_EVENT_TYPE_CONNECT: {
						UniqID id = server->idGenerator.getNewID();
						ForwardClientENet* client = poolForwardClientENet.add();
						client->id = id;
						client->peer = event.peer;
						event.peer->data = client;
						server->clients[id] = static_cast<ForwardClient*>(client);
						getLogger()->info("[c:{0}] connected, from {1}:{2}.",
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
						ForwardClientENet* client = (ForwardClientENet*)event.peer->data;
						getLogger()->info("[c:{0}] disconnected.",
							client->id);
						event.peer->data = nullptr;
						auto it = server->clients.find(client->id);
						if (it != server->clients.end())
							server->clients.erase(it);
						poolForwardClientENet.del(client);
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
			else if (it_server->protocol == Protocol::WS) {
				ForwardServerWS* server = dynamic_cast<ForwardServerWS*>(it_server);
				server->poll();
			}
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
			Value id(server->id);
			add("id", id);
			Value destId(server->destId);
			add("destId", destId);
			Value desc;
			desc.SetString(server->desc.c_str(), server->desc.size(), d.GetAllocator());
			add("desc", desc);
			Value peerLimit(server->peerLimit);
			add("peerLimit", peerLimit);
			if (server->protocol == Protocol::ENet) {
				ForwardServerENet* enetserver = dynamic_cast<ForwardServerENet*>(server);
				Value channelLimit(enetserver->host->channelLimit);
				add("channels", channelLimit);
				Value port(enetserver->host->address.port);
				add("port", port);
			}
			else if (server->protocol == Protocol::WS) {
			}

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