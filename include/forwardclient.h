#ifndef FORWARDCLIENT_H
#define FORWARDCLIENT_H

#include "base.h"
#include "uniqid.h"

class ForwardClient {
public:
	inline UniqID getUniqID() const {
		return id;
	}
	virtual void hh() const {
	}
public:
	UniqID id = 0;
	uint32_t ip = 0;
};

class ForwardClientENet: public ForwardClient {
public:
	ForwardClientENet():
		peer(nullptr)
	{};
	ForwardClientENet(const ForwardClientENet& x) = delete;
	ForwardClientENet& operator=(const ForwardClientENet& x) = delete;
public:
	ENetPeer* peer;
};

class ForwardClientWS : public ForwardClient {
	typedef websocketpp::connection_hdl Hdl;
public:
	ForwardClientWS()
	{};
	ForwardClientWS(const ForwardClientWS& x) = delete;
	ForwardClientWS& operator=(const ForwardClientWS& x) = delete;
public:
	Hdl hdl;
};

#endif