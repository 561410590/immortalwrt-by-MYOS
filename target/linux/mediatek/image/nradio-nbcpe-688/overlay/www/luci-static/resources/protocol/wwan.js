'use strict';
'require form';
'require network';
'require tools.nbcpe-network as nbcpe';
'require uci';

nbcpe.install();

return network.registerProtocol('wwan', {
	getI18n: function() {
		return _('Cellular (WWAN / ODU)');
	},

	isFloating: function() {
		return false;
	},

	isVirtual: function() {
		return false;
	},

	getErrors: function() {
		var errors = this.super('getErrors', arguments);
		if (this.sid != 'cpe' || uci.get('network', 'globals', 'nbcpe') != '1' || !this.isUp())
			return errors;
		var connected = ['_4', '_6'].some(function(suffix) {
			var child = network.instantiateNetwork(this.sid + suffix);
			return child.isUp() && ((child.getIPAddrs() || []).length || (child.getIP6Addrs() || []).length);
		}, this);
		if (!connected || !errors)
			return errors;
		// netifd retains startup diagnostics until a reconnect. Once a dynamic
		// child is up with an address, these two AT acknowledgements are stale.
		// Preserve the raw ubus/log records and every other interface error.
		var raw = (this._ubus('errors') || []).filter(function(error) {
			return L.isObject(error) && typeof(error.code) == 'string';
		});
		var current = errors.filter(function(error, index) {
			return !raw[index] || !['APN_SETUP_FAILED', 'CONNECT_FAILED'].includes(raw[index].code);
		});
		return current.length ? current : null;
	},

	renderFormOptions: function(section) {
		// The native WWAN/ODU stack owns dialing, SIM and APN configuration.
		// These display fields leave its existing UCI options intact.
		[
			['_nbcpe_odu_port', 'phyifname', _('PoE / ODU port')],
			['_nbcpe_odu_mode', 'mode', _('Operating mode')],
			['_nbcpe_odu_model', 'odu_model', _('ODU model')],
			['_nbcpe_odu_management', 'auto_sniffer', _('ODU management address')]
		].forEach(function(field) {
			var option = section.taboption('general', form.DummyValue, field[0], field[2]);
			option.cfgvalue = function(section_id) {
				return uci.get('network', section_id, field[1]) || '—';
			};
			option.write = option.remove = function() {};
		});
	}
});
