#ifndef FORWARDCTRL_H
#define FORWARDCTRL_H

#include "base.h"
#include "forwardclient.h"
#include "forwardserver.h"

class ForwardCtrl {
public:
	ForwardCtrl();

	~ForwardCtrl();

	inline auto logger() {
		return spdlog::get("my_logger");
	}

	void initServers(rapidjson::Value& serversConfig);

	void exist() {
		isExit = true;
	}

	rapidjson::Document stat();

	void loop();
private:
	void onReceived(ForwardServer* server, ForwardClient* client, ENetPacket * inPacket, int channelID) {

	}
private:
	Pool<ForwardServer> poolForwardServer;
	Pool<ForwardClient> poolForwardClient;
	std::vector<ForwardServer*> servers;
	int serverNum;
	bool isExit;
};


#endif