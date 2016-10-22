#ifndef FORWARDCLIENT_H
#define FORWARDCLIENT_H

#include "uniqid.h"

class ForwardClient {
public:
	ENetPeer * peer = nullptr;
	UniqID id = 0;
};

#endif