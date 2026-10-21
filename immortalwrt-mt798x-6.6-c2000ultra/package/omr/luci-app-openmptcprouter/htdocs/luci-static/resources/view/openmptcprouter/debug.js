'use strict';
'require view';
'require form';
'require fs';

return view.extend({
	load: function() {
		return fs.exec('/bin/sh', ['/bin/anonymous_config.sh'])
			.then(function(res) { return res.stdout || ''; })
			.catch(function()   { return ''; });
	},

	render: function(output) {
		var m, s, o;

		m = new form.Map('openmptcprouter', _('All router settings'));

		s = m.section(form.NamedSection, 'settings', 'settings');
		s.addremove = false;

		o = s.option(form.TextValue, '_output');
		o.readonly = true;
		o.rows     = 50;
		o.wrap     = 'off';
		o.cfgvalue = function() { return output; };

		return m.render();
	},

	handleSaveApply: null,
	handleSave:      null,
	handleReset:     null
});
