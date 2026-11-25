/*
 * 策略规则编辑器:覆盖 MwrtPbr.editor。桌面为右侧抽屉,手机为整页。
 * open({ id|null, copyFrom|null }) -> Promise<{ saved:bool, id?:str }>
 * 本地只做格式预检与预览;规则是否有效、是否被遮蔽,以后端 rule_check / rule_save 的结论为准。
 * 动态文本只经 h() 与 textContent 写入。
 */
(function() {
'use strict';

var M = window.MwrtPbr;
if (!M || !M.modal || !M.h || !M.ui || !M.api) return;

var h = M.h, ui = M.ui, icon = M.icon, COPY = M.copy;

var NAME_MAX = 64;
var NOTE_MAX = 200;
var VAL_MAX = 64;
var PORT_MAX = 15;
var CHECK_MS = 450;
var APPLY_MS = 60000;
var DIM_KEYS = ['app', 'src_dev', 'src_ip', 'dst_ip', 'domain'];
var DIM_LABEL = { app: '应用', src_dev: '设备', src_ip: '源地址', dst_ip: '目的地址', domain: '域名' };
var PROTOS = [['any', '不限协议'], ['tcp', 'TCP'], ['udp', 'UDP'], ['tcp_udp', 'TCP + UDP'], ['icmp', 'ICMP']];
var PROTO_LABEL = { any: '不限', tcp: 'TCP', udp: 'UDP', tcp_udp: 'TCP + UDP', icmp: 'ICMP' };
var PORT_PROTO = { tcp: 1, udp: 1, tcp_udp: 1 };
var FIELDS = ['name', 'match', 'target', 'on_fail', 'proto', 'dport', 'sport', 'app', 'src_dev', 'src_ip', 'dst_ip', 'domain'];
var PREVIEW_N = 5;
var UID = 0;
var CUR = null;
var ICON = { ok: 0, bad: 0 };
var APPS = { p: null, at: 0, d: null };
var CLI = { p: null, at: 0, d: null };

function noop() {}
function arr(x) { return Array.isArray(x) ? x : []; }
function msgOf(e) { return e && e.message ? String(e.message) : String(e || '未知错误'); }
function byKey(list, key, val) {
	for (var i = 0; i < list.length; i++) if (list[i] && list[i][key] === val) return list[i];
	return null;
}
function splitTarget(t) {
	t = String(t || '');
	var i = t.indexOf(':');
	return i < 0 ? { type: '', id: t } : { type: t.slice(0, i), id: t.slice(i + 1) };
}
function wanTitle(w) { return w.alias || w.name || w.wan_id || '未命名出口'; }
function clip(s, n) { s = String(s == null ? '' : s); return s.length > n ? s.slice(0, n) : s; }
function firstChar(s) { s = String(s == null ? '' : s).replace(/^\s+/, ''); return s ? s.charAt(0).toUpperCase() : '?'; }

/* ---------- 输入预检(宽松,细节以后端为准) ---------- */
var RE_V4 = /^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$/;
function isV4(s) { return RE_V4.test(s); }
function v6words(part, last) {
	if (part === '') return 0;
	var g = part.split(':'), n = 0, i;
	for (i = 0; i < g.length; i++) {
		if (last && i === g.length - 1 && g[i].indexOf('.') >= 0) {
			if (!isV4(g[i])) return -1;
			n += 2;
		} else if (/^[0-9a-f]{1,4}$/.test(g[i])) n++;
		else return -1;
	}
	return n;
}
function isV6(s) {
	var t = s.toLowerCase(), dc, a, b;
	if (!/^[0-9a-f:.]+$/.test(t) || t.indexOf(':') < 0 || t.indexOf(':::') >= 0) return false;
	dc = t.indexOf('::');
	if (dc < 0) return v6words(t, true) === 8;
	if (t.indexOf('::', dc + 2) >= 0) return false;
	a = v6words(t.slice(0, dc), false);
	b = v6words(t.slice(dc + 2), true);
	return a >= 0 && b >= 0 && a + b <= 7;
}
function addrFam(x) {
	x = x.trim();
	if (x.indexOf(':') >= 0) return isV6(x) ? 6 : 0;
	return isV4(x) ? 4 : 0;
}
function parseAddr(s) {
	var t = s.trim(), i, ip, n, a, b, x, y, max;
	if (!t) return { e: '地址不能为空' };
	i = t.indexOf('/');
	if (i >= 0) {
		ip = t.slice(0, i).trim();
		n = t.slice(i + 1).trim();
		a = addrFam(ip);
		if (!a) return { e: t + ' 不是合法的 ' + (ip.indexOf(':') >= 0 ? 'IPv6' : 'IPv4') + ' 地址' };
		if (!/^\d{1,3}$/.test(n)) return { e: t + ' 的前缀长度不是数字' };
		max = a === 4 ? 32 : 128;
		if (Number(n) < 1 || Number(n) > max) return { e: t + ' 的前缀长度必须在 1-' + max + ' 之间(不接受 /0)' };
		return { v: ip.toLowerCase() + '/' + Number(n), fam: a };
	}
	i = t.indexOf('-');
	if (i >= 0) {
		x = t.slice(0, i).trim();
		y = t.slice(i + 1).trim();
		a = addrFam(x);
		b = addrFam(y);
		if (!a) return { e: x + ' 不是合法的 ' + (x.indexOf(':') >= 0 ? 'IPv6' : 'IPv4') + ' 地址' };
		if (!b) return { e: y + ' 不是合法的 ' + (y.indexOf(':') >= 0 ? 'IPv6' : 'IPv4') + ' 地址' };
		if (a !== b) return { e: t + ' 的两端不是同一种地址(IPv4/IPv6)' };
		return { v: x.toLowerCase() + '-' + y.toLowerCase(), fam: a };
	}
	a = addrFam(t);
	if (!a) return { e: t + ' 不是合法的 IPv4/IPv6 地址、网段(CIDR)或区间' };
	return { v: t.toLowerCase(), fam: a };
}
function parseMac(s) {
	var raw = s.trim(), t = raw.toLowerCase();
	if (/^[0-9a-f]{2}(-[0-9a-f]{2}){5}$/.test(t)) t = t.replace(/-/g, ':');
	else if (/^[0-9a-f]{12}$/.test(t)) t = t.replace(/(..)(?=.)/g, '$1:');
	if (!/^[0-9a-f]{2}(:[0-9a-f]{2}){5}$/.test(t)) return { e: raw + ' 不是合法的 MAC 地址(应为 aa:bb:cc:dd:ee:ff)' };
	if (parseInt(t.slice(0, 2), 16) & 1) return { e: raw + ' 是组播/广播地址,不能作为设备 MAC' };
	if (t === '00:00:00:00:00:00') return { e: raw + ' 不是有效的设备 MAC' };
	return { v: t };
}
function parsePort(s) {
	var raw = s.trim(), m = /^(\d{1,5})(-(\d{1,5}))?$/.exec(raw), lo, hi;
	if (!m) return { e: raw + ' 不是合法的端口(1-65535 的数字,或 起-止 区间)' };
	lo = Number(m[1]);
	hi = m[3] != null ? Number(m[3]) : lo;
	if (lo < 1 || lo > 65535 || hi < 1 || hi > 65535) return { e: raw + ' 超出端口范围 1-65535' };
	if (lo > hi) return { e: raw + ' 的起始端口大于结束端口' };
	return { v: lo === hi ? String(lo) : lo + '-' + hi };
}
function parseDomain(s) {
	var raw = s.trim(), t = raw.toLowerCase(), labels, i;
	if (t.slice(0, 2) === '*.') t = t.slice(2);
	t = t.replace(/^\.+/, '').replace(/\.+$/, '');
	if (!t) return { e: raw + ' 不是有效的域名' };
	if (/[^\x00-\x7f]/.test(t)) return { e: raw + ' 含有非 ASCII 字符,请使用 punycode(xn--)写法' };
	if (t.indexOf(':') >= 0) return { e: raw + ' 是 IPv6 地址,请填到「目的地址」里' };
	if (!/^[a-z0-9.-]+$/.test(t)) return { e: raw + ' 含有不允许的字符(只允许字母、数字、连字符和点)' };
	if (t.length > 253) return { e: raw + ' 超过 253 个字符' };
	labels = t.split('.');
	if (labels.length < 2) return { e: raw + ' 至少需要两段(如 example.com)' };
	for (i = 0; i < labels.length; i++) {
		if (!labels[i]) return { e: raw + ' 含有空的段(连续的点)' };
		if (labels[i].length > 63) return { e: raw + ' 的某一段超过 63 个字符' };
		if (!/^[a-z0-9]([a-z0-9-]*[a-z0-9])?$/.test(labels[i])) return { e: raw + ' 的段不能以连字符开头或结尾' };
	}
	if (/^[0-9]+$/.test(labels[labels.length - 1])) return { e: raw + ' 看起来是 IP 地址,请填到「目的地址」里' };
	return { v: t };
}
/* 粘贴或回车时按空白、逗号、分号、顿号切分;区间两侧的空格先收拢 */
function tokens(text) {
	return String(text == null ? '' : text).replace(/　/g, ' ').replace(/[,;，；、\r\n\t]+/g, ' ')
		.replace(/\s*([\/-])\s*/g, '$1').split(/\s+/).filter(function(x) { return x; });
}

/* ---------- 应用库与设备列表(各自只取一次,失败可重试) ---------- */
function loadApps() {
	if (APPS.p && Date.now() - APPS.at < 300000) return APPS.p;
	APPS.at = Date.now();
	var p = M.api('apps').then(function(j) {
		var names = {}, cls = {}, list = [];
		arr(j.classes).forEach(function(c) {
			cls[c.id] = c.name || ('分类 ' + c.id);
			names['cls:' + c.id] = cls[c.id] + '(整个分类)';
		});
		arr(j.apps).forEach(function(a) {
			names['app:' + a.id] = a.name || ('应用 ' + a.id);
			list.push(a);
		});
		APPS.d = { available: j.available !== false, base: j.icon_base || '', ver: j.version || '', names: names, cls: cls, classes: arr(j.classes), apps: list };
		return APPS.d;
	}, function(e) { APPS.p = null; throw e; });
	APPS.p = p;
	return p;
}
function loadClients(force) {
	if (!force && CLI.p && Date.now() - CLI.at < 20000) return CLI.p;
	CLI.at = Date.now();
	var p = M.api('clients').then(function(j) {
		var names = {};
		arr(j.clients).forEach(function(c) { names[c.mac] = c.alias || c.name || ''; });
		CLI.d = { list: arr(j.clients), names: names };
		return CLI.d;
	}, function(e) { CLI.p = null; throw e; });
	CLI.p = p;
	return p;
}
function appName(tok) {
	var n = APPS.d && APPS.d.names[tok];
	return n || tok;
}
function devName(mac) { return (CLI.d && CLI.d.names[mac]) || ''; }

/* 应用图标:没有图片文件时退回首字母;连续失败后不再发请求 */
function appIcon(tok, name) {
	var box = h('span', { class: 'pr-ed-ai', 'aria-hidden': 'true' }, firstChar(name)), m = /^app:(\d+)$/.exec(tok), d = APPS.d, img, urls, k = 0, v;
	if (!m || !d || !d.base || (ICON.bad >= 3 && !ICON.ok)) return box;
	v = d.ver ? '?v=' + encodeURIComponent(d.ver) : '';
	urls = [d.base + m[1] + '.webp' + v];
	img = h('img', { alt: '' });
	img.addEventListener('load', function() {
		ICON.ok++;
		box.textContent = '';
		box.classList.add('img');
		box.appendChild(img);
	});
	img.addEventListener('error', function() {
		k++;
		if (k < urls.length) img.src = urls[k]; else ICON.bad++;
	});
	img.src = urls[0];
	return box;
}

/* ---------- 取值框:chips + 输入 + 选择按钮 ---------- */
function tagBox(o) {
	var vals = [], T = {}, err = '';
	var chips = h('div', { class: 'pr-chips pr-ed-chips', role: 'list', 'aria-label': o.label + '已添加的值' });
	var none = h('span', { class: 'pr-dim pr-ed-none' }, o.none || '未设置,不限制此项');
	var errEl = h('div', { class: 'pr-fe pr-ed-fe', 'aria-live': 'polite', hidden: true });
	var srvEl = h('div', { class: 'pr-ed-srv', 'aria-live': 'polite', hidden: true });
	var inp = null, addBtn = null, pickBtn = null, row = null;

	function setErr(t) {
		err = t || '';
		errEl.textContent = err;
		errEl.hidden = !err;
		if (inp) { if (err) inp.setAttribute('aria-invalid', 'true'); else inp.removeAttribute('aria-invalid'); }
	}
	function renderChips() {
		chips.textContent = '';
		vals.forEach(function(v) {
			var rm = h('button', { type: 'button', title: '删除', 'aria-label': '删除 ' + (o.text ? o.text(v) : v) }, icon('close'));
			rm.addEventListener('click', function() {
				var i = vals.indexOf(v);
				if (i >= 0) vals.splice(i, 1);
				renderChips();
				setErr('');
				if (inp && !inp.disabled) inp.focus(); else if (pickBtn && !pickBtn.disabled) pickBtn.focus();
				o.onChange(T);
			});
			chips.appendChild(h('span', { class: 'pr-chip', role: 'listitem' }, o.chip ? o.chip(v) : h('span', { class: 'pr-mono pr-ellip' }, v), rm));
		});
		chips.hidden = !vals.length;
		none.hidden = !!vals.length;
	}
	function add(text) {
		var bad = [], msgs = [], added = 0, dup = 0, full = false;
		tokens(text).forEach(function(t) {
			var r = o.parse(t);
			if (r.e) { bad.push(t); msgs.push(r.e); return; }
			if (vals.indexOf(r.v) >= 0) { dup++; return; }
			if (vals.length >= o.max) { bad.push(t); full = true; return; }
			vals.push(r.v);
			added++;
		});
		if (inp) inp.value = bad.join(' ');
		if (msgs.length) setErr(msgs[0] + (msgs.length > 1 ? '(另有 ' + (msgs.length - 1) + ' 项也不合法,已留在输入框里)' : ''));
		else if (full) setErr('最多 ' + o.max + ' 项,多出的已留在输入框里');
		else if (dup && !added) setErr('这个值已经添加过了');
		else setErr('');
		if (added) { renderChips(); o.onChange(T); }
		return !bad.length;
	}

	if (!o.noInput) {
		inp = h('input', { type: 'text', class: 'pr-in', placeholder: o.placeholder, 'aria-label': o.label, autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false' });
		inp.addEventListener('keydown', function(e) {
			if (e.key === 'Enter' && !e.isComposing) { e.preventDefault(); add(inp.value); }
		});
		inp.addEventListener('input', function() { if (err) setErr(''); o.onChange(T); });
		inp.addEventListener('paste', function(e) {
			var cd = e.clipboardData || window.clipboardData, t = cd ? String(cd.getData('text') || '') : '';
			if (/[\s,;，；、]/.test(t.trim())) { e.preventDefault(); add(inp.value + ' ' + t); }
		});
		addBtn = ui.btn({ kind: 'ghost', size: 'sm', icon: 'plus', text: '添加', onClick: function() { add(inp.value); inp.focus(); } });
	}
	if (o.onPick) pickBtn = ui.btn({ kind: 'ghost', size: 'sm', icon: o.pickIcon || 'plus', text: o.pickText || '从列表选择', onClick: function() { o.onPick(T); } });
	row = h('div', { class: 'pr-ed-add' }, inp, addBtn, pickBtn);

	T.el = h('div', { class: 'pr-ed-tb' }, none, chips, row, errEl, srvEl);
	T.get = function() { return vals.slice(); };
	T.set = function(list) {
		vals = [];
		arr(list).forEach(function(v) { v = String(v); if (vals.indexOf(v) < 0) vals.push(v); });
		renderChips();
		setErr('');
	};
	T.refresh = renderChips;
	T.pending = function() { return !!inp && inp.value.trim() !== ''; };
	T.flush = function() { return !T.pending() || add(inp.value); };
	T.focus = function() {
		if (inp && !inp.disabled) inp.focus();
		else if (pickBtn && !pickBtn.disabled) pickBtn.focus();
		else T.el.scrollIntoView({ block: 'center' });
	};
	T.setDisabled = function(b) {
		[inp, addBtn, pickBtn].forEach(function(n) { if (n) n.disabled = !!b; });
		row.hidden = !!b && !!o.hideWhenOff;
	};
	T.setSrv = function(list) {
		srvEl.textContent = '';
		list.forEach(function(m) { srvEl.appendChild(h('div', { class: 'pr-fe' }, m)); });
		srvEl.hidden = !list.length;
		if (inp) { if (list.length) inp.setAttribute('aria-invalid', 'true'); else if (!err) inp.removeAttribute('aria-invalid'); }
	};
	T.count = function() { return vals.length; };
	renderChips();
	return T;
}

function note(kind, ic, text) {
	return h('div', { class: 'pr-note pr-k-' + kind, role: 'note' }, icon(ic), h('div', { class: 'pr-grow' }, text));
}

/* ---------- 编辑器主体 ---------- */
function openEditor(opts, resolve) {
	var uid = ++UID;
	var srcId = opts.id || opts.copyFrom || null;
	var mode = opts.id ? 'edit' : (opts.copyFrom ? 'copy' : 'new');
	var S = { id: opts.id || null, base: '', enabled: true, match: 'all', on_fail: 'default', closed: false, fin: false, saved: false, busy: false, built: false, tried: false, touched: {}, timer: 0, seq: 0, chk: { state: 'idle', sig: '', res: null, err: '' }, lastSig: '' };
	var pub, noteEl, btnCancel, btnSave, btnApply;
	var nameInp, noteTa, protoSel, targetSel, ofLegend, ofHint, ofRadios = [], matchRadios = [], matchHint, enSw;
	var TB = {}, SUB = {}, SLOT = {}, FIELD_CTL = {}, DIMCARD = {};
	var E = {};

	function fid(s) { return 'pr-ed' + uid + '-' + s; }
	function st() { return M.store.state || {}; }
	function caps() { return st().caps || {}; }

	/* ---- 读写表单 ---- */
	function collect() {
		var p = {
			name: nameInp.value.trim(), enabled: !!S.enabled, note: noteTa.value.trim(),
			match: S.match, target: targetSel.value, on_fail: S.on_fail, proto: protoSel.value,
			dport: TB.dport.get(), sport: TB.sport.get()
		};
		DIM_KEYS.forEach(function(k) { p[k] = TB[k].get(); });
		return p;
	}
	function anyPending() {
		var i, ks = Object.keys(TB);
		for (i = 0; i < ks.length; i++) if (TB[ks[i]].pending()) return true;
		return false;
	}
	function flushAll() {
		var ks = Object.keys(TB), i, bad = null;
		for (i = 0; i < ks.length; i++) if (!TB[ks[i]].flush() && !bad) bad = ks[i];
		if (bad) { TB[bad].focus(); M.toast('有填写的内容格式不对,请先改正或清空输入框', 'warn'); return false; }
		return true;
	}
	function dimCount(p) {
		var n = 0;
		DIM_KEYS.forEach(function(k) { if (p[k].length) n++; });
		return n;
	}

	/* ---- 预览 ---- */
	function exitName(target) {
		var t = splitTarget(target), w, g, s = st();
		if (t.type === 'wan') { w = byKey(arr(s.wans), 'wan_id', t.id); return w ? wanTitle(w) : t.id + '(未找到该出口)'; }
		if (t.type === 'group') { g = byKey(arr(s.groups), 'id', t.id); return g ? (g.name || g.id) + '(出口组)' : t.id + '(未找到该出口组)'; }
		return '';
	}
	function valText(k, v) {
		var n;
		if (k === 'app') return appName(v);
		if (k === 'src_dev') { n = devName(v); return n ? n + '(' + v + ')' : v; }
		if (k === 'domain') return v + '(含子域)';
		return v;
	}
	function listText(k, list, sep) {
		var t = list.slice(0, PREVIEW_N).map(function(v) { return valText(k, v); }).join(sep);
		return list.length > PREVIEW_N ? t + sep + '…(共 ' + list.length + ' 项)' : t;
	}
	function describe(p) {
		var dims = [], dimsX = [], svc = [], svcX = [], any = p.match === 'any' && dimCount(p) > 1, dp, sp, cond, condX, px, en, tn, ofT, s;
		DIM_KEYS.forEach(function(k) {
			if (!p[k].length) return;
			dims.push(DIM_LABEL[k] + '是 ' + listText(k, p[k], ' 或 '));
			dimsX.push('(' + DIM_LABEL[k] + ' ∈ {' + listText(k, p[k], ', ') + '})');
		});
		if (p.proto !== 'any') { svc.push('协议是 ' + PROTO_LABEL[p.proto]); svcX.push('协议 = ' + PROTO_LABEL[p.proto]); }
		if (p.dport.length) { svc.push('目的端口是 ' + p.dport.join('、')); svcX.push('目的端口 ∈ {' + p.dport.join(', ') + '}'); }
		if (p.sport.length) { svc.push('源端口是 ' + p.sport.join('、')); svcX.push('源端口 ∈ {' + p.sport.join(', ') + '}'); }
		dp = dims.length ? (any ? '(' + dims.join(' 或 ') + ')' : dims.join(' 并且 ')) : '';
		sp = svc.join(' 并且 ');
		cond = [dp, sp].filter(function(x) { return x; }).join(' 并且 ');
		px = svcX.length ? '(' + svcX.join(' 并且 ') + ')' : '';
		en = dimsX.join(any ? ' 或 ' : ' 并且 ');
		if (any) en = '(' + en + ')';
		condX = [en, px].filter(function(x) { return x; }).join(' 并且 ');
		tn = splitTarget(p.target);
		if (!cond) s = '还没有设置任何匹配条件。规则至少需要一项条件(应用、设备、地址、域名、协议或端口)。';
		else {
			s = '新建连接中,满足 ' + cond + ' 的,走「' + (exitName(p.target) || '(还没有选择出口)') + '」。';
			ofT = p.on_fail === 'block' ? COPY.onFail.block : COPY.onFail['default'];
			if (p.target) s += (tn.type === 'group' ? COPY.onFail.group : COPY.onFail.single) + ofT + '。';
		}
		return { sentence: s, expr: condX };
	}
	function renderPreview() {
		var p = collect(), d = describe(p), dims = dimCount(p), svc = p.proto !== 'any' || p.dport.length || p.sport.length, i, notes = [];
		E.pvS.textContent = d.sentence;
		E.pvX.textContent = d.expr ? '等价说明:' + d.expr : '';
		E.pvX.hidden = !d.expr;
		if (!dims && svc) notes.push('没有设置应用、设备、地址、域名时,规则会对所有设备、所有目的地中符合协议或端口的新连接生效。');
		if (p.match === 'any' && dims > 1 && svc) notes.push('协议和端口始终要同时满足,不属于「满足任一」的范围。');
		E.pvN.textContent = '';
		for (i = 0; i < notes.length; i++) E.pvN.appendChild(note('warn', 'warn', notes[i]));
	}
	function renderSubs() {
		var p = collect();
		DIM_KEYS.forEach(function(k) { SUB[k].textContent = p[k].length > 1 ? '满足其中任一项即可' : ''; });
		matchHint.textContent = (p.match === 'any'
			? '不同类条件满足其中任意一类即可。'
			: '不同类条件需要同时满足。') + '同一类条件里填多个值,满足其中任一个即可。协议和端口始终要同时满足,不受这里影响。' +
			(dimCount(p) < 2 ? '目前只有 ' + dimCount(p) + ' 类条件,两种方式效果相同。' : '');
		['src_ip', 'dst_ip'].forEach(function(k) {
			var v6 = p[k].some(function(v) { return v.indexOf(':') >= 0; });
			DIMCARD[k].v6.hidden = !v6;
		});
		var t = splitTarget(p.target), w = t.type === 'wan' ? byKey(arr(st().wans), 'wan_id', t.id) : null;
		E.v6.hidden = !(t.type === 'group' || (w && w.caps && w.caps.v6 === false));
		ofLegend.textContent = t.type === 'group' ? COPY.onFail.group : COPY.onFail.single;
		ofHint.hidden = p.on_fail !== 'block';
		var portOk = !!PORT_PROTO[p.proto];
		TB.dport.setDisabled(!portOk);
		TB.sport.setDisabled(!portOk);
		E.portNote.hidden = portOk;
	}

	/* ---- 校验 ---- */
	function setNote(t, kind) {
		noteEl.textContent = t || '';
		noteEl.className = 'pr-mf-note' + (kind ? ' pr-k-' + kind : '');
	}
	function clearSlots() {
		var k;
		for (k in SLOT) { SLOT[k].textContent = ''; SLOT[k].hidden = true; }
		for (k in TB) TB[k].setSrv([]);
		for (k in FIELD_CTL) if (FIELD_CTL[k].removeAttribute) FIELD_CTL[k].removeAttribute('aria-invalid');
		E.top.textContent = '';
		E.top.hidden = true;
	}
	function hideEarly(f) {
		/* 新建时还没填的"必填项"不一上来就标红,点保存后再提示 */
		if (S.tried) return false;
		return f === '_rule' || (f === 'target' && !S.touched.target);
	}
	function showErrors(errs) {
		var by = {}, top = [], first = null;
		clearSlots();
		arr(errs).forEach(function(x) {
			var f = x && x.field ? String(x.field) : '_rule', m = x && x.msg ? String(x.msg) : '';
			if (!m || hideEarly(f)) return;
			if (FIELDS.indexOf(f) < 0) { top.push(m); return; }
			(by[f] = by[f] || []).push(m);
		});
		Object.keys(by).forEach(function(f) {
			if (TB[f]) TB[f].setSrv(by[f]);
			else if (SLOT[f]) {
				by[f].forEach(function(m) { SLOT[f].appendChild(h('div', null, m)); });
				SLOT[f].hidden = false;
				if (FIELD_CTL[f] && FIELD_CTL[f].setAttribute) FIELD_CTL[f].setAttribute('aria-invalid', 'true');
			}
			if (!first) first = f;
		});
		top.forEach(function(m) { E.top.appendChild(note('err', 'x-circle', m)); });
		E.top.hidden = !top.length;
		return first || (top.length ? '_top' : null);
	}
	function focusField(f) {
		if (f === '_top') { E.top.scrollIntoView({ block: 'center' }); return; }
		if (TB[f]) { TB[f].focus(); return; }
		var c = FIELD_CTL[f];
		if (c && c.focus) { c.focus(); if (c.scrollIntoView) c.scrollIntoView({ block: 'center' }); }
	}
	function renderCheck() {
		var c = S.chk, res = c.res, n = res ? arr(res.errors).length : 0, w = [], i, sh;
		if (res) showErrors(res.errors); else showErrors([]);
		arr(res && res.warnings).forEach(function(x) {
			var f = x && x.field, m = x && x.msg;
			if (!m || (f === 'name' && !S.touched.name && !S.tried)) return;
			w.push(['warn', 'warn', String(m)]);
		});
		sh = arr(res && res.shadow);
		sh.forEach(function(x) { w.push(['warn', 'info', String(x.reason || ('可能被前面的规则「' + (x.name || '') + '」遮蔽'))]); });
		E.pvW.textContent = '';
		for (i = 0; i < w.length; i++) E.pvW.appendChild(note(w[i][0], w[i][1], w[i][2]));
		if (sh.length) E.pvW.appendChild(h('p', { class: 'pr-hint pr-ed-sh' }, '遮蔽提示是按匹配范围估算的,实际是否被遮蔽还取决于具体连接;可以在规则列表里调整顺序。'));
		E.pvW.hidden = !E.pvW.firstChild;
		if (c.state === 'wait' || c.state === 'run') setNote('校验中…');
		else if (c.state === 'fail') setNote('暂时无法校验,保存时会再次校验', 'warn');
		else if (res && !res.valid) setNote(n + ' 个问题需要先修改', 'err');
		else if (res) setNote('校验通过' + (w.length ? ',有 ' + w.length + ' 条提示' : ''), w.length ? 'warn' : 'ok');
		else setNote('');
	}
	function runCheck() {
		var p = collect(), sig = JSON.stringify(p), my = ++S.seq, data = { json: p };
		if (S.id) data.id = S.id;
		S.chk.state = 'run';
		M.api('rule_check', data).then(function(r) {
			if (S.closed || my !== S.seq) return;
			S.chk = { state: r.valid ? 'ok' : 'bad', sig: sig, res: r, err: '' };
			renderCheck();
		}, function(e) {
			if (S.closed || my !== S.seq) return;
			S.chk = { state: 'fail', sig: sig, res: null, err: msgOf(e) };
			renderCheck();
		});
	}
	function scheduleCheck(now) {
		var sig = JSON.stringify(collect());
		if (sig === S.lastSig && !now) return;
		S.lastSig = sig;
		S.seq++;
		clearTimeout(S.timer);
		S.chk.state = 'wait';
		setNote('校验中…');
		S.timer = setTimeout(runCheck, now ? 0 : CHECK_MS);
	}

	/* ---- 变更 ---- */
	function changed(field) {
		if (S.closed || !S.built) return;
		if (typeof field === 'string') S.touched[field] = true;
		M.dirty.set('pr-editor', !S.saved && (JSON.stringify(collect()) !== S.base || anyPending()));
		renderSubs();
		renderPreview();
		scheduleCheck(false);
	}
	function tbChange(f) { return function() { changed(f); }; }

	/* ---- 选择器 ---- */
	function pickApps(tb) {
		if (caps().apps === false) return;
		M.picker({
			title: '选择应用', sub: '可以选单个应用,也可以选整个分类', max: VAL_MAX, selected: tb.get(),
			searchText: '搜索应用名称', groupLabel: '全部分类', emptyText: '应用库里没有可选的应用',
			emptyHint: '应用识别依赖行为管控(mwrt-behav)的特征库,请先确认特征库已导入。',
			items: function() {
				return loadApps().then(function(d) {
					var out = [];
					d.classes.forEach(function(c) {
						out.push({ id: 'cls:' + c.id, name: c.name || ('分类 ' + c.id), sub: '该分类下的全部应用', group: c.name || ('分类 ' + c.id), badge: '整个分类', keys: '分类 整个分类' });
						d.apps.forEach(function(a) { if (a.cls === c.id) out.push({ id: 'app:' + a.id, name: a.name || ('应用 ' + a.id), group: c.name || ('分类 ' + c.id) }); });
					});
					d.apps.forEach(function(a) { if (!d.cls[a.cls]) out.push({ id: 'app:' + a.id, name: a.name || ('应用 ' + a.id), group: '未分类' }); });
					return out;
				});
			}
		}).then(function(ids) {
			if (S.closed || !ids) return;
			tb.set(ids);
			changed('app');
		});
	}
	function pickDevs(tb) {
		M.picker({
			title: '选择设备', sub: '按设备 MAC 匹配', max: VAL_MAX, selected: tb.get(), searchText: '搜索名称、IP 或 MAC',
			emptyText: '没有发现设备', emptyHint: '设备需要出现在在线列表、DHCP 租约或静态主机里;也可以手动输入 MAC。',
			items: function() {
				return loadClients(true).then(function(d) {
					return d.list.map(function(c) {
						var sub = [c.ip, arr(c.ip6)[0], c.mac].filter(function(x) { return x; }).join(' · ');
						return { id: c.mac, name: c.alias || c.name || c.ip || c.mac, sub: sub, group: c.online ? '在线设备' : '离线或历史设备' };
					});
				});
			}
		}).then(function(ids) {
			if (S.closed || !ids) return;
			tb.set(ids);
			TB.src_dev.refresh();
			changed('src_dev');
		});
	}

	/* ---- 控件搭建 ---- */
	function slot(f) {
		SLOT[f] = h('div', { class: 'pr-fe pr-ed-fe', 'aria-live': 'polite', hidden: true });
		return SLOT[f];
	}
	function sec(title, sub, kids) {
		return h('section', { class: 'pr-ed-sec' }, h('h3', { class: 'pr-ed-h' }, title), sub ? h('p', { class: 'pr-hint pr-ed-sh' }, sub) : null, kids);
	}
	function radios(name, list, onChange) {
		var out = [];
		list.forEach(function(it) {
			var inp = h('input', { type: 'radio', name: fid(name), value: it[0] });
			inp.addEventListener('change', function() { if (inp.checked) onChange(it[0]); });
			out.push({ v: it[0], inp: inp, t: it[1] });
		});
		return out;
	}
	function setRadio(list, v) { list.forEach(function(r) { r.inp.checked = r.v === v; }); }
	function dimCard(k, hints, extra) {
		var sub = h('span', { class: 'pr-sub' }), tid = fid('dt-' + k), v6 = null;
		SUB[k] = sub;
		if (k === 'src_ip' || k === 'dst_ip') v6 = note('info', 'info', COPY.ipv6);
		if (v6) v6.hidden = true;
		DIMCARD[k] = { v6: v6 || h('span', { hidden: true }) };
		return h('section', { class: 'pr-ed-dim', 'aria-labelledby': tid },
			h('div', { class: 'pr-ed-dh' }, h('h4', { class: 'pr-ed-dt', id: tid }, DIM_LABEL[k]), sub),
			extra || null, TB[k].el,
			hints.map(function(t) { return h('p', { class: 'pr-hint pr-ed-dhint' }, t); }),
			v6 || null);
	}
	function targetOptions(cur) {
		var s = st(), wans = arr(s.wans), groups = arr(s.groups), found = false, wg, gg;
		targetSel.textContent = '';
		targetSel.appendChild(h('option', { value: '' }, '请选择出口'));
		wg = wans.map(function(w) {
			var v = 'wan:' + w.wan_id, sfx = w.state === 'absent' ? ',设备未接入' : w.state === 'offline' ? ',离线' : w.state === 'disabled' ? ',已停用' : '';
			if (v === cur) found = true;
			return h('option', { value: v }, wanTitle(w) + '(' + M.fmt.kind_label(w.kind) + sfx + ')');
		});
		gg = groups.map(function(g) {
			var v = 'group:' + g.id;
			if (v === cur) found = true;
			return h('option', { value: v }, (g.name || g.id) + '(出口组,' + arr(g.members).length + ' 个成员' + (g.enabled === false ? ',已停用' : '') + ')');
		});
		if (wg.length) targetSel.appendChild(h('optgroup', { label: 'WAN 出口' }, wg));
		if (gg.length) targetSel.appendChild(h('optgroup', { label: '出口组' }, gg));
		if (cur && !found) targetSel.appendChild(h('option', { value: cur }, cur + '(未找到,保存前请改选)'));
		targetSel.value = cur || '';
	}

	function build(rule) {
		var init = rule || {}, cp = mode === 'copy', nm;
		nm = String(init.name || '');
		if (cp) nm = clip(nm, NAME_MAX - 4) + '(副本)';
		S.enabled = cp ? false : init.enabled !== false;
		S.match = init.match === 'any' ? 'any' : 'all';
		S.on_fail = init.on_fail === 'block' ? 'block' : 'default';

		nameInp = h('input', { type: 'text', class: 'pr-in', id: fid('name'), maxlength: NAME_MAX, placeholder: '例如:办公电脑走宽带 1', autocomplete: 'off', value: nm });
		nameInp.addEventListener('input', function() { changed('name'); });
		noteTa = h('textarea', { class: 'pr-in pr-ta', id: fid('note'), maxlength: NOTE_MAX, rows: 2, placeholder: '可选,写给自己看的备注', value: String(init.note || '') });
		noteTa.addEventListener('input', function() { changed('note'); });
		enSw = ui.toggle({ checked: S.enabled, label: '启用此规则', onChange: function(want) { S.enabled = want; changed('enabled'); } });
		FIELD_CTL.name = nameInp;

		TB.app = tagBox({ label: '应用', noInput: true, none: '未设置,不按应用匹配', onPick: pickApps, pickText: '选择应用…', max: VAL_MAX, onChange: tbChange('app'),
			parse: function(t) { return { v: t }; }, text: appName,
			chip: function(tok) { return [appIcon(tok, appName(tok)), h('span', { class: 'pr-ed-cn pr-ellip', title: appName(tok) }, appName(tok))]; } });
		TB.src_dev = tagBox({ label: '设备 MAC', placeholder: '输入 MAC,如 aa:bb:cc:dd:ee:ff', max: VAL_MAX, parse: parseMac, onChange: tbChange('src_dev'), onPick: pickDevs, pickText: '选择设备…',
			text: function(v) { return devName(v) || v; },
			chip: function(v) { var n = devName(v); return h('span', { class: 'pr-ed-cn pr-ellip', title: n ? n + ' ' + v : v }, n ? [n + ' ', h('span', { class: 'pr-mono pr-dim' }, v)] : h('span', { class: 'pr-mono' }, v)); } });
		TB.src_ip = tagBox({ label: '源地址', placeholder: '如 192.168.1.10 或 192.168.1.0/24', max: VAL_MAX, parse: parseAddr, onChange: tbChange('src_ip') });
		TB.dst_ip = tagBox({ label: '目的地址', placeholder: '如 8.8.8.8、10.0.0.0/8 或 1.1.1.1-1.1.1.9', max: VAL_MAX, parse: parseAddr, onChange: tbChange('dst_ip') });
		TB.domain = tagBox({ label: '域名', placeholder: '如 example.com', max: VAL_MAX, parse: parseDomain, onChange: tbChange('domain') });
		TB.dport = tagBox({ label: '目的端口', placeholder: '如 443 或 8000-8100', max: PORT_MAX, parse: parsePort, onChange: tbChange('dport'), none: '未设置,不限目的端口' });
		TB.sport = tagBox({ label: '源端口', placeholder: '如 1024-65535', max: PORT_MAX, parse: parsePort, onChange: tbChange('sport'), none: '未设置,不限源端口' });
		DIM_KEYS.concat(['dport', 'sport']).forEach(function(k) { TB[k].set(init[k]); });

		/* 匹配方式 */
		matchRadios = radios('match', [['all', '满足全部'], ['any', '满足任一']], function(v) { S.match = v; changed('match'); });
		setRadio(matchRadios, S.match);
		FIELD_CTL.match = matchRadios[0].inp;
		matchHint = h('p', { class: 'pr-hint pr-ed-sh' });

		/* 协议 */
		protoSel = h('select', { class: 'pr-in pr-sel', id: fid('proto') }, PROTOS.map(function(p) { return h('option', { value: p[0] }, p[1]); }));
		protoSel.value = PROTO_LABEL[init.proto] ? init.proto : 'any';
		protoSel.addEventListener('change', function() { changed('proto'); });
		FIELD_CTL.proto = protoSel;
		E.portNote = h('p', { class: 'pr-hint pr-ed-dhint' }, '端口只对 TCP、UDP 有效,请先在左侧选择协议。已填的端口会保留,但保存前需要改回 TCP/UDP 或清空。');

		/* 出口与动作 */
		targetSel = h('select', { class: 'pr-in pr-sel', id: fid('target') });
		targetOptions(String(init.target || ''));
		targetSel.addEventListener('change', function() { changed('target'); });
		FIELD_CTL.target = targetSel;
		ofLegend = h('legend', { class: 'pr-lab' }, COPY.onFail.single);
		ofRadios = radios('of', [['default', COPY.onFail['default']], ['block', COPY.onFail.block]], function(v) { S.on_fail = v; changed('on_fail'); });
		setRadio(ofRadios, S.on_fail);
		FIELD_CTL.on_fail = ofRadios[0].inp;
		ofHint = h('p', { class: 'pr-hint pr-ed-dhint', hidden: true }, '选择阻断后,出口不可用期间匹配到的新连接会被拒绝,相关设备可能暂时无法访问。');
		E.v6 = note('info', 'info', COPY.ipv6);
		E.v6.hidden = true;

		E.top = h('div', { class: 'pr-ed-top', role: 'alert', hidden: true });
		E.pvS = h('p', { class: 'pr-ed-pv', 'aria-live': 'polite' });
		E.pvX = h('p', { class: 'pr-mono pr-dim pr-ed-px', hidden: true });
		E.pvN = h('div', { class: 'pr-ed-notes' });
		E.pvW = h('div', { class: 'pr-ed-notes', 'aria-live': 'polite', hidden: true });

		var dnsOff = caps().dns_sets === false, appOff = caps().apps === false;
		var dApp = dimCard('app', [COPY.app, '应用识别依赖行为管控的特征库;只有在识别出应用之后产生的新连接才会被改道。']);
		var dDev = dimCard('src_dev', ['按设备 MAC 匹配。设备经下级路由器转发(MAC 被替换)、或连接已被硬件加速绑定时,可能匹配不到。']);
		var dSrc = dimCard('src_ip', ['支持单个地址、网段(如 192.168.1.0/24)、区间(如 192.168.1.10-192.168.1.20)。地址写法支持 IPv4 和 IPv6,填入 IPv6 时会提示使用限制。']);
		var dDst = dimCard('dst_ip', ['写法同源地址。网段的主机位必须为 0,例如写 10.0.0.0/8,不要写 10.1.2.3/8。']);
		var dDom = dimCard('domain', [COPY.domain, '填写 example.com 会同时匹配它和它的全部子域。'], dnsOff ? note('warn', 'warn', '当前系统不支持域名匹配(dnsmasq 不支持 nftset,或全局设置里关闭了「域名解析集合」),含域名的规则无法保存。') : null);
		if (appOff) {
			TB.app.setDisabled(true);
		}
		var appExtra = appOff ? note('warn', 'warn', '未安装行为管控(mwrt-behav),应用维度不可用。') : null;
		if (appExtra) dApp.insertBefore(appExtra, dApp.children[1]);

		function fld(label, ctl, fkey, hint) {
			return h('div', { class: 'pr-field' }, h('label', { class: 'pr-lab', for: ctl.id }, label), ctl, hint ? h('div', { class: 'pr-hint' }, hint) : null, slot(fkey));
		}
		function seg(list, lab) { return h('div', { class: 'pr-seg', role: 'radiogroup', 'aria-label': lab }, list.map(function(r) { return h('label', null, r.inp, h('span', null, r.t)); })); }

		var form = h('div', { class: 'pr-ed-form' },
			E.top,
			mode === 'copy' ? note('info', 'info', '这是复制出来的规则,默认停用。保存前不会生效。') : null,
			sec('基本信息', null, [
				fld('规则名称', nameInp, 'name'),
				h('div', { class: 'pr-field' }, h('div', { class: 'pr-ed-sw' }, h('div', { class: 'pr-grow' }, h('div', { class: 'pr-lab' }, '启用此规则'), h('div', { class: 'pr-hint' }, '停用后规则保留在列表里,但不参与匹配。')), enSw)),
				fld('备注', noteTa, 'note')
			]),
			sec('匹配条件', '规则命中后,之后新建立的连接会走下面选定的出口;已建立的连接不会改道。', [
				h('div', { class: 'pr-field' }, h('div', { class: 'pr-lab', id: fid('mt') }, '多类条件如何组合'), seg(matchRadios, '条件组合方式'), matchHint, slot('match')),
				dApp, dDev, dSrc, dDst, dDom,
				h('section', { class: 'pr-ed-dim', 'aria-labelledby': fid('svc') },
					h('div', { class: 'pr-ed-dh' }, h('h4', { class: 'pr-ed-dt', id: fid('svc') }, '协议与端口'), h('span', { class: 'pr-sub' }, '始终与上面的条件同时满足')),
					h('div', { class: 'pr-ed-fg' },
						h('div', { class: 'pr-field' }, h('label', { class: 'pr-lab', for: fid('proto') }, '协议'), protoSel, slot('proto')),
						h('div', { class: 'pr-field' }, h('div', { class: 'pr-lab' }, '目的端口'), TB.dport.el),
						h('div', { class: 'pr-field' }, h('div', { class: 'pr-lab' }, '源端口'), TB.sport.el)),
					E.portNote,
					h('p', { class: 'pr-hint pr-ed-dhint' }, '端口写法:80 或 8000-8100,最多 ' + PORT_MAX + ' 项,同类多个端口是「或」。'))
			]),
			sec('走向', null, [
				h('div', { class: 'pr-field' }, h('label', { class: 'pr-lab', for: fid('target') }, '走哪个出口'), targetSel, slot('target'), E.v6),
				h('fieldset', { class: 'pr-ed-fs' }, ofLegend,
					ofRadios.map(function(r) { return h('label', { class: 'pr-chk' }, r.inp, h('span', null, r.t)); }),
					h('div', { class: 'pr-hint' }, COPY.failover), ofHint, slot('on_fail'))
			]),
			sec('预览', null, [
				h('div', { class: 'pr-ed-pvbox' }, E.pvS, E.pvX, E.pvN),
				E.pvW
			])
		);
		S.built = true;
		pub.body.textContent = '';
		pub.body.appendChild(form);
		pub.body.scrollTop = 0;
		[btnSave, btnApply].forEach(function(b) { b.disabled = false; });
		S.base = JSON.stringify(collect());
		renderSubs();
		renderPreview();
		if (mode !== 'new') scheduleCheck(true);
		if (caps().apps !== false && arr(init.app).length) loadApps().then(function() { if (!S.closed) { TB.app.refresh(); renderPreview(); } }, noop);
		if (arr(init.src_dev).length) loadClients().then(function() { if (!S.closed) { TB.src_dev.refresh(); renderPreview(); } }, noop);
		setTimeout(function() { if (!S.closed && nameInp.isConnected !== false) nameInp.focus(); }, 60);
	}

	/* ---- 保存 ---- */
	function setBusy(b, which) {
		S.busy = b;
		[btnSave, btnApply, btnCancel].forEach(function(x) { x.disabled = b; });
		[btnSave, btnApply].forEach(function(x) { x.classList.remove('pr-busy'); x.removeAttribute('aria-busy'); });
		if (b && which) { which.classList.add('pr-busy'); which.setAttribute('aria-busy', 'true'); }
	}
	function finish(v) { if (S.fin) return; S.fin = true; resolve(v); }
	function reportSaved(r, wantApply) {
		var a = r.applied && typeof r.applied === 'object' ? r.applied : {}, w = arr(r.warnings), wt = w.length ? '。注意:' + ((w[0] && w[0].msg) || w[0]) : '';
		if (!wantApply) { M.toast('规则已保存,将在 1 分钟内的下次对账自动生效,也可点右上角「应用」立即生效' + wt, w.length ? 'warn' : 'info'); return; }
		if (a.ok === false || r.applied === false) {
			M.pending.set(true);
			M.toast('规则已保存,但应用失败:' + (a.error || '引擎没有给出原因') + ' ' +
				(a.failed_open ? '恢复上一次配置也失败,已清除策略路由规则,流量按系统默认路由走。' : '已保持上一次有效配置。'), 'err');
			return;
		}
		if (a.disabled === true) { M.toast('规则已保存。策略路由总开关是关闭的,规则暂时不会生效' + wt, 'warn'); return; }
		M.toast('规则已保存并应用' + wt, w.length ? 'warn' : 'ok');
	}
	function save(wantApply, btn) {
		var p, sig, data, first;
		if (S.busy || !S.built) return;
		S.tried = true;
		if (!flushAll()) return;
		p = collect();
		sig = JSON.stringify(p);
		if (S.chk.sig === sig && S.chk.res && !S.chk.res.valid) {
			first = showErrors(S.chk.res.errors);
			if (first) focusField(first);
			M.toast('还有 ' + arr(S.chk.res.errors).length + ' 个问题需要先修改', 'warn');
			return;
		}
		data = { json: p };
		if (S.id) data.id = S.id;
		if (!wantApply) data.apply = 0;
		setBusy(true, btn);
		M.api('rule_save', data, { timeout: wantApply ? APPLY_MS : 20000 }).then(function(r) {
			setBusy(false);
			S.saved = true;
			M.dirty.set('pr-editor', false);
			reportSaved(r, wantApply);
			finish({ saved: true, id: r.id || S.id || undefined });
			pub.close();
		}, function(e) {
			setBusy(false);
			if (S.closed) return;
			if (e.errors && e.errors.length) {
				S.chk = { state: 'bad', sig: sig, res: { valid: false, errors: e.errors, warnings: [], shadow: [] }, err: '' };
				renderCheck();
				first = showErrors(e.errors);
				if (first) focusField(first);
			}
			M.toast(e.message + (e.kind !== 'server' ? ' 保存结果未确认,请先到规则列表核对,避免重复保存。' : ''), 'err');
		});
	}

	/* ---- 弹层与加载 ---- */
	function onClose() {
		S.closed = true;
		clearTimeout(S.timer);
		M.dirty.set('pr-editor', false);
		finish({ saved: false });
		if (CUR === S) CUR = null;
	}
	function ensureState() {
		return M.store.state ? Promise.resolve(M.store.state) : M.store.load();
	}
	function boot() {
		pub.body.textContent = '';
		pub.body.appendChild(ui.loading({ rows: 5, text: '正在读取规则…' }));
		[btnSave, btnApply].forEach(function(b) { b.disabled = true; });
		setNote('');
		ensureState().then(function() {
			if (S.closed) return undefined;
			if (mode === 'new') return null;
			return M.api('rule_get', { id: srcId }).then(function(r) { return r.rule || null; });
		}).then(function(rule) {
			if (S.closed || rule === undefined) return;
			if (mode !== 'new' && !rule) throw new Error('规则不存在或已被删除,请刷新');
			if (mode === 'edit') pub.setTitle('编辑策略规则', rule.name || '');
			if (mode === 'copy') pub.setTitle('复制策略规则', '基于「' + (rule.name || '未命名') + '」创建新规则');
			build(rule);
		}).catch(function(e) {
			if (S.closed) return;
			pub.body.textContent = '';
			pub.body.appendChild(ui.error({ title: '无法读取规则', message: msgOf(e), onRetry: boot }));
		});
	}

	noteEl = h('span', { class: 'pr-mf-note', 'aria-live': 'polite' });
	btnCancel = ui.btn({ kind: 'ghost', text: '取消', onClick: function() { pub.tryClose(); } });
	btnSave = ui.btn({ kind: 'ghost', text: '仅保存', title: '只保存,稍后点右上角「应用」才生效', disabled: true, onClick: function() { save(false, btnSave); } });
	btnApply = ui.btn({ kind: 'primary', text: '保存并应用', disabled: true, onClick: function() { save(true, btnApply); } });
	pub = M.modal({
		title: mode === 'edit' ? '编辑策略规则' : mode === 'copy' ? '复制策略规则' : '新建策略规则',
		drawer: true, guard: true, cls: 'pr-ed', focus: false,
		body: ui.loading({ rows: 5, text: '正在读取规则…' }),
		actions: [{ node: noteEl }, { node: btnCancel }, { node: btnSave }, { node: btnApply }],
		onClose: onClose
	});
	S.pub = pub;
	CUR = S;
	boot();
}

M.editor = {
	open: function(o) {
		o = o || {};
		return new Promise(function(resolve) {
			if (CUR && !CUR.closed) { resolve({ saved: false }); return; }
			try { openEditor({ id: o.id || null, copyFrom: o.copyFrom || null }, resolve); }
			catch (e) { resolve({ saved: false, error: msgOf(e) }); M.toast('规则编辑器打开失败:' + msgOf(e), 'err'); }
		});
	}
};
})();
