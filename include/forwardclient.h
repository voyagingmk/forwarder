#ifndef FORWARDCLIENT_H
#define FORWARDCLIENT_H

#include "base.h"
#include "uniqid.h"


namespace forwarder {

	class ForwardClient {
	public:
		ForwardClient() {
		}
		virtual ~ForwardClient() {
		}
		inline UniqID getUniqID() const {
			return id;
		}	
	public:
		UniqID id = 0;
		uint32_t ip = 0;
	};

	class ForwardClientENet : public ForwardClient {
	public:
		ForwardClientENet() :
			peer(nullptr)
		{
#ifdef DEBUG_MODE
			printf("[forwarder] ForwardClient ENet created.\n");
#endif
		};
		virtual ~ForwardClientENet() {
#ifdef DEBUG_MODE
			printf("[forwarder] ForwardClient ENet released.\n");
#endif
		}
		ForwardClientENet(const ForwardClientENet& x) = delete;
		ForwardClientENet& operator=(const ForwardClientENet& x) = delete;
	public:
		ENetPeer* peer;
	};

	class ForwardClientWS : public ForwardClient {
		typedef websocketpp::connection_hdl Hdl;
	public:
		ForwardClientWS()
		{
#ifdef DEBUG_MODE
			printf("[forwarder] ForwardClient WS created.\n");
#endif
		};
		virtual ~ForwardClientWS() {
#ifdef DEBUG_MODE
			printf("[forwarder] ForwardClient WS released.\n");
#endif
		}
		ForwardClientWS(const ForwardClientWS& x) = delete;
		ForwardClientWS& operator=(const ForwardClientWS& x) = delete;
	public:
		Hdl hdl;
	};

};

#endif