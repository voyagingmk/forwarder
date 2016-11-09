#include "forwardctrl.h"
#include "utils.h"
#include "base64.h"
#include "aes_ctr.h"
#include "aes.h"

namespace spd = spdlog;
using namespace std;
using namespace rapidjson;
using namespace forwarder;

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
	for (rapidjson::Value& serverConfig : serversConfig.GetArray()) {
		createServer(serverConfig);
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


uint32_t ForwardCtrl::createServer(rapidjson::Value& serverConfig) {
	auto logger = spdlog::get("my_logger");
	int protocol = strcmp(serverConfig["protocol"].GetString(), "enet") == 0 ? NetType::ENet : NetType::WS;
	ForwardServer* server = createForwardServer(protocol);
	server->desc = serverConfig["desc"].GetString();
	server->peerLimit = serverConfig["peers"].GetInt();
	server->admin = serverConfig.HasMember("admin") && serverConfig["admin"].GetBool();
	server->encrypt = serverConfig.HasMember("encrypt") && serverConfig["encrypt"].GetBool();
	server->base64 = serverConfig.HasMember("base64") && serverConfig["base64"].GetBool();

	if (server->encrypt) {
		if (serverConfig.HasMember("encryptkey")) {
			server->initCipherKey(serverConfig["encryptkey"].GetString());
		}
		else {
			logger->error("no encryptkey");
			return -1;
		}
	}

	if (serverConfig.HasMember("destId"))
		server->destId = serverConfig["destId"].GetInt();

	server->id = idGenerator.getNewID();
	servers.push_back(server);
	serverDict[server->id] = server;

	if (server->netType == NetType::WS) {
		ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(server);
		auto logger = getLogger();
		auto on_open = [=](websocketpp::connection_hdl hdl) {
			UniqID id = wsServer->idGenerator.getNewID();
			ForwardClientWS* client = poolForwardClientWS.add();
			client->id = id;
			client->hdl = hdl;
			wsServer->clients[id] = static_cast<ForwardClient*>(client);
			wsServer->hdlToClientId[hdl] = id;
			logger->info("[WS,c:{0}] connected.", id);
		};

		auto on_close = [=](websocketpp::connection_hdl hdl) {
			auto it = wsServer->hdlToClientId.find(hdl);
			if (it != wsServer->hdlToClientId.end()) {
				UniqID id = it->second;
				logger->info("[WS,c:{0}] disconnected.", id);
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

	for (auto it = servers.begin(); it != servers.end(); it++) {
		ForwardServer* _server = *it;
		if (_server->id == server->destId) {
			server->dest = _server;
			break;
		}
	}
	return server->id;
}

uint32_t ForwardCtrl::removeServer(int id) {
	auto it_server = serverDict.find(id);
	if (it_server == serverDict.end()) {
		return -1;
	}
	for (auto it = servers.begin(); it != servers.end(); it++) {
		ForwardServer* server = *it;
		if (server->destId == id) {
			server->dest = nullptr;
		}
	}
	serverDict.erase(it_server);
	return 0;
}

void ForwardCtrl::sendPacket(ForwardParam& param) {
	if (param.server->netType == NetType::ENet) {
		ForwardClientENet* client = dynamic_cast<ForwardClientENet*>(param.client);
		ForwardPacketPtr outPacket = param.packet;
		ENetPacket* packet = static_cast<ENetPacket*>(outPacket->getRawPtr());
		enet_peer_send(client->peer, param.channelID, packet);
	}
	else if (param.server->netType == NetType::WS) {
		ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(param.server);
		ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(param.client);
		wsServer->server.send(client->hdl,
			param.packet->getRawPtr(),
			param.packet->getTotalLength(),
			websocketpp::frame::opcode::value::BINARY);
	}
}

void ForwardCtrl::broadcastPacket(ForwardParam& param) {
	if (param.server->netType == NetType::ENet) {
		ForwardServerENet* enetServer = dynamic_cast<ForwardServerENet*>(param.server);
		ForwardPacketPtr outPacket = param.packet;
		ENetPacket* enetPacket = static_cast<ENetPacket*>(outPacket->getRawPtr());
		for (auto it : enetServer->clients) {
			ForwardClientENet* client = dynamic_cast<ForwardClientENet*>(it.second);
			enet_peer_send(client->peer, param.channelID, enetPacket);
		}
		getLogger()->info("broadcast, len:{0}", enetPacket->dataLength);
	}
	else if (param.server->netType == NetType::WS) {
		ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(param.server);
		for (auto it : wsServer->clients) {
			ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it.second);
			wsServer->server.send(client->hdl,
				param.packet->getRawPtr(),
				param.packet->getTotalLength(),
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
	return nullptr;
}

ForwardPacketPtr ForwardCtrl::createPacket(ENetPacket* packet) {
	return std::make_shared<ForwardPacketENet>(packet);
}

ForwardPacketPtr ForwardCtrl::createPacket(const char* packet) {
	return std::make_shared<ForwardPacketWS>((uint8_t*)(packet));
}


ForwardPacketPtr ForwardCtrl::convertPacket(ForwardPacketPtr packet, ForwardServer* inServer, ForwardServer* outServer) {
	if (inServer->hasConsistConfig(outServer)) {
		return packet;
	}	
	constexpr size_t ivSize = 16;
	Base64Codec& base64 = Base64Codec::get();
	uint8_t * data = packet->getDataPtr();
	size_t dataLength = packet->getDataLength();

	//1. convert to raw
	//1.1 Base64
	if (inServer->base64) {
		size_t newDataLength = 0;
		uint8_t* newData = nullptr;
		base64.toByteArray((const char*)newData, dataLength, newData, &newDataLength);
		data = newData;
		dataLength = newDataLength;
	}

	//1.2 now data is raw or encrypted
	if (inServer->encrypt) { // DO decrypt
		size_t newDataLength = dataLength - ivSize;
		uint8_t* encryptedData = data + ivSize;
		uint8_t* newData = new uint8_t[newDataLength];
		uint8_t* iv = data;
		unsigned char ecount_buf[AES_BLOCK_SIZE];
		unsigned int num = 0;
		AES_ctr128_encrypt(encryptedData, newData, newDataLength, &inServer->encryptkey, iv, ecount_buf, &num);
		if (data != packet->getDataPtr()) {
			delete data;
		}
		data = newData;
		dataLength = newDataLength;
	}
	// now it's raw

	//2. convert raw to target
	//2.1 
	if (outServer->encrypt) { // DO encrypt
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_int_distribution<> dis(0, std::pow(2, 8) - 1);
		uint8_t* newData = new uint8_t[dataLength + ivSize];
		uint8_t* iv = newData;
		uint8_t ivTmp[ivSize];
		for (int i = 0; i < ivSize; i++) {
			iv[i] = dis(gen);
		}
		memcpy(ivTmp, iv, ivSize);
		uint8_t* encryptedData = newData + ivSize;
		unsigned char ecount_buf[AES_BLOCK_SIZE];
		unsigned int num = 0;

		AES_ctr128_encrypt(data, encryptedData, dataLength, &outServer->encryptkey, ivTmp, ecount_buf, &num);
		if (data != packet->getDataPtr()) {
			delete data;
		}
		data = newData;
		dataLength = dataLength + ivSize;
	}

	//2.2 
	std::string b64("");
	if (outServer->base64) {
		b64 = base64.fromByteArray(data, dataLength);
		dataLength = b64.size();
	}

	//3. make packet
	ForwardPacketPtr newPacket = createPacket(outServer->netType, dataLength + sizeof(ForwardHeader));
	if (b64.size() > 0) {
		newPacket->setData((uint8_t*)b64.c_str(), dataLength);
	}
	else {
		newPacket->setData(data, dataLength);
	}

	return newPacket;
}

// system command
ReturnCode ForwardCtrl::handlePacket_1(ForwardParam& param) {
	if(!param.server->admin)
		return ReturnCode::Err;
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
	return ReturnCode::Ok;
}

// has destHostID and has destCID
ReturnCode ForwardCtrl::handlePacket_2(ForwardParam& param) {
	ForwardServer* inServer = param.server;
	ForwardClient* inClient = param.client;
	ForwardPacketPtr inPacket = param.packet;
	ForwardHeader* inHeader = param.header;


	ForwardServer* outServer = getOutServer(inHeader, inServer);
	if (!outServer)
		return ReturnCode::Err;

	ForwardClient* outClient = getOutClient(inHeader, inServer, outServer);

	ForwardPacketPtr outPacket;

	outPacket = convertPacket(inPacket, inServer, outServer);

	if (!outPacket)
		return ReturnCode::Err;

	ForwardHeader outHeader;
	outHeader.protocol = 2;

	if (inHeader->getProtocolFlag(ProtocolFlag::WithAddress)) {
		// add src address to out packet
		outHeader.hostID = param.server->id;
		outHeader.clientID = param.client->id;
	}
	outPacket->setHeader(&outHeader);

	param.header = nullptr;
	param.packet = outPacket;
	param.client = outClient;
	param.server = outServer;

	if (outClient) {
		//single send
		sendPacket(param);
	}
	else {
		// broadcast the incoming packet to dest host's peers
		broadcastPacket(param);
	}
	getLogger()->info("forwarded 2");
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::handlePacket_3(ForwardParam& param) {
	return ReturnCode::Ok;
}
ReturnCode ForwardCtrl::handlePacket_4(ForwardParam& param) {
	return ReturnCode::Ok;
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
	logger->info("[WS,cli:{0}][len:{1}]",
		clientID,
		msg->get_payload().size());
	ForwardHeader header;
	std::string const & payload = msg->get_payload();
	ReturnCode code = getHeader(&header, payload);
	if (code == ReturnCode::Err) {
		getLogger()->warn("[onWSReceived] getHeader err");
		return;
	}
	//getLogger()->info("[data]{0}", content);
	auto it = handleFuncs.find(header.getProtocolType());
	if (it == handleFuncs.end()) {
		getLogger()->warn("[onENetReceived] wrong protocol:{0}", header.getProtocolType());
		return;
	}
	ForwardParam param;
	param.header = &header;
	param.packet = createPacket(NetType::WS, msg->get_payload().size());
	const char * content = payload.data() + sizeof(ForwardHeader);
	param.packet->setData((uint8_t*)content, msg->get_payload().size() - sizeof(ForwardHeader));
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
	ReturnCode err = getHeader(&header, inPacket);
	if (err == ReturnCode::Err) {
		getLogger()->warn("[onENetReceived] getHeader err");
		return;
	}
	const char * content = (const char*)(inPacket->data) + sizeof(header);
	getLogger()->info("[data]{0}", content);
	auto it = handleFuncs.find(header.getProtocolType());
	if (it == handleFuncs.end()) {
		getLogger()->warn("[onENetReceived] wrong protocol:{0}", header.getProtocolType());
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

ReturnCode ForwardCtrl::validHeader(ForwardHeader* header) {
	if (header->version != Version) {
		getLogger()->warn("[validHeader] wrong version {0} != {1}", header->version, Version);
		return ReturnCode::Err;
	}
	if (header->length != sizeof(ForwardHeader)) {
		getLogger()->warn("[validHeader] wrong length {0} != {1}", header->length, sizeof(ForwardHeader));
		return ReturnCode::Err;
	}
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::getHeader(ForwardHeader* header, const std::string& packet) {
	memcpy(header, (void*)packet.c_str(), sizeof(ForwardHeader));
	return validHeader(header);
}



ReturnCode ForwardCtrl::getHeader(ForwardHeader * header, ENetPacket * packet) {
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
							getLogger()->info("[ENet,c:{0}] connected, from {1}:{2}.",
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
							getLogger()->info("[ENet,c:{0}] disconnected.",
								client->id);
							event.peer->data = nullptr;
							auto it = server->clients.find(client->id);
							if (it != server->clients.end())
								server->clients.erase(it);
							poolForwardClientENet.del(client);
							break;
						}
						case ENET_EVENT_TYPE_NONE:
							break;
						}
						if (isExit)
							break;
					}
					else if(ret == 0) {
						break;
					}
					else if (ret < 0) {
#ifdef _MSC_VER
						printf("WSAGetLastError(): %d\n", WSAGetLastError());
#endif
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
				desc: str,
				admin: bool,
				encrypt: bool,
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
			Value isAdmin(server->admin);
			add("admin", isAdmin);
			Value isEncrypt(server->encrypt);
			add("encrypt", isEncrypt);
			if (server->netType == NetType::ENet) {
				ForwardServerENet* enetserver = dynamic_cast<ForwardServerENet*>(server);
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