const jsface = require('jsface');
const defines = require('./forwarddifines');
const ForwardServerWS = require('./forwardserver').ForwardServerWS;
const ForwardServerENet = require('./forwardserver').ForwardServerENet;
const UniqIDGenerator = require('./forwarduniqid');


const ForwardCtrl = jsface.Class({
    constructor: function() {},
    reset() {
        this.m_Servers = [];
        this.m_ServerDict = {};
        this.m_IDGenerator = new UniqIDGenerator();
    },
    /* --------------------------------------- */
    /* ------------- Public API -------------- */
    /* --------------------------------------- */
    /*
        批量初始化本地服务器
    */
    initServers(serversConfig) {
        for (let i = 0; i < serversConfig.length; i++) {
            const serverConfig = serversConfig[i];
            this.createServer(serverConfig);
        }

        // init dest host
        for (let i = 0; i < this.m_Servers.length; i++) {
            const server = this.m_Servers[i];
            for (let j = 0; j < this.m_Servers.length; j++) {
                const _server = this.m_Servers[j];
                if (_server.id === server.destId) {
                    server.dest = _server;
                    break;
                }
            }
        }
    },
    /*
        增加一个本地服务器
    */
    createServer(serverConfig) {
        const netType = serverConfig.netType;
        const server = this.createServerByNetType(netType);
        server.initCommon(serverConfig);
        const id = this.m_IDGenerator.getNewID();
        server.id = id;
        this.m_Servers.push(server);
        this.m_ServerDict[server.ID()] = server;

        for (let i = 0; i < this.m_Servers.length; i++) {
            const _server = this.m_Servers[i];
            if (_server.id === server.destId) {
                server.dest = _server;
                break;
            }
        }
        return id;
    },
    /*
        删除服务器对象
    */
    removeServerByID(id) {
        if (this.m_ServerDict[id]) {
            const server = this.m_ServerDict[id];
            delete this.m_ServerDict[id];
            const idx = this.m_Servers.indexOf(server);
            this.m_Servers.splice(idx, 1);
        }
    },
    /*
        获取服务器对象
    */
    getServerByID(id) {
        return this.m_ServerDict[id];
    },
    /* --------------------------------------- */
    /* ------------- Private API ------------- */
    /* --------------------------------------- */
    createServerByNetType(netType) {
        if (netType === defines.NetType.WS) {
            return new ForwardServerWS();
        }
        return new ForwardServerENet();
    },
});


const NetCtrlBase = jsface.Class({
    constructor: function() {
        this.m_serverConfig = null;
        this.m_activeTime = 0; // 最后活跃时间
        this.m_PacketCount = 1;
        this.m_PacketQueue = [];
        this.m_Callbacks = { /* queIndex: callbackFunc  */ };
    },
    setPrototool(prototool) {
        this.prototool = prototool;
    },
    getProtoconfig() {
        return this.prototool && this.prototool.getConfig();
    },
    getHost() {
        if (!this.m_serverConfig) {
            return;
        }
        return this.m_serverConfig.host;
    },
    getPort() {
        if (!this.m_serverConfig) {
            return;
        }
        return this.m_serverConfig.port;
    },
    releaseConnect() {
        this.disconnect(true);
        this.m_serverConfig = null;
    },
    cleanPacketQueue() {
        this.m_PacketQueue = [];
    },
    // 发送队列中的包
    flushMessage() {
        // cc.log("[flushMessage]");
        const self = this;
        this.m_PacketQueue.forEach((dPacketData) => {
            if (dPacketData.header._queStatus > 0) {
                return;
            }
            if (self.isConnected()) {
                dPacketData.header._queStatus = 1;
                const cmd = dPacketData.header._cmd;
                dPacketData._cmd = cmd;
                self.onSendMsg(cmd, dPacketData);
            }
        });
    },
    pushPacket(cmd, dData) {
        const dPacketData = {
            header: {
                _cmd: cmd,
                _queIndex: this.m_PacketCount++,
                _queStatus: 0, //0:未发送，1:发送， 2:已收到反馈，可删除
            },
            data: dData,
        };
        this.onPushPacket(dPacketData);
        this.m_PacketQueue.push(dPacketData);
        return dPacketData;
    },
    // 可重载
    onPushPacket(dPacketData) {
        if (mbgGame.state && mbgGame.state.pid) {
            dPacketData.header._pid = mbgGame.state.pid;
        }
        if (mbgGame.setting && mbgGame.setting.token) {
            dPacketData.header._token = mbgGame.setting.token.substr(-6);
        } else if (mbgGame.state && mbgGame.state.token) {
            dPacketData.header._token = mbgGame.state.token.substr(-6);
        }
    },
    // 发包：先放入m_PacketQueue，网络可用时再发送
    sendMessage(cmd, dData, callback) {
        this.onBeforeSend(cmd, dData);
        const dPacketData = this.pushPacket(cmd, dData);
        if (!dPacketData) {
            cc.error("[sendMessage] no dPacketData");
            return;
        }
        if (callback) {
            this.m_Callbacks[dPacketData.header._queIndex] = callback;
        }
        this.checkConnect();
        this.flushMessage();
    },
    sendMsg(cmd, dData, callback) {
        return this.sendMessage(cmd, dData, callback);
    },
    cleanServerConfig() {
        this.m_serverConfig = null;
    },
    setServerConfig(serverConfig) {
        this.m_serverConfig = serverConfig;
    },
    getServerConfig() {
        return this.m_serverConfig;
    },
    dispatchMessage(dData) {
        if (!dData.header) {
            return;
        }
        // cc.log("[dispatchMessage] msg:\n", JSON.stringify(dData));
        if (dData.header._queIndex) {
            const callback = this.m_Callbacks[dData.header._queIndex];
            if (callback) {
                cc.log("[dispatchMessage.callback]");
                callback(dData.data, dData.header);
            }
        } else {
            // 服务器主动发的包？
            if (this.onReceivedServerMsg) {
                this.onReceivedServerMsg(dData.header._cmd, dData.data);
            }
        }
        // dData.header, dData.data
        // switch (msg.header._cmd);
    },
    fromJSON(textData) {
        let dPacketData;
        try {
            dPacketData = JSON.parse(textData);
        } catch (error) {
            cc.error("[wsCon.parseUtf8Data] JSON.parse failed", textData, error);
            return;
        }
        const dNetHeader = {};
        if (dPacketData._type === "encrypt") {
            try {
                const compressed_msg = dPacketData.msg;
                const server_hash = dPacketData.hash;
                if (!server_hash) {
                    return;
                }
                // password放在这里不是很安全
                let hash = CryptoJS.SHA3(`${compressed_msg }NeverEverTellOthers!`, {
                    outputLength: 224,
                });
                hash = hash.toString(CryptoJS.enc.Hex);
                if (hash != server_hash) {
                    cc.error("server_hash unvalid");
                    return;
                }
                const real_msg = this.uncompress(compressed_msg);
                dPacketData = JSON.parse(real_msg);
            } catch (error) {
                cc.error("[wsCon.parseUtf8Data] uncompress failed", data, error);
                return;
            }
        }
        if (dPacketData._meta) {
            dNetHeader.meta = dPacketData._meta;
            delete dPacketData._meta;
        }
        const result = {
            dNetHeader,
            dPacketData,
        };
        return result;
    },
    fromProto(bytesData) {
        const protoconfig = this.getProtoconfig();
        const GS2C_CMD = protoconfig.GS2C_CMD;
        const ProtoMsg_NetPacket = this.prototool.findMsg(protoconfig.NetPacket);
        const msgNetPacket = this.prototool.fromBuf(bytesData, ProtoMsg_NetPacket, true);

        const dNetHeader = {
            // meta:{},
        };
        const dGSPacketData = {
            data: {
                // data:{},
                // code:"",
            },
            header: {},
        };

        if (msgNetPacket.get("header")) {
            const jsonHaader = msgNetPacket.get("header").encodeJSON();
            dGSPacketData.header = JSON.parse(jsonHaader);
        }
        if (msgNetPacket.get("meta")) {
            dNetHeader.meta = msgNetPacket.get("meta");
        }
        let oneOfFieldName = null;
        const self = this;
        _.find(GS2C_CMD, (_protoData) => {
            const _oneOfFieldName = self.prototool.getOneOfFieldName(msgNetPacket, _protoData.oneOfID);
            if (_oneOfFieldName === msgNetPacket.data) {
                oneOfFieldName = _oneOfFieldName;
            }
        });
        if (oneOfFieldName) {
            const msgOneof = msgNetPacket.get(oneOfFieldName);
            const jsonOneof = msgOneof.encodeJSON();
            const dOneofData = JSON.parse(jsonOneof);
            dGSPacketData.data.data = dOneofData; // data域
        }

        const code = msgNetPacket.get("code");
        if (code) {
            dGSPacketData.data.code = code;
        } // code域

        const result = {
            dNetHeader,
            dPacketData: dGSPacketData,
        };
        return result;
    },
    toJSON(dPacketData) {
        try {
            const sData = JSON.stringify(dPacketData);
            let logStr = `toJSON <${ dPacketData._cmd }>,_queIndex:${ dPacketData.header._queIndex},length:${(sData.length / 1024).toFixed(2) }k`;
            const compressed_msg = this.compress(sData);
            let hash = CryptoJS.SHA3(`${compressed_msg}NeverEverTellOthers!`, {
                outputLength: 224,
            });
            hash = hash.toString(CryptoJS.enc.Hex);
            const dNewPacketData = {
                _type: "encrypt",
                msg: compressed_msg,
                hash,
            };
            const sPacketData = JSON.stringify(dNewPacketData);
            logStr += `,com length:${(sPacketData.length / 1024).toFixed(2) }k`;
            // cc.log(logStr);
            return sPacketData;
        } catch (error) {
            cc.error("[toJSON] failed", dPacketData, error);
            return;
        }
    },
    toProto(dPacketData, protoData) {
        /*
        var a = new ArrayBuffer(3);
        var b = new Uint8Array(a, 0);
        b[0] = 10;
        b[1] = 15;
        b[2] = 5;
        return a;*/
        if (!this.prototool) {
            cc.error("[client_common.net.toProto] no this.prototool");
            return;
        }

        const dData = dPacketData;
        const dUserHeader = dData.header;
        const dUserData = dData.data;
        const prototool = this.prototool;
        const protoconfig = this.getProtoconfig();
        try {
            const ProtoMsg_NetPacket = prototool.findMsg(protoconfig.NetPacket);
            const msgNetPacket = new ProtoMsg_NetPacket();
            /*
            message NetPacket {
                Header header = 1;          //消息头，系统层的数据，用户层不能修改
                optional Meta meta = 2;     //用户层的元数据
                optional string code = 3;   //返回码 ok、err
                oneof data {                //用户层的数据
                    player.Baseinfo baseinfo = 4;
                    ···
                    ···
                }
            }*/
            const ProtoMsg_Header = prototool.findMsg(protoconfig.Header);
            const msgHeader = new ProtoMsg_Header(dUserHeader);
            /*
            message Header {
                string _cmd = 1;
                optional int32 _queIndex = 2;
                optional int32 _queStatus = 3;
                optional string _pid = 4;
                optional string _token = 5;
            }
             */
            msgNetPacket.set("header", msgHeader);
            /*
            if (dC2SHeader.meta) {
                var ProtoMsg_Meta = prototool.findMsg(protoconfig.Meta);
                var msgMeta = new ProtoMsg_Meta(dFS2CHeader.meta);
                msgNetPacket.set("meta", msgMeta);
            }*/

            if (dUserData.data) {
                let oneOfTypeName = prototool.getOneOfTypeName(msgNetPacket, protoData.oneOfID);
                oneOfTypeName = protoconfig.rootPackageName + oneOfTypeName;
                const oneOfFieldName = prototool.getOneOfFieldName(msgNetPacket, protoData.oneOfID);

                const ProtoMsg_UserData = prototool.findMsg(oneOfTypeName);
                const msgUserData = new ProtoMsg_UserData(dUserData.data);
                msgNetPacket.set(oneOfFieldName, msgUserData);
            }
            if (dUserData.code) {
                msgNetPacket.set("code", dUserData.code);
            }
            const buf = prototool.toBuf(msgNetPacket, true);
            if (!buf) {
                cc.error(`[netSendBytes] toBuf failed, cmd=${cmd},dGSPacketData=${ JSON.stringify(dGSPacketData)}`);
                return;
            }
            return buf;
        } catch (error) {
            cc.error("[netSendProto] failed", dData, error);
            return;
        }
    },
    compressArray(array) {
        const gzip = new Zlib.Gzip(array);
        const compressed_array = gzip.compress();
        return base64js.fromByteArray(compressed_array);
    },
    compress(msg) {
        return this.compressArray(base64js.toByteArray(Base64.encode(msg)));
    },
    uncompress(compressed_msg) {
        const array = base64js.toByteArray(compressed_msg);
        const gunzip = new Zlib.Gunzip(array);
        const origin_array = gunzip.decompress();
        let msg = base64js.fromByteArray(origin_array);
        msg = Base64.decode(msg);
        return msg;
    },
    onSendMsg(cmd, dPacketData) {
        if (this.getProtoDataByCmd(cmd)) {
            const bytesData = this.toProto(dPacketData, this.getProtoDataByCmd(cmd));
            this.onSendProto(bytesData);
            return;
        }
        const sData = this.toJSON(dPacketData);
        this.onSendJson(sData);
    },
    // 注意：dHeader可能为空
    onConMessage(dNetHeader, dPacketData) {
        if (dPacketData.header) {
            // cc.log("[onConMessage] <" + dPacketData.header._cmd + ">, _queIndex:" + dPacketData.header._queIndex + ", PacketQueue.len:" + this.m_PacketQueue.length);
        }
        this.dispatchMessage(dPacketData);
        if (dPacketData.header._queIndex) {
            // 处理队列，把收到返回的包标记已处理，并移出队列
            this.m_PacketQueue = this.m_PacketQueue.filter((p) => {
                if (dPacketData.header && p.header._queIndex == dPacketData.header._queIndex) {
                    p.header._queStatus = 2; // 标记，移出队列
                    // cc.log("[onConMessage] get return, deleted.", dPacketData);
                    return false;
                }
                return true;
            });
        }
        this.m_activeTime = mbgGame_utils.nowTime();
    },
    // override
    isConnected() {
        return true;
    },
    // override
    disconnect() {
        cc.log("[NetCtrl.disconnect]");
    },
    // override
    onSendJson(textData) {},
    // override
    onSendProto(byteData) {},
    // override
    // 如果没有连接，那么连上去
    checkConnect() {
        /*
        var serverConfig = this.getServerConfig();
        if(serverConfig){
            this.setupConnect();
        }
        */
    },
    // override
    setupConnect() {},
    // override
    onDisconnect() {},
    // override
    connectObj() {
        return null;
    },
    // override
    onBeforeSend(cmd, dData) {},
    connected() {
        this.m_activeTime = mbgGame_utils.nowTime();
        this.m_serverConfig.status = net.Status.Connected; // 连接成功
        this.flushMessage(); // 连接成功则发送队列中的包
        this.onConnected();
    },
    onConnected() {

    },
});

module.exports = {
    ForwarderBase,
    NetCtrlBase,
};