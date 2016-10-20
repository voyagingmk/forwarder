var enet = require('enet');

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
		});
	}
});