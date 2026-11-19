'use strict';
'require view';
'require rpc';

var listDevices = rpc.declare({
	object: 'myos.usb-network',
	method: 'devices',
	expect: { devices: [] }
});

function configureLink(device, role, title) {
	return E('a', {
		'class': 'btn',
		'href': L.url('admin', 'network', 'network') +
			'?myos_usb_role=' + role + '&myos_usb_device=' + encodeURIComponent(device)
	}, title);
}

return view.extend({
	load: function() {
		return listDevices();
	},

	render: function(devices) {
		var types = {
			ethernet: _('USB 网卡 / CDC 共享'),
			rndis: _('安卓 USB 共享 (RNDIS)'),
			iphone: _('iPhone USB 共享')
		};
		var rows = devices.map(function(device) {
			return E('tr', { 'class': 'tr cbi-rowstyle-1' }, [
				E('td', { 'class': 'td' }, device.name),
				E('td', { 'class': 'td' }, types[device.kind] || _('USB 网络设备')),
				E('td', { 'class': 'td' }, device.driver),
				E('td', { 'class': 'td' }, [
					configureLink(device.name, 'lan', _('配置 LAN 网桥')), ' ',
					configureLink(device.name, 'wan', _('新建 WAN 接口'))
				])
			]);
		});

		return E([], [
			E('h2', {}, _('USB 网络')),
			E('p', {}, _('加入 LAN 时，在网桥的桥接端口中选择对应设备。新建 WAN 时，选择协议及 wan 防火墙区域，再保存并应用。')),
			E('p', {}, _('手机需先开启 USB 共享。iPhone 首次连接时，请解锁并在手机端确认信任。')),
			E('button', {
				'class': 'btn',
				'click': function() { window.location.reload(); }
			}, _('刷新设备')),
			rows.length ? E('table', { 'class': 'table cbi-section-table' }, [
				E('tr', { 'class': 'tr table-titles' }, [
					E('th', { 'class': 'th' }, _('设备')),
					E('th', { 'class': 'th' }, _('类型')),
					E('th', { 'class': 'th' }, _('驱动')),
					E('th', { 'class': 'th' }, _('配置'))
				])
			].concat(rows)) : E('p', {}, _('当前未检测到支持的 USB 网络设备。'))
		]);
	},

	handleSave: null,
	handleSaveApply: null,
	handleReset: null
});
