// To deploy: C:\bin\putty\pscp.exe D:\csgo\trunk\src\engine\hltvbroadcastrelay.js sergiy@sdr-04.iad.valve.net:/home/sergiy/csgo_relay/hltvbroadcastrelay.js 
var http = require('http');
var accounts = {};
var url = require('url');
var fs = require("fs");
var os = require("os");
var adminCookie = Math.random();
var anonymousGetAllowance = 10; // this is how many GET requests are currently allowed  (goes up with every POST up to a point) without the Akamai x-origin-auth header
function zeroArray(len) {
    var arr = new Array(len);
    for (var i = 0; i < len; ++i)
        arr[i] = 0;
    return arr;
}
var stats = { post_field: 0, get_field: 0, get_start: 0, get_frag_meta: 0, sync: 0, not_found: 0, new_acct: 0, err: zeroArray(8), requests: 0, started: Date.now(), version: 5 };

"use strict";
var port = 8080;

function datasize( obj, field ) {
    var f = obj[field];
    if (f == null)
        return '';
    if (typeof f === 'string' || Buffer.isBuffer( f ) )
        return f.length;
    else if (f == null)
        return '';
    else return typeof f;
}

function listAllAccounts() {
    var uptime = ( Date.now() - stats.started ) / 1000;

    var html = '<p>Uptime ' + Math.floor((uptime / 60 / 60) % 60) + ':' + Math.floor((uptime / 60) % 60) + ':' + Math.floor(uptime % 60) + '</p>\n<!-- Listing accounts -->\n<p>Stats: ' + JSON.stringify(stats) + '</p>\n';
    html += '<form action="/save" method="post"><input type="submit" value="Save State To Disk"/></form>';
	for (var acct in accounts) {
	    var acc = accounts[acct];
	    html += '<p><a href="/' + acct + '">' + acct + '</a> -> ' + acc.length + ' frames, map ' + ( acc[0] == null ? "unknown" : acc[0].map ) + ' </p>\n';
	}
	return html;
}

function listSingleAccount( accId, acc )
{
    var title = '';
    if (acc[0]){
        title += '<p>map <b>' + acc[0].map;
        title += '</b> started ' + new Date(acc[0].timestamp).toUTCString();
        if (acc[0].signup_fragment)
            title += ', signup fragment ' + acc[0].signup_fragment;
        title += '</p><code>playcast "http://csgo-broadcast.akamai.steamstatic.com/' + accId + '"</code>';
        title += '<form action="/' + accId + '/delete" method="post"><input type="submit" value="Delete Broadcast"/></form>';
    }

    var html = '<p><i>' + title + '</i></p>';
        
    html += "<table><tr> <td>fragment</td> <td>ticks</td> <td>start</td> <td>full</td> <td>delta</td> <td>timestamp</td></tr>\n";
    var last_frag = 0;
    for (var fragment in acc) {
        if (last_frag + 1 < fragment)
            html += '<tr><td colspan="6"><i>' + (last_frag + 1) + ' ... ' + (fragment - 1) + 'missing</i></td></tr>\n';
        last_frag = parseInt(fragment);
		html += '<tr><td>' + fragment + '</td>';
		var f = acc[fragment];
		if (f) {
		    html += '<td>' + (f.tick ? f.tick : "") + ' - ' + (f.endtick ? f.endtick : "") + '</td>';
		    html += '<td>' + datasize(f, 'start');
		    if (acc[0] != null && acc[0].signup_fragment == fragment)
		        html += '<b>signup</b>';
		    html += '</td><td>' + datasize(f, 'full') + '</td>';
		    html += '<td>' + datasize(f, 'delta') + '</td>';
		    html += '<td>' + (f.timestamp ? new Date(f.timestamp).toUTCString() : '') + '</td>';
		    html += '<td><form action="/' + accId + '/' + fragment + '?delete=fragment" method="post"><input type="submit" value="Delete fragment"/></form></td></tr>\n'
		}
		else
		    html += '<td colspan="5">null</td>';
	}
	html += "</table>";
	return html;
}


function respondSimpleHtml( response, htmlBody )  {
	response.writeHead(200, {'Content-Type': 'text/html'});
	response.end('<html><body>' + htmlBody + '</body></html>');
}

function respondSimpleError(uri, response, code, explanation) {
	// if( uri ) console.log( uri + " => " + code + " " + explanation );
	response.writeHead(code, explanation);
	response.end();
}

function getAccountBufferSize( acc ){
	var totalSize = 0;
	for	(var fragment in acc )
	{
		for (var field in acc[fragment]) {
			var length = acc[fragment][field].length;
			if( length )
				totalSize += length;
		}
	}
	return totalSize;
}

function isSyncReady( f ) {
    return f != null && typeof( f ) == "object" && f.full != null && f.delta != null && f.tick != null && f.endtick != null;
}


function respondAccSync( param, uri, response, acc )
{
	var nowMs = Date.now();
	response.setHeader( 'Cache-Control', 'public, max-age=3');
	response.setHeader( 'Expires', new Date( nowMs + 3000 ).toUTCString() ); // whatever we find out, this information is going to be stale 3-5 seconds from now

	var acc0 = acc[0];
	if (acc0 != null && acc0.start != null)
	{	
		var fragment = param.query.fragment, frag = null;

		if( fragment == null )
		{
		    // skip the last 3-4 fragments, to let the front-running clients get 404, and akamai wait for 3+ seconds, and re-try that fragment again
		    // then go back another 3 fragments that are the buffer size for the client - we want to have the full 3 fragments ahead of whatever the user is streaming for the smooth experience
            // if we don't, then legit in-sync clients will often hit akamai-cached 404 on buffered fragments
		    fragment = acc.length - 8;
		    if (fragment >= 0 && fragment >= acc0.signup_fragment) {
                // can't serve anything before the start fragment
		        var f = acc[fragment];
		        if ( isSyncReady(f) )
		            frag = f;
		    }
		} else {
		    if (fragment < acc0.signup_fragment)
		        fragment = acc0.signup_fragment;
		    for (; fragment < acc.length; fragment++) {
		        var f = acc[fragment];
		        if (isSyncReady(f)) {
		            frag = f;
		            break;
		        }
		    }
		}

        if( frag )
        {
            console.log("Sync fragment " + fragment);
			// found the fragment that we want to send out
			response.writeHead(200, { "Content-Type": "application/json" });
			if (acc0.protocol == null)
			    acc0.protocol = 4;
			response.end(JSON.stringify({
				tick:frag.tick,
				endtick: frag.endtick,
				rtdelay: ( nowMs - frag.timestamp ) / 1000, // delay of this fragment from real-time, in seconds
                rcvage: ( nowMs - acc[acc.length - 1].timestamp ) / 1000, // Receive age: how many seconds since relay last received data from game server
				fragment: fragment,
				signup_fragment: acc0.signup_fragment,
				tps: acc0.tps,
                keyframe_interval: acc0.keyframe_interval,
				map: acc0.map,
                protocol: acc0.protocol
			}));
			return; // success!
		}
		// not found
		response.writeHead(405, "Fragment not found, please check back soon");
	}
	else
		response.writeHead(404, "Broadcast has not started yet");
	response.end();
}

function postField( request, param, response, acc, fragment, field )
{
	// decide on what exactly the response code is - we have enough info now
	if (field == "start") {
		console.log("Start tick " + param.query.tick + " in fragment " + fragment);
		response.writeHead(200);
		if (acc[0] == null)
		    acc[0] = {};
		if (acc[0].signup_fragment > fragment)
		    console.log("UNEXPECTED new start fragment " + fragment + " after " + acc[0].signup_fragment);
		acc[0].signup_fragment = fragment;
		fragment = 0; // keep the start in the fragment 0
	} else {
		if (acc[0] == null) {
			console.log("205 - need start fragment");
			response.writeHead(205);
		} else {
			if (acc[0].start == null) {
				console.log("205 - need start data");
				response.writeHead(205);
			} else {
				response.writeHead(200);
			}
		}
		if (acc[fragment] == null) {
		    //console.log("Creating fragment " + fragment + " in account " + path[1]);
		    acc[fragment] = {};
		}
    }

	for (q in param.query) {
	    var v = param.query[q], n = parseInt( v );
	    acc[fragment][q] = ( v == n ? n : v );
	}
	
	var body = [];
	request.on('data', function (data) { body.push( data ); });
	request.on('end', function () {
		var totalBufer = Buffer.concat(body)
		acc[fragment][field] = totalBufer;
		acc[fragment].timestamp = Date.now();
		if( field == "start")
			console.log("Received [" + fragment + "]." + field + ", " + totalBufer.length + " bytes in " + body.length + " pieces");
		response.end();
	});
}


function serveBlob( request, response, blob ) {
    if (blob == null) {
        response.writeHead(404, "Field not found");
        response.end();
    } else {// we have data to serve
        if (Buffer.isBuffer(blob)) {
            response.writeHead(200, { 'Content-Type': 'application/octet-stream' });
            //console.log("Serving " + blob.length + " bytes: " + request.url);
            response.end(blob);
        } else {
            response.writeHead(404, "Unexpected field type " + typeof (blob)); // we only serve strings
            console.log("Unexpected Field type " + typeof (blob)); // we only serve strings
            response.end();
        }
    }
}

function getStart(request, response, acc, fragment, field) {
    if (acc[0] == null || acc[0].signup_fragment != fragment) {
        respondSimpleError(request.url, response, 404, "Invalid or expired start fragment, please re-sync");
    } else{
        // always take start data from the 0th fragment
        serveBlob(request, response, acc[0][field]);
    }
}


function getField(request, response, acc, fragment, field) {
    serveBlob( request, response, acc[fragment][field] );
}

function getFragmentMetadata(response, acc, fragment)
{
	var res = {};
	for( var field in acc[ fragment ] )
	{
		var f = acc[fragment][field];
		if( typeof( f ) == 'number' ) res[ field] = f;
		else if( Buffer.isBuffer( f ) ) res[ field ] = f.length;
	}
	response.writeHead( 200, {"Content-Type": "application/json"});
	response.end(JSON.stringify(res));
}


function getSaveStateFileName() {
    if( os.type() == 'Linux' )
        return '/var/log/csgo-relay-save/' + Date.now() + '.json';
    else
        return 'hltvbroadcastrelay_server_state.json';
}

function getLoadStateFileName() {
    if (os.type() == 'Linux')
        return '/var/log/csgo-relay-save/autoload.json';
    else
        return 'hltvbroadcastrelay_server_state.json';
}

fs.readFile(getLoadStateFileName(), "utf8", function (err, data) {
    if (err == null) {
        if (accounts.length > 0)
            console.log("Cannot replace accounts array because accounts has already been modified");
        else {
            accounts = JSON.parse(data);
            var accDigest = [];
            for (var a in accounts) {
                var acc = accounts[a];
                for (var f in acc) {
                    var fragment = acc[f];
                    for (var ff in fragment) {
                        var field = fragment[ff];
                        if (typeof (field) == 'object') {
                            if (Array.isArray(field)) {
                                fragment[ff] = new Buffer(field);
                            } else if (field.type == 'Buffer' && typeof (field.data) == 'object') {
                                fragment[ff] = new Buffer(field.data);
                            } else console.log("Cannot recover fragment field " + f + "/" + ff + ": it's an object, and I can't guess how to restore it to Buffer object");
                        }
                    }
                }
                accDigest.push(a + '[' + acc.length + ']');
            }
            console.log("Restored accounts: " + accDigest.join(','));
        }
    }
});



function checkOriginAuth( originAuth, expected )
{
    return typeof (originAuth) == 'string' && (originAuth == expected || originAuth.indexOf(expected) >= 0);
}

function processRequestUnprotected(request, response) {
    // https://nodejs.org/api/http.html#http_class_http_incomingmessage
    var uri, isAdmin = false, isAkamai = false;
    if (request.url == '/idebug?admin') {
        adminCookie = Math.floor( Math.random() * 1e9 );
        console.log("Authenticating new admin: " + adminCookie );
        isAdmin = true;
        uri = '/';
    }
    else {
        uri = decodeURI(request.url);
        var cookies = request.headers.cookie;
        if (cookies)
            cookies.split(';').forEach(function (cookie) {
                var parts = cookie.split('=');
                if (parts[0].trim() == 'admin') {
                    if (parts[1].trim() == adminCookie)
                        isAdmin = true;
                    else
                        console.log('Admin cookie out of date: ' + adminCookie);
                }
            });
    }

    if (isAdmin) {
        console.log('Admin ' + request.method + uri);
        response.setHeader('Set-Cookie', 'admin=' + adminCookie);
    }
    
    var param = url.parse(uri, true);
    var path = param.pathname.split("/");
    path.shift(); // the first element is always empty, because the path starts with /
    response.httpVersion = '1.0';
    
    var prime = path.shift();

	if( prime == null || prime == '' || prime == 'index.html') {
	    if( isAdmin )
	        respondSimpleHtml( response, listAllAccounts());
	    else
	        respondSimpleError(uri, response, 401, 'Unauthorized');
		return;
	}

	var isPost, originAuth = request.headers['x-origin-auth'];
	if (request.method == 'POST') {
	    isPost = true;
	    if (isAdmin) {

	        if (prime == 'save' ) {
	            var str = JSON.stringify(accounts);
	            var fileName = getSaveStateFileName();
	            var printout = 'Saving server state into ' + fileName + ', ' + str.length + ' bytes';
	            console.log(printout);
	            fs.writeFile( fileName, str, "utf8", function (err) {
	                if (err) console.log(err); else console.log("Current server state saved");
	            });
	            respondSimpleHtml(response, printout);
	            return;
	        }

	    } else {
	        if (checkOriginAuth(originAuth, "hMugYm7Lv4o5")) {
	            if (anonymousGetAllowance < 20)
	                anonymousGetAllowance += 2;
	        } else {
	            console.log("Unauthorized POST to " + request.url + ", origin auth " + originAuth);
	            respondSimpleError(uri, response, 403, "Not Authorized");
	            return;
	        }
	    }
	}
	else if (request.method == 'GET') {
	    if ( checkOriginAuth( originAuth, 'c93a737c6aeab43b7f4ce18394f9374332c8f935' ) )
	        isAkamai = true;
	    else if (!isAdmin) {
	        if (anonymousGetAllowance <= 0) {
	            respondSimpleError(uri, response, 403, "Not Authorized");
	            return;
	        } else {
	            anonymousGetAllowance--;
	            console.log('Non-Akamai GET (' + originAuth + ') allowed (' + anonymousGetAllowance + ') ' + uri);
	        }
	    }

	    isPost = false;
	} else {
	    respondSimpleError(uri, response, 404, "Only POST or GET in this API");
	    return;
	}

	var acc = accounts[ prime ];
	if( acc == null )
	{
		// the account does not exist
		if( isPost )
		{
			// it's ok, we'll create a new account
			console.log("Creating account '" + prime + "'");
			accounts[prime] = acc = [];
			stats.new_acct++;
		}else{
			// GET requests don't create new accounts; but we can add more routing here later
		    respondSimpleError(uri, response, 404, "Account " + prime + " not found"); // invalid account
		    stats.err[0]++;
			return;
		}
	}

    var subAcc = path.shift();
	if( subAcc == null || subAcc == '' )
	{
	    if (isPost) {
	        respondSimpleError(uri, response, 405, "Invalid POST: no fragment or field");
	        stats.err[1]++;
	    }
	    else if (isAdmin)
	        respondSimpleHtml(response, listSingleAccount(prime, acc));
	    else
	        respondSimpleError(uri, response, 401, "Unauthorized");
		return;
	}
	
	stats.requests++;
	
    var fragment = parseInt(subAcc);
    if (fragment != subAcc ) {
		if( subAcc == "sync" ) {
		    respondAccSync(param, uri, response, acc);
		    stats.sync++;
        }
		else if( subAcc == "size" )
		    respondSimpleHtml(response, "Account " + prime + " all buffers use: <b>" + getAccountBufferSize(acc) + "</b> bytes");
		else if (subAcc == "delete") {
		    if (isPost) {
		        delete accounts[prime];
		        respondSimpleHtml(response, "<p><emp>Account " + prime + " and all its buffers are deleted</emp><p>" + listAllAccounts());
		    } else respondSimpleError(uri, response, 405, "Fragment delete must be a POST request");
		}
		else {
		    respondSimpleError(uri, response, 405, "Fragment is not an int or pseudo-fragment name (sync, size)");
		    stats.err[2]++;
		}
        return;
    }
	
	var field = path.shift();
	if( isPost )
	{
	    stats.post_field++;
	    if (field != null) {
	        postField(request, param, response, acc, fragment, field);
	        var fragRemove = fragment - 1200;
	        //if (fragRemove > 100 && acc[fragRemove] != null) { // keep the first 100 fragments for reference/debugging; keep the last 1200 (1 hour worth of data, ~200Mb at most) because we have enough memory for that. In actuality we only need ~15-20 last fragments and fragment [0]
	        //    delete acc[fragRemove]; // free the memory
	        //}
	    }
	    else if (param.query.delete == "fragment" && isAdmin) {
	        acc[fragment] = null
	        respondSimpleHtml(response, "<p>Fragment " + fragment + " is deleted</p>" + listSingleAccount(prime, acc));
	    }
	    else {
	        respondSimpleError(uri, response, 405, "Cannot post fragment without field name");
	        stats.err[3]++;
	    }
	} else {
	    if (field == 'start') {
	        getStart(request, response, acc, fragment, field);
	        stats.get_start++;
	    } else if( acc[ fragment ] == null ) {
		    stats.err[4]++;
			response.writeHead(404, "Fragment " + fragment + " not found");
			response.end();
		} else if (field == null || field == '') {
		    getFragmentMetadata(response, acc, fragment);
		    stats.get_frag_meta++;
		} else {
		    getField(request, response, acc, fragment, field);
		    stats.get_field++;
		}
	}
}


function processRequest(request, response)
{
    try{
        processRequestUnprotected(request, response);
    } catch (err) {
        console.log(( new Date ).toUTCString() + " Exception when processing request " + request.url);
        console.log(err);
        console.log(err.stack);
    }
}

var newServer = http.createServer(processRequest).listen(port);
if( newServer)
    console.log(( new Date() ).toUTCString() + " Started in " + os.type() + " at " + __dirname + " on port " + port);
else
	console.log(( new Date() ).toUTCString() + " Failed to start on port " + port);
