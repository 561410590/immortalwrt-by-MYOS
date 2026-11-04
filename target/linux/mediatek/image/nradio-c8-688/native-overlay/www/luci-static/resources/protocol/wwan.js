'use strict';
'require network';

// The MT5700 uses Ethernet E5/CID8; USB provides AT control only.
return network.registerProtocol('wwan', {
	getI18n: function() { return _('MT5700 cellular (2.5G Ethernet PHY)'); },
	getIfname: function() { return this._ubus('l3_device') || this.get('device') || 'eth1'; },
	isFloating: function() { return true; },
	getDevices: function() { return null; },
	containsDevice: function(ifname) { return network.getIfnameOf(ifname) == this.getIfname(); },
	_lease: function(suffix, proto) {
		return network.instantiateNetwork(this.sid + suffix, proto);
	},
	getIPAddrs: function() { return this._lease('_4', 'dhcp').getIPAddrs(); },
	getIP6Addrs: function() { return this._lease('_6', 'dhcpv6').getIP6Addrs(); },
	getIP6Prefixes: function() { return this._lease('_6', 'dhcpv6').getIP6Prefixes(); },
	getGatewayAddr: function() { return this._lease('_4', 'dhcp').getGatewayAddr(); },
	getDNSAddrs: function() { return this._lease('_4', 'dhcp').getDNSAddrs(); },
	renderFormOptions: function() {}
});
