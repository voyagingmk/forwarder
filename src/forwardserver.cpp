#include "forwardserver.h"
#include "utils.h"

namespace forwarder {

	ReturnCode ForwardServer::initCommon(rapidjson::Value& serverConfig) {
		auto logger = getLogger();
		desc = serverConfig["desc"].GetString();
		peerLimit = serverConfig["peers"].GetInt();
		admin = serverConfig.HasMember("admin") && serverConfig["admin"].GetBool();
		encrypt = serverConfig.HasMember("encrypt") && serverConfig["encrypt"].GetBool();
		base64 = serverConfig.HasMember("base64") && serverConfig["base64"].GetBool();
		isClient = serverConfig.HasMember("isClient") && serverConfig["isClient"].GetBool();
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
		port = serverConfig["port"].GetInt();
		if (serverConfig.HasMember("address")) {
			address = serverConfig["address"].GetString();
		}
		if (!isClient) {
			enet_address_set_host(&enetAddress, "0.0.0.0");
			enetAddress.port = port;
		}
		else {
			enet_address_set_host(&enetAddress, address.c_str());
			enetAddress.port = port;
		}
		size_t channelLimit = 1;
		//address.host = ENET_HOST_ANY;
		host = enet_host_create(isClient? nullptr: &enetAddress,
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
			enet_host_connect(host, &enetAddress, channelLimit, 0);

		}
	}

	void ForwardServerENet::doReconect() {
		ENetAddress enetAddress; 
		enet_address_set_host(&enetAddress, address.c_str());
		enetAddress.port = port;
		size_t channelLimit = 1;
		enet_host_connect(host, &enetAddress, channelLimit, 0);
	};

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