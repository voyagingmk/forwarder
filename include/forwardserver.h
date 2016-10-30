#ifndef FORWARDSERVER_H
#define FORWARDSERVER_H

#include "uniqid.h"
#include "defines.h"
#include "forwardclient.h"


class ForwardServer {
protected:
	ForwardServer(int protocol) :
		id(0),
		destId(0),
		peerLimit(0),
		desc(""),
		admin(false),
		protocol(protocol),
		dest(nullptr)
	{}
	~ForwardServer() {
		dest = nullptr;
		admin = false;
		clients.clear();
	}
public:
	virtual void release() = 0;
	virtual void init(rapidjson::Value& serverConfig) = 0;
public:
	UniqID id;
	int destId;
	bool admin;
	int peerLimit;
	int protocol;
	ForwardServer* dest;
	UniqIDGenerator idGenerator;
	std::map<UniqID, ForwardClient*> clients;
	std::string desc;
};

class ForwardServerENet: public ForwardServer {
public:
	ForwardServerENet() :
		host(nullptr),
		ForwardServer(Protocol::ENet)
	{}
	~ForwardServerENet() {
		host = nullptr;
	}
	ForwardServerENet(const ForwardServerENet& x) = delete;
	ForwardServerENet& operator=(const ForwardServerENet& x) = delete;

	virtual void release();

	virtual void init(rapidjson::Value& serverConfig);
public:
	ENetHost * host;
};

class ForwardServerWS : public ForwardServer {
public:
	typedef websocketpp::server<websocketpp::config::asio> WebsocketServer;
public:
	ForwardServerWS() :
		ForwardServer(Protocol::WS)
	{}
	~ForwardServerWS() {
	}
	ForwardServerWS(const ForwardServerWS& x) = delete;
	ForwardServerWS& operator=(const ForwardServerWS& x) = delete;

	virtual void release();

	virtual void init(rapidjson::Value& serverConfig);
	
	void setMessageHandler(WebsocketServer::message_handler h);

	void poll();
public:
	WebsocketServer server;
};

#endif 