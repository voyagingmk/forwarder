#include "forwardserver.h"
#include "utils.h"

namespace forwarder {

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
		ENetAddress address;
		bool isClient = serverConfig.HasMember("isClient") && serverConfig["isClient"].GetBool();
		if (!isClient) {
			enet_address_set_host(&address, "0.0.0.0");
			address.port = serverConfig["port"].GetInt();
		}
		else {
			enet_address_set_host(&address, serverConfig["address"].GetString());
			address.port = serverConfig["port"].GetInt();
		}
		size_t channelLimit = 1;
		//address.host = ENET_HOST_ANY;
		host = enet_host_create(isClient? nullptr: &address,
			peerLimit,
			channelLimit,
			0      /* assume any amount of incoming bandwidth */,
			0      /* assume any amount of outgoing bandwidth */);
		if (!host) {
			getLogger()->error("An error occurred while trying to create an ENet server host.");
			exit(1);
			return;
		}
		if (isClient) {
			reconnect = serverConfig.HasMember("reconnect") && serverConfig["reconnect"].GetBool();
			enet_host_connect(host, &address, channelLimit, 0);

		}
	}

	void  ForwardServerENet::release() {
		enet_host_destroy(host);
		host = nullptr;
	}


	void ForwardServerWS::init(rapidjson::Value& serverConfig) {
		server.set_error_channels(websocketpp::log::elevel::all);
		server.set_access_channels(websocketpp::log::alevel::none);
		server.init_asio();
		server.listen(serverConfig["port"].GetInt());
		server.start_accept();
	}

	void  ForwardServerWS::release() {
		hdlToClientId.clear();
	}



	void ForwardServerWS::poll() {
		server.poll_one();
	}
}