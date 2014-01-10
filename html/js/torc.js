$(document).ready(function() {
    "use strict";

    function peerListChanged(name, value) {
        if (name === 'peers') {
            var item;

            // remove old peers
            $(".torc-peer").remove();

            // and add new
            if ($.isArray(value) && value.length) {
                // and a link to each peer
                value.forEach( function (element) {
                    $(".torc-peer-menu").append($('<li/>', { class: "divider torc-peer"} ));
                    item = $('<li/>', {
                        html: $('<a/>', { href: 'http://' + element.address + ':' + element.port + '/html/index.html', html: element.name }),
                        class: "torc-peer"
                    });

                    $(".torc-peer-menu").append(item);
                });
            }
        }
    }

    function peerSubscriptionChanged(version, methods, properties) {
        if (version !== undefined && typeof properties === 'object' && properties.hasOwnProperty('peers') &&
            properties.peers.hasOwnProperty('value')) {
            peerListChanged('peers', properties.peers.value);
            return;
        }

        // this is either a subscription failure or the socket was closed/disconnected
        peerListChanged('peers', []);
    }

    function statusChanged (status) {
        $(".torc-socket-status").text(status);

        if (status === torc.SocketNotConnected) {
            $(".torc-socket-address").text('');
        } else if (status === torc.SocketConnected) {
            $(".torc-socket-address").text(torc.ConnectedTo + torc.ServerAuthority);
        } else if (status === torc.SocketReady) {
            torcconnection.subscribe('peers', ['peers'], peerListChanged, peerSubscriptionChanged);
        }
    }

    $(".torc-application-name").text(torc.ServerApplication);
    statusChanged(torc.SocketNotConnected);
    var torcconnection = new TorcConnection($, torc, statusChanged);
});
