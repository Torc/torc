/* TorcConnection
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

var TorcConnection = function ($, torc, statusChanged) {
    "use strict";

    var socket,
    subscriptions = {},
    defaultServiceList = { services: { path: torc.ServicesPath } },
    serviceList = defaultServiceList,
    that = this;

    this.call = function(serviceName, method, params, success, failure) {
        if (socket !== undefined && subscriptions[serviceName] && subscriptions[serviceName].methods[method]) {
            socket.call(serviceName + method, params, success, failure);
            return;
        }

        console.log('Failed to call ' + serviceName + method);
    };

    this.subscribe = function (serviceName, properties, propertyChanges, subscriptionChanges) {
        // is this a known service
        if (!serviceList.hasOwnProperty(serviceName)) {
            console.log('Cannot subscribe to unknown service ' + serviceName);
            if (typeof subscriptionChanges === 'function') { subscriptionChanges(); }
            return;
        }

        // avoid double subscriptions
        if (subscriptions[serviceName]) {
            console.log('Already subscribed to ' + serviceName);
            if (typeof subscriptionChanges === 'function') { subscriptionChanges(); }
            return;
        }

        // actually subscribe
        subscriptions[serviceName] = {
            properties: properties,
            propertyChanges: propertyChanges,
            subscriptionChanges: subscriptionChanges,
            subscription: new TorcSubscription(socket, serviceName, serviceList[serviceName].path, subscriptionChanged)
        };
    };

    function subscriptionChanged(name, version, methods, properties) {
        // is this a known service subscription
        if (!subscriptions[name]) {
            console.log('Received subscription response for unknown service' + name);
            return;
        }

        // there was an error subscribing
        if (version === undefined) {
            console.log('Error subscribing to service' + name);
            if (typeof subscriptions[name].subscriptionChanges === 'function') { subscriptions[name].subscriptionChanges(); }
            return;
        }

        // loop through the requested properties and listen for updates
        if (typeof properties === 'object' && $.isArray(subscriptions[name].properties)) {
            subscriptions[name].properties.forEach(function(element) {
                if (properties.hasOwnProperty(element)) {
                    subscriptions[name].subscription.listen(element, subscriptions[name].propertyChanges);
                }});
        }

        // save the methods for validating calls
        subscriptions[name].methods = methods;

        // and notifiy subscriber
        if (typeof subscriptions[name].subscriptionChanges === 'function') {
            subscriptions[name].subscriptionChanges(version, methods, properties);
        }
    }

    function serviceListChanged(name, value) {
        // there is currently no service that starts or stops other than at startup/shutdown. So this list
        // shouldn't currently change and the subscriber list will be updated if a new socket is used.
        serviceList = value;
    }

    function serviceSubscriptionChanged(version, methods, properties) {
        if (version !== undefined && typeof properties === 'object' && properties.hasOwnProperty('serviceList') &&
            properties.serviceList.hasOwnProperty('value')) {
            serviceListChanged('serviceList', properties.serviceList.value);
            statusChanged(torc.SocketReady);
            return;
        }

        serviceListChanged('serviceList', []);
    }

    function connect() {
        socket = new TorcWebsocket($, torc, socketStatusChanged);
    }

    function connected() {
        that.subscribe('services', ['serviceList'], serviceListChanged, serviceSubscriptionChanged);
    }

    function disconnected() {
        // notify subscribers that they have been disconnected and delete subscription
        Object.getOwnPropertyNames(subscriptions).forEach(function (element) {
            if (typeof subscriptions[element].subscriptionChanges === 'function') { subscriptions[element].subscriptionChanges(); }
            delete subscriptions[element];
        });

        // clear state and schedule reconnect
        socket = undefined;
        serviceList = defaultServiceList;
        setTimeout(connect, torc.SocketReconnectAfterMs);
    }

    function socketStatusChanged (status) {
        // notify the ui
        statusChanged(status);

        if (status === torc.SocketNotConnected) {
            disconnected();
        } else if (status === torc.SocketConnected) {
            connected();
        }
    }

    connect();
};
