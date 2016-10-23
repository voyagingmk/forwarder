#ifndef FORWARDHEADER_H
#define FORWARDHEADER_H

#include "base.h"
#include "defines.h"

// small endian
class ForwardHeader
{
public:
	uint32_t getFlag(uint8_t pos) {
		uint8_t flag = (0xf0 & protocol) >> 4;
		return flag & pos;
	}
	uint8_t getProtocol() {
		return 0x0f & protocol;
	}
public:
	uint8_t version = FORWARDER_VERSION;
	uint8_t length = sizeof(ForwardHeader);
	uint8_t protocol = 0;
	uint8_t hash = 0;
	uint8_t subID = 0;
	uint8_t hostID = 0;
	uint16_t clientID = 0;
};

#endif