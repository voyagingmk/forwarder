#ifndef DEFINES_H
#define DEFINES_H

constexpr int FORWARDER_VERSION = 1;

constexpr int FORWARDER_OK = 0;
constexpr int FORWARDER_ERR = 1;

constexpr int FORWARDER_FLAG_WITH_ADDRESS = 1;


constexpr int FORWARDER_PROTOCOL_UNKNOWN = 0;

enum Protocol {
	ENet = 1,
	WS = 2
};
constexpr int FORWARDER_PROTOCOL_ENET = 1;
constexpr int FORWARDER_PROTOCOL_WS = 2;

#endif