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
}

#endif