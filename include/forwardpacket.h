#ifndef FORWARDPACKET_H
#define FORWARDPACKET_H


namespace forwarder {

	class ForwardPacket {
	public:
		virtual uint8_t* getDataPtr() const = 0;
		virtual void* getRawPtr() const = 0;
		size_t getLength() const {
			return length;
		}
		virtual void setHeader(ForwardHeader* header) = 0;
		virtual void setData(uint8_t* data, size_t len) = 0;
		ForwardPacket() {
		}
		~ForwardPacket() {
		}
	protected:
		size_t length = 0;
	};

	typedef std::shared_ptr<ForwardPacket> ForwardPacketPtr;


	class ForwardPacketENet : public ForwardPacket {
	public:
		ForwardPacketENet(ENetPacket* p_packet) :
			owned(false),
			packet(p_packet)
		{
			length = p_packet->dataLength;
		}

		ForwardPacketENet(size_t len) :
			owned(true)
		{
			packet = enet_packet_create(NULL, len, ENET_PACKET_FLAG_RELIABLE);
			memset(packet->data, '\0', len);
			length = len;
		}

		~ForwardPacketENet() {
			packet = nullptr;
		}

		virtual uint8_t* getDataPtr() const {
			return static_cast<uint8_t*>(packet->data);
		}

		virtual void* getRawPtr() const {
			return static_cast<void*>(packet);
		}

		virtual void setHeader(ForwardHeader* header) {
			memcpy(packet->data, header, sizeof(ForwardHeader));
		}

		virtual void setData(uint8_t* data, size_t len) {
			if ((len + sizeof(ForwardHeader)) > packet->dataLength) {
				printf("err");
				return;
			}
			memcpy(packet->data + sizeof(ForwardHeader), data, len);
			if (!length) {
				length = sizeof(ForwardHeader) + len;
			}
		}
	public:
		bool owned;
		ENetPacket* packet = nullptr;
	};

	class ForwardPacketWS : public ForwardPacket {
	public:
		ForwardPacketWS(void* p_data) :
			owned(false),
			packetData(static_cast<uint8_t*>(p_data))
		{}

		ForwardPacketWS(uint8_t* p_data) :
			owned(false),
			packetData(p_data)
		{}

		ForwardPacketWS(size_t len) :
			owned(true)
		{
			length = len;
			packetData = new uint8_t[len]{ 0 };
		}

		~ForwardPacketWS() {
			if (owned && packetData) {
				delete packetData;
				packetData = nullptr;
			}
		}

		virtual uint8_t* getDataPtr() const {
			return packetData;
		}

		virtual void* getRawPtr() const {
			return static_cast<void*>(packetData);
		}

		virtual void setHeader(ForwardHeader* header) {
			memcpy(packetData, header, sizeof(ForwardHeader));
		}

		virtual void setData(uint8_t* data, size_t len) {
			memcpy(packetData + sizeof(ForwardHeader), data, len);
			if (!length) {
				length = sizeof(ForwardHeader) + len;
			}
		}
	public:
		bool owned = false;
		uint8_t* packetData = nullptr;
	};
}

#endif