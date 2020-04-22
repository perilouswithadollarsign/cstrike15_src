var dgram = require("dgram");

var server1 = dgram.createSocket("udp4");
server1.bind(10001);
var server2 = dgram.createSocket("udp4");
server2.bind(10002);
var server3 = dgram.createSocket("udp4");
server3.bind(10003);
var server4 = dgram.createSocket("udp4");
server4.bind(10004);
var server5 = dgram.createSocket("udp4");
server5.bind(10005);

var mapping = {
    28015: server1,
    28016: server2,
    28017: server3,
    28018: server4,
    28019: server5,
    25005: server1,
    25006: server2,
    25007: server3,
    25008: server4,
    25009: server5
};

var server = dgram.createSocket("udp4");

server.on("error", function(err) {
    console.log("server error:\n" + err.stack);
});

server.on("message", function(msg, rinfo) {
    if (rinfo.port in mapping) {
        // eStats server
        mapping[rinfo.port].send(msg, 0, msg.length, 33702, "176.31.211.227");
        // HLTV server
        mapping[rinfo.port].send(msg, 0, msg.length, 30001, "web5.hltv.org");
        console.log("Forwaded", rinfo.address + ":" + rinfo.port, msg.toString());
    } else {
        console.log("NOT FORWARDED, NOT IN MAPPING", rinfo.address + ":" + rinfo.port, msg.toString());
    }

});

server.on("listening", function() {
    var address = server.address();
    console.log("server listening " + address.address + ":" + address.port);
});

server.bind(10000);
