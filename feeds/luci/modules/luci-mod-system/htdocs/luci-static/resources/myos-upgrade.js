'use strict';
'require baseclass';

var labels = {
	wifi: _('Wi-Fi 名称、加密方式和密码'),
	sim: _('SIM 卡槽设置'),
	fan: _('风扇设置'),
	led: _('指示灯设置'),
	behav: _('行为管理'),
	pbr: _('策略路由')
};
var icons = { wifi: '◉', sim: '▣', fan: '✣', led: '☀', behav: '☷', pbr: '↗' };

return baseclass.extend({
	supports: function(item) {
		return Object.hasOwn(labels, item);
	},

	keepOptions: function(items) {
		var mode = E('select', { 'class': 'cbi-input-select', 'aria-label': _('配置保留方式') }, [
			E('option', { value: 'discard' }, _('不保留配置')),
			E('option', { value: 'select' }, _('按项保留配置')),
			E('option', { value: 'keep' }, _('完整保留配置'))
		]);
		var summary = E('p', { 'aria-live': 'polite' });
		var fields = E('div', { style: 'display:grid;grid-template-columns:repeat(auto-fit,minmax(12rem,1fr));gap:.6rem;margin:.75rem 0' });
		var choices = items.filter(this.supports).map(function(item) {
			var input = E('input', { type: 'checkbox' });
			var card = E('label', { style: 'display:flex;align-items:center;gap:.5rem;padding:.7rem;border:1px solid currentColor;border-radius:.7rem;cursor:pointer' }, [
				input, E('span', { 'aria-hidden': 'true' }, icons[item]), E('span', {}, labels[item])
			]);
			fields.appendChild(card);
			input.addEventListener('change', function() {
				card.style.fontWeight = input.checked ? 'bold' : '';
				card.style.boxShadow = input.checked ? 'inset 0 0 0 1px currentColor' : '';
				update();
			});
			return { item: item, input: input };
		});
		function selected() {
			return choices.filter(function(choice) { return choice.input.checked; })
				.map(function(choice) { return choice.item; });
		}
		function update() {
			fields.style.display = mode.value === 'select' ? 'grid' : 'none';
			var chosen = mode.value === 'select' ? selected() : [];
			summary.textContent = mode.value === 'keep' ? _('将完整保留配置。') :
				chosen.length ? _('将保留 %s。其余配置恢复默认。').format(chosen.map(function(item) { return labels[item]; }).join('、')) :
				_('不保留配置，升级后恢复固件默认设置。');
		}
		mode.addEventListener('change', update);
		update();
		return { mode: mode, selected: selected, element: E('div', {}, [
			E('label', {}, [ _('配置保留方式'), ' ', mode ]), fields, summary
		]) };
	},

	flashStatus: function() {
		return E('div', { 'role': 'status' }, [
			E('progress', { style: 'width:100%', 'aria-label': _('正在写入固件') }),
			E('p', {}, _('正在提交固件写入。请保持供电，设备重启后将尝试重新连接。'))
		]);
	}
});
