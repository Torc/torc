$(document).ready(function() {
    "use strict";

    function addNavbarDropdown(dropdownClass, toggleClass, listClass) {
        $('<li/>', { class: 'dropdown ' + dropdownClass})
          .append($('<a/>', {
            class: 'dropdown-toggle',
            href:  '#'})
            .attr('data-toggle', 'dropdown')
                  .append($('<span/>', { class: 'glyphicon ' + toggleClass})))
          .append($('<ul/>', {
            class: 'dropdown-menu ' + listClass,
            role: 'menu'
          }))
          .appendTo('.navbar-nav');
    }

    function removeNavbarDropdown(dropdownClass) {
        $('.' + dropdownClass).remove();
    }

    function addDropdownMenuDivider(menu, identifier) {
        $('.' + menu).append($('<li/>', { class: 'divider ' + identifier} ));
    }

    function addDropdownMenuItem(menu, identifier, link, text) {
        $('<li/>', { class: identifier }).append($('<a/>', { href: link, html: text } )).appendTo($('.' + menu));
    }

    function peerListChanged(name, value) {
        if (name === 'peers') {
            var item;

            // remove old peers
            $(".torc-peer").remove();

            // and add new
            if ($.isArray(value) && value.length) {
                value.forEach( function (element) {
                    item = $('<li/>', {
                        html: $('<a/>', { href: 'http://' + element.address + ':' + element.port + '/html/index.html', html: torc.ConnectTo + element.name }),
                        class: "torc-peer"
                    });

                    $(".torc-peer-menu").append(item);
                });
            }
        }
    }

    function peerSubscriptionChanged(version, methods, properties) {
        if (version !== undefined && typeof properties === 'object' && properties.hasOwnProperty('peers') &&
            properties.peers.hasOwnProperty('value') && $.isArray(properties.peers.value) && properties.peers.value.length) {
            addNavbarDropdown('torc-peer-dropdown', 'glyphicon-link', 'torc-peer-menu');
            peerListChanged('peers', properties.peers.value);
            return;
        }

        // this is either a subscription failure or the socket was closed/disconnected
        removeNavbarDropdown('torc-peer-dropdown');
    }

    function powerChanged(name, value) {
        var li, translatedName, translatedConfirmation, method;

        if (name === 'batteryLevel') {
            if (value === undefined) {
                translatedName = '';
            } else if (value === torc.ACPower) {
                translatedName = torc.ACPowerTr;
            } else if (value === torc.UnknownPower) {
                translatedName = torc.UnknownPowerTr;
            } else {
                translatedName = value + '%';
            }

            $('.torc-power-status a').text(translatedName);
            return;
        } else if (name === 'canSuspend') {
            translatedName = torc.Suspend;
            translatedConfirmation = torc.ConfirmSuspend;
            method = 'Suspend';
        } else if (name === 'canShutdown') {
            translatedName = torc.Shutdown;
            translatedConfirmation = torc.ConfirmShutdown;
            method = 'Shutdown';
        } else if (name === 'canHibernate') {
            translatedName = torc.Hibernate;
            translatedConfirmation = torc.ConfirmHibernate;
            method = 'Hibernate';
        } else if (name === 'canRestart') {
            translatedName = torc.Restart;
            translatedConfirmation = torc.ConfirmRestart;
            method = 'Restart';
        } else { return; }

        if (value === false || value === undefined) {
            $('.torc-' + name).remove();
        } else {
            li = $('<li/>', { class: 'torc-' + name,
                              html: '<a>' + translatedName + '...</span></a>'})
            .click(function() {
                bootbox.confirm(translatedConfirmation, function(result) {
                    if (result === true) {
                        torcconnection.call('power', method);
                    }
                })});
            $(".torc-power-menu").append(li);
        }
    }

    function powerSubscriptionChanged(version, methods, properties) {
        if (version !== undefined && typeof properties === 'object') {
            addNavbarDropdown('torc-power-dropdown', 'glyphicon-off', 'torc-power-menu');
            addDropdownMenuItem('torc-power-menu', 'torc-power-status', '#', '');
            addDropdownMenuDivider('torc-power-menu', '');
            powerChanged('canSuspend', properties.canSuspend.value);
            powerChanged('canShutdown', properties.canShutdown.value);
            powerChanged('canHibernate', properties.canHibernate.value);
            powerChanged('canRestart', properties.canRestart.value);
            powerChanged('batteryLevel', properties.batteryLevel.value);
            return;
        }

        removeNavbarDropdown('torc-power-dropdown')
    }

    function statusChanged (status) {
        if (status === torc.SocketNotConnected) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-ok glyphicon-ok-circle glyphicon-question-sign").addClass("glyphicon-exclamation-sign")
            $(".torc-socket-status-text a").text(torc.SocketNotConnected);
        } else if (status === torc.SocketConnecting) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-ok glyphicon-ok-circle glyphicon-exclamation-sign").addClass("glyphicon-question-sign")
            $(".torc-socket-status-text a").text(torc.SocketConnecting);
        } else if (status === torc.SocketConnected) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-exclamation-sign glyphicon-ok-circle glyphicon-question-sign").addClass("glyphicon-ok")
            $(".torc-socket-status-text a").text(torc.ConnectedTo + window.location.host);
        } else if (status === torc.SocketReady) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-ok glyphicon-exclamation-sign glyphicon-question-sign").addClass("glyphicon-ok-circle")
            torcconnection.subscribe('peers', ['peers'], peerListChanged, peerSubscriptionChanged);
            torcconnection.subscribe('power', ['canShutdown', 'canSuspend', 'canRestart', 'canHibernate', 'batteryLevel'], powerChanged, powerSubscriptionChanged);
        }
    }

    // set 'brand'
    $(".navbar-brand").text(torc.ServerApplication);

    // add a socket status/connection dropdown with icon
    addNavbarDropdown('', 'torc-socket-status-glyph', 'torc-socket-menu');

    // add the connection text item
    addDropdownMenuItem('torc-socket-menu', 'torc-socket-status-text', '#', '');

    // connect
    statusChanged(torc.SocketNotConnected);
    var torcconnection = new TorcConnection($, torc, statusChanged);
});
