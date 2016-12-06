#ifndef FORWARDSERVER_H
#define FORWARDSERVER_H

#include "uniqid.h"
#include "defines.h"
#include "forwardclient.h"
#include "aes_ctr.h"
#include "aes.h"

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
			netType(p_netType),
			dest(nullptr),
			isClientMode(false),
			clientID(0),
			reconnect(false)
		{}
		~ForwardServer() {
			dest = nullptr;
			admin = false;
			clients.clear();
		}
	public:
		virtual void release() = 0;
		virtual ReturnCode initCommon(rapidjson::Value& serverConfig) final;
		virtual void init(rapidjson::Value& serverConfig) = 0;
		void initCipherKey(const char* key);
		bool hasConsistConfig(ForwardServer*);

		// used for client mode
		virtual void doReconnect() {};
		virtual void doDisconnect() {};
		virtual bool isConnected() { return false; };
	public:
		UniqID id;
		int destId;
		bool admin;
		bool encrypt;
		bool base64;
		int peerLimit;
		AES_KEY encryptkey;
		NetType netType;
		ForwardServer* dest;
		UniqIDGenerator idGenerator;
		std::map<UniqID, ForwardClient*> clients;
		std::string desc;
		uint16_t port;

		// used for client mode
		bool isClientMode;
		std::string address;
		UniqID clientID;
		bool reconnect; // auto reconncet to target host when disconnected
	};



class ForwardServerENet : public ForwardServer {
	public:
		ForwardServerENet() :
			host(nullptr),
			ForwardServer(NetType::ENet)
		{}
		~ForwardServerENet() {
			host = nullptr;
		}
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
		~ForwardServerWS() {
		}
		ForwardServerWS(const ForwardServerWS& x) = delete;
		ForwardServerWS& operator=(const ForwardServerWS& x) = delete;

		virtual void release();

		virtual void init(rapidjson::Value& serverConfig);
		
		virtual void doReconnect();

		virtual void doDisconnect();

		virtual bool isConnected();

		void poll();
	private:
		std::string getUri() {
			return "ws://" + address + ":" + std::to_string(port);
		}
	public:
		WebsocketServer server;
		WebsocketClient serverAsClient;
		std::map<websocketpp::connection_hdl, UniqID, std::owner_less<websocketpp::connection_hdl> > hdlToClientId;
	};

}
#endif 