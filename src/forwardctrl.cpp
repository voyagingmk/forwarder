#include "forwardctrl.h"
#include "utils.h"
#include "aes_ctr.h"
#include "aes.h"

namespace spd = spdlog;
using namespace std;
using namespace rapidjson;
using namespace forwarder;


size_t ForwardCtrl::bufferNum = 4;
UniqID ForwardCtrl::ForwardCtrlCount = 0;

ForwardCtrl::ForwardCtrl() :
	poolForwardServerENet(sizeof(ForwardServerENet)),
	poolForwardClientENet(sizeof(ForwardClientENet)),
	poolForwardServerWS(sizeof(ForwardServerWS)),
	poolForwardClientWS(sizeof(ForwardClientWS)),
	serverNum(0),
	debug(false),
	base64Codec(Base64Codec::get()),
	isExit(false),
	curProcessServer(nullptr),
	curProcessClient(nullptr),
	curProcessHeader(nullptr),
	curProcessData(nullptr),
	curProcessDataLength(0),
	logger(nullptr),
	id(0)
{
	buffers = new uint8_t*[bufferNum];
	bufferSize = new size_t[bufferNum];
	for (size_t i = 0; i < bufferNum; i++) {
		bufferSize[i] = 0xff;
		buffers[i] = new uint8_t[bufferSize[i]]{ 0 };
	}
	//default
	handleFuncs[0] = &ForwardCtrl::handlePacket_SysCmd;
	handleFuncs[2] = &ForwardCtrl::handlePacket_Forward;
	handleFuncs[3] = &ForwardCtrl::handlePacket_Process;
	id = ++ForwardCtrlCount;
}


ForwardCtrl::~ForwardCtrl() {
	for (size_t i = 0; i < bufferNum; i++) {
		if (buffers[i]) {
			delete[] buffers[i];
		}
	}
	delete[] buffers;
	delete[] bufferSize;
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


void ForwardCtrl::setupLogger(const char* filename) {
	std::vector<spdlog::sink_ptr> sinks;
	if (filename) {
		sinks.push_back(make_shared<spdlog::sinks::rotating_file_sink_st>(filename, "txt", 1048576 * 5, 3));
	}
	//sinks.push_back(make_shared<spdlog::sinks::daily_file_sink_st>(filename, "txt", 0, 0));
#ifdef _MSC_VER
	sinks.push_back(make_shared<spdlog::sinks::wincolor_stdout_sink_st>());
#else
	sinks.push_back(make_shared<spdlog::sinks::stdout_sink_st>());
#endif
	std::string name("ctrl" + std::to_string(id));
	logger = make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
	if (spdlog::get(name)) {
		spdlog::drop(name);
	}
	spdlog::register_logger(logger);
	logger->flush_on(spdlog::level::err);
	spdlog::set_pattern("[%D %H:%M:%S:%e][%l] %v");
	spdlog::set_level(spdlog::level::info);
	logInfo("logger created successfully.");
}

void ForwardCtrl::setDebug(bool enabled) {
	debug = enabled;
	if(logger) logger->set_level(spdlog::level::debug);
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

ReturnCode ForwardCtrl::sendBinary(UniqID serverId, UniqID clientId, uint8_t* data, size_t dataLength) {
	ForwardServer* outServer = getServerByID(serverId);
	if (!outServer) {
		return ReturnCode::Err;
	}
	ForwardClient* outClient = nullptr;
	if (clientId) {
		auto it_client = outServer->clients.find(clientId);
		if (it_client != outServer->clients.end())
			outClient = it_client->second;
	}
	ForwardHeader outHeader;
	outHeader.setProtocol(2);
	outHeader.cleanFlag();
	if (outServer->base64)
		outHeader.setFlag(HeaderFlag::Base64, true);
	if (outServer->encrypt)
		outHeader.setFlag(HeaderFlag::Encrypt, true);
	if (outServer->compress)
		outHeader.setFlag(HeaderFlag::Compress, true);
	outHeader.resetHeaderLength();
	ForwardPacketPtr packet = encodeData(outServer, &outHeader, data, dataLength);
	if (!packet)
		return ReturnCode::Err;
	ForwardParam param;
	param.header = nullptr;
	param.packet = packet;
	param.client = outClient;
	param.server = outServer;
	if (outClient) {
		sendPacket(param);
	}
	else {
		broadcastPacket(param);
	}
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::sendText(UniqID serverId, UniqID clientId, std::string data) {
	return sendBinary(serverId, clientId, (uint8_t*)data.c_str(), data.size());
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
		if (!destId){
			logDebug("Server[{0}] has no destId");
			continue;
		}
		for (auto it2 = servers.begin(); it2 != servers.end(); it2++) {
			ForwardServer* _server = *it2;
			if (_server->id == destId) {
				server->dest = _server;
				logDebug("Server[{0}] -> Server[{1}]", server->id, _server->id);
				break;
			}
		}
		if (!server->dest){
			logDebug("Server[{0}] has no dest server", server->id);
		}
	}
}

uint32_t ForwardCtrl::createServer(rapidjson::Value& serverConfig) {
	NetType netType = strcmp(serverConfig["netType"].GetString(), "enet") == 0 ? NetType::ENet : NetType::WS;
	ForwardServer* server = createServerByNetType(netType);
	ReturnCode code = server->initCommon(serverConfig);
	if (code == ReturnCode::Err) {
		return code;
	}
	server->id = serverConfig["id"].GetInt();
	servers.push_back(server);
	serverDict[server->id] = server;

	if (server->netType == NetType::WS) {
		ForwardServerWS* wsServer = dynamic_cast<ForwardServerWS*>(server);
		if (!wsServer->isClientMode) {
			wsServer->server.set_message_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSReceived,
				this,
				wsServer,
				websocketpp::lib::placeholders::_1,
				websocketpp::lib::placeholders::_2));
			wsServer->server.set_open_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSConnected,
				this,
				wsServer,
				websocketpp::lib::placeholders::_1));
			wsServer->server.set_close_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSDisconnected,
				this,
				wsServer,
				websocketpp::lib::placeholders::_1));
		}
		else {
			wsServer->serverAsClient.set_message_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSReceived,
				this,
				wsServer,
				websocketpp::lib::placeholders::_1,
				websocketpp::lib::placeholders::_2));
			wsServer->serverAsClient.set_open_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSConnected,
				this,
				wsServer,
				websocketpp::lib::placeholders::_1));
			wsServer->serverAsClient.set_close_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSDisconnected,
				this,
				wsServer,
				websocketpp::lib::placeholders::_1));
		}
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

void ForwardCtrl::registerCallback(Event evt, eventCallback callback) {

}

void ForwardCtrl::sendPacket(ForwardParam& param) {
	if (param.server->netType == NetType::ENet) {
		ForwardClientENet* client = dynamic_cast<ForwardClientENet*>(param.client);
		ForwardPacketPtr outPacket = param.packet;
		ENetPacket* packet = static_cast<ENetPacket*>(outPacket->getRawPtr());
		uint8_t channelID = 0;
		enet_peer_send(client->peer, channelID, packet);
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
			uint8_t channelID = 0;
			enet_peer_send(client->peer, channelID, enetPacket);
		}
		logDebug("enet.broadcast, len:{0}, clientNum:{1}", enetPacket->dataLength, enetServer->clients.size());
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
		logDebug("ws.broadcast, len:{0}, clientNum:{1}", param.packet->getTotalLength(), wsServer->clients.size());
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

ForwardPacketPtr ForwardCtrl::createPacket(const std::string& packet) {
	return std::make_shared<ForwardPacketWS>(packet);
}

ForwardPacketPtr ForwardCtrl::createPacket(ENetPacket* packet) {
	return std::make_shared<ForwardPacketENet>(packet);
}

ForwardPacketPtr ForwardCtrl::encodeData(
	ForwardServer* outServer, ForwardHeader* outHeader, 
	uint8_t* data, size_t dataLength) 
{
	if (debug) debugBytes("encodeData, raw Data", data, dataLength);
	if (outServer->compress) {
		size_t bufferLen = compressBound(dataLength);
		uint8_t* newData = getBuffer(0, bufferLen);
		uLongf realLen;
		outHeader->setUncompressedSize(dataLength);// used for uncompression
		compress((Bytef*)newData, &realLen, data, dataLength);
		data = newData;
		dataLength = realLen;
		if (debug) debugBytes("encodeData, compressed", data, dataLength);
	}

	if (outServer->encrypt) {
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_int_distribution<> dis(0, int(std::pow(2, 8)) - 1);
		uint8_t* newData = getBuffer(1, dataLength + ivSize);
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
		data = newData;
		dataLength = dataLength + ivSize;
		if (debug) debugBytes("encodeData, encrypted", data, dataLength);
	}

	std::string b64("");
	if (outServer->base64) {
		b64 = base64Codec.fromByteArray(data, dataLength);
		data = (uint8_t*)b64.c_str();
		dataLength = b64.size();
		if (debug) debugBytes("encodeData, b64", data, dataLength);
	}

	//3. make packet
	ForwardPacketPtr newPacket = createPacket(outServer->netType, outHeader->getHeaderLength() + dataLength);
	// copy
	newPacket->setHeader(outHeader);
	newPacket->setData(data, dataLength);
	if (debug) debugBytes("encodeData, final", (uint8_t*)newPacket->getHeaderPtr(), newPacket->getTotalLength());
	return newPacket;
}

void ForwardCtrl::decodeData(ForwardServer* inServer, ForwardHeader* inHeader, uint8_t* data, size_t dataLength, uint8_t* &outData, size_t& outDataLength) {
	outData = data;
	outDataLength = dataLength;
	if (debug) debugBytes("decodeData, inHeader", inHeader->data, inHeader->getHeaderLength());
	if (inHeader->isFlagOn(HeaderFlag::Base64)) {
		if (debug) debugBytes("decodeData, originData", data, dataLength);
		size_t newDataLength = base64Codec.calculateDataLength((const char*)data, dataLength);
		uint8_t* newData = getBuffer(0, newDataLength);
		base64Codec.toByteArray((const char*)data, dataLength, newData, &newDataLength);
		outData = newData;
		outDataLength = newDataLength;
		if (debug) debugBytes("decodeData, base64decoded Data", outData, outDataLength);
	}

	if (inHeader->isFlagOn(HeaderFlag::Encrypt)) { // DO decrypt
		size_t newDataLength = outDataLength - ivSize;
		uint8_t* encryptedData = outData + ivSize;
		uint8_t* newData = getBuffer(1, newDataLength);
		uint8_t* iv = outData;
		unsigned char ecount_buf[AES_BLOCK_SIZE];
		unsigned int num = 0;
		AES_ctr128_encrypt(encryptedData, newData, newDataLength, &inServer->encryptkey, iv, ecount_buf, &num);
		outData = newData;
		outDataLength = newDataLength;
		if (debug) debugBytes("decodeData, decrypted Data", outData, outDataLength);
	}


	if (inHeader->isFlagOn(HeaderFlag::Compress)) {
		uLongf bufferLen = inHeader->getUncompressedSize();
		uint8_t* newData = getBuffer(2, bufferLen);
		uLongf realLen = bufferLen;
		int ret = uncompress((Bytef*)newData, &realLen, outData, outDataLength);
		logInfo("uncompress, bufferLen={0},realLen={1},outDataLength={2}",
			bufferLen, realLen, outDataLength);
		if (ret == Z_OK) {
			outData = newData;
			outDataLength = realLen;
			if (debug) debugBytes("decodeData, uncompressed Data", outData, outDataLength);
		}
		else {
			logError("uncompress failed");
			if (ret == Z_MEM_ERROR)
				logError("Z_MEM_ERROR");
			else if (ret == Z_BUF_ERROR)
				logError("Z_BUF_ERROR");
			else if (ret == Z_DATA_ERROR)
				logError("Z_DATA_ERROR");
		}
	}

}

ForwardPacketPtr ForwardCtrl::convertPacket(ForwardPacketPtr packet, ForwardServer* inServer, ForwardServer* outServer, ForwardHeader* outHeader) {
	uint8_t* rawData;
	size_t rawDataLength;
	decodeData(
		inServer, packet->getHeader(),
		packet->getDataPtr(), packet->getDataLength(),
		rawData, rawDataLength);
	logDebug("raw data:{0}", rawData);
	ForwardPacketPtr outPacket = encodeData(outServer, outHeader, rawData, rawDataLength);
	return outPacket;
}

ReturnCode ForwardCtrl::handlePacket_SysCmd(ForwardParam& param) {
	if(!param.server->admin)
		return ReturnCode::Err;
	ForwardHeader outHeader;
	outHeader.setProtocol(1);
	int subID = param.header->getSubID();
	if (subID == 1) {
		//stat
		const rapidjson::Document& d = stat();
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);
		const char* statJson = buffer.GetString();
		int statJsonLength = strlen(statJson);
		int totalLength = outHeader.getHeaderLength() + statJsonLength + 1;
		ForwardPacketPtr packet = createPacket(param.server->netType, totalLength);
		packet->setHeader(&outHeader);
		packet->setData((uint8_t*)(statJson), statJsonLength);
		param.packet = packet;
		sendPacket(param);
		logInfo("SysCmd finish");
	}
	else if (subID == 2){
		//force disconnect
	}
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::handlePacket_Forward(ForwardParam& param) {
	logDebug("forward begin");

	ForwardServer* inServer = param.server;
	ForwardClient* inClient = param.client;
	ForwardPacketPtr inPacket = param.packet;
	ForwardHeader* inHeader = param.header;

	ForwardServer* outServer = getOutServer(inHeader, inServer);
	if (!outServer) {
		logWarn("[forward] no outServer");
		return ReturnCode::Err;
	}

	ForwardClient* outClient = getOutClient(inHeader, inServer, outServer);

	ForwardHeader outHeader;
	outHeader.setProtocol(2);
	outHeader.cleanFlag();
	// Default flag
	outHeader.setFlag(HeaderFlag::IP, true);
	outHeader.setFlag(HeaderFlag::HostID, true);
	outHeader.setFlag(HeaderFlag::ClientID, true);

	if (outHeader.isFlagOn(HeaderFlag::IP)) {
		outHeader.setIP(inClient->ip);
	}
	if (outHeader.isFlagOn(HeaderFlag::HostID)) {
		outHeader.setHostID(param.server->id);
	}
	if (outHeader.isFlagOn(HeaderFlag::ClientID)) {
		outHeader.setClientID(param.client->id);
	}

	ForwardPacketPtr outPacket;

	outPacket = convertPacket(inPacket, inServer, outServer, &outHeader);

	if (!outPacket) {
		logWarn("[forward] convertPacket failed.");
		return ReturnCode::Err;
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
	logDebug("forward finish");
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::handlePacket_Process(ForwardParam& param) {
	ForwardServer* inServer = param.server;
	ForwardClient* inClient = param.client;
	ForwardPacketPtr inPacket = param.packet;
	ForwardHeader* inHeader = inPacket->getHeader();
	uint8_t * data = inPacket->getDataPtr();
	size_t dataLength = inPacket->getDataLength();
	decodeData(inServer, inHeader, data, dataLength, curProcessData, curProcessDataLength);
	curProcessServer = inServer;
	curProcessClient = inClient;
	curProcessHeader = inHeader;
	curEvent = Event::Message;
	return ReturnCode::Ok;
}

void ForwardCtrl::onWSConnected(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl) {
	ForwardServerWS::WebsocketServer::connection_ptr con = wsServer->server.get_con_from_hdl(hdl);
	UniqID id = wsServer->idGenerator.getNewID();
	ForwardClientWS* client = poolForwardClientWS.add();
	client->id = id;
	client->hdl = hdl;
	std::string host = con->get_host();
	uint16_t port = con->get_port();
	if (host == "localhost")
		host = "127.0.0.1";
	asio::ip::address_v4::bytes_type ip = asio::ip::address_v4::from_string(host).to_bytes();
	memcpy(&client->ip, ip.data(), 4);
	wsServer->clients[id] = static_cast<ForwardClient*>(client);
	wsServer->hdlToClientId[hdl] = id;
	logDebug("[WS,c:{0}] connected, from {1}:{2}", id, host, port);
	logDebug("ip = {0}", client->ip);
	curEvent = Event::Connected;
	curProcessClient = client;
	sendText(wsServer->id, 0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
			aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

void ForwardCtrl::onWSDisconnected(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl) {
	auto it = wsServer->hdlToClientId.find(hdl);
	if (it != wsServer->hdlToClientId.end()) {
		UniqID id = it->second;
		logDebug("[WS,c:{0}] disconnected.", id);
		wsServer->hdlToClientId.erase(it);
		auto it = wsServer->clients.find(id);
		if (it != wsServer->clients.end()) {
			ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it->second);
			wsServer->clients.erase(it);
			poolForwardClientWS.del(client);
			curProcessClient = client;
		}
	}
	curEvent = Event::Disconnected;
}

void ForwardCtrl::onWSReceived(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg) {
	auto it1 = wsServer->hdlToClientId.find(hdl);
	if (it1 == wsServer->hdlToClientId.end()) {
		logError("[onWSReceived] no such hdl");
		return;
	}
	UniqID clientID = it1->second;
	auto it2 = wsServer->clients.find(clientID);
	if (it2 == wsServer->clients.end()) {
		logError("[onWSReceived] no such clientID:{0}",
			clientID);
		return;
	}
	ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it2->second);
	logDebug("[WS,cli:{0}][len:{1}]",
								clientID,
								msg->get_payload().size());
	ForwardHeader header;
	const std::string& payload = msg->get_payload();
	ReturnCode code = getHeader(&header, payload);
	if (code == ReturnCode::Err) {
		logWarn("[onWSReceived] getHeader err");
		return;
	}
	auto it = handleFuncs.find(header.getProtocol());
	if (it == handleFuncs.end()) {
		logWarn("[onENetReceived] wrong protocol:{0}", header.getProtocol());
		return;
	}
	ForwardParam param;
	param.header = &header;
	param.packet = createPacket(payload);
	param.client = client;
	param.server = static_cast<ForwardServer*>(wsServer);
	handlePacketFunc handleFunc = it->second;
	(this->*handleFunc)(param);
}

void ForwardCtrl::onENetConnected(ForwardServer* server, ENetPeer* peer) {
	UniqID id = server->idGenerator.getNewID();
	ForwardClientENet* client = poolForwardClientENet.add();

	client->id = id;
	client->peer = peer;
	client->ip = peer->address.host;
	peer->data = client;
	server->clients[id] = static_cast<ForwardClient*>(client);
	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &peer->address.host, str, INET_ADDRSTRLEN);
	curEvent = Event::Connected;
	if (server->isClientMode) {
		server->clientID = id;
	}
	logDebug("[ENet,c:{0}] connected, from {1}:{2}.",
		client->id,
		str,
		peer->address.port);
	logDebug("ip = {0}", client->ip);
	curProcessClient = client;
	//sendText(server->id, 0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
		aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

void ForwardCtrl::onENetDisconnected(ForwardServer* server, ENetPeer* peer) {
	ForwardClientENet* client = peer->data ? (ForwardClientENet*)peer->data : nullptr;
	if (client) {
		logDebug("[ENet,c:{0}] disconnected.", client->id);
		peer->data = nullptr;
		auto it = server->clients.find(client->id);
		if (it != server->clients.end())
			server->clients.erase(it);
		poolForwardClientENet.del(client);
	}
	if (server->isClientMode && server->reconnect) {
		server->doReconnect();
	}
	curEvent = Event::Disconnected;
	if (server->isClientMode) {
		server->clientID = 0;
	}
	curProcessClient = client;
}

void ForwardCtrl::onENetReceived(ForwardServer* server, ENetPeer* peer, ENetPacket* inPacket) {
	ForwardClient* client = (ForwardClient*)peer->data;
	logDebug("[cli:{0}][len:{1}]", client->id, inPacket->dataLength);
	ForwardHeader header;
	ReturnCode err = getHeader(&header, inPacket);
	if (err == ReturnCode::Err) {
		logWarn("[onENetReceived] getHeader err");
		return;
	}
	auto it = handleFuncs.find(header.getProtocol());
	logDebug("[onENetReceived] protocol:{0}", header.getProtocol());
	if (it == handleFuncs.end()) {
		logWarn("[onENetReceived] wrong protocol:{0}", header.getProtocol());
		return;
	}
	ForwardParam param;
	param.header = &header;
	param.packet = createPacket(inPacket);
	param.client = client;
	param.server = server;
	handlePacketFunc handleFunc = it->second;
	(this->*handleFunc)(param);
}

ReturnCode ForwardCtrl::validHeader(ForwardHeader* header) {
	if (header->getVersion() != Version) {
		logWarn("[validHeader] wrong version {0} != {1}", header->getVersion(), Version);
		return ReturnCode::Err;
	}
	return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::getHeader(ForwardHeader* header, const std::string& packet) {
	uint8_t* data = (uint8_t*)packet.c_str();
	memcpy(header, data, HeaderBaseLength);
	memcpy(header->data, data + HeaderBaseLength, header->getHeaderLength() - HeaderBaseLength);
	return validHeader(header);
}

ReturnCode ForwardCtrl::getHeader(ForwardHeader * header, ENetPacket * packet) {
	uint8_t* data = packet->data;
	memcpy(header, data, HeaderBaseLength);
	memcpy(header->data, data + HeaderBaseLength, header->getHeaderLength() - HeaderBaseLength);
	return validHeader(header);
}

ForwardClient* ForwardCtrl::getOutClient(ForwardHeader* inHeader, ForwardServer* inServer, ForwardServer* outServer) const {
	ForwardClient* outClient = nullptr;
	if (!inServer->dest) {
		// only use inHeader->clientID when inServer has no destServer
		int clientID = inHeader->getClientID();
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
		int destHostID = inHeader->getHostID();
		if (!destHostID)
			return nullptr;
		outServer = getServerByID(destHostID);
	}
	return outServer;
}

void ForwardCtrl::pollOnceByServerID(UniqID serverId) {
	ForwardServer* pServer = getServerByID(serverId);
	if (!pServer) {
		return;
	}
	pollOnce(pServer);
}

void ForwardCtrl::pollOnce(ForwardServer* pServer) {
	ENetEvent event;
	curEvent = Event::Nothing;
	curProcessServer = nullptr;
	curProcessClient = nullptr;
	curProcessHeader = nullptr;
	curProcessData = nullptr;
	curProcessDataLength = 0;
	if (pServer->netType == NetType::ENet) {
		ForwardServerENet* server = dynamic_cast<ForwardServerENet*>(pServer);
		int ret = enet_host_service(server->host, &event, 5);
		if (ret > 0) {
			logDebug("event.type = {}", event.type);
			curProcessServer = pServer;
			switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT: {
				onENetConnected(server, event.peer);
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE: {
				onENetReceived(server, event.peer, event.packet);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT: {
				onENetDisconnected(server, event.peer);
				break;
			}
			case ENET_EVENT_TYPE_NONE:
				break;
			}
			if (isExit)
				return;
		}
		else if (ret == 0) {
			// nothing happened
			return;
		}
		else if (ret < 0) {
			// error
#ifdef _MSC_VER
			logError("WSAGetLastError(): {0}\n", WSAGetLastError());
#endif
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	else if (pServer->netType == NetType::WS) {
		ForwardServerWS* server = dynamic_cast<ForwardServerWS*>(pServer);
		server->poll();
	}
}

void ForwardCtrl::pollAllOnce() {
	for (ForwardServer* pServer : servers) {
		pollOnce(pServer);
	}
}

void ForwardCtrl::loop() {
	while (!isExit) {
		pollAllOnce();
	}
}

Document ForwardCtrl::stat() const {
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