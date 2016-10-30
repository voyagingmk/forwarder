#ifndef UTILS_H
#define UTILS_H
#include "base.h"

std::string readFile(const std::string &fileName);

bool isFileExist(const char *fileName);

bool setupLogger();

typedef std::function< void(int) > SignalFunc;

static std::array< SignalFunc, 64 > signalsFunc;

static void cHandler(int nSig)
{
    signalsFunc.at(nSig)(nSig);
}

static SignalFunc RegisterSystemSignal( int sig, SignalFunc func)
{
    if( signal( sig, func ? &cHandler : SIG_DFL ) != SIG_ERR )
    { 
        func.swap( signalsFunc.at( sig ) );
        return func;
    }
	throw std::runtime_error("register signal failed.");
}

void debugDocument(const rapidjson::Document& d);


static inline std::shared_ptr<spdlog::logger> logger() {
	return spdlog::get("my_logger");
}

static inline std::shared_ptr<spdlog::logger> logger(const char * name) {
	return spdlog::get(name);
}
#endif
