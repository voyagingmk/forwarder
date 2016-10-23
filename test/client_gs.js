var enet = require('enet');
var forwarder = require('./forwarder');

var s_addr = new enet.Address("127.0.0.1", 9999);

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
				console.log("got message, len=", packet.dataLength());
				const result = forwarder.unmakePacket({
					data: packet.data()
				});
				console.log("content", result.content);

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

			var packet = forwarder.makePacket({
				protocol: 1,
				subID: 1,
				content: ""
			});
			console.log("sending packet 1...");
			peer.send(0, packet, function (err) {
				if (err) {
					console.log("error sending packet 1:", err);
				} else {
					console.log("packet 1 sent.");
				}
			});
		});
	}
});