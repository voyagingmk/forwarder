const mbgGame = require('mbgGame');
const mbgGameTimer = require('timer');
const mbgGameUtils = require('utils');

const EventType = {
    Nothing: 0,
    Connected: 1,
    Diconnected: 2,
    Message: 3,
};

class NetMgr {
    constructor() {
        this.m_serverConfig = null;
        this.m_activeTime = 0; // 最后活跃时间
        this.m_PacketCount = 1;
        this.m_PacketQueue = [];
        this.m_Callbacks = { /* queIndex: callbackFunc  */ };
        this.m_TimerOwnerID = mbgGameTimer.newOwnerID();
        this.m_Ctrl = forwarder.ForwardCtrl.create();
        this.m_Ctrl.initProtocolMap(JSON.stringify({
            2: "Process",
        }));
    }
    setServerConfig(serverConfig) {
        this.m_serverConfig = serverConfig;
    }
    cleanServerConfig() {
        this.m_serverConfig = null;
    }
    getServerConfig() {
        return this.m_serverConfig;
    }
    setupConnect() {
        const serverID = this.m_Ctrl.createServer(JSON.stringify({
            id: 1, // doesn't matter
            desc: "C2FS_ENet",
            netType: "enet",
            encrypt: true,
            encryptkey: "1234567812345678",
            base64: true,
            peers: 1,
            port: this.getPort(),
            isClient: true,
            address: this.getHost(),
            reconnect: true,
        }));
        this.m_ServerID = serverID;
    }
    onConnected() {
        this.m_activeTime = mbgGameUtils.nowTime();
        this.flushMessage(); // 连接成功则发送队列中的包
    }
    isConnected() {
        return this.m_Ctrl.isConnected(this.m_ServerID);
    }
    disconnect() {
        return this.m_Ctrl.doDisconnect(this.m_ServerID);
    }
    onDisconnect() {
        this.m_disconnectedTime = mbgGameUtils.nowTime();
    }
    getHost() {
        return this.m_serverConfig.host;
    }
    getPort() {
        return this.m_serverConfig.port;
    }
    releaseConnect() {
        this.disconnect(true);
        this.m_serverConfig = null;
    }
    setupUpdateTimer() {
        mbgGameTimer.pauseAllCallOutByOwner(this);
        mbgGameTimer.callOut(this, this.onPoll.bind(this), {
            time: 0.01,
            flag: "onPoll",
            forever: true,
        });
        mbgGameTimer.callOut(this, this.onFlushMessage.bind(this), {
            time: 1,
            flag: "onFlushMessage",
            forever: true,
        });
    }
    onPoll() {
        this.m_Ctrl.pollOnce(this.m_ServerID);
        const evt = this.m_Ctrl.getCurEvent();
        if (evt === EventType.Nothing) {
            return;
        } else if (evt === EventType.Message) {
            const sData = this.m_Ctrl.getCurPacketData();
            const dData = JSON.parse(sData);
            const dNetHeader = dData.dNetHeader;
            const dPacketData = dData.dPacketData;
            this.onConMessage(dNetHeader, dPacketData);
        } else if (evt === EventType.Connected) {
            this.onConnected();
        } else if (evt === EventType.Diconnected) {
            this.onDisconnect();
        }
    }
    cleanPacketQueue() {
        this.m_PacketQueue = [];
    }
    flushMessage() {
        // 发送队列中的包
        for (let i = 0; i < this.m_PacketQueue.length; i++) {
            const dPacketData = this.m_PacketQueue[i];
            if (dPacketData.header._queStatus > 0) {
                continue;
            }
            if (this.isConnected()) {
                dPacketData.header._queStatus = 1;
                const cmd = dPacketData.header._cmd;
                dPacketData._cmd = cmd;
                this.onSendMsg(cmd, dPacketData);
            }
        }
    }
    onFlushMessage() {
        if (this.isConnected()) {
            this.flushMessage();
        }
    }
    pushPacket(cmd, dData) {
        const _queIndex = this.m_PacketCount;
        this.m_PacketCount += 1;
        const dPacketData = {
            header: {
                _cmd: cmd,
                _queIndex,
                _queStatus: 0, //0:未发送，1:发送， 2:已收到反馈，可删除
            },
            data: dData,
        };
        NetMgr.onPushPacket(dPacketData);
        this.m_PacketQueue.push(dPacketData);
        return dPacketData;
    }
    static onPushPacket(dPacketData) {
        if (mbgGame.state && mbgGame.state.pid) {
            dPacketData.header._pid = mbgGame.state.pid;
        }
        if (mbgGame.setting && mbgGame.setting.token) {
            dPacketData.header._token = mbgGame.setting.token.substr(-6);
        } else if (mbgGame.state && mbgGame.state.token) {
            dPacketData.header._token = mbgGame.state.token.substr(-6);
        }
    }
    sendMessage(cmd, dData, callback) {
        const dPacketData = this.pushPacket(cmd, dData);
        if (!dPacketData) {
            cc.error("[sendMessage] no dPacketData");
            return;
        }
        if (callback) {
            this.m_Callbacks[dPacketData.header._queIndex] = callback;
        }
        this.flushMessage();
    }
    sendMsg(cmd, dData, callback) {
        return this.sendMessage(cmd, dData, callback);
    }
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
        } else if (this.onReceivedServerMsg) { // 服务器主动发的包？
            this.onReceivedServerMsg(dData.header._cmd, dData.data);
        }
        // dData.header, dData.data
        // switch (msg.header._cmd);
    }
    onSendMsg(cmd, dPacketData) {
        const sData = JSON.stringify(dPacketData);
        this.m_Ctrl.sendText(this.m_ServerID, sData);
    }
    onConMessage(dNetHeader, dPacketData) {
        // 注意：dHeader可能为空
        if (dPacketData.header) {
            // cc.log("[onConMessage] <" + dPacketData.header._cmd + ">, _queIndex:" + dPacketData.header._queIndex + ", PacketQueue.len:" + this.m_PacketQueue.length);
        }
        this.dispatchMessage(dPacketData);
        if (dPacketData.header._queIndex) {
            // 处理队列，把收到返回的包标记已处理，并移出队列
            this.m_PacketQueue = this.m_PacketQueue.filter((p) => {
                if (dPacketData.header && p.header._queIndex === dPacketData.header._queIndex) {
                    p.header._queStatus = 2; // 标记，移出队列
                    // cc.log("[onConMessage] get return, deleted.", dPacketData);
                    return false;
                }
                return true;
            });
        }
        this.m_activeTime = mbgGameUtils.nowTime();
    }
}


module.exports = NetMgr;