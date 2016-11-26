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
	};
	

	class ForwardCtrl {
	public:
		ForwardCtrl();

		virtual ~ForwardCtrl();

		void setupLogger(const char* filename = nullptr);

		void setDebug(bool enabled);

		ReturnCode initProtocolMap(rapidjson::Value& protocolConfig);

		void initServers(rapidjson::Value& serversConfig);

		uint32_t createServer(rapidjson::Value& serverConfig);

		ReturnCode removeServerByID(UniqID serverId);

		ForwardServer* getServerByID(UniqID serverId) const;

		ReturnCode sendBinary(UniqID serverId, uint8_t* data, size_t dataLength);

		ReturnCode sendText(UniqID serverId, std::string data);

		typedef void(*eventCallback)();

		void registerCallback(Event evt, eventCallback callback);

		void exist() {
			isExit = true;
		}

		inline Event getCurEvent() const {
			return curEvent;
		}

		inline ForwardServer* getCurProcessServer() const {
			return curProcessServer;
		}
	
		inline ForwardClient* getCurProcessClient() const {
			return curProcessClient;
		}

		inline ForwardHeader* getCurProcessHeader() const {
			return curProcessHeader;
		}

		inline uint8_t* getCurProcessData() const {
			return curProcessData;
		}

		inline size_t getCurProcessDataLength() const {
			return curProcessDataLength;
		}

		void pollOnceByServerID(UniqID serverId);
		
		void pollOnce(ForwardServer* server);

		void pollAllOnce();

		void loop();

		rapidjson::Document stat() const;

	private:
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		void onENetConnected(ForwardServer* server, ENetPeer* peer);

		void onENetDisconnected(ForwardServer* server, ENetPeer* peer);

		void onENetReceived(ForwardServer* server, ENetPeer* peer, ENetPacket* inPacket);

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		
		void onWSConnected(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl);

		void onWSDisconnected(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl);

		void onWSReceived(ForwardServerWS* server, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg);

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////

		ForwardPacketPtr createPacket(NetType netType, size_t len);

		ForwardPacketPtr createPacket(ENetPacket* packet);

		ForwardPacketPtr encodeData(ForwardServer* outServer, ForwardHeader* outHeader, uint8_t* data, size_t dataLength);

		void decodeData(ForwardServer* inServer, ForwardHeader* inHeader, uint8_t* data, size_t dataLength, uint8_t* &outData, size_t& outDataLength);
		
		ReturnCode validHeader(ForwardHeader* header);

		ReturnCode getHeader(ForwardHeader* header, const std::string& packet);

		ReturnCode getHeader(ForwardHeader* header, ENetPacket * packet);

		ForwardPacketPtr convertPacket(ForwardPacketPtr packet, ForwardServer* inServer, ForwardServer* outServer, ForwardHeader* outHeader);

		/* ----  protocol   ----- */
		// System Cmd
		ReturnCode handlePacket_SysCmd(ForwardParam& param);
		// Auto Forward to next server
		ReturnCode handlePacket_Forward(ForwardParam& param);
		// process the packet locally
		ReturnCode handlePacket_Process(ForwardParam& param);

		ForwardServer* createServerByNetType(NetType netType);

		ForwardClient* createClientByNetType(NetType netType);

		ForwardClient* getOutClient(ForwardHeader* inHeader, ForwardServer* inServer, ForwardServer* outServer) const;

		ForwardServer* getOutServer(ForwardHeader* inHeader, ForwardServer* inServer) const;

		void sendPacket(ForwardParam& param);

		void broadcastPacket(ForwardParam& param);

		uint8_t* getBuffer() const {
			return buffer;
		}

		template <typename... Args>
		inline void logDebug(const char* fmt, const Args&... args) {
			if (debug && logger) logger->info(fmt, args...);
		}

		template <typename... Args>
		inline void logInfo(const char* fmt, const Args&... args) {
			if (debug && logger) logger->info(fmt, args...);
		}

		template <typename... Args>
		inline void logWarn(const char* fmt, const Args&... args) {
			if (logger) logger->warn(fmt, args...);
		}

		template <typename... Args>
		inline void logError(const char* fmt, const Args&... args) {
			if (logger) logger->error(fmt, args...);
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
		bool debug;
		bool isExit;
		Base64Codec& base64Codec;
		Event curEvent;
		ForwardServer* curProcessServer;
		ForwardClient* curProcessClient;
		ForwardHeader* curProcessHeader;
		uint8_t* curProcessData;
		size_t curProcessDataLength;
		static const size_t ivSize = 16;
		std::shared_ptr<spdlog::logger> logger;
		UniqID id;
		static UniqID ForwardCtrlCount;
	};
}

#endif