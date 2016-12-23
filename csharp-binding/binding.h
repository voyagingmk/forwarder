#ifndef FORWARDER_BINDING_H
#define FORWARDER_BINDING_H
#include <string>
#include "forwardctrl.h"

#define DLL_EXPORT extern "C" __declspec(dllexport)


DLL_EXPORT void SetDebugFunction(forwarder::DebugFuncPtr fp);

DLL_EXPORT int initENet();

DLL_EXPORT void release();

DLL_EXPORT int version();

DLL_EXPORT void setupLogger(const char* filename);

DLL_EXPORT void setDebug(bool debug);

DLL_EXPORT int initProtocolMap(const char* config);

DLL_EXPORT void initServers(const char* sConfig);

DLL_EXPORT uint32_t createServer(const char* sConfig);

DLL_EXPORT uint32_t removeServerByID(int serverId);

DLL_EXPORT bool disconnect(int serverId);

DLL_EXPORT bool isConnected(int serverId);

DLL_EXPORT void setTimeout(int serverId, int timeoutLimit, int timeoutMinimum, int timeoutMaximum);

DLL_EXPORT void setPingInterval(int serverId, int interval);

DLL_EXPORT uint32_t sendText(int serverId, int clientId, const char* data);

DLL_EXPORT uint32_t sendBinary(int serverId, int clientId, void* data, int length);

DLL_EXPORT uint32_t getCurEvent();

// inline ForwardServer* getCurProcessServer();
DLL_EXPORT uint32_t getCurProcessServerID();

DLL_EXPORT uint32_t getCurProcessClientID();

DLL_EXPORT uint8_t* getCurProcessPacket();

DLL_EXPORT void pollOnceByServerID(int serverId);

DLL_EXPORT std::string getStatInfo();


#endif