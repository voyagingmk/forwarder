#ifndef DEFINES_H
#define DEFINES_H

namespace forwarder {

	constexpr int ForwarderVersion = 1;
	constexpr int HeaderVersion = 1;
	constexpr int MaxBufferSize = 1024 * 1024 * 256; // 256 MB
    
	enum class ReturnCode {
		Ok = 1,
		Err = 2
	};

	enum class ProtocolType {
		Unknown = 0
	};

	enum class NetType {
		ENet = 1,
		WS = 2,
        TCP = 3
	};

	enum class Event {
		Nothing = 0,
		Connected = 1,
		Disconnected = 2,
        Message = 3,
        Forward = 4
	};

	enum class Convert {
		None = 0,
		ENet_to_WS = 1,
		WS_to_ENet = 2,
		Raw_to_Base64 = 3,
		Base64_to_Raw = 4,
		Encrypt = 5,
		Decrypt = 6
	};

	typedef uint32_t ProtocolID;
	
    // protocol -> rule
    enum class Protocol {
        Unknown = 0,
        SysCmd = 1,
        Forward = 2,
        Process = 3,
        BatchForward = 4
    };
    
	enum class HandleRule {
		Unknown,
		SysCmd,
		Forward,
		Process,
        BatchForward
	};
}

#endif
