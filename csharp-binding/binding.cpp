#include "binding.h"
#include "forwardctrl.h"



static forwarder::ForwardCtrl* getForwarder() {
	static forwarder::ForwardCtrl fwd;
	return &fwd;
}

int initENet() {
	static bool IsENetInited = false;
	if (!IsENetInited) {
		int ret = enet_initialize();
		IsENetInited = true;
		return ret;
	}
	return 0;
}

//FUNCTION: adds two values
void release() {
	getForwarder()->release();
}


int version() {
	return getForwarder()->version();
}


void SetDebugFunction(forwarder::DebugFuncPtr fp) {
	getForwarder()->SetDebugFunction(fp);
}


void setupLogger(const char* filename) {
	getForwarder()->setupLogger(filename);
}

void setDebug(bool debug) {
	getForwarder()->setDebug(debug);
}

int initProtocolMap(const char* sConfig) {
	rapidjson::Document config;
	config.Parse(sConfig);
	return getForwarder()->initProtocolMap(config);
}

void initServers(const char* sConfig) {
	rapidjson::Document config;
	config.Parse(sConfig);
	getForwarder()->initServers(config);
}

uint32_t createServer(const char* sConfig) {
	rapidjson::Document config;
	config.Parse(sConfig);
	return getForwarder()->createServer(config);
}

uint32_t removeServerByID(int serverId) {
	return getForwarder()->removeServerByID(serverId);
}

bool disconnect(int serverId) {
	forwarder::ForwardServer* server = getForwarder()->getServerByID(serverId);
	if (server) {
		server->doDisconnect();
		return true;
	}
	return false;
}

bool isConnected(int serverId) {
	forwarder::ForwardServer* server = getForwarder()->getServerByID(serverId);
	if (!server || !server->isClientMode) {
		return false;
	}
	return server->isConnected();
}

void setTimeout(int serverId, int timeoutLimit, int timeoutMinimum, int timeoutMaximum) {
	forwarder::ForwardServer* server = getForwarder()->getServerByID(serverId);
	if (!server || !server->isClientMode || server->netType != forwarder::NetType::ENet) {
		return;
	}
	forwarder::ForwardClient* client = server->getClient(server->clientID);
	if (!client) {
		return;
	}
	forwarder::ForwardClientENet* clientENet = dynamic_cast<forwarder::ForwardClientENet*>(client);
	clientENet->setPeerTimeout(timeoutLimit, timeoutMinimum, timeoutMaximum);
}

void setPingInterval(int serverId, int interval) {
	forwarder::ForwardServer* server = getForwarder()->getServerByID(serverId);
	if (!server || !server->isClientMode || server->netType != forwarder::NetType::ENet) {
		return;
	}
	forwarder::ForwardClient* client = server->getClient(server->clientID);
	if (!client) {
		return;
	}
	forwarder::ForwardClientENet* clientENet = dynamic_cast<forwarder::ForwardClientENet*>(client);
	clientENet->setPing(interval);
}

uint32_t sendText(int serverId, int clientId, const char* data) {
	auto ret = getForwarder()->sendText(serverId, clientId, data);
	return ret;
}

uint32_t sendBinary(int serverId, int clientId, void* data, int length) {
	auto ret = getForwarder()->sendBinary(serverId, clientId, (uint8_t*)data, length);
	return ret;
}

uint32_t getCurEvent() {
	return getForwarder()->getCurEvent();
}

// inline ForwardServer* getCurProcessServer();
uint32_t getCurProcessServerID() {
	forwarder::ForwardServer* server = getForwarder()->getCurProcessServer();
	if (server) {
		return server->id;
	}
	return 0;
}

uint32_t getCurProcessClientID() {
	forwarder::ForwardClient* client = getForwarder()->getCurProcessClient();
	if (client) {
		return client->getUniqID();
	}
	return 0;
}

uint8_t* getCurProcessPacket() {
	return nullptr;
}

void pollOnceByServerID(int serverId) {
	getForwarder()->pollOnceByServerID(serverId);
}

std::string getStatInfo() {
	rapidjson::Document d = getForwarder()->stat();
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	d.Accept(writer);
	const char* s = buffer.GetString();
	return std::string(s);
}