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
	released(false),
	base64Codec(Base64Codec::get()),
	isExit(false),
	curProcessServer(nullptr),
	curProcessClient(nullptr),
	curProcessHeader(nullptr),
	curProcessData(nullptr),
	debugFunc(nullptr),
	curProcessDataLength(0),
	logger(nullptr),
	id(0)
{
#ifdef DEBUG_MODE
	printf("[forwarder] ForwardCtrl created.\n");
#endif
	buffers = new uint8_t*[bufferNum];
	bufferSize = new size_t[bufferNum];
	for (size_t i = 0; i < bufferNum; i++) {
		bufferSize[i] = 0xff;
		buffers[i] = new uint8_t[bufferSize[i]]{ 0 };
	}
	//default
	handleFuncs[HandleRule::SysCmd] = &ForwardCtrl::handlePacket_SysCmd;
	handleFuncs[HandleRule::Forward] = &ForwardCtrl::handlePacket_Forward;
	handleFuncs[HandleRule::Process] = &ForwardCtrl::handlePacket_Process;
	id = ++ForwardCtrlCount;
}

ForwardCtrl::~ForwardCtrl() {
	release();
}

void ForwardCtrl::release() {
	if (released) {
		return;
	}
	released = true;
#ifdef DEBUG_MODE
	printf("[forwarder] ForwardCtrl released\n");
#endif
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
		if (!server->isClientMode) {
			for (auto it = server->clients.begin(); it != server->clients.end(); it++) {
				auto client = it->second;
				if (server->netType == NetType::WS) {
					poolForwardClientWS.del(dynamic_cast<ForwardClientWS*>(client));
				}
				else if (server->netType == NetType::ENet) {
					poolForwardClientENet.del(dynamic_cast<ForwardClientENet*>(client));
				}
			}
		}
		if (server->netType == NetType::WS) {
			poolForwardServerWS.del(dynamic_cast<ForwardServerWS*>(server));
		}else if (server->netType == NetType::ENet) {
			poolForwardServerENet.del(dynamic_cast<ForwardServerENet*>(server));
		}
	}
	poolForwardServerENet.clear();
	poolForwardClientENet.clear();
	poolForwardServerWS.clear();
	poolForwardClientWS.clear();
	handleFuncs.clear();
}


void ForwardCtrl::setupLogger(const char* filename) {
	std::vector<spdlog::sink_ptr> sinks;
	if (filename && strlen(filename) > 0) {
		sinks.push_back(make_shared<spdlog::sinks::rotating_file_sink_st>(filename, "txt", 1048576 * 5, 3));
	}
	//sinks.push_back(make_shared<spdlog::sinks::daily_file_sink_st>(filename, "txt", 0, 0));
#ifdef _MSC_VER
	sinks.push_back(make_shared<spdlog::sinks::wincolor_stdout_sink_st>());
#else
	sinks.push_back(make_shared<spdlog::sinks::stdout_sink_st>());
#endif
	std::string name("ctrl" + to_string(id));
	logger = make_shared<spdlog::logger>(name, begin(sinks), end(sinks));
	if (spdlog::get(name)) {
		spdlog::drop(name);
	}
	spdlog::register_logger(logger);
    logger->flush_on(spdlog::level::debug);
	spdlog::set_pattern("[%D %H:%M:%S:%e][%l] %v");
	spdlog::set_level(spdlog::level::info);
	logInfo("logger created successfully.");
}

void ForwardCtrl::setDebug(bool enabled) {
	debug = enabled;
	if(logger) logger->set_level(spdlog::level::debug);
}


ForwardServer* ForwardCtrl::createServerByNetType(NetType& netType) {
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
		return  static_cast<uint32_t>(code);
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
			wsServer->serverAsClient.set_fail_handler(websocketpp::lib::bind(
				&ForwardCtrl::onWSError,
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

uint8_t* ForwardCtrl::getBuffer(uint8_t bufferID, size_t n) {
    if(n > MaxBufferSize) {
        logError("[forwarder] getBuffer[{0}], exceed max size: {1}", bufferID, n);
        return nullptr;
    }
    uint8_t* buffer = buffers[bufferID];
    size_t size = bufferSize[bufferID];
    if (!buffer || n > size) {
        while (n > size) {
            size = size << 1;
        }
        if (buffer) {
            delete buffer;
        }
        buffer = new uint8_t[size]{ 0 };
        logDebug("[forwarder] change buffer[{0}] size: {1}=>{2} success.", bufferID, bufferSize[bufferID], size);
        bufferSize[bufferID] = size;
        buffers[bufferID] = buffer;
    }
    return buffer;
}


ReturnCode ForwardCtrl::sendBinary(UniqID serverId, UniqID clientId, uint8_t* data, size_t dataLength) {
    return _sendBinary(serverId, clientId, data, dataLength);
}

ReturnCode ForwardCtrl::sendText(UniqID serverId, UniqID clientId, std::string& data) {
    return _sendText(serverId, clientId, data);
}

ReturnCode ForwardCtrl::sendText(UniqID serverId, UniqID clientId, const char* data) {
    return _sendText(serverId, clientId, data);
}

ReturnCode ForwardCtrl::broadcastBinary(UniqID serverId, uint8_t* data, size_t dataLength) {
    const UniqID clientId = 0;
    return _sendBinary(serverId, clientId, data, dataLength);
}

ReturnCode ForwardCtrl::broadcastText(UniqID serverId, std::string& data) {
    const UniqID clientId = 0;
    return _sendText(serverId, clientId, data);
}

ReturnCode ForwardCtrl::broadcastText(UniqID serverId, const char* data) {
    const UniqID clientId = 0;
    return _sendText(serverId, clientId, data);
}

ReturnCode ForwardCtrl::forwardBinary(UniqID serverId, UniqID clientId, uint8_t* data, size_t dataLength, int forwardClientId, bool isBroadcast) {
    const bool forwardMode = true;
    return _sendBinary(serverId, clientId, data, dataLength,
                     forwardMode,
                     forwardClientId,
                     isBroadcast);
}

ReturnCode ForwardCtrl::forwardText(UniqID serverId, UniqID clientId, std::string& data, int forwardClientId, bool isBroadcast) {
    const bool forwardMode = true;
    return _sendText(serverId, clientId, data,
                       forwardMode,
                       forwardClientId,
                       isBroadcast);
}

ReturnCode ForwardCtrl::forwardText(UniqID serverId, UniqID clientId, const char* data, int forwardClientId, bool isBroadcast) {
    const bool forwardMode = true;
    return _sendText(serverId, clientId, data,
                       forwardMode,
                       forwardClientId,
                       isBroadcast);
}



ReturnCode ForwardCtrl::_sendBinary(UniqID serverId,
                                    UniqID clientId,
                                    uint8_t* data,
                                    size_t dataLength,
                                    bool forwardMode,
                                    int forwardClientId,
                                    bool forwardBroadcast) {
    ForwardServer* outServer = getServerByID(serverId);
    if (!outServer) {
        return ReturnCode::Err;
    }
    ForwardClient* outClient = nullptr;
    if (clientId) {
        outClient = outServer->getClient(clientId);
        if(!outClient) {
            logError("[forwarder][sendBinary] outClient not found, clientId={0}", clientId);
            return ReturnCode::Err;
        }
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
    
    // forward config
    if(forwardMode) {
        if (forwardBroadcast) {
            outHeader.setFlag(HeaderFlag::Broadcast, true);
        } else {
            if (forwardClientId > 0) {
                outHeader.setFlag(HeaderFlag::ClientID, true);
                outHeader.setClientID((UniqID)(forwardClientId));
            } else {
                return ReturnCode::Err;
            }
        }
    }

    outHeader.resetHeaderLength();
    ForwardPacketPtr packet = encodeData(outServer, &outHeader, data, dataLength);
    if (!packet)
        return ReturnCode::Err;
    ForwardParam param;
    param.header = nullptr;
    param.packet = packet;
    param.client = outClient;
    param.server = outServer;
    if (!outClient) {
        sendPacket(param);
    }
    else {
        broadcastPacket(param);
    }
    return ReturnCode::Ok;
}

ReturnCode ForwardCtrl::_sendText(UniqID serverId, UniqID clientId, std::string& data,
                                  bool forwardMode,
                                  int forwardClientId,
                                  bool forwardBroadcast) {
    return _sendBinary(serverId, clientId, (uint8_t*)data.c_str(), data.size(),
                       forwardMode,
                       forwardClientId,
                       forwardBroadcast);
}

ReturnCode ForwardCtrl::_sendText(UniqID serverId, UniqID clientId, const char* data,
                                  bool forwardMode,
                                  int forwardClientId,
                                  bool forwardBroadcast) {
    return _sendBinary(serverId, clientId, (uint8_t*)data, strlen(data),
                       forwardMode,
                       forwardClientId,
                       forwardBroadcast);
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
	//if (debug) debugBytes("encodeData, raw Data", data, dataLength);
	if (outServer->compress) {
		size_t bufferLen = compressBound(dataLength);
		//logDebug("encodeData, compressBound={0}", bufferLen);
		uint8_t* newData = getBuffer(0, bufferLen);
        if(!newData) {
            logError("[encodeData] step_Compress no newData");
            return nullptr;
        }
		uLongf realLen = bufferLen;
        outHeader->setUncompressedSize(static_cast<uint32_t>(dataLength));// used for uncompression
		int ret = compress((Bytef*)newData, &realLen, data, dataLength);
		if (ret == Z_OK) {
			data = newData;
            dataLength = realLen;
            logDebug("[encodeData] after step_Compress dataLength:{0}", dataLength);
			//if (debug) debugBytes("encodeData, compressed", data, dataLength);
		}
		else {
            logError("[encodeData] step_Compress, compress failed.");
			if (ret == Z_MEM_ERROR)
				logError("Z_MEM_ERROR");
			else if (ret == Z_BUF_ERROR)
				logError("Z_BUF_ERROR");
			else if (ret == Z_DATA_ERROR)
				logError("Z_DATA_ERROR");
			return nullptr;
		}
	}

	if (outServer->encrypt) {
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_int_distribution<> dis(0, int(std::pow(2, 8)) - 1);
		uint8_t* newData = getBuffer(1, dataLength + ivSize);
        if(!newData) {
            logError("[encodeData] step_Encrypt no newData");
            return nullptr;
        }
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
        logDebug("[encodeData] after step_Encrypt dataLength:{0}", dataLength);
		//if (debug) debugBytes("encodeData, encrypted", data, dataLength);
	}

	std::string b64("");
	if (outServer->base64) {
		b64 = base64Codec.fromByteArray(data, dataLength);
		data = (uint8_t*)b64.c_str();
        dataLength = b64.size();
        logDebug("[encodeData] after step_Base64 dataLength:{0}", dataLength);
		//if (debug) debugBytes("encodeData, b64", data, dataLength);
	}
    if(!data || !dataLength){
        logError("[encodeData] final, no data");
        return nullptr;
    }
	//3. make packet
	ForwardPacketPtr newPacket = createPacket(outServer->netType, outHeader->getHeaderLength() + dataLength);
	// copy
	newPacket->setHeader(outHeader);
	newPacket->setData(data, dataLength);
	//if (debug) debugBytes("encodeData, final", (uint8_t*)newPacket->getHeaderPtr(), newPacket->getTotalLength());
	return newPacket;
}

void ForwardCtrl::decodeData(ForwardServer* inServer, ForwardHeader* inHeader, uint8_t* data, size_t dataLength, uint8_t* &outData, size_t& outDataLength) {
	outData = data;
	outDataLength = dataLength;
    if(!outData || !outDataLength) {
        return;
    }
	//logDebug("inHeader,ver={0},len={1},ip={2}",
    //    inHeader->getVersion(), inHeader->getHeaderLength(), inHeader->getIP());
	//if (debug) debugBytes("decodeData, inHeader'data", inHeader->data, inHeader->getHeaderLength() - HeaderBaseLength);
	if (inHeader->isFlagOn(HeaderFlag::Base64)) {
		//if (debug) debugBytes("decodeData, originData", data, dataLength);
		size_t newDataLength = base64Codec.calculateDataLength((const char*)data, dataLength);
		uint8_t* newData = getBuffer(0, newDataLength);
        if(!newData) {
            logError("[decodeData] step_Base64 no newData");
            outData = nullptr;
            outDataLength = 0;
            return;
        }
		base64Codec.toByteArray((const char*)data, dataLength, newData, &newDataLength);
		outData = newData;
		outDataLength = newDataLength;
		//if (debug) debugBytes("decodeData, base64decoded Data", outData, outDataLength);
        logDebug("[decodeData] after step_Base64 outDataLength:{0}", outDataLength);
	}
    if(!outData || !outDataLength) {
        return;
    }
	if (inHeader->isFlagOn(HeaderFlag::Encrypt)) { // DO decrypt
		size_t newDataLength = outDataLength - ivSize;
		uint8_t* encryptedData = outData + ivSize;
		uint8_t* newData = getBuffer(1, newDataLength);
        if(!newData) {
            logError("[decodeData] step_Encrypt no newData");
            outData = nullptr;
            outDataLength = 0;
            return;
        }
		uint8_t* iv = outData;
		unsigned char ecount_buf[AES_BLOCK_SIZE];
		unsigned int num = 0;
		AES_ctr128_encrypt(encryptedData, newData, newDataLength, &inServer->encryptkey, iv, ecount_buf, &num);
		outData = newData;
		outDataLength = newDataLength;
        logDebug("[decodeData] after step_Encrypt outDataLength:{0}", outDataLength);
		//if (debug) debugBytes("decodeData, decrypted Data", outData, outDataLength);
	}
    if(!outData || !outDataLength) {
        return;
    }
	if (inHeader->isFlagOn(HeaderFlag::Compress)) {
		uLongf bufferLen = inHeader->getUncompressedSize();
		uint8_t* newData = getBuffer(2, bufferLen);
        if(!newData) {
            logError("[decodeData] step_Compress, no newData");
            outData = nullptr;
            outDataLength = 0;
            return;
        }
		uLongf realLen = bufferLen;
		int ret = uncompress((Bytef*)newData, &realLen, outData, outDataLength);
		//logInfo("uncompress, bufferLen={0},realLen={1},outDataLength={2}",
		//	bufferLen, realLen, outDataLength);
		if (ret == Z_OK) {
			outData = newData;
            outDataLength = realLen;
            logDebug("[decodeData] after step_Compress outDataLength:{0}", outDataLength);
			//if (debug) debugBytes("decodeData, uncompressed Data", outData, outDataLength);
		}
        else {
            outData = nullptr;
            outDataLength = 0;
			logError("[decodeData] step_Compress, uncompress failed");
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
	//logDebug("raw data:{0}", rawData);
	if (!rawData || rawDataLength <= 0) {
		logError("[convertPacket] no raw data");
		return nullptr;
	}
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
		size_t statJsonLength = strlen(statJson);
		size_t totalLength = outHeader.getHeaderLength() + statJsonLength + 1;
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
    ForwardClient* outClient;
    if(inHeader->isFlagOn(HeaderFlag::Broadcast)) {
        outClient = nullptr; // no outClient means Broadcast
    } else {
        // check if outClient exists
        int clientID = inHeader->getClientID();
        if(clientID <= 0) {
            logWarn("[forward.single] wrong clientID.");
            return ReturnCode::Err;
        }
        outClient = getOutClient(inHeader, inServer, outServer);
        if(!outClient) {
            logWarn("[forward.single] outClient[{0}] not found.", clientID);
            return ReturnCode::Err;
        }
    }

	ForwardHeader outHeader;
	outHeader.setProtocol(2);
	outHeader.cleanFlag();
	// outServer's flag
	if (outServer->base64)
		outHeader.setFlag(HeaderFlag::Base64, true);
	if (outServer->encrypt)
		outHeader.setFlag(HeaderFlag::Encrypt, true);
	if (outServer->compress)
		outHeader.setFlag(HeaderFlag::Compress, true);
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

	outHeader.resetHeaderLength();

	ForwardPacketPtr outPacket;

	outPacket = convertPacket(inPacket, inServer, outServer, &outHeader);

	if (!outPacket) {
		logWarn("[forward] convertPacket failed.");
		return ReturnCode::Err;
	}
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
	curEvent = Event::Connected; 
	curProcessServer = wsServer;
	curProcessClient = client;
	// sendText(wsServer->id, 0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\
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
	if (wsServer->isClientMode) {
		wsServer->clientID = 0;
		if (wsServer->reconnect) {
			wsServer->serverAsClient.set_timer(wsServer->reconnectdelay, websocketpp::lib::bind(
				&ForwardCtrl::onWSReconnectTimeOut,
				this,
				websocketpp::lib::placeholders::_1,
				wsServer
				));
		}
	}
	curProcessServer = wsServer;
	curEvent = Event::Disconnected;
}

void ForwardCtrl::onWSReconnectTimeOut(websocketpp::lib::error_code const & ec, ForwardServerWS* wsServer) {
	logDebug("[onWSReconnectTimeOut]");
	if (ec) {
		logError("[onWSReconnectTimeOut] err: {0}", ec.message());
		return;
	}
	wsServer->doReconnect();
}

void ForwardCtrl::onWSError(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl) {
	auto con = wsServer->server.get_con_from_hdl(hdl);
	logDebug("[forwarder] onWSError:");
	logDebug("get_state:{0}", con->get_state());
	logDebug("local_close_code:{0}", con->get_local_close_code());
	logDebug("local_close_reason:{0}", con->get_local_close_reason());
	logDebug("remote_close_code:{0}", con->get_remote_close_code());
	logDebug("remote_close_reason:{0}", con->get_remote_close_reason());
	logDebug("get_ec:{0} ,msg:{1}", con->get_ec().value(), con->get_ec().message());
	onWSDisconnected(wsServer, hdl);
}

void ForwardCtrl::onWSReceived(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg) {
	auto it1 = wsServer->hdlToClientId.find(hdl);
	if (it1 == wsServer->hdlToClientId.end()) {
		logError("[forwarder][ws.recv] no such hdl");
		return;
	}
	UniqID clientID = it1->second;
	ForwardClient* client = wsServer->getClient(clientID);
	if (!client) {
		logError("[forwarder][ws.recv] no such cli:{0}", clientID);
		return;
	}
	logDebug("[forwarder][ws.recv][{0}][cli:{1}][len:{2}]", wsServer->desc, clientID, msg->get_payload().size());
	ForwardHeader header;
	const std::string& payload = msg->get_payload();
	ReturnCode code = getHeader(&header, payload);
	if (code == ReturnCode::Err) {
		logWarn("[forwarder][ws.recv] getHeader err");
		return;
	}
	HandleRule rule = wsServer->getRule(header.getProtocol());
	if (rule == HandleRule::Unknown) {
		logWarn("[forwarder][ws.recv] wrong protocol:{0}", header.getProtocol());
		return;
	}
	handlePacketFunc handleFunc = handleFuncs[rule];
	ForwardParam param;
	param.header = &header;
	param.packet = createPacket(payload);
	param.client = client;
	param.server = static_cast<ForwardServer*>(wsServer);
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
	logDebug("[forwarder][enet][c:{0}] connected, from {1}:{2}.",
		client->id,
		str,
		peer->address.port);
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
		logDebug("[forwarder][enet][c:{0}] disconnected.", client->id);
		peer->data = nullptr;
		auto it = server->clients.find(client->id);
		if (it != server->clients.end())
			server->clients.erase(it);
		poolForwardClientENet.del(client);
	}
	if (server->isClientMode && server->reconnect) {
		server->doReconnect();
	}
	if (server->isClientMode) {
		server->clientID = 0;
	}
	curEvent = Event::Disconnected;
	curProcessClient = client;
}

void ForwardCtrl::onENetReceived(ForwardServer* server, ENetPeer* peer, ENetPacket* inPacket) {
	ForwardClient* client = (ForwardClient*)peer->data;
	logDebug("[forwarder][enet.recv][{0}][cli:{1}][len:{2}]", server->desc, client->id, inPacket->dataLength);
	ForwardHeader header;
	ReturnCode err = getHeader(&header, inPacket);
	if (err == ReturnCode::Err) {
		logWarn("[forwarder][enet.recv] getHeader err");
		return;
	}
	HandleRule rule = server->getRule(header.getProtocol());
	if (rule == HandleRule::Unknown) {
		logWarn("[forwarder][enet.recv] wrong protocol:{0}", header.getProtocol());
		return;
	}
	handlePacketFunc handleFunc = handleFuncs[rule];
	ForwardParam param;
	param.header = &header;
	param.packet = createPacket(inPacket);
	param.client = client;
	param.server = server;
	(this->*handleFunc)(param);
}

ReturnCode ForwardCtrl::validHeader(ForwardHeader* header) {
	if (header->getVersion() != HeaderVersion) {
		logWarn("[validHeader] wrong version {0} != {1}", header->getVersion(), HeaderVersion);
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
	int clientID = inHeader->getClientID();
	outClient = outServer->getClient(clientID);
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

void ForwardCtrl::pollOnceByServerID(UniqID serverId, int ms) {
	ForwardServer* pServer = getServerByID(serverId);
	if (!pServer) {
		return;
	}
	pollOnce(pServer, ms);
}

void ForwardCtrl::pollOnce(ForwardServer* pServer, int ms) {
	ENetEvent event;
	curEvent = Event::Nothing;
	curProcessServer = nullptr;
	curProcessClient = nullptr;
	curProcessHeader = nullptr;
	curProcessData = nullptr;
	curProcessDataLength = 0;
	if (pServer->netType == NetType::ENet) {
		ForwardServerENet* server = dynamic_cast<ForwardServerENet*>(pServer);
		int ret = enet_host_service(server->host, &event, ms);
		if (ret > 0) {
			//logDebug("event.type = {0}", event.type);
			curProcessServer = pServer;
			switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT: {
                logDebug("[forwarder] enet.evt = connected");
				onENetConnected(server, event.peer);
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE: {
				onENetReceived(server, event.peer, event.packet);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT: {
                logDebug("[forwarder] enet.evt = disconnected");
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
			logError("[forwarder] WSAGetLastError(): {0}\n", WSAGetLastError());
#else
            logError("[forwarder] enet.evt = error");
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
			Value maxCount((int)server->idGenerator.getCount());
			Value recyled((int)server->idGenerator.getPecycledLength());
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

void ForwardCtrl::SetDebugFunction(DebugFuncPtr fp) {
	debugFunc = fp;
}
