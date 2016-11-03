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
	if (protocol == NetType::ENet) {
		return static_cast<ForwardServer*>(poolForwardServerENet.add());
	}
	else if (protocol == NetType::WS) {
		return static_cast<ForwardServer*>(poolForwardServerWS.add());
	}
	return nullptr;
}

ForwardClient* ForwardCtrl::createForwardClient(int protocol) {
	if (protocol == NetType::ENet) {
		return static_cast<ForwardClient*>(poolForwardClientENet.add());
	}
	else if (protocol == NetType::WS) {
		return static_cast<ForwardClient*>(poolForwardClientWS.add());
	}
	return nullptr;
}

void ForwardCtrl::initServers(rapidjson::Value& serversConfig) {
	serverNum = serversConfig.GetArray().Size();
	auto logger = spdlog::get("my_logger");
	UniqIDGenerator idGenerator;
	for (rapidjson::Value& serverConfig : serversConfig.GetArray()) {
		int protocol = strcmp(serverConfig["protocol"].GetString(), "enet") == 0 ? NetType::ENet : NetType::WS;
		ForwardServer* server = createForwardServer(protocol);
		server->desc = serverConfig["desc"].GetString();
		server->peerLimit = serverConfig["peers"].GetInt();
		server->admin = (serverConfig.HasMember("admin") ? serverConfig["admin"].GetBool() : false);
		if (serverConfig.HasMember("destId"))
			server->destId = serverConfig["destId"].GetInt();

		server->id = idGenerator.getNewID();
		servers.push_back(server);
		serverDict[server->id] = server;

		if (server->netType == NetType::WS) {
			ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(server);
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
			wsServer->server.set_message_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSReceived, 
				this,
				wsServer,
				websocketpp::lib::placeholders::_1,
				websocketpp::lib::placeholders::_2));
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


void ForwardCtrl::sendPacket(ForwardParam& param) {
	if (param.server->netType == NetType::ENet) {
		ForwardClientENet* client = dynamic_cast<ForwardClientENet*>(param.client);
		ENetPacket* packet = static_cast<ENetPacket*>(param.packet->getRawPtr());
		enet_peer_send(client->peer, param.channelID, packet);
	}
	else if (param.server->netType == NetType::WS) {
		ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(param.server);
		ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(param.client);
		wsServer->server.send(client->hdl,
			param.packet->getRawPtr(),
			param.packet->getLength(),
			websocketpp::frame::opcode::value::BINARY);
	}
}

void ForwardCtrl::broadcastPacket(ForwardParam& param) {
	if (param.server->netType == NetType::ENet) {
		ForwardServerENet* enetServer = dynamic_cast<ForwardServerENet*>(param.server);
		ENetPacket* packet = static_cast<ENetPacket*>(param.packet->getRawPtr());
		enet_host_broadcast(enetServer->host, param.channelID || enetServer->broadcastChannelID, packet);
		getLogger()->info("broadcast");
	}
	else if (param.server->netType == NetType::WS) {
		ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(param.server);
		for (auto it : wsServer->clients) {
			ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it.second);
			wsServer->server.send(client->hdl,
				param.packet->getRawPtr(),
				param.packet->getLength(),
				websocketpp::frame::opcode::value::BINARY);
		}
	}
}

ForwardPacketPtr ForwardCtrl::createPacket(NetType netType, size_t len) {
	if (netType == NetType::ENet) {
		return std::make_shared<ForwardPacketENet>(len);
	}else if (netType == NetType::WS) {
		return std::make_shared<ForwardPacketWS>(len);
	}
}

ForwardPacketPtr ForwardCtrl::createPacket(ENetPacket* packet) {
	return std::make_shared<ForwardPacketENet>(packet);
}

ForwardPacketPtr ForwardCtrl::createPacket(const char* packet) {
	return std::make_shared<ForwardPacketWS>((uint8_t*)(packet));
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
		const char* statJson = buffer.GetString();
		int statJsonLength = strlen(statJson);
		int totalLength = sizeof(ForwardHeader) + statJsonLength + 1;
		ForwardPacketPtr packet = createPacket(param.server->netType, totalLength);
		packet->setHeader(&outHeader);
		packet->setData((uint8_t*)(statJson), statJsonLength);
		param.packet = packet;
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
	
	ForwardPacketPtr outPacket = param.packet;
	outPacket->setHeader(&outHeader);

	ForwardServer* outServer = nullptr;
	if (param.server->dest) {
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
	param.server = outServer;

	int destClientID = 0;
	ForwardClient* destClient = nullptr;
	if (!param.server->dest) {
		destClientID = inHeader.clientID;
		auto it_client = outServer->clients.find(destClientID);
		if (it_client == outServer->clients.end())
			return FORWARDER_ERR;
		destClient = it_client->second;
	}
	if (destClient) {
		//single send
		param.client = destClient;
		sendPacket(param);
	}
	else {
		// broadcast the incoming packet to dest host's peers
		param.client = nullptr;
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


void ForwardCtrl::onWSReceived(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg) {
	auto logger = getLogger(); 
	auto it1 = wsServer->hdlToClientId.find(hdl);
	if (it1 == wsServer->hdlToClientId.end()) {
		logger->error("[onWSReceived] no such hdl");
		return;
	}
	UniqID clientID = it1->second;
	auto it2 = wsServer->clients.find(clientID);
	if (it2 == wsServer->clients.end()) {
		logger->error("[onWSReceived] no such clientID:{0}",
			clientID);
		return;
	}
	ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it2->second);
	logger->info("[cli:{0}][len:{1}]",
		clientID,
		msg->get_payload().size());
	ForwardHeader header;
	bool err = getHeader(&header, msg->get_payload());
	if (err) {
		getLogger()->warn("[onWSReceived] getHeader err");
		return;
	}
	const char * content = msg->get_payload().c_str() + sizeof(header);
	//getLogger()->info("[data]{0}", content);
	auto it = handleFuncs.find(header.getProtocol());
	if (it == handleFuncs.end()) {
		getLogger()->warn("[onENetReceived] wrong protocol:{0}", header.getProtocol());
		return;
	}
	ForwardParam param;
	param.header = &header;
	param.packet = createPacket(NetType::WS, msg->get_payload().size());
	param.packet->setData((uint8_t*)content, msg->get_payload().size() - sizeof(header));
	param.client = client;
	param.server = static_cast<ForwardServer*>(wsServer);
	handlePacketFunc handleFunc = it->second;
	(this->*handleFunc)(param);
}


void  ForwardCtrl::onENetReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID) {
	getLogger()->info("[cli:{0}][c:{1}][len:{2}]",
		client->id,
		channelID,
		inPacket->dataLength);
	ForwardHeader header;
	bool err = getHeader(&header, inPacket);
	if (err) {
		getLogger()->warn("[onENetReceived] getHeader err");
		return;
	}
	const char * content = (const char*)(inPacket->data) + sizeof(header);
	getLogger()->info("[data]{0}", content);
	auto it = handleFuncs.find(header.getProtocol());
	if (it == handleFuncs.end()) {
		getLogger()->warn("[onENetReceived] wrong protocol:{0}", header.getProtocol());
		return;
	}
	ForwardParam param;
	param.header = &header;
	param.channelID = channelID;
	param.packet = createPacket(inPacket);
	param.client = client;
	param.server = server;
	handlePacketFunc handleFunc = it->second;
	(this->*handleFunc)(param);
}

bool ForwardCtrl::validHeader(ForwardHeader* header) {
	if (header->version != FORWARDER_VERSION)
		return FORWARDER_ERR;
	if (header->length != sizeof(ForwardHeader))
		return FORWARDER_ERR;
	return FORWARDER_OK;
}

bool ForwardCtrl::getHeader(ForwardHeader* header, const std::string& packet) {
	memcpy(header, (void*)packet.c_str(), sizeof(ForwardHeader));
	return validHeader(header);
}



bool ForwardCtrl::getHeader(ForwardHeader * header, ENetPacket * packet) {
	memcpy(header, packet->data, sizeof(ForwardHeader));
	return validHeader(header);
}

void ForwardCtrl::loop() {
	ENetEvent event;
	while (!isExit) {
		int ret;
		for (ForwardServer* it_server : servers) {
			if (it_server->netType == NetType::ENet) {
				ForwardServerENet* server = dynamic_cast<ForwardServerENet*>(it_server);
				do {
					ret = enet_host_service(server->host, &event, 5);
					if (ret > 0) {
						getLogger()->info("event.type = {}", event.type);
						switch (event.type) {
						case ENET_EVENT_TYPE_CONNECT: {
							UniqID id = server->idGenerator.getNewID();
							ForwardClientENet* client = poolForwardClientENet.add();
							client->id = id;
							client->peer = event.peer;
							event.peer->data = client;
							server->clients[id] = static_cast<ForwardClient*>(client);
							char str[INET_ADDRSTRLEN];
							inet_ntop(AF_INET, &event.peer->address.host, str, INET_ADDRSTRLEN);
							getLogger()->info("[c:{0}] connected, from {1}:{2}.",
								client->id,
								str,
								event.peer->address.port);
							break;
						}
						case ENET_EVENT_TYPE_RECEIVE: {
							ForwardClient* client = (ForwardClient*)event.peer->data;
							ENetPacket * inPacket = event.packet;
							onENetReceived(server, client, inPacket, event.channelID);
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
					else {
						break;
					}
				} while (true);
				//std::this_thread::sleep_for(std::chrono::milliseconds(20));
				if (isExit)
					break;
			}
			else if (it_server->netType == NetType::WS) {
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
			if (server->netType == NetType::ENet) {
				ForwardServerENet* enetserver = dynamic_cast<ForwardServerENet*>(server);
				Value channelLimit(int(enetserver->host->channelLimit));
				add("channels", channelLimit);
				Value port(enetserver->host->address.port);
				add("port", port);
			}
			else if (server->netType == NetType::WS) {
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
		Value peers(int(server->clients.size()));
		addToServer("peers", peers);
		lstServers.PushBack(dServer.Move(), d.GetAllocator());
	}
	d.AddMember("servers", lstServers.Move(), d.GetAllocator());
	return d;
}