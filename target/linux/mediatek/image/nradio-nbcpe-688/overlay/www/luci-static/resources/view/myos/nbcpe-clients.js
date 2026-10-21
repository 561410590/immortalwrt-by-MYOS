'use strict';
'require view';
'require rpc';
'require poll';
'require ui';

var getClients = rpc.declare({ object: 'c2000max.ui', method: 'clients', reject: true });
return view.extend({
	load: function() { return getClients(); },
	render: function(data) {
		var summary = E('p'), table = E('div', { 'class': 'table cbi-section-table' });
		function update(value) {
			var clients = Array.isArray(value.clients) ? value.clients : [];
			summary.textContent = '在线设备：' + (value.available ? value.count : '未获取') + '；已知终端：' + clients.length;
			table.replaceChildren(E('div', { 'class': 'tr table-titles' }, [
				E('div', { 'class': 'th' }, '设备名称'), E('div', { 'class': 'th' }, 'IP 地址'),
				E('div', { 'class': 'th' }, 'MAC 地址'), E('div', { 'class': 'th' }, '状态')
			]));
			clients.forEach(function(client) {
				var addresses = E('div', { 'class': 'td', 'data-title': 'IP 地址', style: 'overflow-wrap:anywhere' });
				(client.ipaddrs || []).forEach(function(address) { addresses.appendChild(E('div', {}, address)); });
				table.appendChild(E('div', { 'class': 'tr' }, [
					E('div', { 'class': 'td', 'data-title': '设备名称', style: 'overflow-wrap:anywhere' }, client.hostname || '未命名设备'),
					addresses,
					E('div', { 'class': 'td', 'data-title': 'MAC 地址', style: 'overflow-wrap:anywhere' }, client.mac),
					E('div', { 'class': 'td', 'data-title': '状态' }, client.online ? '在线' : client.lease ? '租约有效，在线状态未确认' : '邻居缓存，在线状态未确认')
				]));
			});
			if (!clients.length) table.appendChild(E('div', { 'class': 'tr' }, [E('div', { 'class': 'td' }, value.available ? '暂无终端记录' : '暂未获取终端状态')]));
		}
		function refresh() { return getClients().then(update); }
		update(data);
		poll.add(refresh, 10);
		return E('div', {}, [E('h2', {}, '终端设备'), summary,
			E('button', { 'class': 'cbi-button', click: ui.createHandlerFn(this, refresh) }, '刷新'),
			E('div', { 'class': 'cbi-section', style: 'max-width:100%;overflow-x:auto' }, [table])]);
	},
	handleSave: null, handleSaveApply: null, handleReset: null
});
