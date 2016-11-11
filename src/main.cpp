#include "base.h"
#include "utils.h"
#include "uniqid.h"
#include "forwardctrl.h"


namespace spd = spdlog;
using namespace std;
using namespace rapidjson;
using namespace forwarder;

void onSIGINT(int n)
{
	if (n == SIGINT) {
	}
}

int main(int argc, char ** argv)
{
	printf("forwarder started.\n");
	const char * logfilename = argv[1];
	setupLogger(logfilename);

	auto logger = spdlog::get("my_logger");
	
	char * configPath = argv[2];
	logger->info("config path:{0}", configPath);
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
	ctrl.setDebug(true);

	RegisterSystemSignal(SIGINT, [&](int nSig)->void { ctrl.exist(); });
	
	if(config.HasMember("protocol")){
		ctrl.initProtocolMap(config["protocol"]);
	}
	ctrl.initServers(config["servers"]);

	debugDocument(ctrl.stat());

	ctrl.loop();

	atexit(enet_deinitialize);
	atexit(spdlog::drop_all);
	return 0;
}
