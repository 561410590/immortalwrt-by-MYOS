(function() {
	'use strict';

	if (document.getElementById('myos-disclaimer'))
		return;

	const paragraphs = [
		'本固件为第三方定制固件，由社区维护，仅供个人学习、研究与测试使用，不随设备销售，也不构成任何形式的商业承诺。',
		'固件按“现状”提供，不附带任何明示或暗示的担保。作者与分发者不对使用固件导致的设备无法启动、变砖、数据丢失、网络中断、硬件损坏及其他直接或间接损失负责。',
		'刷写固件、修改配置或启用第三方插件的风险由您自行承担。操作前请备份原厂固件和重要数据，并确认您有权修改该设备。',
		'请遵守所在国家和地区的法律法规。严禁将本固件用于违法用途，违规使用产生的责任由使用者自行承担。',
		'继续使用即表示您已阅读、理解并接受以上条款。若不同意，请停止使用并刷回官方固件。'
	];
	const checkbox = E('input', { type: 'checkbox', disabled: true });
	const status = E('p', { id: 'myos-disclaimer-status', role: 'status' });
	const button = E('button', {
		type: 'button', class: 'btn cbi-button cbi-button-positive',
		disabled: true, 'aria-describedby': 'myos-disclaimer-status'
	}, '请等待 10 秒');
	const dialog = E('dialog', {
		id: 'myos-disclaimer', 'aria-labelledby': 'myos-disclaimer-title'
	}, [
		E('h2', { id: 'myos-disclaimer-title', tabindex: '-1', autofocus: true }, '免责说明'),
		E('div', { class: 'myos-disclaimer-text' }, paragraphs.map(text => E('p', {}, text))),
		E('div', { class: 'myos-disclaimer-actions' }, [
			E('label', {}, [checkbox, '我已阅读并理解上述条款，自愿承担使用本固件的风险']),
			status,
			button
		])
	]);
	const style = E('style', {}, `
		#myos-disclaimer {
			box-sizing: border-box; width: min(35rem, calc(100% - 2rem));
			max-height: calc(100% - 2rem); margin: auto; overflow: auto;
			padding: 0; border: 0; border-radius: 1rem;
			color-scheme: light; color: #172338; background: #fff;
			box-shadow: 0 1.5rem 4rem rgb(0 0 0 / 30%);
		}
		#myos-disclaimer::backdrop { background: rgb(15 23 42 / 56%); }
		#myos-disclaimer-title {
			margin: 0; padding: 1.25rem 1.5rem; font-size: 1.25rem;
			color: #fff; background: #245fc9;
		}
		#myos-disclaimer .myos-disclaimer-text { padding: 1.25rem 1.5rem 0; line-height: 1.6; }
		#myos-disclaimer .myos-disclaimer-text p { margin: 0 0 0.8rem; }
		#myos-disclaimer .myos-disclaimer-actions {
			padding: 0.9rem 1.5rem 1.25rem; border-top: 1px solid #e2e8f0;
		}
		#myos-disclaimer label { display: flex; align-items: flex-start; gap: 0.65rem; }
		#myos-disclaimer input { flex: none; margin-top: 0.25em; }
		#myos-disclaimer-status { margin: 0.45rem 0; color: #a12620; }
		#myos-disclaimer button { width: 100%; min-height: 2.5rem; }
	`);

	function send(action) {
		const fields = new URLSearchParams({ token: L.env.token, action });
		if (action === 'accept')
			fields.set('agree', checkbox.checked ? '1' : '0');
		return fetch(L.url('admin', 'myos_disclaimer'), {
			method: 'POST', credentials: 'same-origin', body: fields
		}).then(response => {
			if (!response.ok)
				throw new Error('Unable to save disclaimer state');
			return response.json();
		});
	}

	function close() {
		dialog.close();
		dialog.remove();
		style.remove();
	}

	function start() {
		button.disabled = true;
		button.textContent = '请等待 10 秒';
		checkbox.disabled = true;
		status.textContent = '';
		button.onclick = null;
		send('start').then(result => {
			if (result.accepted) {
				close();
				return;
			}
			const finish = performance.now() + 10000;
			function tick() {
				const remaining = finish - performance.now();
				if (remaining > 0) {
					button.textContent = '请等待 ' + Math.ceil(remaining / 1000) + ' 秒';
					window.setTimeout(tick, Math.min(remaining, 250));
					return;
				}
				checkbox.disabled = false;
				button.textContent = '我已阅读并同意';
				button.disabled = !checkbox.checked;
			}
			tick();
		}).catch(() => {
			status.textContent = '暂时无法开始计时，请重试。';
			button.textContent = '重试';
			button.disabled = false;
			button.onclick = start;
		});
	}

	dialog.addEventListener('cancel', event => event.preventDefault());
	checkbox.addEventListener('change', () => {
		button.disabled = !checkbox.checked;
	});
	function accept() {
		if (checkbox.disabled || !checkbox.checked)
			return;
		button.disabled = true;
		status.textContent = '';
		send('accept').then(close).catch(() => {
			status.textContent = '暂时无法保存同意记录，请重试。';
			button.disabled = !checkbox.checked;
		});
	}
	button.addEventListener('click', () => {
		if (!checkbox.disabled)
			accept();
	});
	document.head.append(style);
	document.body.append(dialog);
	dialog.showModal();
	start();
})();
