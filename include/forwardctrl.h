#ifndef FORWARDCTRL_H
#define FORWARDCTRL_H

#include "base.h"
#include "defines.h"
#include "forwardclient.h"
#include "forwardserver.h"
#include "forwardheader.h"



class ForwardPacket {
public:
	virtual uint8_t* getDataPtr() const = 0;
	virtual void* getRawPtr() const = 0;
	virtual void setHeader(ForwardHeader* header) = 0;
	virtual void setData(uint8_t* data, size_t len) = 0;
};

typedef std::shared_ptr<ForwardPacket> ForwardPacketPtr;


class ForwardPacketENet: public ForwardPacket {
public:
	ForwardPacketENet(ENetPacket* p_packet):
		owned(false),
		packet(p_packet)
	{}

	ForwardPacketENet(size_t len):
		owned(true)
	{
		packet = enet_packet_create(NULL, len, ENET_PACKET_FLAG_RELIABLE);
		memset(packet->data, '\0', len);
	}

	~ForwardPacketENet() {
		if (owned) {
			enet_packet_destroy(packet);
			packet = nullptr;
		}
	}

	virtual uint8_t* getDataPtr() const {
		return static_cast<uint8_t*>(packet->data);
	}

	virtual void* getRawPtr() const {
		return static_cast<void*>(packet);
	}

	virtual void setHeader(ForwardHeader* header) {
		memcpy(packet->data, header, sizeof(ForwardHeader));
	}

	virtual void setData(uint8_t* data, size_t len) {
		memcpy(packet->data + sizeof(ForwardHeader), data, len);
	}
public:
	bool owned;
	ENetPacket* packet = nullptr;
};

class ForwardPacketWS : public ForwardPacket {
public:
	ForwardPacketWS(void* p_data) :
		owned(false),
		packetData(static_cast<uint8_t*>(p_data))
	{}

	ForwardPacketWS(uint8_t* p_data) :
		owned(false),
		packetData(p_data)
	{}

	ForwardPacketWS(size_t len):
		owned(true),
		packetData(new uint8_t[len])
	{}


	virtual uint8_t* getDataPtr() const {
		return packetData;
	}

	virtual void* getRawPtr() const {
		return static_cast<void*>(packetData);
	}

	virtual void setHeader(ForwardHeader* header) {
		memcpy(packetData, header, sizeof(ForwardHeader));
	}

	virtual void setData(uint8_t* data, size_t len) {
		memcpy(packetData + sizeof(ForwardHeader), data, len);
	}
public:
	bool owned;
	uint8_t* packetData = nullptr;
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
private:
	typedef struct {
		ForwardHeader* header = nullptr;
		ForwardServer* server = nullptr;
		ForwardClient* client = nullptr;
		ForwardPacketPtr packet = nullptr;
		int channelID = 0;
	} ForwardParam;
private:
	void onENetReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID);
	void onWSReceived(ForwardServerWS* server, websocketpp::connection_hdl hdl, ForwardServerWS::WebsocketServer::message_ptr msg);
	
	ForwardPacketPtr createPacket(Protocol protocol, size_t len);
	ForwardPacketPtr createPacket(ENetPacket* packet);
	ForwardPacketPtr createPacket(const char* packet);

	bool validHeader(ForwardHeader * header);
	bool getHeader(ForwardHeader * header, const std::string& packet);
	bool getHeader(ForwardHeader* header, ENetPacket * packet);

	bool handlePacket_1(ForwardParam& param);
	bool handlePacket_2(ForwardParam& param);
	bool handlePacket_3(ForwardParam& param);
	bool handlePacket_4(ForwardParam& param);

	ForwardServer* createForwardServer(int protocol);
	ForwardClient* createForwardClient(int protocol);
	
	void sendPacket(ForwardParam& param);
	void broadcastPacket(ForwardParam& param);
private:
	typedef bool(ForwardCtrl::*handlePacketFunc)(ForwardParam& param);
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


#endif