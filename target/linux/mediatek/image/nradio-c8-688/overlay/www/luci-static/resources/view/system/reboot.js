'use strict';
'require view';
'require rpc';
'require ui';
'require uci';

var callReboot = rpc.declare({
	object: 'system',
	method: 'reboot',
	expect: { result: 0 }
});

return view.extend({
	load: function() {
		return uci.changes();
	},

	render: function(changes) {
		var body = E([
			E('h2', _('Reboot')),
			E('p', {}, _('Reboots the operating system of your device'))
		]);

		for (var config in (changes || {})) {
			body.appendChild(E('p', { 'class': 'alert-message warning' },
				_('Warning: There are unsaved changes that will get lost on reboot!')));
			break;
		}

		body.appendChild(E('hr'));
		body.appendChild(E('button', {
			'class': 'cbi-button cbi-button-action important',
			'click': ui.createHandlerFn(this, 'confirmReboot')
		}, _('Perform reboot')));

		return body;
	},

	confirmReboot: function() {
		ui.showModal(_('确认重启设备？'), [
			E('p', {}, _('重启期间网络会暂时断开。确定要立即重启吗？')),
			E('div', { 'class': 'right' }, [
				E('button', { 'class': 'btn', 'click': ui.hideModal }, _('取消')),
				' ',
				E('button', {
					'class': 'btn cbi-button-action important',
					'click': ui.createHandlerFn(this, 'handleReboot')
				}, _('确认重启'))
			])
		]);
	},

	handleReboot: function(ev) {
		return callReboot().then(function(res) {
			if (res != 0) {
				L.ui.addNotification(null, E('p', _('The reboot command failed with code %d').format(res)));
				L.raise('Error', 'Reboot failed');
			}

			L.ui.showModal(_('Rebooting…'), [
				E('p', { 'class': 'spinning' }, _('Waiting for device...'))
			]);

			window.setTimeout(function() {
				L.ui.showModal(_('Rebooting…'), [
					E('p', { 'class': 'spinning alert-message warning' },
						_('Device unreachable! Still waiting for device...'))
				]);
			}, 150000);

			L.ui.awaitReconnect();
		})
		.catch(function(e) { L.ui.addNotification(null, E('p', e.message)) });
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
