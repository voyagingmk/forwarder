var enet = require('enet');

var s_addr = new enet.Address("127.0.0.1", 9998);

enet.createClient(function (err, client) {
	if (err) {
		console.log(err);
		return;
	}
	//client.enableCompression();
	client.on("destroy", function () {
		console.log("shutdown!");
	});

	connect();

	console.log("connecting...");

	function connect() {
		client.connect(s_addr, 1, 0, function (err, peer, data) {
			if (err) {
				console.log(err);
				if (err.message === "host-destroyed") process.exit();
				console.log("retrying...");
				setTimeout(connect, 1000);
				return;
			}

			console.log("connected to:", peer.address());

			peer.on("message", function (packet, chan) {
				console.log("got message:", packet.data().toString());
			});

			peer.on("disconnect", function () {
				console.log("disconnected, sending final packet");
				peer.send(0, "final packet", function (err) {
					console.log(err || "final packet sent!");
				});

				console.log("shutting down");
				setTimeout(function () {
					client.destroy();
				});
			});
			//small-endian
			const arr = new Uint8Array(8);
			/*
			uint8_t version;
			uint8_t length;
			uint8_t protocol;
			uint8_t hash;
			uint8_t subID;
			uint8_t hostID;
			uint16_t clientID;
			*/
			arr[0] = 1; //version
			arr[1] = 8; //length
			arr[2] = 1; //protocol
			arr[3] = 255; //hash
			arr[4] = 1; //subID
			arr[5] = 255; //hostID
			arr[6] = 10; //clientID
			arr[7] = 5; //clientID 
			//client ID = 

			// Shares memory with `arr`
			const headerBuf = new Buffer(arr.buffer);
			const contentBuf = new Buffer("Hello\n")
			const buf = Buffer.concat([headerBuf, contentBuf], headerBuf.length + contentBuf.length);
			var packet1 = new enet.Packet(buf, enet.PACKET_FLAG.RELIABLE);
			console.log("sending packet 1...");
			peer.send(0, packet1, function (err) {
				if (err) {
					console.log("error sending packet 1:", err);
				} else {
					console.log("packet 1 sent.");
				}
			});
			/*
			var packet2 = new enet.Packet(new Buffer("test unreliable packet\n"), enet.PACKET_FLAG.UNRELIABLE);
			console.log("sending packet 2...");
			peer.send(0, packet2, function (err) {
				if (err) {
					console.log("error sending packet 2:", err);
				} else {
					console.log("packet 2 sent.");
				}
			});
			*/

			peer.disconnectLater();

			var packet3 = new enet.Packet(new Buffer("test after disconnect\n"), enet.PACKET_FLAG.RELIABLE);
			console.log("sending packet 3...");
			peer.send(0, packet3, function (err) {
				if (err) {
					console.log("error sending packet 3:", err);
				} else {
					console.log("packet 3 sent.");
				}
			});
		});
	}
});