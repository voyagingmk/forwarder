const WebSocketClient = require('websocket').client;
const forwarder = require('./forwarder');
const base64 = require('./base64');

const client = new WebSocketClient();

client.on('connectFailed', (error) => {
    console.log(`Connect Error: ${error.toString()}`);
});

client.on('connect', (connection) => {
    console.log('WebSocket Client Connected');
    connection.on('error', (error) => {
        console.log(`Connection Error: ${error.toString()}`);
    });
    connection.on('close', () => {
        console.log('echo-protocol Connection Closed');
    });
    connection.on('message', (message) => {
        if (message.type === 'utf8') {
            console.log("Received utf8:", message.utf8Data);
        } else {
            console.log("Received binary: ", message.binaryData);
            const result = forwarder.unmakePacket({
                encrypt: true,
                encryptkey: "1234567812345678",
                data: message.binaryData,
            });
            console.log("result", result);
        }
    });

    function sendTest() {
        if (connection.connected) {
            const packet = forwarder.makePacket({
                type: "ws",
                protocol: 2,
                subID: 1,
                base64: false,
                encrypt: true,
                encryptkey: "1234567812345678",
                content: "hello",
            });
            connection.sendBytes(packet);
            setTimeout(sendTest, 3000);
        }
    }
    // sendTest();
});

client.connect('ws://localhost:9998/');