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

	typedef void(*DebugFuncPtr)(const char *);

	class ForwardCtrl {
	public:
		ForwardCtrl();

		virtual ~ForwardCtrl();

		void release();
	
		int version() {
			return ForwarderVersion;
		}

		void setupLogger(const char* filename = nullptr);

		void setDebug(bool enabled);

		void initServers(rapidjson::Value& serversConfig);

		uint32_t createServer(rapidjson::Value& serverConfig);

		ReturnCode removeServerByID(UniqID serverId);

		ForwardServer* getServerByID(UniqID serverId) const;
        
        // single send
		ReturnCode sendBinary(UniqID serverId, UniqID clientId, uint8_t* data, size_t dataLength);

		ReturnCode sendText(UniqID serverId, UniqID clientId, std::string& data);
	
		ReturnCode sendText(UniqID serverId, UniqID clientId, const char* data);
        
        // broadcast send
        ReturnCode broadcastBinary(UniqID serverId, uint8_t* data, size_t dataLength);
        
        ReturnCode broadcastText(UniqID serverId, std::string& data);

        ReturnCode broadcastText(UniqID serverId, const char* data);
        
        // forward send
        ReturnCode forwardBinary(UniqID serverId, UniqID clientId, uint8_t* data, size_t dataLength, int forwardClientId, bool isBroadcast);
        
        ReturnCode forwardText(UniqID serverId, UniqID clientId, std::string& data, int forwardClientId, bool isBroadcast);
        
        ReturnCode forwardText(UniqID serverId, UniqID clientId, const char* data, int forwardClientId, bool isBroadcast);
   
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

		void pollOnceByServerID(UniqID serverId, int ms = 0);
		
		void pollOnce(ForwardServer* server, int ms = 0);

		void pollAllOnce();

		void loop();

		rapidjson::Document stat() const;

		void SetDebugFunction(DebugFuncPtr fp);

	private:
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		void onENetConnected(ForwardServer* server, ENetPeer* peer);

		void onENetDisconnected(ForwardServer* server, ENetPeer* peer);

		void onENetReceived(ForwardServer* server, ENetPeer* peer, ENetPacket* inPacket);

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		
		void onWSConnected(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl);

		void onWSDisconnected(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl);

		void onWSError(ForwardServerWS* wsServer, websocketpp::connection_hdl hdl);

		void onWSReceived(ForwardServerWS* server, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg);
		
		void onWSReconnectTimeOut(websocketpp::lib::error_code const & ec, ForwardServerWS* wsServer);
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ReturnCode _sendBinary(UniqID serverId, UniqID clientId, uint8_t* data, size_t dataLength,
                               bool forwardMode = false,
                               int forwardClientId = 0,
                               bool forwardBroadcast = false);
        
        ReturnCode _sendText(UniqID serverId, UniqID clientId, std::string& data,
                             bool forwardMode = false,
                             int forwardClientId = 0,
                             bool forwardBroadcast = false);
        
        ReturnCode _sendText(UniqID serverId, UniqID clientId, const char* data,
                             bool forwardMode = false,
                             int forwardClientId = 0,
                             bool forwardBroadcast = false);
        
		ForwardPacketPtr createPacket(NetType netType, size_t len);

		ForwardPacketPtr createPacket(const std::string& packet);

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

		ForwardServer* createServerByNetType(NetType& netType);

		ForwardClient* createClientByNetType(NetType netType);

		ForwardClient* getOutClient(ForwardHeader* inHeader, ForwardServer* inServer, ForwardServer* outServer) const;

		ForwardServer* getOutServer(ForwardHeader* inHeader, ForwardServer* inServer) const;

		void sendPacket(ForwardParam& param);

		void broadcastPacket(ForwardParam& param);

		uint8_t* getBuffer(uint8_t bufferID, size_t n) {
			uint8_t* buffer = buffers[bufferID];
			size_t size = bufferSize[bufferID];
			if (!buffer || n > size) {
				while (n > size) {
					size = size << 1;
				}
				if (buffer) {
					delete buffer;
				}
				buffer = new uint8_t[size]{ 0 };
                logDebug("[forwarder] change buffer[{0}] size: {1}=>{2} success.", bufferID, bufferSize[bufferID], size);
                bufferSize[bufferID] = size;
				buffers[bufferID] = buffer;
			}
			return buffer;
		}
	public:
		template <typename... Args>
		inline void logDebug(const char* fmt, const Args&... args) {
			if (debug && logger) logger->info(fmt, args...);
			if (debugFunc) {
				debugFunc(fmt);
			}
		}

		template <typename... Args>
		inline void logInfo(const char* fmt, const Args&... args) {
			if (debug && logger) logger->info(fmt, args...);
			if (debugFunc) {
				debugFunc(fmt);
			}
		}

		template <typename... Args>
		inline void logWarn(const char* fmt, const Args&... args) {
			if (logger) logger->warn(fmt, args...);
			if (debugFunc) {
				debugFunc(fmt);
			}
		}

		template <typename... Args>
		inline void logError(const char* fmt, const Args&... args) {
			if (logger) logger->error(fmt, args...);
			if (debugFunc) {
				debugFunc(fmt);
			}
		}

	private:
		typedef ReturnCode(ForwardCtrl::*handlePacketFunc)(ForwardParam& param);
		Pool<ForwardServerENet> poolForwardServerENet;
		Pool<ForwardClientENet> poolForwardClientENet;
		Pool<ForwardServerWS> poolForwardServerWS;
		Pool<ForwardClientWS> poolForwardClientWS;
		std::vector<ForwardServer*> servers;
		std::map<UniqID, ForwardServer*> serverDict;
		std::map<HandleRule, handlePacketFunc> handleFuncs;
		UniqIDGenerator idGenerator;
		uint8_t** buffers;
		size_t* bufferSize;
		int serverNum;
		bool debug;
		bool released;
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
		DebugFuncPtr debugFunc;

		// static members
		static size_t bufferNum;
		static UniqID ForwardCtrlCount;
	};
}

#endif
