var WebSocketClient = require('websocket').client;
var forwarder = require('./forwarder');

var client = new WebSocketClient();
 
client.on('connectFailed', function(error) {
    console.log('Connect Error: ' + error.toString());
});
 
client.on('connect', function(connection) {
    console.log('WebSocket Client Connected');
    connection.on('error', function(error) {
        console.log("Connection Error: " + error.toString());
    });
    connection.on('close', function() {
        console.log('echo-protocol Connection Closed');
    });
    connection.on('message', function(message) {
        if (message.type === 'utf8') {
            console.log("Received utf8:", message.utf8Data);
        }else{
            console.log("Received binary: ", message.binaryData);
        }
    });

    function sendTest() {
        if (connection.connected) {
            var packet = forwarder.makePacket({
                protocol: 2,
                subID: 1,
                content: "jjj"
            });
            connection.sendBytes(packet);
            setTimeout(sendTest, 3000);
        }
    }
    sendTest();
    
});
 
client.connect('ws://localhost:9998/');