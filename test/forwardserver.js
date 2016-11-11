const jsface = require('jsface');
const defines = require('./forwarddifines');
const UniqIDGenerator = require('./forwarduniqid');

const ForwardServerBase = jsface.Class({
    constructor: function() {
        this.reset();
    },
    reset() {
        this.id = 0;
        this.destId = 0;
        this.admin = false;
        this.encrypt = false;
        this.base64 = false;
        this.peerLimit = 0;
        this.encryptkey = null;
        this.netType = defines.NetType.Null;
        this.dest = null;
        this.idGenerator = null;
        this.clients = {}; // {UniqID: ForwardClient}
        this.desc = "";
        this.reconnect = false;
        this.m_IDGenerator = new UniqIDGenerator();
    },
    initCommon(serverConfig) {
        this.desc = serverConfig.desc;
        this.peerLimit = serverConfig.peers;
        this.admin = serverConfig.admin;
        this.encrypt = serverConfig.encrypt;
        this.base64 = serverConfig.base64;
    },
});




const ForwardServerENet = jsface.Class(ForwardServerBase, {
    constructor: function() {

    },
    release() {
        this.reset();
    },
});


const ForwardServerWS = jsface.Class(ForwardServerBase, {
    constructor: function() {

    },
    release() {
        this.reset();
    },
});

module.exports = {
    ForwardServerWS,
    ForwardServerENet,
};