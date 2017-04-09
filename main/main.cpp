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
	char * configPath = argv[1];
	printf("config path:%s\n", configPath);
	if(!isFileExist(configPath)){	
		printf("[error] config.json not found!");
		return EXIT_FAILURE;
	}

	Document config;
	config.Parse(readFile(configPath).c_str());

	if (enet_initialize() != 0){
		printf("[error] An error occurred while initializing ENet");
		return EXIT_FAILURE;
	}

	ForwardCtrl ctrl;
	if (argv[2]) {
		ctrl.setupLogger(argv[2]);
	}
	else {
		ctrl.setupLogger("debug");
	}
    ctrl.setDebug(true);
    ctrl.setLogLevel(spdlog::level::level_enum::debug);

	RegisterSystemSignal(SIGINT, [&](int nSig)->void { ctrl.exist(); });
	
	/*if(config.HasMember("protocol")){
		ctrl.initProtocolMap(config["protocol"]);
	}*/
	ctrl.initServers(config["servers"]);

	debugDocument(ctrl.stat());

	ctrl.loop();

	atexit(enet_deinitialize);
	atexit(spdlog::drop_all);
	return 0;
}
