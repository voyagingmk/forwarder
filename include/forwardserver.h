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
		dest(nullptr),
		host(nullptr)
	{}
	~ForwardServer() {
		dest = nullptr;
		host = nullptr;
		clients.clear();
		printf("dtor\n");
	}
public:
	UniqID id;
	int destId;
	ForwardServer* dest;
	ENetHost * host;
	UniqIDGenerator idGenerator;
	std::map<UniqID, ForwardClient*> clients;
	std::string desc;
	int peerLimit;
};

#endif 