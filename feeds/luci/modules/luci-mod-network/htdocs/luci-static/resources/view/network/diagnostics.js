'use strict';
'require view';
'require dom';
'require fs';
'require ui';
'require uci';
'require network';

return view.extend({
	handleCommand: function(exec, args) {
		var buttons = document.querySelectorAll('.diag-action > .cbi-button, .m-diagnostics-form button[type="submit"]');
		var output = document.getElementById('widget.command-output');

		for (var i = 0; i < buttons.length; i++)
			buttons[i].setAttribute('disabled', 'true');

		return fs.exec(exec, args).then(function(res) {
			if (document.body.classList.contains('m-submenu-compact'))
				output.value = (res.stdout || '') + (res.stderr || '');
			else
				dom.content(output, [ res.stdout || '', res.stderr || '' ]);
		}).catch(function(err) {
			ui.addNotification(null, E('p', [ err ]))
		}).finally(function() {
			for (var i = 0; i < buttons.length; i++)
				buttons[i].removeAttribute('disabled');
		});
	},

	handlePing: function(ev, cmd) {
		var exec = cmd || 'ping',
		    addr = ev.currentTarget.parentNode.previousSibling.value,
		    args = (exec == 'ping') ? [ '-4', '-c', '5', '-W', '1', addr ] : [ '-6', '-c', '5', addr ];

		return this.handleCommand(exec, args);
	},

	handleTraceroute: function(ev, cmd) {
		var exec = cmd || 'traceroute',
		    addr = ev.currentTarget.parentNode.previousSibling.value,
		    args = (exec == 'traceroute') ? [ '-4', '-q', '1', '-w', '1', '-n', '-m', String(L.env.rpctimeout || 20), addr ] : [ '-q', '1', '-w', '2', '-n', addr ];

		return this.handleCommand(exec, args);
	},

	handleNslookup: function(ev, cmd) {
		var addr = ev.currentTarget.parentNode.previousSibling.value;

		return this.handleCommand('nslookup', [ addr ]);
	},

	handleArpScan: function(ev, cmd) {
		var addr = ev.currentTarget.parentNode.previousSibling.value;

		return this.handleCommand('arp-scan', [ '-l', '-I', addr ]);
	},

	load: function() {
		return Promise.all([
			L.resolveDefault(fs.stat('/bin/ping6'), {}),
			L.resolveDefault(fs.stat('/usr/bin/ping6'), {}),
			L.resolveDefault(fs.stat('/bin/traceroute6'), {}),
			L.resolveDefault(fs.stat('/usr/bin/traceroute6'), {}),
			L.resolveDefault(fs.stat('/usr/bin/arp-scan'), {}),
			network.getDevices(),
			uci.load('luci')
		]);
	},

	renderCompact: function(res) {
		const defaults = {
			ping: uci.get('luci', 'diag', 'ping') || 'openwrt.org',
			traceroute: uci.get('luci', 'diag', 'route') || 'openwrt.org',
			nslookup: uci.get('luci', 'diag', 'dns') || 'openwrt.org'
		};
		const tools = [
			{ exec: 'ping', group: 'ping', label: 'IPv4 Ping', args: ['-4', '-c', '5', '-W', '1'] },
			...(res[0].path || res[1].path ? [{ exec: 'ping6', group: 'ping', label: 'IPv6 Ping', args: ['-6', '-c', '5'] }] : []),
			{ exec: 'traceroute', group: 'traceroute', label: 'IPv4 ' + _('Traceroute'), args: ['-4', '-q', '1', '-w', '1', '-n', '-m', String(L.env.rpctimeout || 20)] },
			...(res[2].path || res[3].path ? [{ exec: 'traceroute6', group: 'traceroute', label: 'IPv6 ' + _('Traceroute'), args: ['-q', '1', '-w', '2', '-n'] }] : []),
			{ exec: 'nslookup', group: 'nslookup', label: _('Nslookup'), args: [] }
		];
		const method = E('select', { id: 'm-diagnostics-method', class: 'cbi-input-select' },
			tools.map(tool => E('option', { value: tool.exec }, tool.label)));
		const target = E('input', { id: 'm-diagnostics-target', type: 'text', value: defaults.ping, class: 'cbi-input-text' });
		let previous = defaults.ping;
		method.addEventListener('change', () => {
			const suggested = defaults[tools[method.selectedIndex].group];
			if (target.value === previous)
				target.value = suggested;
			previous = suggested;
		});
		const form = E('form', { class: 'm-diagnostics-form' }, [
			E('label', { for: method.id }, [ '工具', method ]),
			E('label', { for: target.id, class: 'm-diagnostics-address' }, [ '目标地址', target ]),
			E('button', { type: 'submit', class: 'cbi-button cbi-button-action' }, '运行')
		]);
		form.addEventListener('submit', ev => {
			ev.preventDefault();
			const tool = tools[method.selectedIndex];
			this.handleCommand(tool.exec, [...tool.args, target.value]);
		});
		const forms = [form];
		if (res[4].path) {
			const devices = E('select', { class: 'cbi-input-select', id: 'm-diagnostics-interface' },
				res[5].filter(dev => dev.isUp()).map(dev => E('option', { value: dev.getName() }, dev.getI18n())));
			const arp = E('form', { class: 'm-diagnostics-form' }, [
				E('label', { for: devices.id }, [ _('Device'), devices ]),
				E('button', { type: 'submit', class: 'cbi-button cbi-button-action' }, _('Arp-scan'))
			]);
			arp.addEventListener('submit', ev => {
				ev.preventDefault();
				this.handleCommand('arp-scan', ['-l', '-I', devices.value]);
			});
			forms.push(arp);
		}
		return E('div', { class: 'cbi-map m-diagnostics-view' }, [
			E('h2', _('Diagnostics')),
			E('div', { class: 'cbi-section' }, forms),
			E('textarea', { id: 'widget.command-output', readonly: '', wrap: 'off', rows: 16, 'aria-label': '诊断结果' })
		]);
	},

	render: function(res) {
		if (document.body.classList.contains('m-submenu-compact'))
			return this.renderCompact(res);

		var has_ping6 = res[0].path || res[1].path,
		    has_traceroute6 = res[2].path || res[3].path,
		    has_arpscan = res[4].path,
		    devices = res[5],
			dns_host = uci.get('luci', 'diag', 'dns') || 'openwrt.org',
			ping_host = uci.get('luci', 'diag', 'ping') || 'openwrt.org',
			route_host = uci.get('luci', 'diag', 'route') || 'openwrt.org';

		var table = E('table', { 'class': 'table' }, [
				E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td left', 'style': 'overflow:initial' }, [
						E('input', {
							'style': 'margin:5px 0',
							'type': 'text',
							'value': ping_host
						}),
						E('span', { 'class': 'diag-action' }, [
							has_ping6 ? new ui.ComboButton('ping', {
								'ping': '%s %s'.format(_('IPv4'), _('Ping')),
								'ping6': '%s %s'.format(_('IPv6'), _('Ping')),
							}, {
								'click': ui.createHandlerFn(this, 'handlePing'),
								'classes': {
									'ping': 'btn cbi-button cbi-button-action',
									'ping6': 'btn cbi-button cbi-button-action'
								}
							}).render() : E('button', {
								'class': 'cbi-button cbi-button-action',
								'click': ui.createHandlerFn(this, 'handlePing')
							}, [ _('Ping') ])
						])
					]),

					E('td', { 'class': 'td left', 'style': 'overflow:initial' }, [
						E('input', {
							'style': 'margin:5px 0',
							'type': 'text',
							'value': route_host
						}),
						E('span', { 'class': 'diag-action' }, [
							has_traceroute6 ? new ui.ComboButton('traceroute', {
								'traceroute': '%s %s'.format(_('IPv4'), _('Traceroute')),
								'traceroute6': '%s %s'.format(_('IPv6'), _('Traceroute')),
							}, {
								'click': ui.createHandlerFn(this, 'handleTraceroute'),
								'classes': {
									'traceroute': 'btn cbi-button cbi-button-action',
									'traceroute6': 'btn cbi-button cbi-button-action'
								}
							}).render() : E('button', {
								'class': 'cbi-button cbi-button-action',
								'click': ui.createHandlerFn(this, 'handleTraceroute')
							}, [ _('Traceroute') ])
						])
					]),

					E('td', { 'class': 'td left' }, [
						E('input', {
							'style': 'margin:5px 0',
							'type': 'text',
							'value': dns_host
						}),
						E('span', { 'class': 'diag-action' }, [
							E('button', {
								'class': 'cbi-button cbi-button-action',
								'click': ui.createHandlerFn(this, 'handleNslookup')
							}, [ _('Nslookup') ])
						])
					]),

					has_arpscan ? E('td', { 'class': 'td left' }, [
						E('select', {
							'style': 'margin:5px 0'
						}, devices.map(function(device) {
							if (!device.isUp())
								return E([]);

							return E('option', { 'value': device.getName() }, [ device.getI18n() ]);
						})),
						E('span', { 'class': 'diag-action' }, [
							E('button', {
								'class': 'cbi-button cbi-button-action',
								'click': ui.createHandlerFn(this, 'handleArpScan')
							}, [ _('Arp-scan') ])
						])
					]) : E([]),
				])
			]);

		var view = E('div', { 'class': 'cbi-map'}, [
			E('h2', {}, [ _('Diagnostics') ]),
			E('div', { 'class': 'cbi-map-descr'}, _('Execution of various network commands to check the connection and name resolution to other systems.')),
			table,
			E('div', {'class': 'cbi-section'}, [
				E('div', { 'id' : 'command-output'},
					E('textarea', {
						'id': 'widget.command-output',
						'style': 'width: 100%; font-family:monospace; white-space:pre',
						'readonly': true,
						'wrap': 'off',
						'rows': '20'
					})
				)
			])
		]);

		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
