/* TorcSubscription
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

var TorcSubscription = function (socket, serviceName, servicePath, subscribedChanged) {
    "use strict";

    var version, methods, properties, listeners = [];

    // public function to request updates for specific properties. By default, TorcSubscription
    // will be receiving updates but will only pass on changes when requested.
    this.listen = function (serviceProperty, propertyChangedCallback) {
        listeners[serviceProperty] = propertyChangedCallback;
    };

    // callback for changes in property values. Properties are sent as an object with one property - 'value'
    function propertyChanged(name, params) {
        if (listeners[name] && typeof listeners[name] === 'function' &&
            typeof params === 'object' && params.hasOwnProperty('value')) {
            listeners[name](name, params.value);
        }
    }

    // callback to notify successful subscription
    function subscribed(data) {
        // pre-validate the subscription return for the subscriber
        if (typeof data === 'object' && data.hasOwnProperty('methods') &&
            data.hasOwnProperty('version') && data.hasOwnProperty('properties') &&
            typeof data.properties === 'object') {
            version = data.version;
            methods = data.methods;
            properties = data.properties;

            // listen for changes to requested properties
            Object.getOwnPropertyNames(properties).forEach(function (element) {
                socket.listen(element, servicePath + properties[element].notification, propertyChanged);
            });
        } else {
            version = undefined;
            methods = undefined;
            properties = undefined;
            console.log('Invalid subscription response');
        }

        if (typeof subscribedChanged === 'function') { subscribedChanged(serviceName, version, methods, properties); }
    }

    // callback to notify subscription failed
    function failed() {
        version = undefined;
        methods = undefined;
        properties = undefined;
        console.log('Failed to subscribe to ' + servicePath);
        if (typeof subscribedChanged === 'function') { subscribedChanged(serviceName, undefined); }
    }

    // subscribe
    socket.call(servicePath + 'Subscribe', null, subscribed, failed);
};
