/*
 * 策略规则列表页:优先级(拖拽/键盘排序)、条件摘要、出口与动作、命中包数、状态、启用开关、
 * 编辑/复制/删除、搜索与状态筛选、批量启用/停用,以及 #edit=<id|new> 深链。
 * 动态文本只经 ctx.h 与 textContent 写入。遮蔽提示是前端按范围做的估算,以编辑器里后端 rule_check 的结论为准。
 */
(function() {
'use strict';

var M = window.MwrtPbr;
if (!M || !M.pages) return;

var MAX_RULES = 128;
var APPLY_MS = 60000;
var SHOW_N = 2;
var SEARCH_MS = 120;
var DIM_KEYS = ['app', 'src_dev', 'src_ip', 'dst_ip', 'domain'];
var DIM_LABEL = { app: '应用', src_dev: '设备', src_ip: '源地址', dst_ip: '目的地址', domain: '域名' };
var PROTO_LABEL = { any: '不限', tcp: 'TCP', udp: 'UDP', tcp_udp: 'TCP + UDP', icmp: 'ICMP' };
var SUM_KEYS = ['active', 'error', 'pending', 'disabled', 'idle'];
var SUM_LABEL = { active: '生效', error: '有错误', pending: '尚未应用', disabled: '已停用', idle: '未运行' };
var FILTERS = [['all', '全部状态'], ['active', '生效中'], ['error', '有错误'], ['pending', '尚未应用'], ['disabled', '已停用'], ['idle', '未运行'], ['shadow', '可能被遮蔽']];

function noop() {}
function arr(x) { return Array.isArray(x) ? x : []; }
function obj(x) { return x && typeof x === 'object' && !Array.isArray(x) ? x : {}; }
function msgOf(e) { return e && e.message ? String(e.message) : String(e || '未知错误'); }
function byKey(list, key, val) {
	for (var i = 0; i < list.length; i++) if (list[i] && list[i][key] === val) return list[i];
	return null;
}
function wanTitle(w) { return w.alias || w.name || w.wan_id || '未命名出口'; }
function splitTarget(t) {
	t = String(t || '');
	var i = t.indexOf(':');
	return i < 0 ? { type: '', id: t } : { type: t.slice(0, i), id: t.slice(i + 1) };
}
function ruleName(r) { return r.name || r.id || '未命名规则'; }

/* ---------- 遮蔽估算(保守:只在能确定"包含"时才提示) ---------- */
function effAll(r) {
	var n = 0;
	DIM_KEYS.forEach(function(k) { if (arr(r[k]).length) n++; });
	return r.match !== 'any' || n <= 1;
}
function portRange(p) {
	var m = String(p).split('-');
	return [Number(m[0]), Number(m[m.length - 1])];
}
function portsCover(a, b) {
	if (!a.length) return true;
	if (!b.length) return false;
	return b.map(portRange).every(function(x) {
		return a.map(portRange).some(function(y) { return y[0] <= x[0] && x[1] <= y[1]; });
	});
}
function protoCovers(a, b) {
	a = a || 'any';
	b = b || 'any';
	return a === 'any' || a === b || (a === 'tcp_udp' && (b === 'tcp' || b === 'udp'));
}
function v4num(s) {
	var p = String(s).split('.'), i, n = 0;
	if (p.length !== 4) return null;
	for (i = 0; i < 4; i++) {
		if (!/^\d{1,3}$/.test(p[i]) || Number(p[i]) > 255) return null;
		n = n * 256 + Number(p[i]);
	}
	return n;
}
function ipRange(s) {
	var t = String(s).toLowerCase(), i, lo, hi, n, size;
	if (t.indexOf(':') >= 0) return { f: 6, key: t };
	i = t.indexOf('/');
	if (i >= 0) {
		lo = v4num(t.slice(0, i));
		n = Number(t.slice(i + 1));
		if (lo == null || !(n >= 1 && n <= 32)) return null;
		size = Math.pow(2, 32 - n);
		lo = Math.floor(lo / size) * size;
		hi = lo + size - 1;
	} else if ((i = t.indexOf('-')) >= 0) {
		lo = v4num(t.slice(0, i));
		hi = v4num(t.slice(i + 1));
	} else {
		lo = hi = v4num(t);
	}
	return lo == null || hi == null ? null : { f: 4, lo: lo, hi: hi };
}
function valCovers(k, a, b) {
	var ra, rb, ma, mb;
	if (a === b) return true;
	if (k === 'app') {
		ma = /^cls:(\d+)$/.exec(a);
		mb = /^app:(\d+)$/.exec(b);
		return !!(ma && mb && Math.floor(Number(mb[1]) / 1000) === Number(ma[1]));
	}
	if (k === 'src_ip' || k === 'dst_ip') {
		ra = ipRange(a);
		rb = ipRange(b);
		if (!ra || !rb || ra.f !== rb.f) return false;
		return ra.f === 6 ? ra.key === rb.key : (ra.lo <= rb.lo && rb.hi <= ra.hi);
	}
	if (k === 'domain') return b.length > a.length && b.slice(b.length - a.length - 1) === '.' + a;
	return false;
}
function dimCovers(k, a, b) {
	if (!a.length) return true;
	if (!b.length) return false;
	return b.every(function(y) { return a.some(function(x) { return valCovers(k, String(x), String(y)); }); });
}
function covers(a, b) {
	var i;
	if (!protoCovers(a.proto, b.proto)) return false;
	if (!portsCover(arr(a.dport), arr(b.dport)) || !portsCover(arr(a.sport), arr(b.sport))) return false;
	for (i = 0; i < DIM_KEYS.length; i++) if (!dimCovers(DIM_KEYS[i], arr(a[DIM_KEYS[i]]), arr(b[DIM_KEYS[i]]))) return false;
	return true;
}
function estShadow(list) {
	var out = {}, i, j, a, b;
	for (i = 1; i < list.length; i++) {
		b = list[i];
		if (!b.enabled || b.status === 'error' || !effAll(b)) continue;
		for (j = 0; j < i; j++) {
			a = list[j];
			if (!a.enabled || a.status === 'error' || !effAll(a)) continue;
			if (a.target === b.target && (a.on_fail || 'default') === (b.on_fail || 'default')) continue;
			if (covers(a, b)) { out[b.id] = { n: j + 1, name: ruleName(a) }; break; }
		}
	}
	return out;
}

function render(root, ctx) {
	var h = ctx.h, ui = ctx.ui, fmt = ctx.fmt, api = ctx.api, icon = ctx.icon, COPY = ctx.copy;
	var dead = false;
	var S = null;
	var L = [];
	var E = {};
	var ROWS = {};
	var SEL = {};
	var F = { q: '', st: 'all' };
	var N = { apps: null, appsTried: false, dev: null, devTried: false, ver: 0 };
	var X = { mut: 0, ordering: 0, editing: false, syncTimer: 0, qTimer: 0, shadow: {}, cnt: {} };
	var offPend = null;

	function now() { return ctx.store.now(); }
	function filtering() { return F.q !== '' || F.st !== 'all'; }

	/* ---------- 名称解析(应用库、设备列表各取一次,失败就显示原值) ---------- */
	function appLabel(tok) { return (N.apps && N.apps[tok]) || tok; }
	function devLabel(mac) { return (N.dev && N.dev[mac]) || mac; }
	function valLabel(k, v) {
		if (k === 'app') return appLabel(v);
		if (k === 'src_dev') return devLabel(v);
		return v;
	}
	function ensureNames() {
		var needApp = false, needDev = false;
		L.forEach(function(r) {
			if (arr(r.app).length) needApp = true;
			if (arr(r.src_dev).length) needDev = true;
		});
		if (needApp && !N.appsTried && obj(S.caps).apps !== false) {
			N.appsTried = true;
			api('apps').then(function(j) {
				var m = {};
				if (dead) return;
				arr(j.classes).forEach(function(c) { m['cls:' + c.id] = (c.name || ('分类 ' + c.id)) + '(分类)'; });
				arr(j.apps).forEach(function(a) { m['app:' + a.id] = a.name || ('应用 ' + a.id); });
				N.apps = m;
				N.ver++;
				sync();
			}, noop);
		}
		if (needDev && !N.devTried) {
			N.devTried = true;
			api('clients').then(function(j) {
				var m = {};
				if (dead) return;
				arr(j.clients).forEach(function(c) { if (c.alias || c.name) m[c.mac] = (c.alias || c.name) + '(' + c.mac + ')'; });
				N.dev = m;
				N.ver++;
				sync();
			}, noop);
		}
	}

	/* ---------- 状态、出口、条件的文字 ---------- */
	function statusOf(r) {
		var st = r.status || 'pending';
		if (r.enabled === false) {
			if (st === 'active') return { key: 'disabled', kind: 'warn', text: '已停用', note: '已停用,但运行中的规则里还有它,应用后才会停止生效。' };
			return { key: 'disabled', kind: 'off', text: '已停用', note: '' };
		}
		if (obj(S.global).enabled === false) return { key: 'idle', kind: 'off', text: '未运行', note: '策略路由总开关是关闭的,规则不会生效。' };
		if (st === 'active') return { key: 'active', kind: 'ok', text: '生效中', note: '' };
		if (st === 'error') return { key: 'error', kind: 'err', text: '有错误', note: r.reason || '规则有错误,没有生效。' };
		return { key: 'pending', kind: 'warn', text: '尚未应用', note: '规则已保存,应用之后才会生效。' };
	}
	function exitInfo(r) {
		var t = splitTarget(r.target), w, g, main = '', badge = null, miss = false;
		if (t.type === 'wan') {
			w = byKey(arr(S.wans), 'wan_id', t.id);
			if (!w) { main = t.id + '(未找到该出口)'; miss = true; }
			else {
				main = wanTitle(w);
				if (w.state === 'absent') badge = ['off', '设备未接入'];
				else if (w.state === 'offline') badge = ['err', '离线'];
				else if (w.state === 'disabled') badge = ['off', '已停用'];
			}
		} else if (t.type === 'group') {
			g = byKey(arr(S.groups), 'id', t.id);
			if (!g) { main = t.id + '(未找到该出口组)'; miss = true; }
			else {
				main = (g.name || g.id) + '(出口组)';
				if (g.enabled === false) badge = ['off', '已停用'];
			}
		} else main = String(r.target || '未设置出口');
		var fb = r.on_fail === 'block' ? '阻断(不回落)' : '回落到系统默认路由';
		return { main: main, badge: badge, miss: miss, fail: (t.type === 'group' ? '组内可切换;全部不可用时:' : '该出口不可用时:') + fb };
	}
	function listText(k, list) {
		var head = list.slice(0, SHOW_N).map(function(v) { return valLabel(k, String(v)); }).join('、');
		return head + (list.length > SHOW_N ? ' 等 ' + list.length + ' 项' : '');
	}
	function svcText(r) {
		var out = [];
		if (r.proto && r.proto !== 'any') out.push('协议 ' + (PROTO_LABEL[r.proto] || r.proto));
		if (arr(r.dport).length) out.push('目的端口 ' + arr(r.dport).join('、'));
		if (arr(r.sport).length) out.push('源端口 ' + arr(r.sport).join('、'));
		return out.join(' · ');
	}
	function condNode(r) {
		var wrap = h('div', { class: 'pr-rl-cond' }), segs = [], any = r.match === 'any', svc = svcText(r), full = [];
		DIM_KEYS.forEach(function(k) {
			var l = arr(r[k]);
			if (!l.length) return;
			segs.push(h('span', { class: 'pr-rl-seg', title: DIM_LABEL[k] + ':' + l.map(function(v) { return valLabel(k, String(v)); }).join('、') },
				h('b', { class: 'pr-rl-sl' }, DIM_LABEL[k]), listText(k, l)));
			full.push(DIM_LABEL[k] + ' ' + l.length + ' 项');
		});
		segs.forEach(function(s, i) {
			if (i) wrap.appendChild(h('span', { class: 'pr-rl-op' }, any ? '或' : '并且'));
			wrap.appendChild(s);
		});
		if (svc) {
			if (segs.length) wrap.appendChild(h('span', { class: 'pr-rl-op' }, '并且'));
			wrap.appendChild(h('span', { class: 'pr-rl-seg' }, h('b', { class: 'pr-rl-sl' }, '协议端口'), svc));
		}
		if (!wrap.firstChild) wrap.appendChild(h('span', { class: 'pr-dim' }, '没有匹配条件'));
		if (any && segs.length > 1) wrap.title = '满足其中任一类条件即可;协议和端口始终要同时满足。';
		return wrap;
	}

	/* ---------- 行 ---------- */
	function makeRow(r) {
		var o = { id: r.id, rule: r, sig: '', keys: '', st: { key: 'pending' } };
		o.chk = h('input', { type: 'checkbox', class: 'pr-rl-chk' });
		o.chk.addEventListener('change', function() {
			if (o.chk.checked) SEL[o.id] = true; else delete SEL[o.id];
			renderBulk();
		});
		o.grip = ui.grip('');
		o.no = h('span', { class: 'pr-rl-no' });
		o.nm = h('button', { type: 'button', class: 'pr-rl-nm' });
		o.nm.addEventListener('click', function() { openEditor({ id: o.id }, false); });
		o.note = h('div', { class: 'pr-rl-note pr-hint', hidden: true });
		o.sh = h('div', { class: 'pr-rl-sh', hidden: true });
		o.cond = h('div', { class: 'pr-rl-v' });
		o.exit = h('div', { class: 'pr-rl-v' });
		o.hit = h('span', { class: 'pr-rl-hit' });
		o.stEl = h('div', { class: 'pr-rl-v' });
		o.sw = ui.toggle({ checked: r.enabled !== false, label: '启用规则', onChange: function(want) { return toggleOne(o.rule, want); } });
		o.swInp = o.sw.querySelector('input');
		o.bEdit = ui.btn({ kind: 'icon', icon: 'edit', title: '编辑', onClick: function() { openEditor({ id: o.id }, false); } });
		o.bCopy = ui.btn({ kind: 'icon', icon: 'copy', title: '复制', onClick: function() { copyOne(o.rule); } });
		o.bDel = ui.btn({ kind: 'icon', icon: 'trash', title: '删除', onClick: function() { deleteOne(o.rule); } });
		o.tr = h('tr', { role: 'row', dataset: { id: r.id, name: ruleName(r) } },
			h('td', { class: 'pr-rl-c-ord', role: 'cell' }, h('div', { class: 'pr-rl-ord' }, o.grip, o.chk, o.no)),
			h('td', { class: 'pr-rl-c-name', role: 'cell' }, o.nm, o.note, o.sh),
			h('td', { class: 'pr-rl-c-cond', role: 'cell', dataset: { label: '匹配条件' } }, o.cond),
			h('td', { class: 'pr-rl-c-exit', role: 'cell', dataset: { label: '出口与动作' } }, o.exit),
			h('td', { class: 'pr-rl-c-hit', role: 'cell', dataset: { label: '命中包数' } }, o.hit),
			h('td', { class: 'pr-rl-c-st', role: 'cell', dataset: { label: '状态' } }, o.stEl),
			h('td', { class: 'pr-rl-c-en', role: 'cell', dataset: { label: '启用' } }, o.sw),
			h('td', { class: 'pr-rl-c-act', role: 'cell', dataset: { label: '操作' } }, h('div', { class: 'pr-rl-acts' }, o.bEdit, o.bCopy, o.bDel)));
		return o;
	}
	function exitSig(r) {
		var t = splitTarget(r.target), w, g;
		if (t.type === 'wan') { w = byKey(arr(S.wans), 'wan_id', t.id); return w ? [w.alias, w.name, w.state] : 0; }
		g = byKey(arr(S.groups), 'id', t.id);
		return g ? [g.name, g.enabled] : 0;
	}
	function fill(o, r) {
		var sh = X.shadow[r.id] || null, hits = obj(r.hits), sig, st, ex, nm = ruleName(r), keys = [];
		o.rule = r;
		sig = JSON.stringify([r.index, r.name, r.note, r.enabled, r.match, r.target, r.on_fail, r.proto, r.dport, r.sport,
			r.app, r.src_dev, r.src_ip, r.dst_ip, r.domain, r.status, r.reason, hits.pkts, hits.bytes, sh, N.ver, exitSig(r), obj(S.global).enabled]);
		if (sig === o.sig) return;
		o.sig = sig;
		st = statusOf(r);
		ex = exitInfo(r);
		o.st = st;
		o.tr.setAttribute('data-name', nm);
		o.tr.classList.toggle('pr-rl-off', r.enabled === false);
		o.no.textContent = String(r.index || '');
		o.nm.textContent = nm;
		o.nm.title = '编辑规则「' + nm + '」';
		o.nm.setAttribute('aria-label', '编辑规则「' + nm + '」');
		o.note.textContent = r.note || '';
		o.note.hidden = !r.note;
		o.sh.textContent = '';
		if (sh) {
			o.sh.appendChild(icon('info'));
			o.sh.appendChild(h('span', null, '可能被第 ' + sh.n + ' 条「' + sh.name + '」遮蔽(估算):它的匹配范围包含本规则且出口不同,命中后不会再评估本规则。'));
		}
		o.sh.hidden = !sh;
		o.grip.setAttribute('aria-label', '拖动调整「' + nm + '」的顺序,或按上下方向键移动');
		o.chk.setAttribute('aria-label', '选择规则「' + nm + '」');
		o.chk.checked = !!SEL[r.id];
		o.cond.textContent = '';
		o.cond.appendChild(condNode(r));
		o.exit.textContent = '';
		o.exit.appendChild(h('div', { class: 'pr-rl-x1' }, h('span', { class: 'pr-rl-xm' }, ex.main),
			ex.badge ? ui.badge(ex.badge[1], ex.badge[0]) : null));
		o.exit.appendChild(h('div', { class: 'pr-rl-x2 pr-hint' }, ex.fail));
		o.hit.textContent = st.key === 'active' || (st.key === 'disabled' && r.status === 'active') ? fmt.pkts(hits.pkts) : '—';
		o.hit.title = st.key === 'active' ? '匹配到本规则的数据包 ' + (Number(hits.pkts) || 0) + ' 个,约 ' + fmt.bytes(hits.bytes) : '';
		o.stEl.textContent = '';
		o.stEl.appendChild(ui.dot(st.kind, st.text));
		if (st.note) o.stEl.appendChild(h('div', { class: 'pr-rl-sn pr-hint' }, st.note));
		o.swInp.checked = r.enabled !== false;
		o.swInp.setAttribute('aria-label', '启用规则「' + nm + '」');
		o.bEdit.setAttribute('aria-label', '编辑规则「' + nm + '」');
		o.bCopy.setAttribute('aria-label', '复制规则「' + nm + '」');
		o.bDel.setAttribute('aria-label', '删除规则「' + nm + '」');
		keys.push(nm, r.id, r.note, r.target, ex.main, PROTO_LABEL[r.proto] || '', arr(r.dport).join(' '), arr(r.sport).join(' '), st.text);
		DIM_KEYS.forEach(function(k) {
			arr(r[k]).forEach(function(v) { keys.push(String(v), valLabel(k, String(v))); });
		});
		o.keys = keys.join(' ').toLowerCase();
	}

	/* ---------- 同步列表 ---------- */
	function sync() {
		var seen = {}, cur, want, same, i;
		if (dead || !E.tbody) return;
		if (E.tbody.classList.contains('pr-dragging') || X.ordering > 0) {
			clearTimeout(X.syncTimer);
			X.syncTimer = setTimeout(sync, 400);
			return;
		}
		X.shadow = estShadow(L);
		L.forEach(function(r) {
			if (!r.id) return;
			var o = ROWS[r.id];
			if (!o) o = ROWS[r.id] = makeRow(r);
			seen[r.id] = true;
			fill(o, r);
		});
		Object.keys(ROWS).forEach(function(id) {
			if (seen[id]) return;
			if (ROWS[id].tr.parentNode) ROWS[id].tr.parentNode.removeChild(ROWS[id].tr);
			delete ROWS[id];
			delete SEL[id];
		});
		cur = Array.prototype.slice.call(E.tbody.children);
		want = L.filter(function(r) { return ROWS[r.id]; }).map(function(r) { return ROWS[r.id].tr; });
		same = cur.length === want.length;
		for (i = 0; same && i < want.length; i++) if (cur[i] !== want[i]) same = false;
		if (!same) want.forEach(function(tr) { E.tbody.appendChild(tr); });
		applyFilter();
		renderBulk();
		ensureNames();
	}
	function rowShown(o, q) {
		var key = o.st.key;
		if (F.st === 'shadow') { if (!X.shadow[o.id]) return false; }
		else if (F.st !== 'all' && key !== F.st) return false;
		return !q || o.keys.indexOf(q) >= 0;
	}
	function applyFilter() {
		var q = F.q.toLowerCase(), shown = 0, cnt = { all: 0, active: 0, error: 0, pending: 0, disabled: 0, idle: 0, shadow: 0 }, sel = E.fsel, parts = [];
		L.forEach(function(r) {
			var o = ROWS[r.id];
			if (!o) return;
			cnt.all++;
			cnt[o.st.key] = (cnt[o.st.key] || 0) + 1;
			if (X.shadow[r.id]) cnt.shadow++;
		});
		X.cnt = cnt;
		L.forEach(function(r) {
			var o = ROWS[r.id], ok;
			if (!o) return;
			ok = rowShown(o, q);
			o.tr.hidden = !ok;
			if (ok) shown++;
			else delete SEL[r.id];
		});
		sel.textContent = '';
		FILTERS.forEach(function(f) {
			if (f[0] === 'idle' && !cnt.idle && F.st !== 'idle') return;
			sel.appendChild(h('option', { value: f[0] }, f[1] + '(' + (cnt[f[0]] || 0) + ')'));
		});
		sel.value = F.st;
		E.noMatch.hidden = !(L.length && !shown);
		E.tw.hidden = !shown;
		E.filters.hidden = !L.length;
		E.card.hidden = !L.length;
		E.empty.hidden = !!L.length;
		E.newBtn.disabled = L.length >= MAX_RULES;
		E.newBtn.title = L.length >= MAX_RULES ? '规则数量已达上限 ' + MAX_RULES + ' 条' : '';
		SUM_KEYS.forEach(function(k) { if (cnt[k]) parts.push(SUM_LABEL[k] + ' ' + cnt[k]); });
		E.sum.textContent = L.length ? '共 ' + L.length + ' 条(上限 ' + MAX_RULES + '):' + parts.join(',') + (filtering() ? ';当前显示 ' + shown + ' 条' : '') : '';
		E.foot.hidden = !L.length;
		E.tipOrder.textContent = filtering() ? '搜索或筛选时不能调整顺序,请先清除。' : '';
	}

	/* ---------- 横幅 ---------- */
	function renderBanners() {
		var has = ctx.pending.has(), e = obj(S.engine), np = 0, msg = '', g = obj(S.global);
		if (g.enabled === true) L.forEach(function(r) { if (r.status === 'pending' && r.enabled !== false) np++; });
		if (has) msg = '有更改已保存但尚未应用,规则还没有作用到网络。';
		else if (np > 0) msg = np + ' 条规则尚未应用。';
		else if (e.pending === true) msg = '已保存的配置可能比运行中的规则新(按配置文件时间判断,路由器时钟不准时可能误报)。点击应用可让两者一致。';
		E.pend.hidden = !msg;
		E.pendMsg.textContent = msg;
		E.off.hidden = g.enabled !== false;
	}
	function doApply() {
		E.pendBtn.disabled = true;
		return ctx.apply().then(function() { if (!dead) E.pendBtn.disabled = false; });
	}

	/* ---------- 编辑器与深链 ---------- */
	function flashRow(id) {
		var o = ROWS[id];
		if (!o || o.tr.hidden) return;
		try { o.tr.scrollIntoView({ block: 'nearest' }); } catch (e) { /* 忽略 */ }
		o.tr.classList.add('pr-flash');
		setTimeout(function() { o.tr.classList.remove('pr-flash'); }, 1200);
	}
	function openEditor(o, fromHash) {
		var q;
		if (dead) return;
		if (X.editing) return;
		if (!M.editor || typeof M.editor.open !== 'function') { ctx.toast('规则编辑器没有加载,请刷新页面后重试。', 'err'); return; }
		X.editing = true;
		q = 'edit=' + encodeURIComponent(o.id || 'new') + (!o.id && o.copyFrom ? '&copy=' + encodeURIComponent(o.copyFrom) : '');
		if (!fromHash) ctx.setHash(q, true);
		Promise.resolve().then(function() { return M.editor.open({ id: o.id || null, copyFrom: o.copyFrom || null }); }).then(function(res) {
			return res || { saved: false };
		}, function(e) {
			ctx.toast('规则编辑器打开失败:' + msgOf(e), 'err');
			return { saved: false };
		}).then(function(res) {
			X.editing = false;
			if (dead) return;
			if (ctx.hash().edit != null) ctx.setHash('', true);
			if (res.saved) ctx.store.load(true).then(function() { if (!dead) flashRow(res.id || o.id); }, noop);
			else if (o.id) flashRow(o.id);
		});
	}
	function handleHash() {
		var q = ctx.hash(), e = q.edit;
		if (dead || e == null || X.editing || !S) return;
		if (e === 'new' || e === '') openEditor({ id: null, copyFrom: q.copy || null }, true);
		else if (byKey(L, 'id', e)) openEditor({ id: e }, true);
		else {
			ctx.toast('规则「' + e + '」不存在或已被删除。', 'warn');
			ctx.setHash('', true);
		}
	}
	function onHash() { handleHash(); }

	/* ---------- 写操作 ---------- */
	function failText(e, what) {
		return what + '失败:' + msgOf(e) + (e && e.kind && e.kind !== 'server' ? ' 结果未确认,请刷新页面核对当前状态。' : '');
	}
	function busyNow() {
		if (X.mut > 0 || X.ordering > 0) { ctx.toast('上一个操作还在进行,请稍候再试。', 'warn'); return true; }
		return false;
	}
	function runMut(job) {
		var rel = ctx.store.hold();
		var fin = function() { X.mut--; renderBulk(); ctx.store.load(true).then(rel, rel); };
		X.mut++;
		renderBulk();
		return job().then(function(v) { fin(); return v; }, function(e) { fin(); throw e; });
	}
	/* 写接口默认会应用;applied.ok===false 时 core 的待应用标记不会置位,这里补上 */
	function reportApplied(res, okText) {
		var a = obj(obj(res).applied), w = arr(a.warnings);
		if (a.ok === false || obj(res).applied === false) {
			ctx.pending.set(true);
			ctx.toast('已保存,但应用失败:' + (a.error || '引擎没有给出原因') + ' ' +
				(a.failed_open ? '恢复上一次配置也失败,已清除策略路由规则,流量按系统默认路由走。' : '已保持上一次有效配置。'), 'err');
			return;
		}
		if (a.disabled === true) { ctx.toast('已保存。策略路由总开关是关闭的,规则暂时不会生效。', 'warn'); return; }
		ctx.toast(okText + (w.length ? '。注意:' + w[0] + (w.length > 1 ? '(另有 ' + (w.length - 1) + ' 条提示)' : '') : ''), w.length ? 'warn' : 'ok');
	}
	function toggleOne(r, want) {
		if (busyNow()) return false;
		return runMut(function() {
			return api('rule_toggle', { id: r.id, enabled: want }, { timeout: APPLY_MS }).then(function(res) {
				reportApplied(res, '规则「' + ruleName(r) + '」已' + (want ? '启用' : '停用') + '并应用');
				return true;
			}, function(e) {
				ctx.toast(failText(e, want ? '启用' : '停用'), 'err');
				return false;
			});
		});
	}
	function deleteOne(r) {
		if (busyNow()) return;
		ctx.confirmBox({
			title: '删除规则「' + ruleName(r) + '」?',
			body: '删除后会立即应用,这条规则不再参与匹配。此操作不能撤销。',
			okText: '删除', danger: true
		}).then(function(ok) {
			if (!ok || dead || busyNow()) return;
			runMut(function() {
				return api('rule_del', { id: r.id }, { timeout: APPLY_MS }).then(function(res) {
					delete SEL[r.id];
					reportApplied(res, '规则「' + ruleName(r) + '」已删除并应用');
				}, function(e) { ctx.toast(failText(e, '删除'), 'err'); });
			});
		});
	}
	function copyOne(r) {
		var had = ctx.pending.has();
		if (busyNow()) return;
		runMut(function() {
			return api('rule_copy', { id: r.id }).then(function(res) {
				if (had) ctx.pending.set(true);
				return res;
			});
		}).then(function(res) {
			if (dead) return;
			ctx.toast('已复制为停用的新规则,请检查后再启用。', 'ok');
			if (res && res.id) openEditor({ id: res.id }, false);
		}, function(e) {
			if (had) ctx.pending.set(true);
			ctx.toast(failText(e, '复制'), 'err');
		});
	}
	function selectedRules() { return L.filter(function(r) { return SEL[r.id]; }); }
	function batchToggle(want) {
		var list = selectedRules().filter(function(r) { return (r.enabled !== false) !== want; });
		if (!selectedRules().length) return;
		if (!list.length) { ctx.toast('所选规则已经都是' + (want ? '启用' : '停用') + '状态。', 'info'); return; }
		if (busyNow()) return;
		runMut(function() {
			var i = 0, done = 0;
			function next() {
				var r;
				if (i >= list.length) return Promise.resolve(done);
				r = list[i++];
				E.bulkMsg.textContent = '正在处理 ' + i + '/' + list.length + '…';
				return api('rule_toggle', { id: r.id, enabled: want, apply: 0 }).then(function() { done++; return next(); }, function(e) {
					var err = new Error('已处理 ' + done + ' 条,规则「' + ruleName(r) + '」' + (want ? '启用' : '停用') + '失败:' + msgOf(e) + '。其余没有处理。');
					err.done = done;
					throw err;
				});
			}
			return next();
		}).then(function(n) {
			if (dead) return;
			SEL = {};
			ctx.toast('已' + (want ? '启用' : '停用') + ' ' + n + ' 条规则。已保存,将在 1 分钟内的下次对账自动生效,也可点右上角「应用」立即生效。', 'info');
		}, function(e) {
			ctx.toast(msgOf(e), 'err');
		}).then(function() { if (!dead) renderBulk(); });
	}

	/* ---------- 排序 ---------- */
	function localReorder(order) {
		var pos = {};
		order.forEach(function(id, i) { pos[id] = i; });
		L.sort(function(a, b) { return (pos[a.id] == null ? 1e9 : pos[a.id]) - (pos[b.id] == null ? 1e9 : pos[b.id]); });
		L.forEach(function(r, i) { r.index = i + 1; });
		/* DOM 已被后续排序改过时不能按旧顺序重排,否则列表回翻且把手失焦;最终顺序由队列清空后的重新读取校正 */
		if (E.tbody && Array.prototype.some.call(E.tbody.children, function(tr, i) { return tr.dataset.id !== order[i]; })) return;
		sync();
	}
	function onOrder(order) {
		X.ordering++;
		return api('rule_order', { ids: order, apply: 0 }).then(function(res) {
			X.ordering--;
			localReorder(order);
			if (!(res && res.unchanged)) ctx.toast('顺序已保存,将在 1 分钟内的下次对账自动生效,也可点右上角「应用」立即生效。', 'info');
		}, function(e) {
			X.ordering--;
			throw e;
		});
	}
	function onOrderFail(err) {
		if (/规则列表已变化/.test(msgOf(err))) ctx.toast('规则列表已被其它操作改变,已重新读取最新顺序,请再试一次。', 'warn');
	}

	/* ---------- 批量条 ---------- */
	function renderBulk() {
		var n = 0, vis = 0, on = 0, any = !!E.bulk;
		if (!any) return;
		L.forEach(function(r) {
			var o = ROWS[r.id];
			if (SEL[r.id]) n++;
			if (o && !o.tr.hidden) { vis++; if (SEL[r.id]) on++; }
		});
		E.bulk.hidden = !n;
		E.bulkMsg.textContent = X.mut > 0 ? '正在处理…' : '已选 ' + n + ' 条';
		[E.bOn, E.bOff, E.bClr].forEach(function(b) { b.disabled = X.mut > 0 || X.ordering > 0; });
		E.all.checked = vis > 0 && on === vis;
		E.all.indeterminate = on > 0 && on < vis;
		E.all.disabled = !vis;
		L.forEach(function(r) { var o = ROWS[r.id]; if (o) o.chk.checked = !!SEL[r.id]; });
	}

	/* ---------- 搭建 ---------- */
	function buildChrome() {
		var th = function(t, cls) { return h('th', { scope: 'col', role: 'columnheader', class: cls }, t); };
		E.pendMsg = h('div', { class: 'pr-grow' });
		E.pendBtn = ui.btn({ kind: 'primary', size: 'sm', text: '应用', onClick: doApply });
		E.pend = h('div', { class: 'pr-note pr-k-warn pr-rl-pend', role: 'status', hidden: true }, icon('warn'), E.pendMsg, E.pendBtn);
		E.off = h('div', { class: 'pr-note pr-k-warn pr-rl-pend', role: 'status', hidden: true }, icon('warn'),
			h('div', { class: 'pr-grow' }, '策略路由总开关目前是关闭的:规则可以编辑和保存,但不会生效。'),
			ui.btn({ size: 'sm', text: '去总览开启', onClick: function() { ctx.nav('overview'); } }));
		E.tip = h('p', { class: 'pr-hint pr-rl-tip' },
			'规则从上到下依次匹配,第一条命中的规则决定出口,后面的规则不再评估;没有命中任何规则的新连接走系统默认路由。拖动左侧把手(或聚焦把手后按上下方向键)可以调整顺序。规则只影响之后新建立的连接。');

		E.q = h('input', { type: 'search', class: 'pr-in', placeholder: '搜索名称、应用、设备、地址、域名', 'aria-label': '搜索规则', autocomplete: 'off' });
		E.q.addEventListener('input', function() {
			clearTimeout(X.qTimer);
			X.qTimer = setTimeout(function() { F.q = E.q.value.trim(); applyFilter(); renderBulk(); }, SEARCH_MS);
		});
		E.qbox = h('label', { class: 'pr-rl-q' }, icon('search'), E.q);
		E.fsel = h('select', { class: 'pr-in pr-sel pr-rl-fs', 'aria-label': '按状态筛选' });
		E.fsel.addEventListener('change', function() { F.st = E.fsel.value; applyFilter(); renderBulk(); });
		E.filters = h('div', { class: 'pr-rl-fl' }, E.qbox, E.fsel);
		E.newBtn = ui.btn({ kind: 'primary', icon: 'plus', text: '新建规则', onClick: function() { openEditor({ id: null }, false); } });
		E.sum = h('span', { class: 'pr-hint pr-rl-sum', 'aria-live': 'polite' });
		E.bar = h('div', { class: 'pr-toolbar pr-rl-bar' }, E.filters, h('span', { class: 'pr-grow' }), E.newBtn);

		E.bulkMsg = h('span', { class: 'pr-rl-bm', 'aria-live': 'polite' });
		E.bOn = ui.btn({ size: 'sm', text: '启用所选', onClick: function() { batchToggle(true); } });
		E.bOff = ui.btn({ size: 'sm', text: '停用所选', onClick: function() { batchToggle(false); } });
		E.bClr = ui.btn({ kind: 'text', size: 'sm', text: '取消选择', onClick: function() { SEL = {}; renderBulk(); } });
		E.bulk = h('div', { class: 'pr-rl-bulk', role: 'region', 'aria-label': '批量操作', hidden: true },
			E.bulkMsg, E.bOn, E.bOff, E.bClr, h('span', { class: 'pr-hint pr-rl-bh' }, '批量操作只保存,不会立即应用;完成后点右上角「应用」。'));

		E.all = h('input', { type: 'checkbox', class: 'pr-rl-chk', 'aria-label': '选择当前显示的全部规则' });
		E.all.addEventListener('change', function() {
			L.forEach(function(r) {
				var o = ROWS[r.id];
				if (!o || o.tr.hidden) return;
				if (E.all.checked) SEL[r.id] = true; else delete SEL[r.id];
			});
			renderBulk();
		});
		E.tbody = h('tbody', { role: 'rowgroup' });
		E.table = h('table', { class: 'pr-tbl pr-rl-t', role: 'table', 'aria-label': '策略规则,按优先级从高到低排列' },
			h('thead', { role: 'rowgroup' }, h('tr', { role: 'row' },
				h('th', { scope: 'col', role: 'columnheader', class: 'pr-rl-c-ord' }, h('div', { class: 'pr-rl-ord' }, h('span', { class: 'pr-rl-gp' }), E.all, h('span', null, '优先级'))),
				th('规则', 'pr-rl-c-name'), th('匹配条件', 'pr-rl-c-cond'), th('出口与动作', 'pr-rl-c-exit'), th('命中包数', 'pr-rl-c-hit'),
				th('状态', 'pr-rl-c-st'), th('启用', 'pr-rl-c-en'), th('操作', 'pr-rl-c-act'))),
			E.tbody);
		E.tw = h('div', { class: 'pr-rl-tw' }, E.table);
		E.noMatch = h('div', { hidden: true }, ui.empty({
			icon: 'search', title: '没有符合条件的规则', body: '换个关键词,或清除搜索和筛选。', compact: true,
			actions: [{ text: '清除搜索和筛选', icon: 'close', onClick: function() { E.q.value = ''; F.q = ''; F.st = 'all'; applyFilter(); renderBulk(); } }]
		}));
		E.tipOrder = h('p', { class: 'pr-hint pr-rl-to', 'aria-live': 'polite' });
		E.card = h('div', { class: 'pr-card pr-flush pr-rl-card', hidden: true }, E.tw, E.noMatch);
		E.empty = h('div', { class: 'pr-card' }, ui.empty({
			icon: 'arrow-right', title: '还没有策略规则',
			body: '策略规则决定哪些新建连接走哪个出口。没有命中任何规则的连接走系统默认路由。',
			actions: [{ kind: 'primary', icon: 'plus', text: '新建规则', onClick: function() { openEditor({ id: null }, false); } }]
		}));
		E.hitP = h('p', { class: 'pr-hint' });
		E.foot = h('div', { class: 'pr-rl-foot', hidden: true }, E.hitP,
			h('p', { class: 'pr-hint' }, COPY.offload),
			h('p', { class: 'pr-hint' }, '应用条件:' + COPY.app),
			h('p', { class: 'pr-hint' }, '域名条件:' + COPY.domain),
			h('p', { class: 'pr-hint' }, '「可能被遮蔽」是按条件范围做的估算,不一定准确;以规则编辑器里的校验提示为准。'));
	}
	function update(st) {
		if (dead) return;
		S = st;
		L = arr(st.rules).filter(function(r) { return r && r.id; }).slice();
		renderBanners();
		sync();
		if (E.hitP) {
			E.hitP.textContent = '命中包数:统计被该规则匹配的新建连接的数据包,不是该规则转发的总流量。' +
				(obj(st.engine).counters_since > 0 ? '计数从 ' + fmt.since(obj(st.engine).counters_since, now()) + ' 开始累计。' : '');
		}
	}
	function build(st) {
		S = st;
		root.textContent = '';
		if (st.installed === false) {
			root.appendChild(ui.empty({ icon: 'info', title: '策略路由组件未安装', body: '没有找到策略路由引擎(mwrt-pbr),请确认固件包含该组件。' }));
			return;
		}
		buildChrome();
		[E.pend, E.off, E.tip, E.bar, E.sum, E.bulk, E.card, E.empty, E.tipOrder, E.foot].forEach(function(n) { root.appendChild(n); });
		ctx.dragSort(E.tbody, {
			handle: '.pr-grip', onOrder: onOrder, onFail: onOrderFail,
			disabled: function() { return filtering() || X.mut > 0; },
			disabledMsg: '现在不能调整顺序:请先清除搜索和筛选,并等待进行中的操作完成。'
		});
		ctx.store.on('change', function(s) { if (!dead && s) update(s); });
		offPend = ctx.pending.on(function() { if (!dead && S) renderBanners(); });
		window.addEventListener('hashchange', onHash);
		update(st);
		handleHash();
	}
	function showLoading() {
		root.textContent = '';
		root.appendChild(ui.loading({ rows: 5, text: '正在读取策略规则…' }));
	}
	function start() {
		showLoading();
		ctx.store.load().then(function(st) {
			if (!dead) build(st);
		}, function(e) {
			if (dead) return;
			root.textContent = '';
			root.appendChild(ui.error({ title: '无法读取策略规则', message: msgOf(e), onRetry: start }));
		});
	}

	start();
	return {
		destroy: function() {
			dead = true;
			clearTimeout(X.syncTimer);
			clearTimeout(X.qTimer);
			window.removeEventListener('hashchange', onHash);
			if (offPend) offPend();
		}
	};
}

M.pages.rules = { title: '策略规则', render: render };
})();
