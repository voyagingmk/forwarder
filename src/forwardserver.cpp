#include "forwardserver.h"
#include "utils.h"

namespace forwarder {

	ReturnCode ForwardServer::initCommon(rapidjson::Value& serverConfig) {
		auto logger = getLogger();
		desc = serverConfig["desc"].GetString();
		peerLimit = serverConfig["peers"].GetInt();
		port = serverConfig["port"].GetInt();
		admin = serverConfig.HasMember("admin") && serverConfig["admin"].GetBool();
		encrypt = serverConfig.HasMember("encrypt") && serverConfig["encrypt"].GetBool();
		base64 = serverConfig.HasMember("base64") && serverConfig["base64"].GetBool();
		isClientMode = serverConfig.HasMember("isClient") && serverConfig["isClient"].GetBool();
		if (serverConfig.HasMember("address")) {
			address = serverConfig["address"].GetString();
		}
		if (encrypt) {
			if (serverConfig.HasMember("encryptkey")) {
				initCipherKey(serverConfig["encryptkey"].GetString());
			}
			else {
				logger->error("no encryptkey");
				return ReturnCode::Err;
			}
		}
		if (serverConfig.HasMember("destId"))
			destId = serverConfig["destId"].GetInt();

		return ReturnCode::Ok;
	}

	bool ForwardServer::hasConsistConfig(ForwardServer* server) {
		if (netType != server->netType) {
			return false;
		}
		if (base64 != server->base64) {
			return false;
		}
		if (encrypt != server->encrypt) {
			return false;
		}
		if (encrypt) {
			size_t len = sizeof(AES_KEY);
			for (uint32_t i = 0; i < len; i++) {
				if (((uint8_t*)(&encryptkey))[i] != ((uint8_t*)(&server->encryptkey))[i]) {
					return false;
				}
			}
		}
		return true;
	}

	void ForwardServer::initCipherKey(const char* key){
		AES_set_encrypt_key((const unsigned char*)key, 128, &encryptkey);
	}

	void ForwardServerENet::init(rapidjson::Value& serverConfig) {
		ENetAddress enetAddress;
		if (!isClientMode) {
			enet_address_set_host(&enetAddress, "0.0.0.0");
			enetAddress.port = port;
		}
		else {
			enet_address_set_host(&enetAddress, address.c_str());
			enetAddress.port = port;
		}
		size_t channelLimit = 1;
		//address.host = ENET_HOST_ANY;
		host = enet_host_create(isClientMode? nullptr: &enetAddress,
			peerLimit,
			channelLimit,
			0      /* assume any amount of incoming bandwidth */,
			0      /* assume any amount of outgoing bandwidth */);
		if (!host) {
			getLogger()->error("An error occurred while trying to create an ENet server host.");
			exit(1);
			return;
		}
		if (isClientMode) {
			reconnect = serverConfig.HasMember("reconnect") && serverConfig["reconnect"].GetBool();
			enet_host_connect(host, &enetAddress, channelLimit, 0);

		}
	}

	void ForwardServerENet::doReconnect() {
		ENetAddress enetAddress; 
		enet_address_set_host(&enetAddress, address.c_str());
		enetAddress.port = port;
		size_t channelLimit = 1;
		enet_host_connect(host, &enetAddress, channelLimit, 0);
	};

	void ForwardServerENet::doDisconnect() {
		auto it = clients.find(clientID);
		if (it == clients.end()) {
			return;
		}
		ForwardClientENet* client = dynamic_cast<ForwardClientENet*>(it->second);
		auto state = client->peer->state;
		if(state == ENET_PEER_STATE_CONNECTING || state == ENET_PEER_STATE_CONNECTED){
			enet_peer_disconnect(client->peer, 0);
		}
	}

	bool ForwardServerENet::isConnected() {
		auto it = clients.find(clientID);
		if (it == clients.end()) {
			return false;
		}
		ForwardClientENet* client = dynamic_cast<ForwardClientENet*>(it->second);
		auto state = client->peer->state;
		return state == ENET_PEER_STATE_CONNECTED;
	}


	void  ForwardServerENet::release() {
		enet_host_destroy(host);
		host = nullptr;
	}

	void ForwardServerWS::init(rapidjson::Value& serverConfig) {
		if (!isClientMode) {
			server.set_error_channels(websocketpp::log::elevel::all);
			server.set_access_channels(websocketpp::log::alevel::none);
			server.init_asio();
			server.listen(port);
			server.start_accept();
		}
		else {
			serverAsClient.set_error_channels(websocketpp::log::elevel::all);
			serverAsClient.set_access_channels(websocketpp::log::alevel::none);
			serverAsClient.init_asio();
			doReconnect();
		}
	}

	void  ForwardServerWS::release() {
		hdlToClientId.clear();
	}

	void ForwardServerWS::poll() {
		if (!isClientMode) {
			server.poll_one();
		}
		else {
			serverAsClient.poll_one();
		}
	}	

	void ForwardServerWS::doReconnect() {
		std::string uri = getUri();
		websocketpp::lib::error_code ec;
		WebsocketClient::connection_ptr con = serverAsClient.get_connection(uri, ec);
		if (ec) {
			std::cout << "[error][forwarder.WS] could not create connection because: " << ec.message() << std::endl;
			return;
		}
		serverAsClient.connect(con);
	}

	void ForwardServerWS::doDisconnect() {
		auto it = clients.find(clientID);
		if (it == clients.end()) {
			return;
		}
		ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it->second);
		auto hdl = client->hdl;
		websocketpp::lib::error_code ec;
		websocketpp::close::status::value code = websocketpp::close::status::normal;
		std::string reason = "";
		serverAsClient.close(hdl, code, reason, ec);
		if (ec) {
			std::cout << "[error][forwarder.WS] Error initiating close: " << ec.message() << std::endl;
		}
	}


	bool ForwardServerWS::isConnected() {
		auto it = clients.find(clientID);
		if (it == clients.end()) {
			return false;
		}
		ForwardClientWS* client = dynamic_cast<ForwardClientWS*>(it->second);
		auto hdl = client->hdl;
		return server.get_con_from_hdl(hdl)->get_state() == websocketpp::session::state::value::connecting;
	}
}