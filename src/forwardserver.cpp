#include "forwardserver.h"
#include "utils.h"

namespace forwarder {

	void ForwardServer::initCipherKey(const char* key){
		AES_set_encrypt_key((const unsigned char*)key, 128, &encryptkey);
	}


	void ForwardServerENet::init(rapidjson::Value& serverConfig) {
		ENetAddress address;
		enet_address_set_host(&address, "0.0.0.0");
		//address.host = ENET_HOST_ANY;
		address.port = serverConfig["port"].GetInt();
		host = enet_host_create(&address,
			peerLimit,
			1,
			0      /* assume any amount of incoming bandwidth */,
			0      /* assume any amount of outgoing bandwidth */);
		if (!host) {
			getLogger()->error("An error occurred while trying to create an ENet server host.");
			exit(1);
			return;
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