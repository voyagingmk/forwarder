#ifndef FORWARDCTRL_H
#define FORWARDCTRL_H

#include "base.h"
#include "defines.h"
#include "forwardclient.h"
#include "forwardserver.h"
#include "forwardheader.h"
#include "forwardpacket.h"
#include "base64.h"



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

		uint32_t createServer(rapidjson::Value& serverConfig);

		ReturnCode removeServerByID(UniqID serverId);

		ForwardServer* getServerByID(UniqID serverId) const;

		ReturnCode sendBinary(UniqID serverId, uint8_t* data, size_t dataLength);

		ReturnCode sendText(UniqID serverId, std::string data);

		void exist() {
			isExit = true;
		}

		rapidjson::Document stat() const;

		void pollOnce();

		void loop();

	private:
		void onENetReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID);

		void onWSReceived(ForwardServerWS* server, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg);

		ForwardPacketPtr createPacket(NetType netType, size_t len);

		ForwardPacketPtr createPacket(ENetPacket* packet);

		ForwardPacketPtr encodeData(ForwardServer* outServer, uint8_t* data, size_t dataLength);

		ForwardPacketPtr decodeData(ForwardServer* outServer, uint8_t* data, size_t dataLength);

		ReturnCode validHeader(ForwardHeader * header);

		ReturnCode getHeader(ForwardHeader * header, const std::string& packet);

		ReturnCode getHeader(ForwardHeader* header, ENetPacket * packet);

		ForwardPacketPtr convertPacket(ForwardPacketPtr packet, ForwardServer* inServer, ForwardServer* outServer);

		// System Cmd
		ReturnCode handlePacket_1(ForwardParam& param);
		// Auto Forward
		ReturnCode handlePacket_2(ForwardParam& param);
		// Client Mode
		ReturnCode handlePacket_3(ForwardParam& param);

		ReturnCode handlePacket_4(ForwardParam& param);

		ForwardServer* createServerByNetType(NetType netType);

		ForwardClient* createClientByNetType(NetType netType);


		ForwardClient* getOutClient(ForwardHeader* inHeader, ForwardServer* inServer, ForwardServer* outServer) const;

		ForwardServer* getOutServer(ForwardHeader* inHeader, ForwardServer* inServer) const;

		void sendPacket(ForwardParam& param);

		void broadcastPacket(ForwardParam& param);

		uint8_t* getBuffer() const {
			return buffer;
		}

	private:
		typedef ReturnCode(ForwardCtrl::*handlePacketFunc)(ForwardParam& param);
		Pool<ForwardServerENet> poolForwardServerENet;
		Pool<ForwardClientENet> poolForwardClientENet;
		Pool<ForwardServerWS> poolForwardServerWS;
		Pool<ForwardClientWS> poolForwardClientWS;
		std::vector<ForwardServer*> servers;
		std::map<UniqID, ForwardServer*> serverDict;
		std::map<int, handlePacketFunc> handleFuncs;
		UniqIDGenerator idGenerator;
		uint8_t* buffer;
		int serverNum;
		bool isExit;
		Base64Codec& base64Codec;
		static const size_t ivSize = 16;
	};

}

#endif