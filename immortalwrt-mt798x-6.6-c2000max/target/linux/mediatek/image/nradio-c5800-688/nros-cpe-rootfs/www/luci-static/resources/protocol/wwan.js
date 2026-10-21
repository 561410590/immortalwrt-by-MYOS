'use strict';
'require form';
'require network';

return network.registerProtocol('wwan', {
	getI18n: function() {
		return _('NRadio WWAN');
	},

	getIfname: function() {
		return this.get('ifname') || this.get('device') || this._ubus('l3_device') || this._ubus('device') || this.sid;
	},

	getPackageName: function() {
		return 'proto-wwan';
	},

	isFloating: function() {
		return false;
	},

	isVirtual: function() {
		return false;
	},

	renderFormOptions: function(s) {
		var o;

		o = s.taboption('general', form.Value, 'device', _('Device'));
		o.rmempty = false;

		o = s.taboption('general', form.Value, 'ifname', _('Interface name'));
		o.rmempty = true;

		o = s.taboption('general', form.Value, 'phyifname', _('Physical interface'));
		o.rmempty = true;

		o = s.taboption('general', form.Flag, 'auto', _('Bring up on boot'));
		o.default = o.enabled;

		o = s.taboption('advanced', form.Flag, 'defaultroute', _('Use default gateway'));
		o.default = o.enabled;

		o = s.taboption('advanced', form.Value, 'metric', _('Use gateway metric'));
		o.datatype = 'uinteger';
		o.placeholder = '0';
	}
});
