#ifndef FORWARDHEADER_H
#define FORWARDHEADER_H

#include "base.h"
#include "defines.h"

/*
|  1 byte		|  1 byte			|  1 / 2 byte		|  1 / 2 byte		|  1 byte	| 
|  Version		|  Length of Header	|  Protocol.Type	|  Protocol.Flag	|  hash		| 

|  1 byte		|  1 byte			|  						2 bytes						| 
|  SubID		|  Host ID			|  						Client ID					| 

|										4 bytes											|
|									    data / ip										|


*/

namespace forwarder {
	// small endian
	class ForwardHeader
	{
	public:
		inline uint8_t getVersion() {
			return version;
		}

		uint32_t getProtocolFlag(uint8_t pos) {
			uint8_t flag = (0xf0 & protocol) >> 4;
			return flag & pos;
		}

		inline uint8_t getProtocolType() {
			return 0x0f & protocol;
		}

		inline uint8_t getHash() {
			return hash;
		}

		inline uint8_t getHostID() {
			return hostID;
		}

		inline uint16_t getClientID() {
			return clientID;
		}

		inline uint8_t getSubID() {
			return subID;
		}
	public:
		uint8_t version = Version;
		uint8_t length = sizeof(ForwardHeader);
		uint8_t protocol = 0;
		uint8_t hash = 0;
		uint8_t subID = 0;
		uint8_t hostID = 0;
		uint16_t clientID = 0;
		union{
			uint32_t ip;
			uint8_t data[4];
		};
	};

}
#endif