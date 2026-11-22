'use strict';
'require baseclass';
'require tools.myos-ac-ap as mesh';
'require ui';

const setMode = (mode, uplink) => mesh.send('set_mode', { mode, uplink });
const pair = () => mesh.send('pair');
const revoke = () => mesh.send('revoke');

function action(label, run) {
	const button = E('button', { 'class': 'cbi-button cbi-button-action', 'type': 'button' }, label);
	button.addEventListener('click', async () => {
		button.disabled = true;
		try {
			const result = await run();
			if (result && result.error) throw new Error(result.error);
		}
		catch (error) {
			ui.addNotification(null, E('p', {}, error.message));
		}
		finally {
			button.disabled = false;
		}
	});
	return button;
}

return baseclass.extend({
	render: function(info) {
		const content = E('div');
		const page = E('div', { class: 'cbi-section' }, [ E('h3', {}, '工作模式'), content ]);
		if (info.error) {
			content.appendChild(E('p', {}, info.error));
			return page;
		}
		const isAP = info.mode === 'ap';
		const notice = E('p', { 'role': 'status' });
		const code = E('input', {
			'type': 'text', 'class': 'cbi-input-text', 'readonly': '',
			'aria-label': 'AP 配对码', 'spellcheck': 'false', 'style': 'width: 100%; max-width: 620px'
		});
		const pairing = E('div', { 'hidden': '' }, [
			E('p', {}, '复制配对码，在主路由的「网络 → AC / AP 组网」中绑定。'),
			code
		]);
		code.addEventListener('focus', () => code.select());
		content.appendChild(E('p', {}, E('strong', {}, isAP ? '有线 AP' : '路由模式（AC）')));
		const uplink = E('select', { class: 'cbi-input-select', 'aria-label': '上联口' }, [
			E('option', { value: 'lan' }, '现有 LAN 口'),
			info.wired_wan ? E('option', { value: info.wired_wan }, '有线 WAN 口 (' + info.wired_wan + ')') : ''
		]);
		content.appendChild(E('p', {}, isAP
			? '终端由主路由分配地址。无线设置可由已绑定的 AC 下发。'
			: '将所选上联口接到主路由的 LAN。切换后，可在主路由的 AP 列表找到本机管理地址。'));
		if (!isAP) content.appendChild(E('p', {}, E('label', {}, [ '上联口 ', uplink ])));
		if (info.mac) content.appendChild(E('p', {}, 'LAN MAC  ' + info.mac));
		if (isAP && info.router_ip) content.appendChild(E('p', {}, '原路由地址  ' + info.router_ip));
		const switchMode = action(isAP ? '恢复路由模式' : '切换为有线 AP', async () => {
			const next = await setMode(isAP ? 'router' : 'ap', uplink.value);
			if (next.error) return next;
			content.replaceChildren(E('p', { 'role': 'status' }, isAP
				? '路由配置已写回。请从本机 LAN 口重新连接' +
					(next.router_ip ? ' ' + next.router_ip + '。' : '。')
				: 'AP 配置已保存。请从 AC 的 AP 列表查看本机的新管理地址。'));
		});
		content.appendChild(switchMode);
		if (isAP) {
			const reset = action('撤销配对', async () => {
				const result = await revoke();
				if (result.error) return result;
				pairing.hidden = true;
				code.value = '';
				notice.textContent = '已撤销 AC 配对。';
				reset.hidden = true;
			});
			reset.hidden = !info.pairing;
			content.appendChild(E('div', { 'class': 'cbi-section' }, [
				E('h3', {}, 'AC 配对'),
				E('p', {}, '生成新配对码会撤销旧配对。'),
				action('生成配对码', async () => {
					const result = await pair();
					if (result.error) return result;
					code.value = result.code;
					pairing.hidden = false;
					reset.hidden = false;
					notice.textContent = '';
					code.focus();
				}),
				' ',
				reset,
				pairing,
				notice
			]));
		}
		return page;
	}
});
