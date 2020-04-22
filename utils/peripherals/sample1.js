var dgram = require("dgram");

(function() {
    var server = dgram.createSocket("udp4");

    server.on("error", function(err) {
        console.log("server error:\n" + err.stack);
    });

    server.on("message", function(msg, rinfo) {
        // eStats server
        server.send(msg, 0, msg.length, 33702, "176.31.211.227");
        // HLTV server
        server.send(msg, 0, msg.length, 30001, "web5.hltv.org");
        console.log(rinfo.address + ":" + rinfo.port, msg.toString());
    });

    server.on("listening", function() {
        var address = server.address();
        console.log("server listening " + address.address + ":" + address.port);
    });

    server.bind(10000);
})();

(function() {
    var server = dgram.createSocket("udp4");

    server.on("error", function(err) {
        console.log("server error:\n" + err.stack);
    });

    server.on("message", function(msg, rinfo) {
        // eStats server
        server.send(msg, 0, msg.length, 33702, "176.31.211.227");
        // HLTV server
        server.send(msg, 0, msg.length, 30001, "web5.hltv.org");
        console.log(rinfo.address + ":" + rinfo.port, msg.toString());
    });

    server.on("listening", function() {
        var address = server.address();
        console.log("server listening " + address.address + ":" + address.port);
    });

    server.bind(10001);
})();