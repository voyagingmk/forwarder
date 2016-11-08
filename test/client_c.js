const enet = require('enet');
const forwarder = require('./forwarder');

const server_addr = new enet.Address("127.0.0.1", 10000);


const client = enet.createClient({
    peers: 1,
    /* only allow 1 outgoing connection */
    channels: 2,
    /* allow up to 2 channels to be used, 0 and 1 */
    down: 57600 / 8,
    /* 56K modem with 56 Kbps downstream bandwidth */
    up: 14400 / 8,
    /* 56K modem with 14 Kbps upstream bandwidth */
}, (err, host) => {
    if (err) {
        console.error(err);
        return; /* host creation failed */
    }
});

const peer = client.connect(server_addr,
    2, /* channels */
    9, /* data to send, (received in 'connect' event at server) */
    (err, peer) => { /* a connect callback function */
        if (err) {
            console.error(err); // either connect timeout or maximum peers exceeded
            return;
        }
        // connection to the remote host succeeded
        // peer.ping();
    });

function sendTest() {
    const packet = forwarder.makePacket({
        type: "enet",
        protocol: 2,
        subID: 1,
        //encrypt: true,
        //encryptkey: "1234567812345678",
        content: "world",
    });
    peer.send(0, packet, (err) => {
        if (err) {
            console.log("error sending packet 1:", err);
        } else {
            console.log("packet 1 sent.");
        }
    });
    //setTimeout(sendTest, 3000);
}
// succesful connect event can also be handled with an event handler
peer.on("connect", (err) => {
    if (err) {
        console.error(err); // either connect timeout or maximum peers exceeded
        return;
    }
    console.log("connected");

    sendTest();
    /*
    const packet = forwarder.makePacket({
        protocol: 2,
        subID: 1,
        content: "",
    });
    console.log("sending packet 1...");
    peer.send(0, packet, (err) => {
        if (err) {
            console.log("error sending packet 1:", err);
        } else {
            console.log("packet 1 sent.");
        }
    });*/
});

peer.on("disconnect", (err) => {
    if (err) {
        console.error(err); // either connect timeout or maximum peers exceeded
        return;
    }
    console.log("disconnect");
});

peer.on("message", (packet, channelID) => {
    // handle packet from peer
    console.log("message,channelID", channelID, "len", packet.dataLength());
    console.log(packet.data());
    console.log(packet.data().toString());
    const result = forwarder.unmakePacket({
        data: packet.data(),
    });
    console.log("result", result);
});