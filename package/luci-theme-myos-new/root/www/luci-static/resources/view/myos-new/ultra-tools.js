'use strict';
'require baseclass';
'require request';

const route = name => L.url('admin', 'myos', name);
const button = (label, action) => E('button', {type:'button', class:'m-pill-button', click:action}, label);

async function post(name, values) {
	const body = new URLSearchParams(Object.assign({token:L.env.token}, values)).toString();
	const response = await request.post(route(name), body, {
		headers:{'Content-Type':'application/x-www-form-urlencoded'}, timeout:18000
	});
	if (!response.ok) throw new Error('请求失败（HTTP ' + response.status + '）');
	return response.json();
}

function modal(id, title, contents) {
	const heading = E('h2', {id}, title);
	const close = E('button', {type:'button', class:'m-icon-button', 'aria-label':'关闭'}, '×');
	const dialog = E('dialog', {class:'m-client-dialog', 'aria-labelledby':id},
		[E('div', {class:'m-background-heading'}, [heading, close]), contents]);
	close.addEventListener('click', () => dialog.close());
	return dialog;
}

const callLabels = {
	active:'已接通', held:'保持中', dialing:'正在拨号', alerting:'正在等待对方接听',
	ringing:'来电', waiting:'等待接听'
};

const dialKeys = [
	['1',''], ['2','ABC'], ['3','DEF'],
	['4','GHI'], ['5','JKL'], ['6','MNO'],
	['7','PQRS'], ['8','TUV'], ['9','WXYZ'],
	['*',''], ['0','+'], ['#','']
];

function phoneIcon(name) {
	const icon = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
	icon.setAttribute('viewBox', '0 0 24 24');
	icon.setAttribute('aria-hidden', 'true');
	const path = document.createElementNS(icon.namespaceURI, 'path');
	if (name === 'delete') {
		path.setAttribute('d', 'M9 5h12v14H9l-7-7 7-7Zm3 4 6 6m0-6-6 6');
		path.setAttribute('fill', 'none');
		path.setAttribute('stroke', 'currentColor');
		path.setAttribute('stroke-width', '1.65');
		path.setAttribute('stroke-linejoin', 'round');
		path.setAttribute('stroke-linecap', 'round');
	} else {
		path.setAttribute('fill', 'currentColor');
		path.setAttribute('d', 'M7.1 2.5c.7 0 1.2.4 1.4 1l1.3 4.1c.2.6 0 1.2-.5 1.6L7.4 11c1.3 2.5 3.2 4.4 5.7 5.7l1.8-1.9c.4-.5 1-.7 1.6-.5l4.1 1.3c.6.2 1 .8 1 1.4v3c0 .8-.6 1.5-1.5 1.5C10.3 21.5 2.5 13.7 2.5 4c0-.9.7-1.5 1.5-1.5h3.1Z');
		if (name === 'hangup') path.setAttribute('transform', 'rotate(135 12 12)');
	}
	icon.append(path);
	return icon;
}

function createPhone() {
	let running = false, inFlight = false, readIms = true;
	let timer, ims;
	let awaitingStatus = false;
	const number = E('input', {type:'tel', id:'m-ultra-phone-number',
		autocomplete:'off', placeholder:'输入电话号码', 'aria-label':'电话号码', required:''});
	const state = E('p', {role:'status', class:'m-dial-state'}, '正在读取通话状态…');
	const note = E('p', {role:'status', class:'m-muted'});
	const imsText = E('p', {class:'m-muted'});
	const recent = E('ul', {class:'m-dial-recent'});
	const history = E('details', {hidden:true}, [E('summary', {}, '近期通话'), recent]);
	const eventLabels = {missed:'未接来电', unanswered:'未接通'};
	const formatDuration = seconds => Math.floor(seconds / 60) + ':' + String(seconds % 60).padStart(2, '0');
	const settings = E('a', {href:'/5700/', target:'_blank', rel:'noopener'}, '打开 MT5700 设置');
	settings.hidden = true;
	const dial = E('button', {type:'submit', class:'m-dial-call', 'aria-label':'拨打电话', title:'拨打'}, phoneIcon('dial'));
	const answer = E('button', {type:'button', class:'m-dial-call', 'aria-label':'接听', title:'接听',
		click:() => control('answer')}, phoneIcon('dial'));
	const hangup = E('button', {type:'button', class:'m-dial-call m-dial-hangup', 'aria-label':'挂断', title:'挂断',
		click:() => control('hangup')}, phoneIcon('hangup'));
	const backspace = E('button', {type:'button', class:'m-dial-delete', 'aria-label':'删除一位', title:'删除一位',
		click:() => {
			const end = number.selectionEnd ?? number.value.length;
			const start = number.selectionStart ?? end;
			number.setRangeText('', start === end ? Math.max(0, start - 1) : start, end, 'end');
		}}, phoneIcon('delete'));
	const keypad = E('div', {class:'m-dial-grid', role:'group', 'aria-label':'拨号键盘'}, dialKeys.map(([key, letters]) => {
		let hold, longPress = false;
		const digit = E('button', {type:'button', class:'m-dial-key', 'aria-label':key,
			click:() => {
				if (!longPress) insertDigit(key);
				longPress = false;
			}}, [E('span', {}, key), E('small', {'aria-hidden':'true'}, letters)]);
		if (key === '0') {
			digit.setAttribute('aria-description', '长按输入加号');
			digit.addEventListener('pointerdown', event => {
				if (event.button !== 0) return;
				longPress = false;
				hold = setTimeout(() => { longPress = true; insertDigit('+'); }, 500);
			});
			for (const event of ['pointerup','pointercancel','pointerleave']) {
				digit.addEventListener(event, () => clearTimeout(hold));
			}
			digit.addEventListener('contextmenu', event => event.preventDefault());
		}
		return digit;
	}));
	const form = E('form', {class:'m-ultra-dialer'}, [
		E('div', {class:'m-dial-display'}, [state, number]),
		keypad, E('div', {class:'m-dial-actions'}, [dial, answer, hangup, backspace]),
		note, imsText, settings, history,
		E('p', {class:'m-dial-notice'}, '接通后对方听到静音，可能产生话费。')
	]);
	answer.disabled = hangup.disabled = true;
	answer.hidden = hangup.hidden = true;

	function insertDigit(digit) {
		const end = number.selectionEnd ?? number.value.length;
		const start = number.selectionStart ?? end;
		number.setRangeText(digit, start, end, 'end');
	}

	function showCallActions(canAnswer, canHangup) {
		dial.disabled = answer.disabled = hangup.disabled = inFlight;
		answer.hidden = !canAnswer;
		hangup.hidden = !canHangup;
		backspace.hidden = canHangup;
	}

	function schedule() {
		clearTimeout(timer);
		if (!document.hidden && form.isConnected) timer = setTimeout(refresh, 2400);
	}

	async function refresh() {
		if (document.hidden || !form.isConnected || running || inFlight) return;
		running = true;
		const afterCommand = awaitingStatus;
		clearTimeout(timer);
		try {
			const query = '?' + new URLSearchParams({ims: readIms ? '1' : '0', fresh: afterCommand ? '1' : '0'});
			const response = await request.get(route('ultra_phone') + query, {timeout:12000, cache:false});
			if (!response.ok) throw new Error('请求失败（HTTP ' + response.status + '）');
			const result = response.json();
			readIms = false;
			if (result.ims != null) ims = result.ims;
			imsText.textContent = ims === 0 ? '模组 IMS 已关闭，电话能否接通取决于当前语音注册。' : '';
			settings.hidden = ims !== 0;
			const events = Array.isArray(result.recent) ? result.recent : [];
			history.hidden = !events.length;
			recent.replaceChildren(...events.map(event => E('li', {}, [
				eventLabels[event.state] || (event.incoming ? '接听' : '拨出'),
				' · ', event.number || '未知号码',
				' · ', new Date(event.ended * 1000).toLocaleString(),
				event.state === 'ended' ? ' · ' + formatDuration(event.duration || 0) : ''
			])));
			if (!result.supported || result.error) {
				state.textContent = result.error || '未找到 MT5700 的 AT 端口';
				showCallActions(true, true);
				return;
			}
			const calls = Array.isArray(result.calls) ? result.calls : [];
			const next = calls.find(c => c.incoming && (c.state === 'ringing' || c.state === 'waiting'));
			const call = next || calls[0];
			const last = events[0];
			state.textContent = call ? callLabels[call.state] + (call.number ? ' · ' + call.number : '') :
				(last?.state === 'missed' ? '未接来电 · ' + (last.number || '未知号码') : '当前无通话');
			if (call?.connected) state.textContent += ' · ' + formatDuration(call.duration || 0);
			showCallActions(!!next, !!call);
		} catch (error) {
			state.textContent = error.message || '无法读取通话状态';
			showCallActions(true, true);
		} finally {
			if (afterCommand && awaitingStatus && !inFlight) {
				awaitingStatus = false;
				note.textContent = '';
			}
			running = false;
			schedule();
		}
	}

	async function control(action) {
		if (inFlight) return;
		if (action === 'dial' && !/^\+?[0-9*#]+$/.test(number.value.trim())) {
			note.textContent = '请输入号码，可使用数字、*、# 和开头的 +';
			number.focus();
			return;
		}
		inFlight = true;
		awaitingStatus = false;
		dial.disabled = answer.disabled = hangup.disabled = true;
		note.textContent = '正在提交操作…';
		try {
			const result = await post('ultra_phone_control', {action, number:number.value.trim()});
			awaitingStatus = result.ok === true;
			note.textContent = result.ok ? '命令已发送，正在读取通话状态' : (result.error || '操作未成功');
		} catch (error) {
			note.textContent = error.message || '操作未确认，请查看通话状态';
		} finally {
			inFlight = false;
			refresh();
		}
	}

	form.addEventListener('submit', event => {
		event.preventDefault();
		if (!dial.disabled) control('dial');
	});
	document.addEventListener('visibilitychange', () => {
		if (document.hidden) clearTimeout(timer);
		else {
			readIms = true;
			refresh();
		}
	});
	return {form, start:refresh};
}

const targets = [
	['baidu','百度'], ['tencent','腾讯'], ['alibaba','阿里云'],
	['cloudflare','Cloudflare'], ['google','Google'], ['youtube','YouTube']
];

function createEgress() {
	let generation = 0, opener;
	const status = E('p', {role:'status', class:'m-muted'});
	const family = E('select', {id:'m-ultra-egress-family'},
		[E('option', {value:'4'}, 'IPv4'), E('option', {value:'6'}, 'IPv6')]);
	const retry = button('重新检测', run);
	const address = E('p', {class:'m-muted'});
	const sites = E('div', {class:'m-ultra-egress-sites'});
	const timingLabels = ['DNS','TCP','TLS','首字节'];
	const dialog = modal('m-ultra-egress-title', '出口诊断', E('div', {}, [
		E('div', {class:'m-client-tools'}, [E('label', {for:family.id}, '地址类型'), family, retry]),
		address, sites, status,
		E('p', {class:'m-muted'}, '按所选地址类型探测路由器当前出口。首字节为从请求开始到收到首字节的总耗时。')
	]));

	const ms = n => n == null ? '—' : n + ' ms';
	async function run() {
		const runId = ++generation;
		const selected = family.value;
		retry.disabled = true;
		status.textContent = '正在检测…';
		address.textContent = '正在查询公网地址…';
		const opened = matchMedia('(min-width:761px)').matches;
		const entries = targets.map(([target,label]) => {
			const outcome = E('span', {class:'m-muted'}, '检测中');
			const timings = timingLabels.map(() => E('dd', {}, '—'));
			const detail = E('details', opened ? {open:''} : {}, [
				E('summary', {}, E('span', {}, [E('strong', {}, label), outcome])),
				E('dl', {class:'m-kv'}, timingLabels.map((title,i) =>
					E('div', {}, [E('dt', {}, title), timings[i]])))
			]);
			return {target, outcome, timings, detail};
		});
		sites.replaceChildren(...entries.map(entry => entry.detail));
		const addressRequest = post('ultra_egress', {target:'public', family:selected}).then(result => {
			if (runId === generation) address.textContent = result.ok ?
				'公网地址 ' + result.address + (result.location ? ' · ' + result.location : '') : result.error;
		}).catch(() => {
			if (runId === generation) address.textContent = '公网地址查询失败';
		});
		const results = await Promise.all(entries.map(async ({target,outcome,timings}) => {
			let result;
			try {
				result = await post('ultra_egress', {target, family:selected});
			} catch (error) {
				result = {error:error.message};
			}
			if (runId === generation) {
				outcome.textContent = result.ok ? 'HTTP ' + result.code : result.error || '检测失败';
				[result.dns, result.tcp, result.tls, result.ttfb].forEach((value,i) => timings[i].textContent = ms(value));
			}
			return result;
		}));
		await addressRequest;
		if (runId !== generation) return;
		const source = results.find(result => result.source)?.source;
		status.textContent = results.filter(result => result.ok).length + ' / ' + targets.length +
			' 个站点返回 HTTP 响应' + (source ? ' · 本机源地址 ' + source : '');
		retry.disabled = false;
	}

	family.addEventListener('change', run);
	dialog.addEventListener('close', () => {
		generation++;
		if (opener?.isConnected) opener.focus({preventScroll:true});
	});
	return {dialog, open(trigger) {
		opener = trigger;
		if (!dialog.open) dialog.showModal();
		run();
	}};
}

return baseclass.extend({createPhone, createEgress});
