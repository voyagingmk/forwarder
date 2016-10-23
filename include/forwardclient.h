#ifndef FORWARDCLIENT_H
#define FORWARDCLIENT_H

#include "uniqid.h"

class ForwardClient {
public:
	ForwardClient() {};
	ForwardClient(const ForwardClient& x) = delete;
	ForwardClient& operator=(const ForwardClient& x) = delete;
public:
	ENetPeer * peer = nullptr;
	UniqID id = 0;
};

#endif