#ifndef FORWARDCTRL_H
#define FORWARDCTRL_H

#include "base.h"
#include "defines.h"
#include "forwardclient.h"
#include "forwardserver.h"
#include "forwardheader.h"
#include "forwardpacket.h"



namespace forwarder {
	class ForwardParam {
	public:
		ForwardHeader* header = nullptr;
		ForwardServer* server = nullptr;
		ForwardClient* client = nullptr;
		ForwardPacketPtr packet = nullptr;
		int channelID = 0;
	};

	class ForwardCtrl {
	public:
		ForwardCtrl();

		~ForwardCtrl();

		void initServers(rapidjson::Value& serversConfig);

		void exist() {
			isExit = true;
		}

		rapidjson::Document stat();

		void loop();

		ForwardClient* getOutClient(ForwardHeader* inHeader, ForwardServer* inServer, ForwardServer* outServer) const {
			ForwardClient* outClient = nullptr;
			if (!inServer->dest) {
				// only use inHeader->clientID when inServer has no destServer
				int clientID = inHeader->clientID;
				auto it_client = outServer->clients.find(clientID);
				if (it_client == outServer->clients.end())
					return nullptr;
				outClient = it_client->second;
			}
			return outClient;
		}

		ForwardServer* getOutServer(ForwardHeader* inHeader, ForwardServer* inServer) const {
			ForwardServer* outServer = nullptr;
			if (inServer->dest) {
				outServer = inServer->dest;
			}
			else {
				int destHostID = inHeader->hostID;
				if (!destHostID)
					return nullptr;
				outServer = getServerByHostID(destHostID);
			}
			return outServer;
		}
		ForwardServer* getServerByHostID(int hostID) const {
			auto it_server = serverDict.find(hostID);
			if (it_server == serverDict.end())
				return nullptr;
			return it_server->second;
		}
	private:
		void onENetReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID);
		void onWSReceived(ForwardServerWS* server, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg);

		ForwardPacketPtr createPacket(NetType netType, size_t len);
		ForwardPacketPtr createPacket(ENetPacket* packet);
		ForwardPacketPtr createPacket(const char* packet);

		ReturnCode validHeader(ForwardHeader * header);
		ReturnCode getHeader(ForwardHeader * header, const std::string& packet);
		ReturnCode getHeader(ForwardHeader* header, ENetPacket * packet);
		ForwardPacketPtr convertPacket(ForwardPacketPtr packet, ForwardServer* inServer, ForwardServer* outServer, Convert convertNetType, Convert convertBase64, Convert convertCrypt);

		ReturnCode handlePacket_1(ForwardParam& param);
		ReturnCode handlePacket_2(ForwardParam& param);
		ReturnCode handlePacket_3(ForwardParam& param);
		ReturnCode handlePacket_4(ForwardParam& param);

		ForwardServer* createForwardServer(int protocol);
		ForwardClient* createForwardClient(int protocol);

		void sendPacket(ForwardParam& param);
		void broadcastPacket(ForwardParam& param);
	private:
		typedef ReturnCode(ForwardCtrl::*handlePacketFunc)(ForwardParam& param);
		Pool<ForwardServerENet> poolForwardServerENet;
		Pool<ForwardClientENet> poolForwardClientENet;
		Pool<ForwardServerWS> poolForwardServerWS;
		Pool<ForwardClientWS> poolForwardClientWS;
		std::vector<ForwardServer*> servers;
		std::map<UniqID, ForwardServer*> serverDict;
		std::map<int, handlePacketFunc> handleFuncs;
		int serverNum;
		bool isExit;
	};

}

#endif