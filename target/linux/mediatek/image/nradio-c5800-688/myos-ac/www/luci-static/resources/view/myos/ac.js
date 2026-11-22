'use strict';
'require baseclass';
'require tools.myos-ac-ap as mesh';
'require ui';

const discover = () => mesh.send('discover');
const bind = code => mesh.send('bind', { code });
const status = mac => mesh.send('radios', { mac });
const unbind = mac => mesh.send('unbind', { mac });
const configure = (mac, id, ssid, encryption, key, channel, txpower, hidden, enabled) =>
	mesh.send('configure', { mac, id, ssid, encryption, key, channel, txpower, hidden, enabled });

function checked(result) {
	if (result.error) throw new Error(result.error);
	return result;
}

function button(label, run) {
	const control = E('button', { type: 'button', class: 'cbi-button cbi-button-action' }, label);
	control.addEventListener('click', async () => {
		control.disabled = true;
		try { await run(); }
		catch (error) { ui.addNotification(null, E('p', {}, error.message)); }
		finally { control.disabled = false; }
	});
	return control;
}

function select(label, values, selected) {
	const control = E('select', { class: 'cbi-input-select', 'aria-label': label },
		values.map(value => E('option', { value }, value === 'auto' ? '自动' : value)));
	control.value = selected;
	return control;
}

function radioForm(mac, radio, redraw) {
	const ssid = E('input', { type: 'text', class: 'cbi-input-text', value: radio.ssid, required: '' });
	const key = E('input', {
		type: 'password', class: 'cbi-input-password', autocomplete: 'new-password',
		placeholder: '留空保留现有密码'
	});
	const modes = [ 'none', 'owe', 'psk2', 'sae', 'sae-mixed', 'psk', 'psk-mixed' ];
	if (!modes.includes(radio.encryption)) modes.push(radio.encryption);
	const security = select('加密方式', modes, radio.encryption);
	const channel = select('信道', radio.channels, radio.channel);
	const power = radio.txpower == null ? null :
		E('input', { type: 'number', value: radio.txpower, min: 1, max: 100, step: 1, required: '' });
	const hidden = E('input', { type: 'checkbox' });
	const enabled = E('input', { type: 'checkbox' });
	hidden.checked = radio.hidden;
	enabled.checked = radio.enabled;
	const notice = E('span', { role: 'status' });
	const form = E('form', { class: 'cbi-section' }, [
		E('h4', {}, radio.ssid + (radio.band ? ' · ' + radio.band : '')),
		E('p', {}, E('label', {}, [ 'SSID ', ssid ])),
		E('p', {}, E('label', {}, [ '加密方式 ', security ])),
		E('p', {}, E('label', {}, [ '无线密码 ', key ])),
		E('p', {}, E('label', {}, [ '信道 ', channel ])),
		power ? E('p', {}, E('label', {}, [ '发射功率 (%) ', power ])) : '',
		E('p', {}, [ E('label', {}, [ enabled, ' 启用' ]), '　', E('label', {}, [ hidden, ' 隐藏 SSID' ]) ]),
		E('div', {}, [
			button('下发配置', async () => {
				if (!form.reportValidity()) return;
				const result = checked(await configure(mac, radio.id, ssid.value, security.value,
					key.value, channel.value, power ? Number(power.value) : 0, hidden.checked, enabled.checked));
				redraw(result, 'AP 已保存配置并重载无线。');
			}),
			' ', notice
		])
	]);
	form.addEventListener('submit', event => event.preventDefault());
	const setKeyState = () => { key.disabled = [ 'none', 'owe' ].includes(security.value); };
	security.addEventListener('change', setKeyState);
	setKeyState();
	return form;
}

return baseclass.extend({
	load: () => discover().catch(error => ({ error: error.message })),

	render(initial) {
		const list = E('div');
		const detail = E('div');
		const message = E('p', { role: 'status' });
		const code = E('input', {
			type: 'password', autocomplete: 'off', spellcheck: 'false',
			class: 'cbi-input-password', 'aria-label': 'AP 配对码', placeholder: '粘贴设备生成的配对码'
		});
		const renderList = result => {
		const devices = Array.isArray(checked(result).devices) ? result.devices : [];
			list.replaceChildren();
			if (!devices.length) {
				list.appendChild(E('p', {}, '尚未发现有线 AP。将处于 AP 模式的 MYOS 设备接到本机 LAN，再刷新列表。'));
				return;
			}
			const table = E('table', { class: 'table' }, [
				E('tr', { class: 'tr table-titles' }, [ '设备', 'MAC', '管理地址', '绑定状态', '操作' ]
					.map(text => E('th', { class: 'th' }, text)))
			]);
			for (const device of devices) {
				const actions = [];
				if (device.bound && device.ip) {
					actions.push(button('管理无线', async () => {
						const show = (reply, notice) => {
							detail.replaceChildren(E('h3', {}, (reply.model || 'MYOS AP') + ' · ' + device.ip));
							if (notice) detail.appendChild(E('p', { role: 'status' }, notice));
							const radios = Array.isArray(reply.radios) ? reply.radios : [];
							for (const radio of radios) detail.appendChild(radioForm(device.mac, radio, show));
							if (!radios.length) detail.appendChild(E('p', {}, 'AP 未配置桥接到 LAN 的无线网络。'));
						};
						show(checked(await status(device.mac)));
					}));
				}
				if (device.bound) actions.push(' ', button('移除绑定', async () => {
					checked(await unbind(device.mac));
					detail.replaceChildren();
					message.textContent = '本机已移除绑定。也可在 AP 的组网页面撤销配对码。';
					renderList(await discover());
				}));
				table.appendChild(E('tr', { class: 'tr' }, [
				device.model || 'MYOS AP', device.mac.match(/../g).join(':'), device.ip || '离线',
					device.bound ? '已绑定' : '待绑定', actions
				].map(value => E('td', { class: 'td' }, value))));
			}
			list.appendChild(table);
		};
		if (initial.error)
			list.appendChild(E('p', { role: 'alert' }, initial.error));
		else
			renderList(initial);
		return E('div', {}, [
			E('h3', {}, 'AP 管理'),
			E('div', { class: 'cbi-section' }, [
				E('label', {}, [ 'AP 配对码 ', code ]), ' ',
				button('绑定', async () => {
					checked(await bind(code.value.trim()));
					code.value = '';
					message.textContent = 'AP 已绑定。';
					renderList(await discover());
				}),
				' ', button('刷新列表', async () => renderList(await discover())),
				message
			]),
			list, detail
		]);
	}
});
