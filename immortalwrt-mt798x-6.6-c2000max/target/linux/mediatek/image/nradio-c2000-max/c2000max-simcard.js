'use strict';
'require view';
'require form';
'require fs';
'require poll';

function gpioCmd_c8_688(value) {
	switch (value) {
	case 'simcard1':
		return 'echo 1 > /sys/class/gpio/cpe-sel1/value && echo 0 > /sys/class/gpio/cpe-sel0/value';
	case 'simcard2':
	case 'esimcard1':
		return 'echo 1 > /sys/class/gpio/cpe-sel1/value && echo 1 > /sys/class/gpio/cpe-sel0/value';
	case 'esimcard2':
		return 'echo 0 > /sys/class/gpio/cpe-sel1/value && echo 1 > /sys/class/gpio/cpe-sel0/value';
	default:
		return '';
	}
}

return view.extend({
	selector: '/etc/init.d/c2000max-sim-select',
	readStatus: function() {
		return fs.exec('/bin/sh', ['-c', this.selector + ' status_json']).then(function(res) {
			if (!res || res.code !== 0) throw new Error(_('读取切卡状态失败'));
			return JSON.parse(res.stdout || '{}');
		});
	},
	updateStatus: function() {
		return this.readStatus().then(function(status) {
			this.switchBusy = !!status.busy;
			var labels = { '1': _('SIM卡1'), '2': _('SIM卡2'), '3': _('内置卡1') };
			var stages = {
				idle: _('等待状态'), queued: _('任务已提交'), stopping: _('停止旧卡拨号'),
				switching: _('切换并激活 SIM'), checking: _('核对卡槽与识卡'),
				recovering: _('恢复模组识卡'), dialing: _('等待拨号'),
				online: _('蜂窝接口已获取地址'), error: _('切卡失败')
			};
			var rows = [
				[_('执行阶段'), stages[status.stage] || _('等待状态')],
				[_('目标卡槽'), labels[status.target] || _('未知')],
				[_('实际卡槽'), labels[status.actual] || _('正在确认')],
				[_('SIM 状态'), status.cpin || _('正在读取')],
				[_('IPv4 地址'), status.ipv4 || _('尚未获取')],
				[_('IPv6 地址'), status.ipv6 || _('尚未获取')],
				[_('详情'), status.message || '']
			];
			this.statusNode.replaceChildren(E('table', { 'class': 'table' }, rows.map(function(row) {
				return E('tr', { 'class': 'tr' }, [
					E('td', { 'class': 'td', 'style': 'width:30%' }, row[0]),
					E('td', { 'class': 'td', 'style': 'overflow-wrap:anywhere' }, row[1])
				]);
			})), E('small', {}, _('每 5 秒读取状态。实际卡槽确认后恢复拨号；接口地址用于显示蜂窝连接状态。')));
			if (this.resetButton) this.resetButton.disabled = this.switchBusy;
		}.bind(this)).catch(function(err) {
			this.statusNode.replaceChildren(E('p', {}, _('读取状态失败：') + (err.message || String(err))));
		}.bind(this));
	},

	render: function() {
		return fs.exec('/bin/sh', ['-c', 'tr -d "\\0" < /proc/device-tree/model 2>/dev/null || echo unknown'])
			.then(function(res) {
				return ((res && res.stdout) || '').indexOf('C2000-MAX') >= 0 ? 'NRadio C2000-MAX' : 'c8-688';
			}).catch(function(err) {
				console.warn('simcard: failed to detect model:', err);
				return 'c8-688';
			}).then(this.render_form.bind(this));
	},
	render_form: function(model) {
		var isMax = model === 'NRadio C2000-MAX';
		var slots = { simcard1: '1', simcard2: '2', esimcard1: '3' };
		var m = new form.Map('embed_simcard', _('SIM卡切换'), _('当前设备型号：') + model);
		if (isMax) m.chain('cpesel');
		var s = isMax ? m.section(form.NamedSection, 'general', 'general', _('General Settings')) :
			m.section(form.TypedSection, 'general', _('General Settings'));
		s.anonymous = true;
		var o = s.option(form.DummyValue, 'model', _('设备型号'));
		o.default = model;
		o = s.option(form.ListValue, 'slot', _('SIM Slot'), isMax ?
			_('选择要使用的 SIM 卡槽。切卡后自动识卡并重新拨号，请等待网络恢复。') :
			_('选择要使用的 SIM 卡槽。切换后，正在进行的网络连接会中断。'));
		o.value('simcard1', _('SIM卡1'));
		o.value('simcard2', _('SIM卡2'));
		o.value('esimcard1', _('内置卡1'));
		if (!isMax) o.value('esimcard2', _('内置卡2'));
		o.default = 'simcard1';
		o.rmempty = false;
		if (isMax) o.cfgvalue = function(section_id) {
			var cur = this.map.data.get('cpesel', 'sim', 'cur');
			return { '1': 'simcard1', '2': 'simcard2', '3': 'esimcard1' }[cur] ||
				this.map.data.get(this.map.config, section_id, this.option);
		};
		var page = this;
		o.write = function(section_id, value) {
			var cmd = isMax ? (slots[value] && '/etc/init.d/c2000max-sim-select select_sim ' + slots[value]) : gpioCmd_c8_688(value);
			if (!cmd) return Promise.reject(new Error(_('Invalid SIM slot')));
			return fs.exec('/bin/sh', ['-c', cmd]).then(function(res) {
				var reply = {};
				try { reply = JSON.parse((res && res.stdout) || '{}'); } catch (err) {}
				if (!res || res.code !== 0) throw new Error(reply.message || _('切卡任务提交失败'));
				if (isMax) {
					this.map.data.set('cpesel', 'sim', 'cur', slots[value]);
					page.updateStatus();
				}
				return this.map.data.set(this.map.config, section_id, this.option, value);
			}.bind(this));
		};
		var ResetOption = form.DummyValue.extend({
			renderWidget: function() {
				page.resetButton = E('button', {
					'class': 'btn cbi-button cbi-button-negative',
					'click': function() {
						if (!window.confirm(_('确认要对模组执行硬重启吗？'))) return;
						var cmd = 'echo 0 > /sys/class/gpio/cpe-pwr/value && sleep 1 && echo 1 > /sys/class/gpio/cpe-pwr/value';
						fs.exec('/bin/sh', ['-c', cmd]).then(function(res) {
							if (!res || res.code !== 0) throw new Error(_('硬重启失败'));
							window.alert(_('硬重启指令已发送，模组正在重启，请耐心等待。'));
						}).catch(function(err) { window.alert(err.message || String(err)); });
					}
				}, [_('执行硬重启')]);
				return page.resetButton;
			}
		});
		s.option(ResetOption, '_reset', _('模组硬重启'), _('模组无响应时，强制断电重启模组。'));
		return m.render().then(function(node) {
			if (!isMax) return node;
			this.statusNode = E('div', { 'class': 'cbi-section' }, [_('正在读取切卡状态…')]);
			var container = E('div', {}, [this.statusNode, node]);
			poll.add(this.updateStatus.bind(this), 5);
			return this.updateStatus().then(function() { return container; });
		}.bind(this));
	}
});
