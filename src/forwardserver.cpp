#include "forwardserver.h"
#include "utils.h"

namespace forwarder {

	ReturnCode ForwardServer::initCommon(rapidjson::Value& serverConfig) {
		desc = serverConfig["desc"].GetString();
		peerLimit = serverConfig["peers"].GetInt();
		port = serverConfig["port"].GetInt();
		admin = serverConfig.HasMember("admin") && serverConfig["admin"].GetBool();
		encrypt = serverConfig.HasMember("encrypt") && serverConfig["encrypt"].GetBool();
		compress = serverConfig.HasMember("compress") && serverConfig["compress"].GetBool();
		base64 = serverConfig.HasMember("base64") && serverConfig["base64"].GetBool();
		isClientMode = serverConfig.HasMember("isClient") && serverConfig["isClient"].GetBool();
		reconnect = serverConfig.HasMember("reconnect") && serverConfig["reconnect"].GetBool();
		if (serverConfig.HasMember("reconnectdelay")) {
			reconnectdelay = serverConfig["reconnectdelay"].GetUint();
		}
		if (serverConfig.HasMember("address")) {
			address = serverConfig["address"].GetString();
		}
		if (encrypt) {
			if (serverConfig.HasMember("encryptkey")) {
				initCipherKey(serverConfig["encryptkey"].GetString());
			}
			else {
				logError("[forwarder] encrypt mode, but no encryptkey");
				return ReturnCode::Err;
			}
		}
		if (serverConfig.HasMember("destId"))
			destId = serverConfig["destId"].GetInt();
        if (serverConfig.HasMember("timeoutMin"))
            timeoutMin = serverConfig["timeoutMin"].GetInt();
        if (serverConfig.HasMember("timeoutMax"))
            timeoutMax = serverConfig["timeoutMax"].GetInt();
        
        setRule(Protocol::Forward, HandleRule::Forward);
        setRule(Protocol::BatchForward, HandleRule::BatchForward);
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

	ForwardClient*  ForwardServer::getClient(UniqID clientId) {
		if (clientId) {
			auto it_client = clients.find(clientId);
			if (it_client != clients.end())
				return it_client->second;
		}
		return nullptr;
	}

	void ForwardServer::initCipherKey(const char* key){
		AES_set_encrypt_key((const unsigned char*)key, 128, &encryptkey);
	}

	void ForwardServer::setRule(Protocol p, HandleRule rule) {
		ruleDict[p] = rule;
	}

	HandleRule ForwardServer::getRule(Protocol p) {
		auto it = ruleDict.find(p);
		if (it == ruleDict.end()) {
			return HandleRule::Unknown;
		}
		return it->second;
	}

    void ForwardServer::pushToBuffer(uint8_t* data, size_t len) {
        uint8_t* buffer = batchBuffer;
        size_t size = batchBufferSize;
        size_t offset = batchBufferOffset;
        size_t n = offset + len;
        if (n > size) {
            if(size == 0) {
                size = 1;
            }
            size_t newSize = size;
            while (n > newSize) {
                newSize = newSize << 1;
            }
            uint8_t* oldData = buffer;
            buffer = new uint8_t[newSize]{ 0 };
            batchBufferSize = newSize;
            batchBuffer = buffer;
            if(oldData) {
                if(offset > 0) {
                    memcpy(buffer, oldData, offset);
                }
                delete[] oldData;
            }
        }
        memcpy(buffer + offset, data, len);
        offset += len;
        batchBufferOffset = offset;
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
		enet_uint32 incomingBandwidth = 0;  /* assume any amount of incoming bandwidth */
		enet_uint32 outgoingBandwidth = 0;	/* assume any amount of outgoing bandwidth */
		if (serverConfig.HasMember("bandwidth")) {
			incomingBandwidth = serverConfig["bandwidth"]["incoming"].GetUint();
			outgoingBandwidth = serverConfig["bandwidth"]["outgoing"].GetUint();
			logInfo("[forwarder] incomingBandwidth: {0}, outgoingBandwidth: {1}", incomingBandwidth, outgoingBandwidth);
		}

		host = enet_host_create(isClientMode? nullptr: &enetAddress,
			peerLimit,
			channelLimit,
			incomingBandwidth,
			outgoingBandwidth);
		if (!host) {
			logError("[forwarder] An error occurred while trying to create an ENet server host.");
			exit(1);
			return;
		}
		if (isClientMode) {
			enet_host_connect(host, &enetAddress, channelLimit, 0);
		}
	}

	void ForwardServerENet::doReconnect() {
		logInfo("[forwarder] ENet doReconnect");
		ENetAddress enetAddress; 
		enet_address_set_host(&enetAddress, address.c_str());
		enetAddress.port = port;
		size_t channelLimit = 1;
		enet_host_connect(host, &enetAddress, channelLimit, 0);
	};

	void ForwardServerENet::doDisconnect() {
		logInfo("[forwarder] ENet doDisconnect");
		ForwardClient* client = getClient(clientID);
		if (!client) {
			return;
		}
		ForwardClientENet* clientENet = dynamic_cast<ForwardClientENet*>(client);
		auto state = clientENet->peer->state;
		if(state == ENET_PEER_STATE_CONNECTING || state == ENET_PEER_STATE_CONNECTED){
			enet_peer_disconnect(clientENet->peer, 0);
		}
	}

	bool ForwardServerENet::isConnected() {
		ForwardClient* client = getClient(clientID);
		if (!client) {
			return false;
		}
		auto state = dynamic_cast<ForwardClientENet*>(client)->peer->state;
		return state == ENET_PEER_STATE_CONNECTED;
	}
    
    
    
    bool ForwardServerENet::isClientConnected(UniqID targetClientID) {
        ForwardClient* client = getClient(targetClientID);
        if (!client) {
            return false;
        }
        auto state = dynamic_cast<ForwardClientENet*>(client)->peer->state;
        return state == ENET_PEER_STATE_CONNECTED;
    }
    

	void ForwardServerENet::release() {
		enet_host_destroy(host);
		host = nullptr;
	}



	void ForwardServerWS::init(rapidjson::Value& serverConfig) {
		if (!isClientMode) {
			server.set_error_channels(websocketpp::log::elevel::none);
			server.set_access_channels(websocketpp::log::alevel::none);
			server.init_asio();
            server.set_reuse_addr(true);
			server.listen(port);
			server.start_accept();
		}
		else {
			serverAsClient.set_error_channels(websocketpp::log::elevel::none);
			serverAsClient.set_access_channels(websocketpp::log::alevel::none);
			serverAsClient.init_asio();
			doReconnect();
		}
	}

    void  ForwardServerWS::release() {
        doDisconnect();
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
		logInfo("[forwarder] WS doReconnect");
		if (isConnected()) {
			return;
		}
		std::string uri = getUri();
		websocketpp::lib::error_code ec;
		WebsocketClient::connection_ptr con = serverAsClient.get_connection(uri, ec);
		if (ec) {
			logError("[forwarder] WS error, could not create connection because: {0}", ec.message());
			return;
		}
		serverAsClient.connect(con);
	}

	void ForwardServerWS::doDisconnect() {
        logInfo("[forwarder] WS doDisconnect");
        std::string reason = "";
        websocketpp::lib::error_code ec;
        websocketpp::close::status::value code = websocketpp::close::status::normal;
        if (!isClientMode) {
            server.stop_listening();
            server.stop();
        } else {
            auto client = getClient(clientID);
            if (!client) {
                return;
            }
            ForwardClientWS* clientWS = dynamic_cast<ForwardClientWS*>(client);
            auto hdl = clientWS->hdl;
            serverAsClient.close(hdl, code, reason, ec);
            if (ec) {
                logError("[forwarder] WS error, initiating close: {0}", ec.message());
            }
            serverAsClient.stop();
        }
	}


	bool ForwardServerWS::isConnected() {
		auto client = getClient(clientID);
		if (!client) {
			return false;
		}
		auto hdl = dynamic_cast<ForwardClientWS*>(client)->hdl;
		return server.get_con_from_hdl(hdl)->get_state() == websocketpp::session::state::value::connecting;
	}
    
    bool ForwardServerWS::isClientConnected(UniqID targetClientID) {
        auto client = getClient(targetClientID);
        if (!client) {
            return false;
        }
        auto hdl = dynamic_cast<ForwardClientWS*>(client)->hdl;
        return server.get_con_from_hdl(hdl)->get_state() == websocketpp::session::state::value::connecting;
    }
}
