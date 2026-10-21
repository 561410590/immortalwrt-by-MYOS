'use strict';
'require baseclass';
'require firewall';
'require rpc';
'require uci';

var callInterfaceDump = rpc.declare({
	object: 'network.interface',
	method: 'dump',
	expect: { 'interface': [] }
});

var installed = false;
var pendingDump = null;

function interfaceDump() {
	if (!pendingDump) {
		pendingDump = callInterfaceDump().then(function(interfaces) {
			pendingDump = null;
			return interfaces;
		}, function() {
			pendingDump = null;
			return [];
		});
	}
	return pendingDump;
}

return baseclass.extend({
	install: function() {
		if (installed)
			return;
		installed = true;
		var getZoneByNetwork = firewall.getZoneByNetwork;

		firewall.getZoneByNetwork = function(name) {
			return getZoneByNetwork.call(this, name).then(function(zone) {
				// An explicit UCI assignment always takes precedence.
				if (zone || !/^cpe_[46]$/.test(name) ||
				    uci.get('network', 'globals', 'nbcpe') != '1' ||
				    uci.get('network', 'cpe', 'proto') != 'wwan' ||
				    uci.get('network', 'cpe', 'mode') != 'odu')
					return zone;

				return interfaceDump().then(function(interfaces) {
					var status = interfaces.filter(function(iface) {
						return iface.interface == name && iface.dynamic === true;
					})[0];
					var zoneName = status && status.data && status.data.zone;
					// Resolve only a real, existing runtime zone; never create one.
					return typeof(zoneName) == 'string' && zoneName.length
						? firewall.getZone(zoneName) : null;
				}).catch(function() { return null; });
			});
		};
	}
});
