/*
 * Mwrt 策略路由前端内核:window.MwrtPbr
 *
 * page-*.js 只通过 MwrtPbr.pages.<name> = { title, render(root, ctx) -> {destroy()} } 注册,
 * 不直接操作壳和宿主页面 DOM。page.htm 输出 #mwrt-pbr-root 后调用 MwrtPbr.mount(el)。
 *
 * ctx 成员(与 window.MwrtPbr 同名): api h toast confirmBox modal dirty store picker dragSort icon fmt nav
 * 扩展成员: ui copy say apply pending hash setHash page
 *
 * 构建约定: 所有 DOM 由 h()/icon() 创建,文本只走 textContent;h() 拒绝 HTML 字符串。
 */
(function() {
'use strict';

if (window.MwrtPbr && window.MwrtPbr.__core) return;

var KNOWN_PAGES = { overview: '总览', rules: '策略规则', wans: 'WAN 出口', diag: '诊断' };
var NAR_W = 900;
var XS_W = 560;
var POLL_MS = 5000;
var PICK_CAP = 400;
var SVGNS = 'http://www.w3.org/2000/svg';
var FOCUSABLE = 'a[href],button:not([disabled]),input:not([disabled]):not([type=hidden]),select:not([disabled]),textarea:not([disabled]),[tabindex]:not([tabindex="-1"])';

var CFG = { page: '', api: '', menu: '', token: '', res: '/luci-static/resources/mwpbr/', v: '' };
var HOST = null;
var ROOT = null;
var EL = {};
var M = { __core: 1, pages: {} };

function noop() {}
function isNode(x) { return !!x && typeof x === 'object' && typeof x.nodeType === 'number'; }
function isPlain(x) { return !!x && typeof x === 'object' && !isNode(x) && !Array.isArray(x); }
function msgOf(e) { return e && e.message ? String(e.message) : String(e || '未知错误'); }
function logErr(e) { try { console.error(e); } catch (x) {} }
function activeEl() { return ROOT ? ROOT.activeElement : null; }
function focusEl(e) {
	if (!e) return;
	try { e.focus({ preventScroll: true }); } catch (x) { try { e.focus(); } catch (y) {} }
}
function focusables(root) {
	var l = root.querySelectorAll(FOCUSABLE), out = [], act = activeEl();
	for (var i = 0; i < l.length; i++) if (l[i].getClientRects().length || l[i] === act) out.push(l[i]);
	return out;
}
function normKind(k) {
	if (k === 'err' || k === 'bad' || k === 'error' || k === 'fail') return 'err';
	if (k === 'warn' || k === 'warning') return 'warn';
	if (k === 'ok' || k === 'success') return 'ok';
	if (k === 'info' || k === 'accent') return 'info';
	return 'off';
}
function isOff(v) { return v === false || v === 0 || v === '0'; }

/* ---------- h(): 创建元素 ---------- */
var BAN_ATTR = { innerHTML: 1, outerHTML: 1, html: 1, srcdoc: 1 };
var URL_ATTR = { href: 1, src: 1, action: 1, formaction: 1, 'xlink:href': 1 };
var PROP_ATTR = { value: 1, checked: 1, selected: 1, indeterminate: 1 };
var BOOL_ATTR = { disabled: 1, hidden: 1, readonly: 1, required: 1, multiple: 1, open: 1, autofocus: 1 };

function classText(v) {
	if (!v) return '';
	if (typeof v === 'string') return v;
	if (Array.isArray(v)) return v.map(classText).filter(Boolean).join(' ');
	if (typeof v === 'object') return Object.keys(v).filter(function(k) { return v[k]; }).join(' ');
	return String(v);
}
function append(el, k) {
	if (k == null || k === false || k === true) return;
	if (Array.isArray(k)) { for (var i = 0; i < k.length; i++) append(el, k[i]); return; }
	if (isNode(k)) { el.appendChild(k); return; }
	el.appendChild(document.createTextNode(String(k)));
}
function h(tag, attrs) {
	var el = document.createElement(tag);
	var kids = Array.prototype.slice.call(arguments, 2);
	var props = null, refs = null, k, v;
	if (attrs != null && !isPlain(attrs)) { kids.unshift(attrs); attrs = null; }
	if (attrs) for (k in attrs) {
		if (!Object.prototype.hasOwnProperty.call(attrs, k)) continue;
		if (BAN_ATTR[k]) throw new Error('h(): 不接受 HTML 字符串(' + k + ')');
		v = attrs[k];
		if (v == null) continue;
		if (k.slice(0, 2) === 'on') {
			if (typeof v !== 'function') throw new Error('h(): ' + k + ' 必须是函数');
			el.addEventListener(k.slice(2).toLowerCase(), v);
		} else if (k === 'class' || k === 'className') {
			var c = classText(v);
			if (c) el.setAttribute('class', c);
		} else if (k === 'dataset') {
			for (var d in v) if (v[d] != null && v[d] !== false) el.dataset[d] = String(v[d]);
		} else if (k === 'style') {
			if (typeof v === 'string') el.style.cssText = v;
			else for (var s in v) if (v[s] != null) {
				if (s.indexOf('-') >= 0) el.style.setProperty(s, String(v[s])); else el.style[s] = String(v[s]);
			}
		} else if (k === 'ref') {
			(refs = refs || []).push(v);
		} else if (PROP_ATTR[k]) {
			(props = props || []).push([k, v]);
		} else if (BOOL_ATTR[k]) {
			if (v) el.setAttribute(k, '');
		} else if (URL_ATTR[k]) {
			if (!/^\s*(javascript|vbscript|data):/i.test(String(v))) el.setAttribute(k, String(v));
		} else {
			var isA = k.slice(0, 5) === 'aria-' || k.slice(0, 5) === 'data-';
			if (v === true) el.setAttribute(k, isA ? 'true' : '');
			else if (v === false) { if (isA) el.setAttribute(k, 'false'); }
			else el.setAttribute(k, String(v));
		}
	}
	append(el, kids);
	if (props) for (var i = 0; i < props.length; i++) {
		if (props[i][0] === 'value') el.value = String(props[i][1]);
		else el[props[i][0]] = !!props[i][1];
	}
	if (refs) for (var j = 0; j < refs.length; j++) refs[j](el);
	return el;
}

/* ---------- icon(): 内联 SVG(24x24,描边) ---------- */
/* 'c:cx,cy,r' 描边圆;'f:cx,cy,r' 实心圆;'r:x,y,w,h,rx' 矩形;其余为 path d */
var ICONS = {
	'dot': ['f:12,12,5'],
	'grip': ['f:9,6,1.5', 'f:15,6,1.5', 'f:9,12,1.5', 'f:15,12,1.5', 'f:9,18,1.5', 'f:15,18,1.5'],
	'more': ['f:5,12,1.5', 'f:12,12,1.5', 'f:19,12,1.5'],
	'arrow-up': ['M12 19V5', 'M6 11l6-6 6 6'],
	'arrow-down': ['M12 5v14', 'M18 13l-6 6-6-6'],
	'arrow-right': ['M5 12h14', 'M13 6l6 6-6 6'],
	'chevron-up': ['M6 15l6-6 6 6'],
	'chevron-down': ['M6 9l6 6 6-6'],
	'chevron-left': ['M15 6l-6 6 6 6'],
	'chevron-right': ['M9 6l6 6-6 6'],
	'copy': ['r:9,9,11,11,2', 'M5 15V6a2 2 0 0 1 2-2h9'],
	'trash': ['M4 7h16', 'M10 11v6', 'M14 11v6', 'M6 7l1 12a2 2 0 0 0 2 2h6a2 2 0 0 0 2-2l1-12', 'M9 7V4h6v3'],
	'edit': ['M4 20h4L19 9a2.83 2.83 0 0 0-4-4L4 16v4z', 'M13.5 6.5l4 4'],
	'warn': ['M10.3 3.9L1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z', 'M12 9v4', 'M12 17h.01'],
	'info': ['c:12,12,9', 'M12 11v5', 'M12 8h.01'],
	'check': ['M5 12.5l4.5 4.5L19 7.5'],
	'check-circle': ['c:12,12,9', 'M8 12.5l3 3 5-6'],
	'x-circle': ['c:12,12,9', 'M9 9l6 6', 'M15 9l-6 6'],
	'close': ['M6 6l12 12', 'M18 6L6 18'],
	'plus': ['M12 5v14', 'M5 12h14'],
	'minus': ['M5 12h14'],
	'search': ['c:11,11,7', 'M20 20l-4-4'],
	'refresh': ['M23 4v6h-6', 'M20.49 15a9 9 0 1 1-2.12-9.36L23 10'],
	'play': ['M7 5l12 7-12 7z'],
	'power': ['M12 3v9', 'M6.4 6.6a8 8 0 1 0 11.2 0'],
	'globe': ['c:12,12,9', 'M3 12h18', 'M12 3c2.8 3 2.8 15 0 18', 'M12 3c-2.8 3-2.8 15 0 18'],
	'external': ['M14 4h6v6', 'M20 4l-9 9', 'M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5'],
	'spinner': ['M12 3a9 9 0 1 0 9 9']
};
function svgNode(tag, attrs) {
	var n = document.createElementNS(SVGNS, tag);
	for (var k in attrs) n.setAttribute(k, attrs[k]);
	return n;
}
function icon(name, cls) {
	var svg = svgNode('svg', {
		'viewBox': '0 0 24 24', 'fill': 'none', 'stroke': 'currentColor', 'stroke-width': '1.8',
		'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'aria-hidden': 'true', 'focusable': 'false',
		'class': 'pr-ic' + (cls ? ' ' + cls : '')
	});
	var parts = ICONS[name] || [];
	for (var i = 0; i < parts.length; i++) {
		var p = parts[i], a;
		if (p.charAt(1) === ':') {
			a = p.slice(2).split(',');
			if (p.charAt(0) === 'c') svg.appendChild(svgNode('circle', { cx: a[0], cy: a[1], r: a[2] }));
			else if (p.charAt(0) === 'f') svg.appendChild(svgNode('circle', { cx: a[0], cy: a[1], r: a[2], fill: 'currentColor', stroke: 'none' }));
			else svg.appendChild(svgNode('rect', { x: a[0], y: a[1], width: a[2], height: a[3], rx: a[4] }));
		} else svg.appendChild(svgNode('path', { d: p }));
	}
	return svg;
}

/* ---------- 固定文案(SPEC §9.4,页面直接引用,不要改写) ---------- */
var COPY = {
	onFail: {
		single: '该出口不可用时:',
		group: '组内按顺序切换到备用出口;全部不可用时:',
		'default': '回落到系统默认路由',
		block: '阻断(不回落)'
	},
	app: '首个连接不会改道;识别出该应用后,之后到相同目的地的新连接才走所选出口。',
	domain: '按客户端经本路由器 DNS 解析出的 IP 匹配,含全部子域;不支持仅精确主机名;DoH/DoT/自带 DNS、ECH 的客户端可能绕过;不匹配 HTTPS 路径。',
	ipv6: 'IPv6 只在发放 LAN 前缀的出口可用;指向其它出口的规则会让 IPv6 流量不可达,客户端自动改用 IPv4。',
	failover: '仅影响之后的新连接,已建立的连接不会迁移。',
	offload: '硬件加速(HNAT)已绑定的连接不受新规则影响;可在诊断页清除加速表。',
	probe: '仅检测 IPv4 连通性。'
};

/* ---------- 未应用标记(纯前端;sessionStorage 让同一标签页内跨页面保持) ---------- */
var PEND = { on: false, subs: [] };
try { PEND.on = sessionStorage.getItem('mwpbr.pending') === '1'; } catch (e) {}
var pending = {
	has: function() { return PEND.on; },
	set: function(v) {
		v = !!v;
		if (v === PEND.on) return;
		PEND.on = v;
		try { if (v) sessionStorage.setItem('mwpbr.pending', '1'); else sessionStorage.removeItem('mwpbr.pending'); } catch (e) {}
		PEND.subs.slice().forEach(function(f) { try { f(v); } catch (e) { logErr(e); } });
	},
	on: function(fn) {
		PEND.subs.push(fn);
		return function() { var i = PEND.subs.indexOf(fn); if (i >= 0) PEND.subs.splice(i, 1); };
	}
};

/* ---------- api() ---------- */
var TIMEOUT = { apply: 60000, diag: 40000, trace: 30000 };
var WRITE_FN = { rule_save: 1, rule_del: 1, rule_toggle: 1, rule_copy: 1, rule_order: 1, group_save: 1, group_del: 1, wan_save: 1, wan_order: 1, wan_mode: 1, global_save: 1 };

function trimSlash(s) { return String(s || '').replace(/\/+$/, ''); }
function apiUrl(fn) {
	var base = trimSlash(CFG.api);
	if (!base) base = (window.L && window.L.url) ? trimSlash(window.L.url('admin/services/mwpbr')) : '/cgi-bin/luci/admin/services/mwpbr';
	return base + '/' + fn;
}
function mkErr(kind, msg, extra) {
	var e = new Error(msg);
	e.kind = kind;
	if (extra) for (var k in extra) e[k] = extra[k];
	return e;
}
function httpErr(status) {
	if (status === 401 || status === 403) return mkErr('auth', '登录已过期或令牌失效,请刷新页面并重新登录', { status: status });
	if (status === 404) return mkErr('http', '接口不存在(HTTP 404),固件中的页面与后端版本可能不一致', { status: status });
	if (status >= 500) return mkErr('http', '路由器内部错误(HTTP ' + status + ')', { status: status });
	return mkErr('http', '服务返回错误(HTTP ' + status + ')', { status: status });
}
function jsonErr(text, status) {
	if (/^\s*</.test(text)) return mkErr('json', '收到的不是接口数据,登录可能已过期,请刷新页面后重试', { status: status });
	return mkErr('json', '服务返回的内容无法解析', { status: status });
}
/* 写类接口:控制器的 applied 恒为对象({ok,...}),带它说明请求已尝试应用;apply=0 或 applied.ok===false 才有未应用更改。
 * 不带 applied 又不是 apply=0 的写接口(如只改别名)没动配置的应用状态,保持原标记不动 */
function trackPending(fn, data, j) {
	if (fn === 'apply') { pending.set(false); return; }
	if (!WRITE_FN[fn]) return;
	var a = data ? data.apply : undefined;
	if (a === 0 || a === '0' || a === false || (isPlain(j.applied) && j.applied.ok === false)) pending.set(true);
	else if (isPlain(j.applied)) pending.set(false);
}
function api(fn, data, opts) {
	opts = opts || {};
	var body = new URLSearchParams();
	body.append('token', CFG.token || (window.L && window.L.env && window.L.env.token) || '');
	if (data) for (var k in data) {
		if (!Object.prototype.hasOwnProperty.call(data, k)) continue;
		var v = data[k];
		if (v == null) continue;
		if (v === true) v = '1'; else if (v === false) v = '0';
		else if (typeof v === 'object') v = JSON.stringify(v);
		body.append(k, String(v));
	}
	var ms = opts.timeout || TIMEOUT[fn] || 20000;
	var ctl = window.AbortController ? new AbortController() : null;
	var timedOut = false;
	var timer = setTimeout(function() { timedOut = true; if (ctl) ctl.abort(); }, ms);
	var req = { method: 'POST', body: body, credentials: 'same-origin', cache: 'no-store' };
	if (ctl) req.signal = ctl.signal;
	return fetch(apiUrl(fn), req).then(function(r) {
		return r.text().then(function(t) { return { r: r, t: t }; });
	}).then(function(x) {
		clearTimeout(timer);
		var j = null;
		if (!x.r.ok) throw httpErr(x.r.status);
		try { j = JSON.parse(x.t); } catch (e) { throw jsonErr(x.t, x.r.status); }
		if (!isPlain(j)) throw mkErr('json', '服务返回的内容格式不正确', { status: x.r.status });
		if (j.ok === false) {
			throw mkErr('server', j.error ? String(j.error) : '操作失败', {
				data: j, status: x.r.status,
				errors: Array.isArray(j.errors) ? j.errors : null,
				in_use: Array.isArray(j.in_use) ? j.in_use : null
			});
		}
		trackPending(fn, data, j);
		return j;
	}, function() {
		clearTimeout(timer);
		if (timedOut) {
			throw mkErr('timeout', '请求超时(' + Math.round(ms / 1000) + ' 秒内路由器没有响应)' + (fn === 'apply' ? ',应用可能仍在进行,请稍后刷新页面查看结果' : ''));
		}
		throw mkErr('network', '无法连接路由器,请检查网络连接后重试');
	});
}

/* ---------- store: 缓存 state 并定时刷新 ---------- */
var ST = { state: null, error: null, at: 0, loading: null, next: null, timer: 0, holds: 0, deferred: false, subs: {}, fails: 0, started: false, vis: false };

function emit(evt, arg) {
	var l = ST.subs[evt];
	if (!l) return;
	l.slice().forEach(function(f) { try { f(arg, ST.state); } catch (e) { logErr(e); } });
}
function serverNow() {
	if (ST.state && ST.state.ts) return Number(ST.state.ts) + (Date.now() - ST.at) / 1000;
	return Date.now() / 1000;
}
function fetchState() {
	var p = api('state').then(function(j) {
		var recovered = !!ST.error;
		ST.state = j; ST.error = null; ST.at = Date.now(); ST.fails = 0;
		if (ST.holds) ST.deferred = true; else emit('change', j);
		if (recovered) emit('recover', j);
		return j;
	}, function(e) {
		ST.error = e; ST.fails++;
		emit('error', e);
		throw e;
	});
	ST.loading = p;
	var clear = function() { if (ST.loading === p) ST.loading = null; };
	p.then(clear, clear);
	return p;
}
function runNext() { ST.next = null; return fetchState(); }
function storeLoad(force) {
	if (!force && ST.state && Date.now() - ST.at < 1500) return Promise.resolve(ST.state);
	if (ST.loading) {
		if (!force) return ST.loading;
		if (!ST.next) ST.next = ST.loading.then(runNext, runNext);
		return ST.next;
	}
	return fetchState();
}
function pollDelay() { return ST.fails ? Math.min(30000, POLL_MS * Math.pow(2, Math.min(ST.fails, 3))) : POLL_MS; }
function schedule(ms) {
	clearTimeout(ST.timer);
	ST.timer = 0;
	if (!ST.started || document.hidden) return;
	ST.timer = setTimeout(tick, ms == null ? pollDelay() : ms);
}
function tick() {
	ST.timer = 0;
	if (!ST.started || document.hidden) return;
	var done = function() { schedule(); };
	storeLoad(true).then(done, done);
}
function onVisibility() {
	if (!ST.started) return;
	if (document.hidden) { clearTimeout(ST.timer); ST.timer = 0; return; }
	var age = Date.now() - ST.at;
	if (age >= POLL_MS) tick(); else schedule(POLL_MS - age);
}
var store = {
	load: storeLoad,
	on: function(evt, fn) {
		(ST.subs[evt] = ST.subs[evt] || []).push(fn);
		return function() { var l = ST.subs[evt], i = l ? l.indexOf(fn) : -1; if (i >= 0) l.splice(i, 1); };
	},
	start: function() {
		if (ST.started) return;
		ST.started = true;
		if (!ST.vis) { ST.vis = true; document.addEventListener('visibilitychange', onVisibility); }
		var done = function() { schedule(); };
		storeLoad().then(done, done);
	},
	stop: function() { ST.started = false; clearTimeout(ST.timer); ST.timer = 0; },
	hold: function() {
		ST.holds++;
		var rel = false;
		return function() {
			if (rel) return;
			rel = true;
			ST.holds--;
			if (!ST.holds && ST.deferred) { ST.deferred = false; emit('change', ST.state); }
		};
	},
	now: serverNow
};
Object.defineProperty(store, 'state', { get: function() { return ST.state; }, enumerable: true });
Object.defineProperty(store, 'error', { get: function() { return ST.error; }, enumerable: true });
Object.defineProperty(store, 'loadedAt', { get: function() { return ST.at; }, enumerable: true });

/* ---------- fmt ---------- */
var KIND_LABEL = { wired: '有线', cellular: '蜂窝', usbnic: 'USB 网卡', tether: '手机共享', wlbridge: '无线网桥', pppoe: 'PPPoE', tunnel: '隧道', wireless: '无线', other: '其他' };
function pad2(n) { return n < 10 ? '0' + n : String(n); }
var fmt = {
	bytes: function(n) {
		n = Number(n);
		if (!isFinite(n) || n < 0) return '—';
		if (n < 1024) return Math.round(n) + ' B';
		var u = ['KB', 'MB', 'GB', 'TB', 'PB'], i = -1;
		do { n /= 1024; i++; } while (n >= 1024 && i < u.length - 1);
		return (n >= 100 ? n.toFixed(0) : n >= 10 ? n.toFixed(1) : n.toFixed(2)) + ' ' + u[i];
	},
	pkts: function(n) {
		n = Number(n);
		if (!isFinite(n) || n < 0) return '—';
		if (n < 10000) return String(Math.round(n));
		if (n < 1e8) return (n / 1e4).toFixed(1) + ' 万';
		return (n / 1e8).toFixed(2) + ' 亿';
	},
	since: function(ts, now) {
		ts = Number(ts);
		if (!ts) return '从未';
		var d = Math.max(0, (now != null ? now : serverNow()) - ts);
		if (d < 5) return '刚刚';
		if (d < 60) return Math.floor(d) + ' 秒前';
		if (d < 3600) return Math.floor(d / 60) + ' 分钟前';
		if (d < 86400) return Math.floor(d / 3600) + ' 小时前';
		return Math.floor(d / 86400) + ' 天前';
	},
	time: function(ts) {
		ts = Number(ts);
		if (!ts) return '—';
		var d = new Date(ts * 1000);
		return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate()) + ' ' + pad2(d.getHours()) + ':' + pad2(d.getMinutes()) + ':' + pad2(d.getSeconds());
	},
	rtt: function(ms) {
		if (ms == null || ms === '') return '—';
		ms = Number(ms);
		if (!isFinite(ms) || ms < 0) return '—';
		return ms < 1 ? '<1 ms' : Math.round(ms) + ' ms';
	},
	mac: function(s) {
		var t = String(s == null ? '' : s), x = t.replace(/[^0-9a-fA-F]/g, '');
		if (x.length !== 12 || /[^0-9a-fA-F:.\- ]/.test(t)) return t;
		return x.toUpperCase().match(/.{2}/g).join(':');
	},
	kind_label: function(kind) { return KIND_LABEL[kind] || '其他'; }
};

/* ---------- 脏数据守卫 ---------- */
var DIRTY = { keys: {}, p: null };
var dirty = {
	set: function(key, v) { if (v) DIRTY.keys[key] = 1; else delete DIRTY.keys[key]; },
	any: function() { return Object.keys(DIRTY.keys).length > 0; },
	clear: function() { DIRTY.keys = {}; },
	/* 无未保存修改直接 true;否则确认放弃。确认后清空所有脏标记(避免 beforeunload 再弹一次),并调用 fn */
	guard: function(fn) {
		var run = function(ok) { if (ok && typeof fn === 'function') fn(); return ok; };
		if (!dirty.any()) return Promise.resolve(run(true));
		if (!DIRTY.p) {
			DIRTY.p = confirmBox({
				title: '放弃未保存的修改?',
				body: '你有修改还没有保存,现在离开会丢掉这些修改。',
				okText: '放弃修改', cancelText: '继续编辑', danger: true
			}).then(function(ok) { DIRTY.p = null; if (ok) dirty.clear(); return ok; });
		}
		return DIRTY.p.then(run);
	}
};

/* ---------- 弹层栈 ---------- */
var MS = [];
var MID = 0;
var DRAG = { active: null };

function ensureLockCss() {
	if (document.getElementById('mwpbr-lock-css')) return;
	var st = h('style', { id: 'mwpbr-lock-css' });
	st.textContent = 'html.pr-lock #mwrt-bottomnav{display:none!important}' +
		'@media (max-width:860px){html.pr-lock #mwrt-theme-fab:not(#mwrt-nope),html.pr-lock #mwrt-about-btn:not(#mwrt-nope){display:none!important}}';
	document.head.appendChild(st);
}
function placeModals() {
	if (!HOST) return;
	var m = document.querySelector('.main-right') || document.getElementById('maincontent');
	var l = m ? Math.round(m.getBoundingClientRect().left) : 0;
	HOST.style.setProperty('--pr-l', (l > 40 ? l : 0) + 'px');
}
function syncLock() {
	var has = MS.length > 0;
	document.documentElement.classList.toggle('pr-lock', has);
	if (EL.main) { if (has) EL.main.setAttribute('inert', ''); else EL.main.removeAttribute('inert'); }
}
function topModal() { return MS.length ? MS[MS.length - 1] : null; }

function fillBody(el, c) {
	if (c == null || c === false) return;
	if (typeof c === 'string' || typeof c === 'number') {
		String(c).split('\n').forEach(function(line) { if (line) el.appendChild(h('p', { class: 'pr-p' }, line)); });
		return;
	}
	append(el, c);
}
function focusFirst(m) {
	var f = m.md.querySelector('[data-autofocus]') || focusables(m.body)[0] || focusables(m.foot).pop() || m.md;
	focusEl(f);
}
function closeModal(m) {
	var i = MS.indexOf(m);
	if (i < 0) return;
	MS.splice(i, 1);
	m.closed = true;
	m.el.classList.remove('on');
	if (m.el.parentNode) m.el.parentNode.removeChild(m.el);
	syncLock();
	if (m.o.onClose) { try { m.o.onClose(); } catch (e) { logErr(e); } }
	var t = topModal();
	if (m.prev && m.prev.isConnected && (!t || t.el.contains(m.prev))) focusEl(m.prev);
	else if (t) focusEl(t.md);
}
function closeAllModals() { while (MS.length) closeModal(MS[MS.length - 1]); }
function tryClose(m) {
	if (!m || m.closed || m.closing) return Promise.resolve(false);
	m.closing = true;
	var chk = m.o.canClose ? m.o.canClose() : (m.o.guard ? dirty.guard() : true);
	return Promise.resolve(chk).then(function(ok) {
		m.closing = false;
		if (ok === false) return false;
		closeModal(m);
		return true;
	}, function() { m.closing = false; return false; });
}
function setBusy(b, on) {
	b.classList.toggle('pr-busy', !!on);
	if (on) b.setAttribute('aria-busy', 'true'); else b.removeAttribute('aria-busy');
}
function runAction(m, pub, a, b, ev) {
	if (b.disabled || b.getAttribute('aria-busy') === 'true') return;
	if (!a.onClick) { tryClose(m); return; }
	var r;
	try { r = a.onClick(pub, ev); } catch (e) { toast(msgOf(e), 'err'); return; }
	if (r && typeof r.then === 'function') {
		setBusy(b, true);
		r.then(function(v) {
			setBusy(b, false);
			if (v !== false && a.close !== false) closeModal(m);
		}, function(e) { setBusy(b, false); toast(msgOf(e), 'err'); });
	} else if (r !== false && a.close !== false) closeModal(m);
}
function setActions(m, pub, actions) {
	m.foot.textContent = '';
	(actions || []).forEach(function(a) {
		if (!a) return;
		if (a.node) { m.foot.appendChild(a.node); return; }
		var kind = a.kind || 'ghost';
		var b = h('button', {
			type: 'button', disabled: !!a.disabled, title: a.title,
			class: ['pr-btn', 'pr-' + kind, a.left ? 'pr-mf-l' : ''],
			dataset: { autofocus: a.autofocus ? '1' : null }
		}, a.icon ? icon(a.icon) : null, h('span', null, a.text || ''));
		b.addEventListener('click', function(ev) { runAction(m, pub, a, b, ev); });
		m.foot.appendChild(b);
	});
	m.foot.hidden = !m.foot.firstChild;
}
function modal(o) {
	if (!ROOT) throw new Error('MwrtPbr 尚未挂载');
	o = o || {};
	var tid = 'pr-t' + (++MID);
	var size = o.drawer ? 'dr' : (o.size || (o.wide ? 'lg' : 'md'));
	if (!/^(sm|md|lg|dr)$/.test(size)) size = 'md';
	var titleBox = h('div', { class: 'pr-mt' }, h('b', { id: tid }, o.title || ''), o.sub ? h('span', { class: 'pr-sub' }, o.sub) : null);
	var closeBtn = h('button', { type: 'button', class: 'pr-ib', 'aria-label': '关闭', title: '关闭' }, icon('close'));
	var body = h('div', { class: 'pr-mb' });
	var foot = h('footer', { class: 'pr-mf' });
	var md = h('div', { class: 'pr-md', role: o.alert ? 'alertdialog' : 'dialog', 'aria-modal': 'true', 'aria-labelledby': tid, tabindex: '-1' },
		h('header', { class: 'pr-mh' }, titleBox, closeBtn), body, foot);
	var ov = h('div', { class: ['pr-mo', 'pr-sz-' + size, o.cls] }, md);
	var m = { el: ov, md: md, body: body, foot: foot, o: o, prev: activeEl() || document.activeElement, closed: false, closing: false, down: false };
	var pub = {
		el: ov, md: md, body: body, foot: foot,
		close: function() { closeModal(m); },
		tryClose: function() { return tryClose(m); },
		setTitle: function(t, sub) {
			titleBox.textContent = '';
			titleBox.appendChild(h('b', { id: tid }, t || ''));
			if (sub) titleBox.appendChild(h('span', { class: 'pr-sub' }, sub));
		},
		setActions: function(list) { setActions(m, pub, list); }
	};
	closeBtn.addEventListener('click', function() { tryClose(m); });
	ov.addEventListener('mousedown', function(e) { m.down = (e.target === ov); });
	ov.addEventListener('mouseup', function(e) {
		if (m.down && e.target === ov && topModal() === m) tryClose(m);
		m.down = false;
	});
	fillBody(body, o.body);
	setActions(m, pub, o.actions);
	placeModals();
	MS.push(m);
	EL.ml.appendChild(ov);
	syncLock();
	requestAnimationFrame(function() {
		if (m.closed) return;
		ov.classList.add('on');
		if (o.focus !== false) focusFirst(m);
	});
	return pub;
}
function confirmBox(o) {
	o = o || {};
	return new Promise(function(resolve) {
		var done = false;
		modal({
			title: o.title || '请确认', body: o.body, size: 'sm', alert: true,
			onClose: function() { if (!done) { done = true; resolve(false); } },
			actions: [
				{ text: o.cancelText || '取消', kind: 'ghost', autofocus: !!o.danger },
				{ text: o.okText || '确定', kind: o.danger ? 'danger' : 'primary', autofocus: !o.danger, onClick: function() { done = true; resolve(true); } }
			]
		});
	});
}

/* ---------- toast 与读屏播报 ---------- */
var TOAST = { t: 0 };
function say(msg, assertive) {
	var l = assertive ? EL.liveA : EL.liveP;
	if (!l) return;
	l.textContent = '';
	setTimeout(function() { l.textContent = String(msg == null ? '' : msg); }, 30);
}
function toast(msg, kind) {
	kind = normKind(kind);
	if (kind === 'off') kind = 'info';
	var text = String(msg == null ? '' : msg);
	if (!EL.toast) { logErr(text); return; }
	EL.toast.className = 'pr-toast on pr-k-' + kind;
	EL.toast.textContent = '';
	EL.toast.appendChild(icon(kind === 'ok' ? 'check-circle' : kind === 'info' ? 'info' : kind === 'warn' ? 'warn' : 'x-circle'));
	EL.toast.appendChild(h('span', null, text));
	clearTimeout(TOAST.t);
	TOAST.t = setTimeout(function() { EL.toast.classList.remove('on'); }, Math.min(12000, (kind === 'err' ? 6500 : kind === 'warn' ? 5000 : 2800) + text.length * 30));
	say(text, kind === 'err');
}

/* ---------- ui: 通用小部件 ---------- */
function btn(o) {
	o = o || {};
	var kind = o.kind || 'ghost';
	var b = h('button', {
		type: o.type || 'button', disabled: !!o.disabled, title: o.title, 'aria-label': o.label,
		class: [kind === 'icon' ? 'pr-ib' : 'pr-btn', kind === 'icon' ? '' : 'pr-' + kind, o.size === 'sm' ? 'pr-sm' : '', o.cls]
	}, o.icon ? icon(o.icon) : null, o.text != null ? h('span', null, o.text) : null);
	if (o.onClick) b.addEventListener('click', o.onClick);
	return b;
}
function badge(text, kind) { return h('span', { class: 'pr-badge pr-k-' + normKind(kind) }, text); }
function dot(kind, label) {
	return h('span', { class: 'pr-st pr-k-' + normKind(kind) }, h('i', { class: 'pr-dot', 'aria-hidden': 'true' }), label == null ? null : h('span', null, label));
}
function grip(label) {
	return h('button', { type: 'button', class: 'pr-grip', title: '拖动排序', 'aria-label': label || '拖动排序,或按上下方向键调整顺序' }, icon('grip'));
}
function emptyBox(o) {
	o = o || {};
	return h('div', { class: ['pr-state', 'pr-empty', o.compact ? 'pr-sm' : ''] },
		o.icon === false ? null : h('div', { class: 'pr-state-ic' }, icon(o.icon || 'info')),
		h('b', { class: 'pr-state-t' }, o.title || '暂无内容'),
		o.body ? h('p', { class: 'pr-state-p' }, o.body) : null,
		o.actions && o.actions.length ? h('div', { class: 'pr-state-a' }, o.actions.map(btn)) : null);
}
function loadingBox(o) {
	o = o || {};
	var sk = [];
	for (var i = 0; i < (o.rows || 3); i++) sk.push(h('div', { class: 'pr-skel', style: { width: (100 - i * 12) + '%' } }));
	return h('div', { class: 'pr-state pr-loading', role: 'status', 'aria-busy': 'true' },
		h('span', { class: 'pr-sr-only' }, o.text || '加载中…'), h('div', { class: 'pr-skels', 'aria-hidden': 'true' }, sk));
}
function errorBox(o) {
	if (o instanceof Error) o = { message: o.message };
	o = o || {};
	return h('div', { class: ['pr-state', 'pr-err', o.compact ? 'pr-sm' : ''], role: 'alert' },
		h('div', { class: 'pr-state-ic' }, icon('x-circle')),
		h('b', { class: 'pr-state-t' }, o.title || '加载失败'),
		h('p', { class: 'pr-state-p' }, o.message || '请稍后重试'),
		o.onRetry ? h('div', { class: 'pr-state-a' }, btn({ text: o.retryText || '重试', icon: 'refresh', onClick: o.onRetry })) : null);
}
/* 开关: onChange(checked) 可返回 Promise,失败或返回 false 时自动还原 */
function toggle(o) {
	o = o || {};
	var inp = h('input', { type: 'checkbox', role: 'switch', class: 'pr-sw-i', checked: !!o.checked, disabled: !!o.disabled, 'aria-label': o.label });
	var lab = h('label', { class: ['pr-sw', o.cls], title: o.title }, inp, h('span', { class: 'pr-sw-t', 'aria-hidden': 'true' }));
	inp.addEventListener('change', function() {
		var want = inp.checked, r;
		if (!o.onChange) return;
		try { r = o.onChange(want, inp); } catch (e) { inp.checked = !want; toast(msgOf(e), 'err'); return; }
		if (r && typeof r.then === 'function') {
			lab.classList.add('pr-busy');
			r.then(function(v) { lab.classList.remove('pr-busy'); if (v === false) inp.checked = !want; },
				function(e) { lab.classList.remove('pr-busy'); inp.checked = !want; toast(msgOf(e), 'err'); });
		} else if (r === false) inp.checked = !want;
	});
	return lab;
}
var ui = { btn: btn, badge: badge, dot: dot, grip: grip, empty: emptyBox, loading: loadingBox, error: errorBox, toggle: toggle };

/* ---------- picker: 多选对象选择器(应用/设备) ---------- */
var PKID = 0;
function picker(o) {
	o = o || {};
	var multi = o.multi !== false;
	var max = o.max > 0 ? o.max : 0;
	var uid = ++PKID;
	var sel = {}, selOrder = [];
	(o.selected || []).forEach(function(id) {
		id = String(id);
		if (sel[id] || (!multi && selOrder.length)) return;
		sel[id] = 1; selOrder.push(id);
	});
	var P = { items: [], q: '', g: '', loaded: false, rows: [] };

	function normItems(list) {
		var seen = {}, out = [];
		(list || []).forEach(function(it) {
			if (!it || it.id == null) return;
			var id = String(it.id);
			if (seen[id]) return;
			seen[id] = 1;
			out.push({
				id: id, name: it.name != null ? String(it.name) : id, sub: it.sub != null ? String(it.sub) : '',
				group: it.group != null ? String(it.group) : '', disabled: !!it.disabled, badge: it.badge,
				keys: ((it.name || '') + ' ' + id + ' ' + (it.sub || '') + ' ' + (it.keys || '')).toLowerCase()
			});
		});
		selOrder.forEach(function(id) {
			if (!seen[id]) out.push({ id: id, name: id, sub: o.unknownText || '不在当前列表中', group: o.unknownGroup || '不在列表中', disabled: false, keys: id.toLowerCase() });
		});
		return out;
	}
	function count() { return selOrder.length; }
	function groupsOf(items) {
		var order = [], map = {}, none = [];
		items.forEach(function(it) {
			if (!it.group) { none.push(it); return; }
			if (!map[it.group]) { map[it.group] = []; order.push(it.group); }
			map[it.group].push(it);
		});
		var out = order.map(function(g) { return { name: g, items: map[g] }; });
		if (none.length) out.push({ name: out.length ? (o.noGroup || '其它') : '', items: none });
		return out;
	}

	var done = false, resolveFn;
	var result = new Promise(function(res) { resolveFn = res; });
	var list = h('div', { class: 'pr-pk-list' });
	var countEl = h('span', { class: 'pr-mf-note', 'aria-live': 'polite' });
	var barSlot = h('div', { class: 'pr-pk-bar' });
	var qInp = h('input', { type: 'search', class: 'pr-in', placeholder: o.searchText || '搜索名称或内容', 'aria-label': '搜索', autocomplete: 'off' });
	var gSel = null;
	var qTimer = 0;

	function refreshState() {
		var n = count();
		countEl.textContent = (n ? '已选 ' + n + ' 项' : '未选择') + (max ? '(最多 ' + max + ' 项)' : '');
		for (var i = 0; i < P.rows.length; i++) {
			var r = P.rows[i], on = !!sel[r.id];
			r.row.classList.toggle('on', on);
			r.inp.checked = on;
			r.inp.disabled = r.disabled || (multi && max > 0 && !on && n >= max);
		}
	}
	function setSel(id, on) {
		if (!multi) { sel = {}; selOrder = []; }
		if (on) {
			if (multi && max && count() >= max) { toast('最多只能选 ' + max + ' 项', 'warn'); refreshState(); return; }
			if (!sel[id]) { sel[id] = 1; selOrder.push(id); }
		} else if (sel[id]) {
			delete sel[id];
			selOrder.splice(selOrder.indexOf(id), 1);
		}
		refreshState();
	}
	function visible() {
		var q = P.q.toLowerCase();
		return P.items.filter(function(it) {
			if (P.g && it.group !== P.g) return false;
			return !q || it.keys.indexOf(q) >= 0;
		});
	}
	function renderList() {
		var shown = visible(), cut = shown.length > PICK_CAP, vis = cut ? shown.slice(0, PICK_CAP) : shown;
		list.textContent = '';
		P.rows = [];
		P.vis = vis;
		if (!vis.length) {
			list.appendChild(emptyBox({
				compact: true, icon: 'search',
				title: P.items.length ? '没有匹配的结果' : (o.emptyText || '没有可选项'),
				body: P.items.length ? '换个关键词,或切换分类试试。' : o.emptyHint
			}));
			refreshState();
			return;
		}
		groupsOf(vis).forEach(function(g) {
			if (g.name) list.appendChild(h('div', { class: 'pr-pk-gh' }, g.name, h('em', null, String(g.items.length))));
			g.items.forEach(function(it) {
				var inp = h('input', { type: multi ? 'checkbox' : 'radio', name: 'pr-pk-' + uid, checked: !!sel[it.id] });
				var row = h('label', { class: 'pr-pk-row' }, inp,
					h('span', { class: 'pr-pk-n' }, h('b', null, it.name), it.badge ? badge(it.badge, 'off') : null, it.sub ? h('small', null, it.sub) : null));
				inp.addEventListener('change', function() { setSel(it.id, inp.checked); });
				P.rows.push({ id: it.id, row: row, inp: inp, disabled: it.disabled });
				list.appendChild(row);
			});
		});
		if (cut) list.appendChild(h('div', { class: 'pr-pk-more' }, '仅显示前 ' + PICK_CAP + ' 项(共 ' + shown.length + ' 项),请输入关键词缩小范围'));
		refreshState();
	}
	function load() {
		list.textContent = '';
		list.appendChild(loadingBox({ rows: 4 }));
		var src = typeof o.items === 'function' ? o.items() : o.items;
		Promise.resolve(src).then(function(arr) {
			P.items = normItems(arr);
			P.loaded = true;
			var gs = [];
			P.items.forEach(function(it) { if (it.group && gs.indexOf(it.group) < 0) gs.push(it.group); });
			if (o.groupFilter !== false && gs.length > 1) {
				gSel = h('select', { class: 'pr-in pr-sel', 'aria-label': '按分类筛选' }, h('option', { value: '' }, o.groupLabel || '全部分类'),
					gs.map(function(g) { return h('option', { value: g }, g); }));
				gSel.addEventListener('change', function() { P.g = gSel.value; renderList(); });
				barSlot.appendChild(gSel);
			}
			renderList();
		}, function(e) {
			list.textContent = '';
			list.appendChild(errorBox({ message: msgOf(e), onRetry: load, compact: true }));
		});
	}
	function finish(val) { if (done) return; done = true; resolveFn(val); }

	qInp.addEventListener('input', function() {
		clearTimeout(qTimer);
		qTimer = setTimeout(function() { P.q = qInp.value.trim(); if (P.loaded) renderList(); }, 120);
	});
	qInp.addEventListener('keydown', function(e) {
		if (e.key === 'Escape' && qInp.value) { e.stopPropagation(); e.preventDefault(); qInp.value = ''; P.q = ''; if (P.loaded) renderList(); }
	});
	barSlot.appendChild(h('label', { class: 'pr-pk-q' }, icon('search'), qInp));

	var actions = [{ node: countEl }];
	if (multi) {
		actions.push({ text: '全选当前列表', kind: 'text', close: false, onClick: function() {
			(P.vis || []).forEach(function(it) {
				if (!sel[it.id] && !it.disabled && (!max || count() < max)) { sel[it.id] = 1; selOrder.push(it.id); }
			});
			refreshState();
			return false;
		} });
		actions.push({ text: '清除', kind: 'text', close: false, onClick: function() { sel = {}; selOrder = []; refreshState(); return false; } });
	}
	actions.push({ text: o.cancelText || '取消', kind: 'ghost' });
	actions.push({ text: o.okText || '确定', kind: 'primary', onClick: function() {
		finish(P.loaded ? P.items.filter(function(it) { return sel[it.id]; }).map(function(it) { return it.id; }) : selOrder.slice());
	} });
	modal({ title: o.title || '选择', sub: o.sub, size: 'lg', cls: 'pr-pk', body: [barSlot, list], actions: actions, onClose: function() { clearTimeout(qTimer); finish(null); } });
	refreshState();
	load();
	return result;
}

/* ---------- dragSort: 拖拽 + 键盘排序 ---------- */
function scrollerOf(el) {
	var n = el;
	while (n) {
		if (n.nodeType === 1) {
			var oy = getComputedStyle(n).overflowY;
			if ((oy === 'auto' || oy === 'scroll') && n.scrollHeight > n.clientHeight + 2) return n;
		}
		n = n.parentNode || n.host;
	}
	return document.scrollingElement || document.documentElement;
}
/* listEl 的直接子元素带 data-id 即为一项;onOrder(ids) 返回 Promise 时串行执行,失败回滚并重载 state */
function dragSort(listEl, opts) {
	opts = opts || {};
	var handleSel = opts.handle || '.pr-grip';
	var S = { chain: Promise.resolve(), pending: 0, epoch: 0, base: null, release: null, reload: false, dead: false };
	var D = null;

	function rows() {
		var out = [], c = listEl.children;
		for (var i = 0; i < c.length; i++) if (c[i].hasAttribute('data-id')) out.push(c[i]);
		return out;
	}
	function ids() { return rows().map(function(r) { return r.getAttribute('data-id'); }); }
	function rowOf(t) {
		var n = t;
		while (n && n.parentNode !== listEl) n = n.parentNode;
		return n && n.nodeType === 1 && n.hasAttribute('data-id') ? n : null;
	}
	function nameOf(row) { return row.getAttribute('data-name') || (opts.name ? opts.name(row) : '') || '该项'; }
	function blocked() {
		var b = typeof opts.disabled === 'function' ? opts.disabled() : !!opts.disabled;
		if (b) toast(opts.disabledMsg || '当前状态下不能调整顺序', 'warn');
		return b;
	}
	function restore(order) {
		var cur = rows();
		if (!cur.length) return;
		var ref = cur[cur.length - 1].nextSibling, byId = {};
		cur.forEach(function(r) { byId[r.getAttribute('data-id')] = r; });
		order.forEach(function(id) { if (byId[id]) { listEl.insertBefore(byId[id], ref); delete byId[id]; } });
		for (var k in byId) listEl.insertBefore(byId[k], ref);
	}
	function flash(row) {
		row.classList.add('pr-flash');
		setTimeout(function() { row.classList.remove('pr-flash'); }, 1200);
	}
	function drain() {
		S.pending--;
		if (S.pending) return;
		var r = S.release, again = S.reload;
		S.release = null; S.reload = false;
		if (r) r();
		if (again || opts.reload !== false) store.load(true).catch(noop);
	}
	function commit(order, row) {
		var ep = S.epoch;
		S.pending++;
		if (!S.release) S.release = store.hold();
		say('「' + nameOf(row) + '」已移到第 ' + (order.indexOf(row.getAttribute('data-id')) + 1) + ' 位,共 ' + order.length + ' 项');
		S.chain = S.chain.then(function() {
			if (ep !== S.epoch) return null;
			return Promise.resolve(opts.onOrder ? opts.onOrder(order) : null).then(function() {
				S.base = order;
			}, function(err) {
				S.epoch++;
				S.reload = true;
				if (S.base) restore(S.base);
				toast('调整顺序失败:' + msgOf(err), 'err');
				if (opts.onFail) { try { opts.onFail(err); } catch (e) { logErr(e); } }
			});
		}).then(drain, drain);
	}

	/* 指针拖动: 监听挂在 window 上,因为移动时行会被摘下再插入,元素上的指针捕获会丢 */
	function reposition() {
		if (!D || !D.moving) return;
		var others = rows().filter(function(r) { return r !== D.row; }), k = 0;
		for (var i = 0; i < others.length; i++) {
			var b = others[i].getBoundingClientRect();
			if (b.top + b.height / 2 < D.y) k++; else break;
		}
		if (rows().indexOf(D.row) !== k) listEl.insertBefore(D.row, others[k] || null);
	}
	function loop() {
		if (!D || !D.moving) return;
		var sc = D.sc, top, bot, dy = 0;
		if (sc === document.scrollingElement || sc === document.documentElement || sc === document.body) { top = 0; bot = window.innerHeight; }
		else { var r = sc.getBoundingClientRect(); top = r.top; bot = r.bottom; }
		if (D.y < top + 70) dy = -Math.ceil((top + 70 - D.y) / 5);
		else if (D.y > bot - 70) dy = Math.ceil((D.y - (bot - 70)) / 5);
		if (dy) { sc.scrollTop += Math.max(-24, Math.min(24, dy)); reposition(); }
		D.raf = requestAnimationFrame(loop);
	}
	function onMove(e) {
		if (!D || e.pointerId !== D.pid) return;
		D.y = e.clientY;
		if (!D.moving) {
			if (Math.abs(D.y - D.y0) < 5) return;
			D.moving = true;
			D.sc = scrollerOf(listEl);
			listEl.classList.add('pr-dragging');
			D.row.classList.add('pr-drag');
			DRAG.active = ctl;
			loop();
		}
		reposition();
	}
	function stop() {
		var d = D;
		if (!d) return null;
		cancelAnimationFrame(d.raf);
		window.removeEventListener('pointermove', onMove);
		window.removeEventListener('pointerup', onUp);
		window.removeEventListener('pointercancel', onCancel);
		listEl.classList.remove('pr-dragging');
		d.row.classList.remove('pr-drag');
		if (DRAG.active === ctl) DRAG.active = null;
		D = null;
		return d;
	}
	function onUp(e) {
		if (!D || (e && e.pointerId !== D.pid)) return;
		var d = stop(), order = ids();
		if (!d.moving) return;
		if (order.join('\n') === d.from.join('\n')) return;
		flash(d.row);
		commit(order, d.row);
	}
	function onCancel(e) {
		if (!D || (e && e.pointerId !== D.pid)) return;
		var d = stop();
		if (d && d.moving) restore(d.from);
	}
	function onDown(e) {
		var hd = e.target.closest ? e.target.closest(handleSel) : null;
		if (!hd || !listEl.contains(hd) || e.button > 0 || D) return;
		var row = rowOf(hd);
		if (!row) return;
		e.preventDefault();
		if (blocked()) return;
		if (!S.pending) S.base = ids();
		D = { row: row, y0: e.clientY, y: e.clientY, moving: false, pid: e.pointerId, from: ids(), sc: null, raf: 0 };
		window.addEventListener('pointermove', onMove);
		window.addEventListener('pointerup', onUp);
		window.addEventListener('pointercancel', onCancel);
	}
	function onKey(e) {
		var hd = e.target.closest ? e.target.closest(handleSel) : null;
		if (!hd || !listEl.contains(hd) || e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) return;
		var key = e.key;
		if (key !== 'ArrowUp' && key !== 'ArrowDown' && key !== 'Home' && key !== 'End') return;
		e.preventDefault();
		var row = rowOf(hd);
		if (!row || blocked()) return;
		var all = rows(), i = all.indexOf(row), j = key === 'ArrowUp' ? i - 1 : key === 'ArrowDown' ? i + 1 : key === 'Home' ? 0 : all.length - 1;
		if (j < 0 || j >= all.length || j === i) return;
		if (!S.pending) S.base = ids();
		listEl.insertBefore(row, j > i ? all[j].nextSibling : all[j]);
		focusEl(row.querySelector(handleSel));
		flash(row);
		commit(ids(), row);
	}
	var ctl = {
		cancel: function() { onCancel(null); },
		destroy: function() {
			if (S.dead) return;
			S.dead = true;
			ctl.cancel();
			listEl.removeEventListener('pointerdown', onDown);
			listEl.removeEventListener('keydown', onKey);
		}
	};
	listEl.addEventListener('pointerdown', onDown);
	listEl.addEventListener('keydown', onKey);
	return ctl;
}

/* ---------- 页内导航与 hash ---------- */
function hashQuery() {
	var s = String(location.hash || '').replace(/^#/, ''), out = {};
	if (!s) return out;
	s.split('&').forEach(function(p) {
		if (!p) return;
		var i = p.indexOf('='), k = i < 0 ? p : p.slice(0, i), v = i < 0 ? '' : p.slice(i + 1);
		try { k = decodeURIComponent(k); v = decodeURIComponent(v.replace(/\+/g, ' ')); } catch (e) {}
		out[k] = v;
	});
	return out;
}
/* 只改地址栏,不触发 hashchange、不重渲染;replace=true 不留历史记录 */
function setHash(query, replace) {
	var q = String(query == null ? '' : query).replace(/^#/, '');
	var url = location.pathname + location.search + (q ? '#' + q : '');
	if (url === location.pathname + location.search + location.hash) return;
	try { history[replace ? 'replaceState' : 'pushState'](null, '', url); } catch (e) { location.hash = q; }
}
function menuBase() {
	var b = trimSlash(CFG.menu);
	if (!b) b = (window.L && window.L.url) ? trimSlash(window.L.url('admin/services/oaf/policy_routing')) : '/cgi-bin/luci/admin/services/oaf/policy_routing';
	return b;
}
function nav(page, query) {
	return dirty.guard().then(function(ok) {
		if (!ok) return false;
		var q = String(query == null ? '' : query).replace(/^#/, '');
		if (!page || page === CFG.page) {
			closeAllModals();
			setHash(q, false);
			renderPage();
			return true;
		}
		if (!KNOWN_PAGES[page]) { toast('未知页面:' + page, 'err'); return false; }
		location.href = menuBase() + '/' + page + (q ? '#' + q : '');
		return true;
	});
}

/* ---------- 引擎状态与"应用" ---------- */
function engineView() {
	var s = ST.state, e = s && s.engine, g = s && s.global;
	if (!s) return ST.error ? { k: 'err', t: '加载失败', d: ST.error.message } : { k: 'off', t: '加载中', d: '' };
	if (ST.error && ST.fails >= 2) {
		return { k: 'warn', t: '连接中断', d: '无法刷新状态:' + ST.error.message + '。当前显示的是 ' + fmt.since(ST.at / 1000, Date.now() / 1000) + ' 的数据。' };
	}
	if (s.installed === false) return { k: 'off', t: '未安装', d: '' };
	if (!e) return { k: 'off', t: '状态未知', d: '' };
	if (e.failed_open) return { k: 'err', t: '规则已清除(故障保护)', d: (e.error ? e.error + ' ' : '') + '引擎应用失败且无法恢复上一次配置,已清除策略路由规则,流量按系统默认路由走。' };
	if (e.ok === false) return { k: 'err', t: '应用失败', d: (e.error ? e.error + ' ' : '') + '已保持上一次有效配置。' };
	if (g && isOff(g.enabled)) return { k: 'off', t: '已关闭', d: '策略路由总开关未开启,规则不会生效。' };
	if (!e.running) return { k: 'warn', t: '引擎未运行', d: '总开关已开启,但引擎服务没有运行。' };
	var d = '上次应用:' + fmt.since(e.applied_ts) + (e.rules_total != null ? ',共 ' + e.rules_total + ' 条规则' : '');
	if (e.rules_error > 0) return { k: 'warn', t: '运行中 · ' + e.rules_error + ' 条规则有错误', d: d };
	return { k: 'ok', t: '运行中' + (e.rules_active != null ? ' · ' + e.rules_active + ' 条生效' : ''), d: d };
}
function renderTop() {
	if (!EL.pill) return;
	var v = engineView(), had = pending.has(), busy = !!EL.applying;
	EL.pill.className = 'pr-pill pr-k-' + v.k;
	if (EL.pillT.textContent !== v.t) EL.pillT.textContent = v.t;
	EL.pill.title = v.d;
	EL.apply.classList.toggle('pr-hot', had && !busy);
	EL.apply.classList.toggle('pr-busy', busy);
	if (busy) EL.apply.setAttribute('aria-busy', 'true'); else EL.apply.removeAttribute('aria-busy');
	EL.apply.setAttribute('aria-label', busy ? '正在应用' : had ? '应用(有未应用的更改)' : '应用');
	EL.pend.hidden = !had || busy;
	EL.applyIc.textContent = '';
	EL.applyIc.appendChild(icon(busy ? 'spinner' : 'check', busy ? 'pr-spin' : ''));
	EL.applyT.textContent = busy ? '应用中…' : '应用';
}
function applyNow() {
	if (EL.applying) return EL.applying;
	var p = api('apply').then(function(j) {
		var n = j.rules_active, w = Array.isArray(j.warnings) ? j.warnings : [];
		var text = '已应用:' + (n != null ? n + ' 条规则生效' : '配置已生效') +
			(j.rules_error > 0 ? ',' + j.rules_error + ' 条规则有错误,请在规则页查看' : '') +
			(j.changed === false ? '(与上次相同,无变化)' : '');
		if (w.length) text += '。注意:' + w[0] + (w.length > 1 ? '(另有 ' + (w.length - 1) + ' 条提示)' : '');
		toast(text, w.length || j.rules_error > 0 ? 'warn' : 'ok');
		return true;
	}, function(e) {
		var d = e.data || {}, tail;
		if (e.kind !== 'server') tail = '应用结果未确认,请稍后刷新页面查看当前状态。';
		else if (d.failed_open) tail = '恢复上一次配置也失败,已清除策略路由规则,流量按系统默认路由走。';
		else tail = '已保持上一次有效配置。';
		toast('应用失败:' + e.message + ' ' + tail, 'err');
		return false;
	}).then(function(ok) {
		EL.applying = null;
		renderTop();
		storeLoad(true).catch(noop);
		return ok;
	});
	EL.applying = p;
	renderTop();
	return p;
}

/* ---------- 页面渲染 ---------- */
var PAGE = { inst: null, ctx: null, seq: 0 };

function makeCtx() {
	var scope = { offs: [], drags: [] };
	var st = Object.create(store);
	st.on = function(evt, fn) { var off = store.on(evt, fn); scope.offs.push(off); return off; };
	return {
		scope: scope, page: CFG.page,
		api: api, h: h, toast: toast, confirmBox: confirmBox, modal: modal, dirty: dirty, store: st, icon: icon, fmt: fmt,
		nav: nav, picker: picker,
		dragSort: function(el, o) { var d = dragSort(el, o); scope.drags.push(d); return d; },
		ui: ui, copy: COPY, say: say, apply: applyNow, pending: pending, hash: hashQuery, setHash: setHash
	};
}
function destroyPage() {
	PAGE.seq++;
	var inst = PAGE.inst, ctx = PAGE.ctx;
	PAGE.inst = null; PAGE.ctx = null;
	closeAllModals();
	if (DRAG.active) DRAG.active.cancel();
	if (inst && typeof inst.destroy === 'function') { try { inst.destroy(); } catch (e) { logErr(e); } }
	if (ctx) {
		ctx.scope.offs.forEach(function(f) { try { f(); } catch (e) { logErr(e); } });
		ctx.scope.drags.forEach(function(d) { try { d.destroy(); } catch (e) { logErr(e); } });
	}
}
function showPageError(title, msg) {
	EL.view.textContent = '';
	EL.view.appendChild(errorBox({ title: title, message: msg, onRetry: renderPage, retryText: '重新加载页面' }));
}
function renderPage() {
	destroyPage();
	var seq = PAGE.seq;
	var reg = M.pages[CFG.page];
	if (!reg || typeof reg.render !== 'function') {
		showPageError('页面脚本没有加载', '「' + (KNOWN_PAGES[CFG.page] || CFG.page) + '」页面的脚本没有注册,请刷新页面(Ctrl+F5)后重试。');
		return;
	}
	var content = h('div', { class: 'pr-pg pr-pg-' + CFG.page });
	EL.view.textContent = '';
	EL.view.appendChild(content);
	var ctx = makeCtx();
	PAGE.ctx = ctx;
	var r;
	try { r = reg.render(content, ctx); } catch (e) { logErr(e); showPageError('页面渲染出错', msgOf(e)); return; }
	if (r && typeof r.then === 'function') {
		r.then(function(inst) {
			if (seq !== PAGE.seq) { if (inst && inst.destroy) { try { inst.destroy(); } catch (e) { logErr(e); } } return; }
			PAGE.inst = inst || null;
		}, function(e) { if (seq === PAGE.seq) { logErr(e); showPageError('页面渲染出错', msgOf(e)); } });
	} else PAGE.inst = r || null;
}

/* ---------- 键盘 ---------- */
function onRootKey(e) {
	if (e.key !== 'Tab' || !MS.length) return;
	var m = topModal(), f = focusables(m.md), act = activeEl();
	if (!f.length) { e.preventDefault(); focusEl(m.md); return; }
	var first = f[0], last = f[f.length - 1];
	if (!m.md.contains(act)) { e.preventDefault(); focusEl(first); }
	else if (act === m.md) { e.preventDefault(); focusEl(e.shiftKey ? last : first); }
	else if (e.shiftKey && act === first) { e.preventDefault(); focusEl(last); }
	else if (!e.shiftKey && act === last) { e.preventDefault(); focusEl(first); }
}
function onDocKey(e) {
	if (e.key !== 'Escape' || e.defaultPrevented) return;
	if (DRAG.active) { e.preventDefault(); DRAG.active.cancel(); return; }
	if (MS.length) { e.preventDefault(); tryClose(topModal()); }
}

/* ---------- mount ---------- */
function mount(el, page) {
	if (!el || el.__mwpbr) return;
	el.__mwpbr = true;
	var ds = el.dataset || {};
	CFG.page = page || ds.page || 'overview';
	CFG.api = trimSlash(ds.api);
	CFG.menu = trimSlash(ds.menu);
	CFG.token = ds.token || '';
	CFG.res = (ds.res || CFG.res).replace(/\/*$/, '/');
	CFG.v = ds.v || '';
	if (!el.attachShadow) { el.textContent = '当前浏览器不支持本页面所需的特性,请更新浏览器后重试。'; return; }

	HOST = el;
	ROOT = el.attachShadow({ mode: 'open' });
	el.style.display = 'block';
	var lk = h('link', { rel: 'stylesheet', href: CFG.res + 'pr.css' + (CFG.v ? '?v=' + encodeURIComponent(CFG.v) : '') });
	el.style.visibility = 'hidden';
	var shown = false;
	function reveal() { if (shown) return; shown = true; el.style.visibility = ''; }
	lk.addEventListener('load', reveal);
	lk.addEventListener('error', reveal);
	setTimeout(reveal, 2500);
	ROOT.appendChild(lk);

	EL.title = h('h1', { class: 'pr-title', tabindex: '-1' }, (M.pages[CFG.page] && M.pages[CFG.page].title) || KNOWN_PAGES[CFG.page] || '策略路由');
	EL.pillT = h('span', { class: 'pr-pill-t' }, '加载中');
	EL.pill = h('span', { class: 'pr-pill pr-k-off', role: 'status' }, h('i', { class: 'pr-dot', 'aria-hidden': 'true' }), EL.pillT);
	EL.pend = h('span', { class: 'pr-pend', hidden: true }, '有未应用的更改');
	EL.applyIc = h('span', { class: 'pr-apply-ic' });
	EL.applyT = h('span', null, '应用');
	EL.apply = h('button', { type: 'button', class: 'pr-btn pr-apply', onclick: function() { applyNow(); } }, EL.applyIc, EL.applyT);
	EL.view = h('main', { class: 'pr-view' });
	EL.main = h('div', { class: 'pr-main' },
		h('header', { class: 'pr-top' }, EL.title, h('div', { class: 'pr-top-r' }, EL.pill, EL.pend, EL.apply)), EL.view);
	EL.ml = h('div', { class: 'pr-ml' });
	EL.toast = h('div', { class: 'pr-toast', 'aria-hidden': 'true' });
	EL.liveP = h('div', { 'aria-live': 'polite', 'aria-atomic': 'true' });
	EL.liveA = h('div', { 'aria-live': 'assertive', 'aria-atomic': 'true' });
	EL.app = h('div', { class: 'pr-app' }, EL.main, EL.ml, EL.toast, h('div', { class: 'pr-sr-only' }, EL.liveP, EL.liveA));
	ROOT.appendChild(EL.app);

	ensureLockCss();
	function syncDark() { el.classList.toggle('dark', document.documentElement.getAttribute('data-darkmode') === 'true'); }
	syncDark();
	new MutationObserver(syncDark).observe(document.documentElement, { attributes: true, attributeFilter: ['data-darkmode'] });
	function fit() {
		var w = EL.app.getBoundingClientRect().width;
		EL.app.classList.toggle('pr-nar', w > 0 && w < NAR_W);
		EL.app.classList.toggle('pr-xs', w > 0 && w < XS_W);
	}
	if (window.ResizeObserver) new ResizeObserver(fit).observe(EL.app); else window.addEventListener('resize', fit);
	fit();
	window.addEventListener('resize', function() { if (MS.length) placeModals(); });
	ROOT.addEventListener('keydown', onRootKey);
	document.addEventListener('keydown', onDocKey);
	window.addEventListener('beforeunload', function(e) {
		if (!dirty.any()) return undefined;
		e.preventDefault();
		e.returnValue = '';
		return '';
	});

	store.on('change', renderTop);
	store.on('error', renderTop);
	pending.on(renderTop);
	renderTop();
	renderPage();
	store.start();
}

M.api = api;
M.h = h;
M.toast = toast;
M.confirmBox = confirmBox;
M.modal = modal;
M.dirty = dirty;
M.store = store;
M.icon = icon;
M.fmt = fmt;
M.nav = nav;
M.dragSort = dragSort;
M.picker = picker;
M.mount = mount;
M.ui = ui;
M.copy = COPY;
M.say = say;
M.apply = applyNow;
M.pending = pending;
M.hash = hashQuery;
M.setHash = setHash;
M.editor = {
	placeholder: true,
	open: function() { return Promise.reject(new Error('规则编辑器脚本没有加载,请刷新页面后重试')); }
};
window.MwrtPbr = M;
})();
