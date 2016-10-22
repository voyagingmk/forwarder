#include "base.h"
#include "utils.h"
#include "uniqid.h"
#include "forwardctrl.h"

namespace spd = spdlog;
using namespace std;
using namespace rapidjson;

void onSIGINT(int n)
{
	if (n == SIGINT) {
	}
}


int main(int argc, char ** argv)
{
	printf("forwarder started.\n");

	setupLogger();

	auto logger = spdlog::get("my_logger");

	const char * configPath = "./../config.json";
	if(!isFileExist(configPath)){	
		logger->error("config.json not found!");
		return EXIT_FAILURE;
	}

	Document config;
	config.Parse(readFile(configPath).c_str());

	if (enet_initialize() != 0){
		logger->error("An error occurred while initializing ENet");
		return EXIT_FAILURE;
	}

	ForwardCtrl ctrl;

	RegisterSystemSignal(SIGINT, [&](int nSig)->void { ctrl.exist(); });

	ctrl.initServers(config["servers"]);

	//debugDocument(ctrl.stat());

	ctrl.loop();

	atexit(enet_deinitialize);
	atexit(spdlog::drop_all);
	return 0;
}
