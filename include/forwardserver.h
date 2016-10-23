#ifndef FORWARDSERVER_H
#define FORWARDSERVER_H

#include "uniqid.h"

class ForwardServer {
public:
	ForwardServer() :
		id(0),
		destId(0),
		peerLimit(0),
		desc(""),
		admin(false),
		dest(nullptr),
		host(nullptr)
	{}
	~ForwardServer() {
		dest = nullptr;
		host = nullptr;
		admin = false;
		clients.clear();
		printf("dtor\n");
	}
	ForwardServer(const ForwardServer& x) = delete;
	ForwardServer& operator=(const ForwardServer& x) = delete;
public:
	UniqID id;
	int destId;
	bool admin;
	ForwardServer* dest;
	ENetHost * host;
	UniqIDGenerator idGenerator;
	std::map<UniqID, ForwardClient*> clients;
	std::string desc;
	int peerLimit;
};

#endif 