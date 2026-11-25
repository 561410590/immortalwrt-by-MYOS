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
			'click': ui.createHandlerFn(this, 'handleReboot')
		}, _('Perform reboot')));

		return body;
	},

	handleReboot: function(ev) {
		var waitTimer;
		var progress = function(message, delayed) {
			ui.showModal(_('Rebooting…'), [
				E('p', { 'role': 'status', 'class': delayed ? 'alert-message warning' : '' }, message),
				E('progress', { 'aria-label': _('Waiting for device...') })
			]);
		};
		progress(_('正在发送重启命令…'));
		return callReboot().then(function(res) {
			if (res != 0) {
				throw new Error(_('The reboot command failed with code %d').format(res));
			}

			progress(_('设备已接受重启命令，正在等待管理页面重新上线。'));

			waitTimer = window.setTimeout(function() {
				progress(_('Device unreachable! Still waiting for device...'), true);
			}, 150000);

			return ui.awaitReconnect();
		})
		.catch(function(e) {
			window.clearTimeout(waitTimer);
			ui.hideModal();
			ui.addNotification(null, E('p', e.message));
		});
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
