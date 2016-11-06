#ifndef DEFINES_H
#define DEFINES_H

namespace forwarder {

	constexpr int Version = 1;

	enum ReturnCode {
		Ok = 1,
		Err = 2
	};

	enum ProtocolFlag {
		WithAddress = 1
	};

	enum ProtocolType {
		Unknown = 0
	};

	enum NetType {
		ENet = 1,
		WS = 2
	};

	enum Convert {
		None = 0,
		ENet_to_WS = 1,
		WS_to_ENet = 2,
		Raw_to_Base64 = 3,
		Base64_to_Raw = 4,
		Encrypt = 5,
		Decrypt = 6
	};
}

#endif