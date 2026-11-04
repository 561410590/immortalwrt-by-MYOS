'use strict';
'require view';
'require rpc';
'require ui';

var getStatus = rpc.declare({ object: 'myos.system', method: 'status', reject: true });
var returnOfficial = rpc.declare({ object: 'myos.system', method: 'official', params: ['confirmed'], reject: true });

return view.extend({
	load: function() {
		return getStatus().catch(function(error) {
			return { error: error.message || String(error) };
		});
	},
	render: function(status) {
		var button = E('button', {
			'class': 'cbi-button cbi-button-apply',
			click: ui.createHandlerFn(this, function() {
				ui.showModal('返回官方系统（实验性）', [
					E('p', {}, '确认返回官方系统并重启？重启后，请使用官方系统原有的 Wi-Fi 或管理地址重新连接。'),
					E('div', { 'class': 'right' }, [
						E('button', { 'class': 'cbi-button', click: ui.hideModal }, '取消'), ' ',
						E('button', { 'class': 'cbi-button cbi-button-apply', click: ui.createHandlerFn(this, function(ev) {
							ev.currentTarget.disabled = true;
							return returnOfficial(true).then(function(result) {
								if (result.error || !result.started) throw new Error(result.error || '系统切换未启动');
								button.disabled = true;
								ui.showModal('正在返回官方系统', [E('p', { 'class': 'spinning' }, '启动标记已确认，设备正在重启。请使用官方系统原有管理地址重新连接。')]);
							}).catch(function(error) {
								ui.hideModal();
								ui.addNotification(null, E('p', {}, error.message || String(error)), 'error');
							});
						}) }, '确认并重启')
					])
				]);
			})
		}, '返回官方系统（实验性）');
		return E('div', {}, [
			E('h2', {}, '返回官方系统（实验性）'),
			E('div', { 'class': 'cbi-section' }, [
				E('p', {}, '切换到设备 eMMC 中保留的官方系统。官方系统和 MYOS 各自保留自己的配置。'),
				E('p', {}, status.error || status.message || '无法取得双系统状态'),
				button
			])
		]);
	},
	handleSave: null, handleSaveApply: null, handleReset: null
});
