const enet = require('enet');
const crypto = require('crypto');
const base64 = require('./base64');

const FORWARDER_VERSION = 1;
const HEADER_LENGTH = 8;
const algorithm = "AES-128-CTR";
const originEncoding = 'utf8';
const ivSize = 16;

function unmakePacket(param) {
    const data = param.data;
    // console.log("data", data);
    // console.log("data.length", data.length);
    const header = data.slice(0, HEADER_LENGTH);
    // console.log("header", header, 'len', header.length);
    let content;
    if (param.encrypt) {
        const ivBuf = data.slice(HEADER_LENGTH, HEADER_LENGTH + ivSize);
        console.log("ivBuf", ivBuf, 'len', ivBuf.length);
        const key = param.encryptkey;
        const decipher = crypto.createDecipheriv(algorithm, Buffer(key), ivBuf);
        const plainChunks = [];
        const encryptedContent = data.slice(HEADER_LENGTH + ivSize);
        console.log("encryptedContent", encryptedContent, 'len', encryptedContent.length);
        plainChunks.push(decipher.update(encryptedContent));
        plainChunks.push(decipher.final());
        content = Buffer.concat(plainChunks);
        console.log("content", Buffer(content), 'len', Buffer(content).length);
        content = content.toString();
    } else {
        content = data.toString();
    }
    return {
        header,
        content,
    };
}

function makePacket(param) {
    if (!param.content) {
        return null;
    }
    // small-endian
    const arr = new Uint8Array(HEADER_LENGTH);
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
    arr[1] = HEADER_LENGTH;
    arr[2] = param.protocol || 1;
    arr[3] = 255; // hash
    arr[4] = param.subID || 0; // subID
    arr[5] = typeof(param.hostID) === "number" ? parseInt(param.hostID) : 0;
    arr[6] = 0;
    arr[7] = 0;
    if (typeof(param.clientID) === "number") {
        arr[6] = 0xff & param.clientID >> 8; // clientID
        arr[7] = 0xff & param.clientID;
    }

    // Shares memory with `arr`
    const headerBuf = new Buffer(arr.buffer);
    let contentBuf;
    if (param.encrypt) {
        const key = param.encryptkey;
        const ivBuf = crypto.randomBytes(ivSize);
        // console.log("ivBuf", ivBuf, 'len', ivBuf.length);
        const cipher = crypto.createCipheriv(algorithm, Buffer(key), ivBuf);
        const cipherChunks = [ivBuf];
        console.log("originBuf", Buffer(param.content), 'len', Buffer(param.content).length);
        cipherChunks.push(cipher.update(param.content, originEncoding));
        cipherChunks.push(cipher.final());
        contentBuf = Buffer.concat(cipherChunks);
        // console.log("contentBuf", contentBuf, 'len', contentBuf.length);
    } else if (param.base64) {
        contentBuf = new Buffer(param.content);
        const contentBase64 = base64.fromByteArray(contentBuf);
        contentBuf = new Buffer(contentBase64);
    } else {
        contentBuf = new Buffer(param.content);
    }
    const buf = Buffer.concat([headerBuf, contentBuf], headerBuf.length + contentBuf.length);
    if (param.type === "enet") {
        const packet = new enet.Packet(buf, enet.PACKET_FLAG.RELIABLE);
        return packet;
    } else {
        return buf;
    }
}

module.exports = {
    makePacket,
    unmakePacket,
};