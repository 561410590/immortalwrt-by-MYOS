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
