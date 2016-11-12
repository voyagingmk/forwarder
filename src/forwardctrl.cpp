#include "forwardctrl.h"
#include "utils.h"
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
	buffer(nullptr),
	debug(false),
	base64Codec(Base64Codec::get()),
	isExit(false)
{	//default
	handleFuncs[0] = &ForwardCtrl::handlePacket_SysCmd;
	handleFuncs[2] = &ForwardCtrl::handlePacket_Forward;
	handleFuncs[3] = &ForwardCtrl::handlePacket_Process;
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

void ForwardCtrl::setDebug(bool enabled) {
	debug = enabled;
}

ReturnCode ForwardCtrl::initProtocolMap(rapidjson::Value& protocolConfig) {
	if (protocolConfig.IsNull()) {
		return ReturnCode::Err;
	}
	for (Value::ConstMemberIterator it = protocolConfig.MemberBegin(); it != protocolConfig.MemberEnd(); ++it){
		std::string protocolID = it->name.GetString();
		int protocol = std::stoi(protocolID);
		std::string name = it->value.GetString();
		if (name == "SysCmd") {
			handleFuncs[protocol] = &ForwardCtrl::handlePacket_SysCmd;
		} else if (name == "Forward") {
			handleFuncs[protocol] = &ForwardCtrl::handlePacket_Forward;
		} else if (name == "Process") {
			handleFuncs[protocol] = &ForwardCtrl::handlePacket_Process;
		}
	}
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::sendBinary(UniqID serverId, uint8_t* data, size_t dataLength) {
	ForwardServer * server = getServerByID(serverId);
	if (!server) {
		return ReturnCode::Err;
	}
	ForwardPacketPtr packet = encodeData(server, data, dataLength);
	if (!packet)
		return ReturnCode::Err;
	ForwardHeader outHeader;
	outHeader.protocol = 2;
	packet->setHeader(&outHeader);
	ForwardParam param;
	param.header = nullptr;
	param.packet = packet;
	param.client = nullptr;
	param.server = server;

	broadcastPacket(param);
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::sendText(UniqID serverId, std::string data) {
	ForwardServer * server = getServerByID(serverId);
	if (!server) {
		return ReturnCode::Err;
	}ForwardPacketPtr packet = encodeData(server, (uint8_t*)data.c_str(), data.size());
	if (!packet)
		return ReturnCode::Err;
	ForwardHeader outHeader;
	outHeader.protocol = 2;
	packet->setHeader(&outHeader);
	ForwardParam param;
	param.header = nullptr;
	param.packet = packet;
	param.client = nullptr;
	param.server = server;

	broadcastPacket(param);
	return ReturnCode::Ok;
}


ForwardServer* ForwardCtrl::createServerByNetType(NetType netType) {
	if (netType == NetType::ENet) {
		return static_cast<ForwardServer*>(poolForwardServerENet.add());
	}
	else if (netType == NetType::WS) {
		return static_cast<ForwardServer*>(poolForwardServerWS.add());
	}
	return nullptr;
}

ForwardClient* ForwardCtrl::createClientByNetType(NetType netType) {
	if (netType == NetType::ENet) {
		return static_cast<ForwardClient*>(poolForwardClientENet.add());
	}
	else if (netType == NetType::WS) {
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
	auto logger = getLogger();
	NetType netType = strcmp(serverConfig["netType"].GetString(), "enet") == 0 ? NetType::ENet : NetType::WS;
	ForwardServer* server = createServerByNetType(netType);
	ReturnCode code = server->initCommon(serverConfig);
	if (code == ReturnCode::Err) {
		return code;
	}
	server->id = idGenerator.getNewID();
	servers.push_back(server);
	serverDict[server->id] = server;

	if (server->netType == NetType::WS) {
		ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(server);
		auto on_open = [=](websocketpp::connection_hdl hdl) {
			UniqID id = wsServer->idGenerator.getNewID();
			ForwardClientWS* client = poolForwardClientWS.add();
			client->id = id;
			client->hdl = hdl;
			wsServer->clients[id] = static_cast<ForwardClient*>(client);
			wsServer->hdlToClientId[hdl] = id;
			if (debug) logger->info("[WS,c:{0}] connected.", id);
		};

		auto on_close = [=](websocketpp::connection_hdl hdl) {
			auto it = wsServer->hdlToClientId.find(hdl);
			if (it != wsServer->hdlToClientId.end()) {
				UniqID id = it->second;
				if (debug) logger->info("[WS,c:{0}] disconnected.", id);
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

ReturnCode ForwardCtrl::removeServerByID(UniqID serverId) {
	auto it_server = serverDict.find(serverId);
	if (it_server == serverDict.end()) {
		return ReturnCode::Err;
	}
	for (auto it = servers.begin(); it != servers.end(); it++) {
		ForwardServer* server = *it;
		if (server->destId == serverId) {
			server->dest = nullptr;
		}
	}
	serverDict.erase(it_server);
	return ReturnCode::Ok;
}

ForwardServer* ForwardCtrl::getServerByID(UniqID serverId) const {
	auto it_server = serverDict.find(serverId);
	if (it_server == serverDict.end())
		return nullptr;
	return it_server->second;
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
		if (debug) getLogger()->info("broadcast, len:{0}", enetPacket->dataLength);
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

ForwardPacketPtr ForwardCtrl::encodeData(ForwardServer* outServer, uint8_t* data, size_t dataLength) {
	uint8_t* allocData = nullptr;
	if (outServer->encrypt) {
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_int_distribution<> dis(0, std::pow(2, 8) - 1);
		if (debug) debugBytes("encodeData, originData", data, dataLength);
		allocData = new uint8_t[dataLength + ivSize];
		uint8_t* newData = allocData;
		uint8_t* iv = newData;
		uint8_t ivTmp[ivSize];
		for (int i = 0; i < ivSize; i++) {
			iv[i] = dis(gen);
		}
		memcpy(ivTmp, iv, ivSize);
		if (debug) debugBytes("encodeData, iv", iv, ivSize);
		uint8_t* encryptedData = newData + ivSize;
		unsigned char ecount_buf[AES_BLOCK_SIZE];
		unsigned int num = 0;
		AES_ctr128_encrypt(data, encryptedData, dataLength, &outServer->encryptkey, ivTmp, ecount_buf, &num);
		if (debug) debugBytes("encodeData, encryptedData", encryptedData, dataLength);
		data = newData;
		dataLength = dataLength + ivSize;
	}

	std::string b64("");
	if (outServer->base64) {
		b64 = base64Codec.fromByteArray(data, dataLength);
		data = (uint8_t*)b64.c_str();
		dataLength = b64.size();
		if (debug) debugBytes("encodeData, b64", data, dataLength);
	}

	//3. make packet
	ForwardPacketPtr newPacket = createPacket(outServer->netType, dataLength + sizeof(ForwardHeader));
	// copy
	newPacket->setData(data, dataLength);

	if (allocData) {
		delete allocData;
	}
	return newPacket;
}

ForwardPacketPtr ForwardCtrl::decodeData(ForwardServer* inServer, uint8_t* data, size_t dataLength) {
	uint8_t* allocData1 = nullptr;
	uint8_t* allocData2 = nullptr;

	//1. convert to raw
	//1.1 Base64
	if (inServer->base64) {
		if (debug) debugBytes("decodeData, originData", data, dataLength);
		size_t newDataLength = base64Codec.calculateDataLength((const char*)data, dataLength);
		uint8_t* allocData1 = new uint8_t[newDataLength];
		uint8_t* newData = allocData1;
		base64Codec.toByteArray((const char*)data, dataLength, newData, &newDataLength);
		data = newData;
		dataLength = newDataLength;
		if (debug) debugBytes("decodeData, base64decoded Data", data, dataLength);
	}

	//1.2 now data is raw or encrypted
	if (inServer->encrypt) { // DO decrypt
		size_t newDataLength = dataLength - ivSize;
		uint8_t* encryptedData = data + ivSize;
		allocData2 = new uint8_t[newDataLength];
		uint8_t* newData = allocData2;
		uint8_t* iv = data;
		unsigned char ecount_buf[AES_BLOCK_SIZE];
		unsigned int num = 0;
		AES_ctr128_encrypt(encryptedData, newData, newDataLength, &inServer->encryptkey, iv, ecount_buf, &num);
		data = newData;
		dataLength = newDataLength;
		if (debug) debugBytes("decodeData, decrypt Data", data, dataLength);
	}

	//3. make packet
	ForwardPacketPtr newPacket = createPacket(inServer->netType, dataLength + sizeof(ForwardHeader));
	// copy
	newPacket->setData(data, dataLength);

	if (allocData1) {
		delete allocData1;
		allocData1 = nullptr;
	}
	if (allocData2) {
		delete allocData2;
		allocData1 = nullptr;
	}
	return newPacket;
	// now data pointer is raw
}

ForwardPacketPtr ForwardCtrl::convertPacket(ForwardPacketPtr packet, ForwardServer* inServer, ForwardServer* outServer) {
	if (inServer->hasConsistConfig(outServer)) {
		return packet;
	}
	uint8_t * data = packet->getDataPtr();
	size_t dataLength = packet->getDataLength();
	ForwardPacketPtr rawPacket = decodeData(inServer, data, dataLength);
	ForwardPacketPtr outPacket = encodeData(outServer, rawPacket->getDataPtr(), rawPacket->getDataLength());
	return outPacket;
}

// system command
ReturnCode ForwardCtrl::handlePacket_SysCmd(ForwardParam& param) {
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
		if(debug) getLogger()->info("SysCmd finish");
	}
	else if (subID == 2){
		//force disconnect
	}
	return ReturnCode::Ok;
}

// has destHostID and has destCID
ReturnCode ForwardCtrl::handlePacket_Forward(ForwardParam& param) {
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
	if (debug) getLogger()->info("forward finish");
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::handlePacket_Process(ForwardParam& param) {
	ForwardServer* inServer = param.server;
	ForwardPacketPtr inPacket = param.packet;
	uint8_t * data = inPacket->getDataPtr();
	size_t dataLength = inPacket->getDataLength();
	ForwardPacketPtr rawPacket = decodeData(inServer, data, dataLength);
	return ReturnCode::Ok;
}

void ForwardCtrl::onWSReceived(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg) {
	auto logger = getLogger(); 
	auto it1 = wsServer->hdlToClientId.find(hdl);
	if (it1 == wsServer->hdlToClientId.end()) {
		if (debug) logger->error("[onWSReceived] no such hdl");
		return;
	}
	UniqID clientID = it1->second;
	auto it2 = wsServer->clients.find(clientID);
	if (it2 == wsServer->clients.end()) {
		if (debug) logger->error("[onWSReceived] no such clientID:{0}",
			clientID);
		return;
	}
	ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it2->second);
	if (debug) logger->info("[WS,cli:{0}][len:{1}]",
								clientID,
								msg->get_payload().size());
	ForwardHeader header;
	std::string const & payload = msg->get_payload();
	ReturnCode code = getHeader(&header, payload);
	if (code == ReturnCode::Err) {
		if (debug) getLogger()->warn("[onWSReceived] getHeader err");
		return;
	}
	//getLogger()->info("[data]{0}", content);
	auto it = handleFuncs.find(header.getProtocolType());
	if (it == handleFuncs.end()) {
		if (debug) getLogger()->warn("[onENetReceived] wrong protocol:{0}", header.getProtocolType());
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
	if (debug) getLogger()->info("[cli:{0}][c:{1}][len:{2}]",
									client->id,
									channelID,
									inPacket->dataLength);
	ForwardHeader header;
	ReturnCode err = getHeader(&header, inPacket);
	if (err == ReturnCode::Err) {
		if (debug) getLogger()->warn("[onENetReceived] getHeader err");
		return;
	}
	const char * content = (const char*)(inPacket->data) + sizeof(header);
	if (debug) getLogger()->info("[onENetReceived] data:{0}", content);
	auto it = handleFuncs.find(header.getProtocolType());
	if (debug) getLogger()->info("[onENetReceived] protocol:{0}", header.getProtocolType());
	if (it == handleFuncs.end()) {
		if (debug) getLogger()->warn("[onENetReceived] wrong protocol:{0}", header.getProtocolType());
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
		if (debug) getLogger()->warn("[validHeader] wrong version {0} != {1}", header->version, Version);
		return ReturnCode::Err;
	}
	if (header->length != sizeof(ForwardHeader)) {
		if (debug) getLogger()->warn("[validHeader] wrong length {0} != {1}", header->length, sizeof(ForwardHeader));
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

ForwardClient* ForwardCtrl::getOutClient(ForwardHeader* inHeader, ForwardServer* inServer, ForwardServer* outServer) const {
	ForwardClient* outClient = nullptr;
	if (!inServer->dest) {
		// only use inHeader->clientID when inServer has no destServer
		int clientID = inHeader->clientID;
		auto it_client = outServer->clients.find(clientID);
		if (it_client == outServer->clients.end())
			return nullptr;
		outClient = it_client->second;
	}
	return outClient;
}

ForwardServer* ForwardCtrl::getOutServer(ForwardHeader* inHeader, ForwardServer* inServer) const {
	ForwardServer* outServer = nullptr;
	if (inServer->dest) {
		outServer = inServer->dest;
	}
	else {
		int destHostID = inHeader->hostID;
		if (!destHostID)
			return nullptr;
		outServer = getServerByID(destHostID);
	}
	return outServer;
}

void ForwardCtrl::pollOnce() {
	ENetEvent event;
	
	for (ForwardServer* it_server : servers) {
		if (it_server->netType == NetType::ENet) {
			ForwardServerENet* server = dynamic_cast<ForwardServerENet*>(it_server);
			do {
				int ret = enet_host_service(server->host, &event, 5);
				if (ret > 0) {
					if (debug) getLogger()->info("event.type = {}", event.type);
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
						if (debug) getLogger()->info("[ENet,c:{0}] connected, from {1}:{2}.",
														client->id,
														str,
														event.peer->address.port);
						sendText(server->id, "hello");
						break;
					}
					case ENET_EVENT_TYPE_RECEIVE: {
						ForwardClient* client = (ForwardClient*)event.peer->data;
						ENetPacket * inPacket = event.packet;
						onENetReceived(server, client, inPacket, event.channelID);
						break;
					}
					case ENET_EVENT_TYPE_DISCONNECT: {
						ForwardClientENet* client = event.peer->data?(ForwardClientENet*)event.peer->data: nullptr;
						if (client) {
							if(debug) getLogger()->info("[ENet,c:{0}] disconnected.", client->id);
							event.peer->data = nullptr;
							auto it = server->clients.find(client->id);
							if (it != server->clients.end())
								server->clients.erase(it);
							poolForwardClientENet.del(client);
						}
						if (server->isClient) {
							server->doReconect();
						}
						break;
					}
					case ENET_EVENT_TYPE_NONE:
						break;
					}
					if (isExit)
						break;
				}
				else if (ret == 0) {
					break;
				}
				else if (ret < 0) {
#ifdef _MSC_VER
					getLogger()->error("WSAGetLastError(): {0}\n", WSAGetLastError());
#endif
					break;
				}
			} while (true);
			//std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		else if (it_server->netType == NetType::WS) {
			ForwardServerWS* server = dynamic_cast<ForwardServerWS*>(it_server);
			server->poll();
		}
	}
}


void ForwardCtrl::loop() {
	while (!isExit) {
		pollOnce();
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

Document ForwardCtrl::stat() const {
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