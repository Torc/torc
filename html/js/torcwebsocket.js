/* TorcWebSocket
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

var TorcWebsocket = function ($, torc, socketStatusChanged) {
    "use strict";

    // initial socket state
    if (typeof socketStatusChanged === 'function') { socketStatusChanged(torc.SocketConnecting); }

    // current JSON-RPC Id
    var currentRpcId = 1,
    // current calls
    currentCalls = [],
    // event handlers for notifications
    eventHandlers = [],
    // interval timer to check for call expiry/failure
    expireTimer,
    // socket
    socket;

    // clear out any old remote calls (over 60 seconds) for which we did not receive a response
    function expireCalls() {
        var i, callback,
        timenow = new Date().getTime();

        for (i = currentCalls.length - 1; i >= 0; i -= 1) {
            if ((currentCalls[i].sent + 60000) < timenow) {
                console.log('Expiring call id ' + currentCalls[i].id + ' : no response received');
                callback = currentCalls[i].failure;
                currentCalls.splice(i, 1);
                if (typeof callback === 'function') { callback(); }
            }
        }

        if (currentCalls.length < 1) {
            clearInterval(expireTimer);
            expireTimer = undefined;
        }
    }

    // add event callback
    this.listen = function (id, method, callback) {
        eventHandlers[method] = { id: id, callback: callback };
    };

    // make a remote call (public)
    this.call = function (methodToCall, params, successCallback, failureCallback) {
        // create the base call object
        var invocation = {
            jsonrpc: '2.0',
            method: methodToCall
        };

        // add params if supplied
        if (typeof params === 'object' && params !== null) {
            invocation.params = params;
        }

        // no callback indicates a notification
        if (typeof successCallback === 'function') {
            invocation.id = currentRpcId;
            currentCalls.push({ id: currentRpcId, success: successCallback, failure: failureCallback, sent: new Date().getTime() });
            currentRpcId += 1;

            // wrap the id when it gets large...
            if (currentRpcId > 0x2000000) {
                currentRpcId = 1;
            }

            // start the expire timer if not already running
            if (expireTimer === undefined) {
                expireTimer = setInterval(expireCalls, 10000);
            }
        }

        socket.send(JSON.stringify(invocation));
    };

    // handle individual responses
    function processResult(data) {
        var i, id, callback, method;

        // we only understand JSON-RPC 2.0
        if (!(data.hasOwnProperty('jsonrpc') && data.jsonrpc === '2.0')) {
            id = data.hasOwnProperty('id') ? data.id : null;
            return {jsonrpc: '2.0', error: {code: '-32600', message: 'Invalid request'}, id: id};
        }

        if (data.hasOwnProperty('result') && data.hasOwnProperty('id')) {
            // this is the result of a successful call
            id = parseInt(data.id, 10);

            for (i = 0; i < currentCalls.length; i += 1) {
                if (currentCalls[i].id === id) {
                    callback = currentCalls[i].success;
                    currentCalls.splice(i, 1);
                    if (typeof callback === 'function') {
                        callback(data.result);
                    }
                    return;
                }
            }

            console.log('Unknown RPC result for id:' + id);
        } else if (data.hasOwnProperty('method')) {
            // there is no support for calls to the browser...
            if (data.hasOwnProperty('id')) {
                return {jsonrpc: '2.0', error: {code: '-32601', message: 'Method not found'}, id: data.id};
            }

            // notification
            method = data.method;

            if (eventHandlers[method]) {
                eventHandlers[method].callback(eventHandlers[method].id, data.params);
            } else {
                console.log('No event handler for ' + method);
            }
        } else if (data.hasOwnProperty('error')) {
            // error...

            if (data.hasOwnProperty('id')) {
                // call failed (but valid JSON)
                id = parseInt(data.id, 10);
                for (i = 0; i < currentCalls.length; i += 1) {
                    if (currentCalls[i].id === id) {
                        callback = currentCalls[i].failure;
                        currentCalls.splice(i, 1);
                        if (typeof callback === 'function') { callback(); }
                    }
                }
            }

            console.log('JSON-RPC error: ' + (data.error.hasOwnProperty('code') ? data.error.code : null) +
                        '(' + (data.error.hasOwnProperty('code') ? data.error.message : 'unknown') + ')');
        }
    }

    function connect(token) {
        var url = 'ws://' + torc.ServerAuthority + '?accesstoken=' + token;
        socket = (typeof MozWebSocket === 'function') ? new MozWebSocket(url, 'torc.json-rpc') : new WebSocket(url, 'torc.json-rpc');

        // socket error
        socket.onerror = function (event) {
            // this never seems to contain any pertinent information
            console.log('Websocket error (' + event.toString() + ')');
        };

        // socket closed
        socket.onclose = function () {
            if (typeof socketStatusChanged === 'function') { socketStatusChanged(torc.SocketNotConnected); }
        };

        // socket opened
        socket.onopen = function () {
            // connected
            if (typeof socketStatusChanged === 'function') { socketStatusChanged(torc.SocketConnected); }
        };

        // socket message
        socket.onmessage = function (event) {
            var i, result, batchresult,

            // parse the JSON result
            data = $.parseJSON(event.data);

            if ($.isArray(data)) {
                // array of objects (batch)
                batchresult = [];

                for (i = 0; i < data.length; i += 1) {
                    result = processResult(data[i]);
                    if (typeof result === 'object') { batchresult.push(result); }
                }

                // a batch call of notifications requires no response
                if (batchresult.length > 0) { socket.send(JSON.stringify(batchresult)); }
            } else if (typeof data === 'object') {
                // single object
                result = processResult(data);

                if (typeof result === 'object') { socket.send(JSON.stringify(result)); }
            } else {
                socket.send(JSON.stringify({jsonrpc: '2.0', error: {code: '-32700', message: 'Parse error'}, id: null}));
            }
        };
    }

    // start the connection by requesting a token. If authentication is not required, it will be silently ignored.
    $.ajax({ url: 'http://' + torc.ServerAuthority + torc.ServicesPath + 'GetWebSocketToken',
             dataType: "json",
             xhrFields: { withCredentials: true }
           }).done(function(result) { connect(result.accesstoken); });

};
