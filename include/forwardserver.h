#ifndef FORWARDSERVER_H
#define FORWARDSERVER_H

#include "uniqid.h"
#include "defines.h"
#include "forwardclient.h"
#include "aes_ctr.h"
#include "aes.h"
#include "utils.h"

namespace forwarder {
	class ForwardServer {
	protected:
		ForwardServer(NetType p_netType) :
			id(0),
			destId(0),
			peerLimit(0),
			desc(""),
			admin(false),
			base64(false),
			encrypt(false),
			compress(false),
			netType(p_netType),
			dest(nullptr),
			isClientMode(false),
			clientID(0),
			reconnect(false),
            reconnectdelay(1000),
            timeoutMin(0),
            timeoutMax(0),
            batchBuffer(nullptr),
            batchBufferSize(0),
            batchBufferOffset(0)
		{
#ifdef DEBUG_MODE
			printf("[forwarder] ForwardServer created, netType:%d\n", int(netType));
#endif
            ruleDict[Protocol::SysCmd] = HandleRule::SysCmd;
			ruleDict[Protocol::Forward] = HandleRule::Forward;
			ruleDict[Protocol::Process] = HandleRule::Process;
		}

		~ForwardServer() {
#ifdef DEBUG_MODE
			printf("[forwarder] ForwardServer released, netType:%d\n", int(netType));
#endif
			dest = nullptr;
			admin = false;
			clients.clear();
			release();
		}
	public:
		virtual void release() {};
		virtual ReturnCode initCommon(rapidjson::Value& serverConfig) final;
		virtual void init(rapidjson::Value& serverConfig) = 0;
		void initCipherKey(const char* key);
		bool hasConsistConfig(ForwardServer*);
		ForwardClient* getClient(UniqID clientId);
		void setRule(Protocol p, HandleRule rule);
		HandleRule getRule(Protocol p);
        // use buffer as a cache list
        // will auto realloc when there's no enough room for push,
        // and rewrite the prev data into new buffer memory
        void pushToBuffer(uint8_t* data, size_t len);
        

		// used for client mode
		virtual void doReconnect() {};
		virtual void doDisconnect() {};
		virtual bool isConnected() { return false; };
		virtual void poll() {};
	public:
		UniqID id;
		int destId;
		bool admin;
		bool encrypt;
		bool base64;
		bool compress;
        int peerLimit;
        int timeoutMin;
        int timeoutMax;
		AES_KEY encryptkey;
		NetType netType;
		ForwardServer* dest;
		UniqIDGenerator idGenerator;
		std::map<UniqID, ForwardClient*> clients;
		std::string desc;
		uint16_t port;
		std::map<Protocol, HandleRule> ruleDict;
        uint8_t* batchBuffer;
        size_t batchBufferSize;
        size_t batchBufferOffset;

		// used for client mode
		bool isClientMode;
		std::string address;
		UniqID clientID;
		bool reconnect; // auto reconncet to target host when disconnected
		size_t reconnectdelay; // ms
	};




class ForwardServerENet : public ForwardServer {
	public:
		ForwardServerENet() :
			host(nullptr),
			ForwardServer(NetType::ENet)
		{}
		ForwardServerENet(const ForwardServerENet& x) = delete;
		ForwardServerENet& operator=(const ForwardServerENet& x) = delete;

		virtual void release();

		virtual void init(rapidjson::Value& serverConfig);

		virtual void doReconnect();

		virtual void doDisconnect();

		virtual bool isConnected();
	public:
		ENetHost * host = nullptr;
		uint8_t broadcastChannelID = 0;
	};




class ForwardServerWS : public ForwardServer {
	public:
		typedef websocketpp::server<websocketpp::config::asio> WebsocketServer;
		typedef websocketpp::client<websocketpp::config::asio_client> WebsocketClient;
	public:
		ForwardServerWS() :
			ForwardServer(NetType::WS)
		{}
		ForwardServerWS(const ForwardServerWS& x) = delete;
		ForwardServerWS& operator=(const ForwardServerWS& x) = delete;

		virtual void release();

		virtual void init(rapidjson::Value& serverConfig);
		
		virtual void doReconnect();

		virtual void doDisconnect();

		virtual bool isConnected();

		virtual void poll();
	private:
		std::string getUri() {
			if (address == "127.0.0.1" || address == "localhost") {
				return "http://localhost:" + to_string(port);
			}
			return "ws://" + address + ":" + to_string(port);
		}
	public:
		WebsocketServer server;
		WebsocketClient serverAsClient;
		std::map<websocketpp::connection_hdl, UniqID, std::owner_less<websocketpp::connection_hdl> > hdlToClientId;
	};

}
#endif 
