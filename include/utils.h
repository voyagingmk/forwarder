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
	throw std::exception("register signal failed.");
}

void debugDocument(rapidjson::Document& d);

#endif
