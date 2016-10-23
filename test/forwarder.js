var enet = require('enet');
const FORWARDER_VERSION = 1;
const FORWARDER_PACKET_LENGTH = 8;

function unmakePacket(param){
	const data = param.data;
	const header = Buffer.from(data,0,8);
	console.log("header.length",header.length);
	console.log("param.data.length",data.length);
	const contentBuf = Buffer.from(data,8, data.length);
	let content = data.slice(8, data.length).toString("ascii");
	return {
		content
	};
}

function makePacket(param){
	//small-endian
	let arr = new Uint8Array(FORWARDER_PACKET_LENGTH);
	/*
	uint8_t version;
	uint8_t length;
	uint8_t protocol;
	uint8_t hash;
	uint8_t subID;
	uint8_t hostID;
	uint16_t clientID;
	*/
	arr[0] = FORWARDER_VERSION; 
	arr[1] = FORWARDER_PACKET_LENGTH; 
	arr[2] = param.protocol || 1;
	arr[3] = 255; //hash
	arr[4] = param.subID || 0; //subID
	arr[5] = typeof(param.hostID) === "number"? parseInt(param.hostID) : 0;
	arr[6] = 0;
	arr[7] = 0;
	if(typeof(param.clientID) === "number") {
		arr[6] = 0xff & param.clientID>>8; //clientID
		arr[7] = 0xff & param.clientID; 
	}

	// Shares memory with `arr`
	const headerBuf = new Buffer(arr.buffer);
	const contentBuf = new Buffer(param.content||"");
	const buf = Buffer.concat([headerBuf, contentBuf], headerBuf.length + contentBuf.length);
	const packet = new enet.Packet(buf, enet.PACKET_FLAG.RELIABLE);
	return packet;
}

module.exports = {
	makePacket,
	unmakePacket
};