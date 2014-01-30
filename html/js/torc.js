$(document).ready(function() {
    "use strict";

    function qsTranslate(context, string, disambiguation, plural, callback) {
        if (disambiguation === undefined) { disambiguation = ''; }
        if (plural === undefined) { plural = 0; }
        torcconnection.call('languages', 'GetTranslation',
                            { Context: context, String: string, Disambiguation: disambiguation, Number: plural},
                            function (translated) { if (typeof callback === 'function') { callback(translated); }},
                            function () { if (typeof callback === 'function') { callback(); }});
    }

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
            $(".torc-peer-status").remove();

            // and add new
            if ($.isArray(value) && value.length) {
                addDropdownMenuItem('torc-peer-menu', 'torc-peer-status', '#', '');
                qsTranslate('TorcNetworkedContext', '%n other Torc device(s) discovered', '', value.length,
                            function(result) { $(".torc-peer-status a").html(result); });
                addDropdownMenuDivider('torc-peer-menu', 'torc-peer');
                value.forEach( function (element, index) {
                    addDropdownMenuItem('torc-peer-menu', 'torc-peer torc-peer' + index, 'http://' + element.address + ':' + element.port + '/html/index.html', '');
                    $(".torc-peer-menu").append(item);
                    qsTranslate('TorcNetworkedContext', 'Connect to %1', '', 0,
                                function(result) { $(".torc-peer" + index + " a").html('<span class=\'glyphicon glyphicon-share\'>&nbsp</span>' + result.replace("%1", element.name)); });
                });
            } else {
                addDropdownMenuItem('torc-peer-menu', 'torc-peer-status', '#', torc.NoPeers);
            }
        }
    }

    function peerSubscriptionChanged(version, methods, properties) {
        if (version !== undefined && typeof properties === 'object' && properties.hasOwnProperty('peers') &&
            properties.peers.hasOwnProperty('value') && $.isArray(properties.peers.value)) {
            addNavbarDropdown('torc-peer-dropdown', 'glyphicon-link', 'torc-peer-menu');
            peerListChanged('peers', properties.peers.value);
            return;
        }

        // this is either a subscription failure or the socket was closed/disconnected
        removeNavbarDropdown('torc-peer-dropdown');
    }

    function languageChanged(name, value) {
    }

    function languageSubscriptionChanged(version, methods, properties) {
        if (version !== undefined && typeof properties === 'object') {
            // this should happen when the socket status transitions to Connected but we don't have the
            // translation service at that point
            qsTranslate('TorcNetworkedContext', 'Connected to %1', '', 0,
                        function (result) { $(".torc-socket-status-text a").html(result.replace('%1', window.location.host)); });
        }
    }

    function powerChanged(name, value) {
        var li, translatedName, translatedConfirmation, method;

        if (name === 'batteryLevel') {
            if (value === undefined) {
                translatedName = '';
            } else if (value === torc.ACPower) {
                translatedName = '<span class=\'glyphicon glyphicon-flash\'>&nbsp</span>' + torc.ACPowerTr;
            } else if (value === torc.UnknownPower) {
                translatedName = torc.UnknownPowerTr;
            } else {
                translatedName = '<span class=\'glyphicon glyphicon-dashboard\'>&nbsp</span>' + value + '%';
                qsTranslate('TorcPower', 'Battery %n%', '', value,
                            function (result) { $('.torc-power-status a').html('<span class=\'glyphicon glyphicon-dashboard\'>&nbsp</span>' + result); });
            }

            $('.torc-power-status a').html(translatedName);
            return;
        } else if (name === 'canSuspend') {
            translatedName = '<span class=\'glyphicon glyphicon-remove\'>&nbsp</span>' + torc.Suspend;
            translatedConfirmation = torc.ConfirmSuspend;
            method = 'Suspend';
        } else if (name === 'canShutdown') {
            translatedName = '<span class=\'glyphicon glyphicon-remove-circle\'>&nbsp</span>' + torc.Shutdown;
            translatedConfirmation = torc.ConfirmShutdown;
            method = 'Shutdown';
        } else if (name === 'canHibernate') {
            translatedName = '<span class=\'glyphicon glyphicon-remove\'>&nbsp</span>' + torc.Hibernate;
            translatedConfirmation = torc.ConfirmHibernate;
            method = 'Hibernate';
        } else if (name === 'canRestart') {
            translatedName = '<span class=\'glyphicon glyphicon-refresh\'>&nbsp</span>' + torc.Restart;
            translatedConfirmation = torc.ConfirmRestart;
            method = 'Restart';
        } else { return; }

        if (value === false || value === undefined) {
            $('.torc-' + name).remove();
        } else {
            li = $('<li/>', { class: 'torc-' + name,
                              html: '<a>' + translatedName + '</a>'})
            .click(function() {
                bootbox.confirm(translatedConfirmation, function(result) {
                    if (result === true) {
                        torcconnection.call('power', method);
                    }
                }); });
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

        removeNavbarDropdown('torc-power-dropdown');
    }

    function statusChanged (status) {
        if (status === torc.SocketNotConnected) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-ok glyphicon-ok-circle glyphicon-question-sign").addClass("glyphicon-exclamation-sign");
            $(".torc-socket-status-text a").html(torc.SocketNotConnected);
        } else if (status === torc.SocketConnecting) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-ok glyphicon-ok-circle glyphicon-exclamation-sign").addClass("glyphicon-question-sign");
            $(".torc-socket-status-text a").html(torc.SocketConnecting);
        } else if (status === torc.SocketConnected) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-exclamation-sign glyphicon-ok-circle glyphicon-question-sign").addClass("glyphicon-ok");
        } else if (status === torc.SocketReady) {
            $(".torc-socket-status-glyph").removeClass("glyphicon-ok glyphicon-exclamation-sign glyphicon-question-sign").addClass("glyphicon-ok-circle");
            torcconnection.subscribe('languages', ['languageString', 'languages'], languageChanged, languageSubscriptionChanged);
            torcconnection.subscribe('peers', ['peers'], peerListChanged, peerSubscriptionChanged);
            torcconnection.subscribe('power', ['canShutdown', 'canSuspend', 'canRestart', 'canHibernate', 'batteryLevel'], powerChanged, powerSubscriptionChanged);
        }
    }

    // set 'brand'
    $(".navbar-brand").html(torc.ServerApplication);

    // add a socket status/connection dropdown with icon
    addNavbarDropdown('', 'torc-socket-status-glyph', 'torc-socket-menu');

    // add the connection text item
    addDropdownMenuItem('torc-socket-menu', 'torc-socket-status-text', '#', '');

    // connect
    statusChanged(torc.SocketNotConnected);
    var torcconnection = new TorcConnection($, torc, statusChanged);
});
