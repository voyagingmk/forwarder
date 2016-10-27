#ifndef FORWARDCTRL_H
#define FORWARDCTRL_H

#include "base.h"
#include "defines.h"
#include "forwardclient.h"
#include "forwardserver.h"
#include "forwardheader.h"

class ForwardCtrl {
public:
	ForwardCtrl();

	~ForwardCtrl();

	inline std::shared_ptr<spdlog::logger> logger() {
		return spdlog::get("my_logger");
	}

	void initServers(rapidjson::Value& serversConfig);

	void exist() {
		isExit = true;
	}

	rapidjson::Document stat();

	void loop();
private:
	typedef struct {
		ForwardHeader* header;
		ForwardServer* server;
		ForwardClient* client;
		ENetPacket * packet;
		int channelID;
	} ForwardParam;
private:
	void onReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID);
	bool getHeader(ForwardHeader* header, ENetPacket * packet);
	bool handlePacket_1(ForwardParam& param);
	bool handlePacket_2(ForwardParam& param);
	bool handlePacket_3(ForwardParam& param);
	bool handlePacket_4(ForwardParam& param);
private:
	typedef bool(ForwardCtrl::*handlePacketFunc)(ForwardParam& param);
	Pool<ForwardServer> poolForwardServer;
	Pool<ForwardClient> poolForwardClient;
	std::vector<ForwardServer*> servers;
	std::map<int, ForwardServer*> serverDict;
	std::map<int, handlePacketFunc> handleFuncs;
	int serverNum;
	bool isExit;
};


#endif