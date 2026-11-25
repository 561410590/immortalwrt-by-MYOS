/*
 * 总览页:总开关与引擎状态、当前出口、规则命中、需要注意、全局设置。
 * 动态文本只经 ctx.h 与 textContent 写入;出口与规则来自 state,诊断来自 diag。
 */
(function() {
'use strict';

var M = window.MwrtPbr;
if (!M || !M.pages) return;

var TOP_N = 5;
var ATTN_MAX = 5;
var APPLY_MS = 60000;
var WAN_ST = { online: ['ok', '在线'], offline: ['err', '离线'], disabled: ['off', '已停用'], absent: ['off', '设备未接入'] };
var WAN_RANK = { online: 0, offline: 1, disabled: 2, absent: 3 };
var PROTO = { dhcp: 'DHCP', 'static': '静态地址', pppoe: 'PPPoE', dhcpv6: 'DHCPv6' };
var RULE_ST = [['active', 'ok', '生效'], ['error', 'err', '有错误'], ['pending', 'warn', '尚未应用'], ['disabled', 'off', '已停用']];
var LV_TXT = { err: '失败', warn: '注意', info: '提示' };
var DIAG_LV = { fail: 'err', warn: 'warn' };
var TTL_UNIT = { s: 1, m: 60, h: 3600, d: 86400 };

function noop() {}
function arr(x) { return Array.isArray(x) ? x : []; }
function obj(x) { return x && typeof x === 'object' && !Array.isArray(x) ? x : {}; }
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
function ttlSec(v) {
	var m = /^(\d{1,7})([smhd])$/.exec(String(v || '').toLowerCase());
	return m ? Number(m[1]) * TTL_UNIT[m[2]] : null;
}
function parseZones(s) { return String(s || '').split(/[\s,;]+/).filter(function(x) { return x; }); }
function sameList(a, b) {
	if (a.length !== b.length) return false;
	for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
	return true;
}

function render(root, ctx) {
	var h = ctx.h, ui = ctx.ui, fmt = ctx.fmt, api = ctx.api, icon = ctx.icon;
	var dead = false;
	var S = null;
	var E = {};
	var F = {};
	var D = { checks: null, at: 0, loading: false, error: null, seq: 0 };
	var G = { base: null, dirty: false, open: false, busy: false };
	var offPend = null;
	var offRec = null;
	var hitCells = {};

	function go(page, query) { return function() { ctx.nav(page, query); }; }
	function now() { return ctx.store.now(); }
	function swap(host, sig, fill) {
		if (host.__sig === sig) return;
		host.__sig = sig;
		host.textContent = '';
		fill(host);
	}

	function targetLabel(target) {
		var t = splitTarget(target), w, g;
		if (t.type === 'wan') {
			w = byKey(arr(S.wans), 'wan_id', t.id);
			return w ? wanTitle(w) + (w.state === 'absent' ? '(设备未接入)' : '') : t.id + '(未找到该出口)';
		}
		if (t.type === 'group') {
			g = byKey(arr(S.groups), 'id', t.id);
			return g ? (g.name || g.id) + '(出口组)' : t.id + '(未找到该出口组)';
		}
		return String(target || '—');
	}

	/* ---------- 加载与错误 ---------- */
	function showLoading() {
		root.textContent = '';
		root.appendChild(ui.loading({ rows: 4, text: '正在读取策略路由状态…' }));
	}
	function showError(e) {
		root.textContent = '';
		root.appendChild(ui.error({ title: '无法读取策略路由状态', message: e && e.message, onRetry: start }));
	}
	function start() {
		if (offRec) { offRec(); offRec = null; }
		showLoading();
		ctx.store.load().then(function(st) {
			if (!dead && !S) build(st);
		}, function(e) {
			if (dead) return;
			showError(e);
			/* 顶栏轮询恢复后页面主体也要跟上;S 非空说明已经 build 过,避免重复 build */
			offRec = ctx.store.on('recover', function(st) {
				if (offRec) { offRec(); offRec = null; }
				if (!dead && !S) build(st);
			});
		});
	}

	/* ---------- 未应用提示 ---------- */
	function buildBanner() {
		E.pendMsg = h('div', { class: 'pr-grow' });
		E.pendBtn = ui.btn({ kind: 'primary', size: 'sm', text: '应用', onClick: doApply });
		E.pend = h('div', { class: 'pr-note pr-k-warn pr-ov-pend', role: 'status', hidden: true }, icon('warn'), E.pendMsg, E.pendBtn);
	}
	function renderBanner() {
		var has = ctx.pending.has(), e = obj(S.engine), np = 0, msg = '';
		if (obj(S.global).enabled === true) arr(S.rules).forEach(function(r) { if (r.status === 'pending') np++; });
		if (has) msg = '有更改已保存但尚未应用,规则还没有作用到网络。';
		else if (np > 0) msg = np + ' 条规则尚未应用。';
		else if (e.pending === true) msg = '已保存的配置可能比运行中的规则新(按配置文件时间判断,路由器时钟不准时可能误报)。点击应用可让两者一致。';
		E.pend.hidden = !msg;
		if (E.pendMsg.textContent !== msg) E.pendMsg.textContent = msg;
	}
	function doApply() {
		E.pendBtn.disabled = true;
		return ctx.apply().then(function(ok) {
			if (dead) return;
			E.pendBtn.disabled = false;
			if (ok) runDiag();
		});
	}

	/* 全局设置保存后的统一反馈;applied 是控制器随响应带回的引擎应用结果 */
	function reportSaved(r, wantApply, okText) {
		var a = obj(r.applied), w;
		if (!wantApply) {
			ctx.toast('已保存。将在 1 分钟内的下次对账自动生效,也可点「应用」立即生效。', 'info');
			return;
		}
		if (a.ok === false) {
			ctx.pending.set(true);
			ctx.toast('已保存,但应用失败:' + (a.error || '引擎没有给出原因') + ' ' +
				(a.failed_open ? '恢复上一次配置也失败,已清除策略路由规则,流量按系统默认路由走。' : '已保持上一次有效配置。'), 'err');
			return;
		}
		w = arr(a.warnings);
		ctx.toast(okText + (w.length ? '。注意:' + w[0] + (w.length > 1 ? '(另有 ' + (w.length - 1) + ' 条提示)' : '') : ''), w.length ? 'warn' : 'ok');
	}

	/* ---------- 引擎状态与总开关 ---------- */
	function tile(label) {
		var v = h('div', { class: 'pr-ov-stat-v' }), s = h('div', { class: 'pr-ov-stat-s' });
		return { el: h('div', { class: 'pr-ov-stat' }, h('div', { class: 'pr-ov-stat-l' }, label), v, s), v: v, s: s };
	}
	function buildEngine() {
		E.engDot = h('i', { class: 'pr-dot', 'aria-hidden': 'true' });
		E.engT = h('span', null);
		E.engPill = h('span', { class: 'pr-pill pr-k-off' }, E.engDot, E.engT);
		E.engD = h('p', { class: 'pr-ov-eng-d' });
		E.sw = ui.toggle({ checked: false, label: '策略路由总开关', onChange: onSwitch });
		E.swInp = E.sw.querySelector('input');
		E.tApply = tile('上次应用');
		E.tExits = tile('出口');
		E.tRules = tile('规则');
		E.tProbe = tile('健康探测');
		E.tErr = tile('最近错误');
		E.eng = h('section', { class: 'pr-card pr-ov-eng', 'aria-labelledby': 'pr-ov-h-eng' },
			h('div', { class: 'pr-ov-eng-top' },
				h('div', { class: 'pr-grow' },
					h('h2', { class: 'pr-h2', id: 'pr-ov-h-eng' }, '引擎状态'),
					h('div', { class: 'pr-ov-eng-s' }, E.engPill),
					E.engD),
				h('div', { class: 'pr-ov-sw' },
					h('span', { class: 'pr-lab' }, '总开关'),
					E.sw)),
			h('p', { class: 'pr-hint pr-ov-sw-h' }, '关闭时不会向系统下发任何策略路由规则;开启后只影响之后新建立的连接,已建立的连接不会迁移。'),
			h('div', { class: 'pr-ov-stats' }, E.tApply.el, E.tExits.el, E.tRules.el, E.tProbe.el, E.tErr.el));
	}
	function engView() {
		var e = obj(S.engine), g = obj(S.global);
		if (e.failed_open) return { k: 'err', t: '规则已清除(故障保护)', d: '应用失败且无法恢复上一次配置,策略路由规则已清除,流量按系统默认路由走。' };
		if (e.ok === false) return { k: 'err', t: '上次应用失败', d: '已保持上一次有效配置,详情见下方「需要注意」。' };
		if (!g.enabled) return { k: 'off', t: '总开关已关闭', d: '规则不会生效,流量按系统默认路由走。' };
		if (!e.running) return { k: 'warn', t: '引擎未运行', d: '总开关已开启,但内核里没有策略路由表。点击「应用」重试,或到诊断页查看原因。' };
		if (e.rules_error > 0) return { k: 'warn', t: '运行中', d: e.rules_error + ' 条规则有错误,没有生效。' };
		if (!e.rules_total) return { k: 'ok', t: '运行中', d: '还没有规则。新建规则并应用后,匹配的新连接才会改走指定出口。' };
		return { k: 'ok', t: '运行中', d: '共 ' + e.rules_total + ' 条规则,其中 ' + (e.rules_active || 0) + ' 条生效。' };
	}
	function renderEngine() {
		var e = obj(S.engine), g = obj(S.global), wans = arr(S.wans), v = engView(), on = 0, ab = 0, t;
		E.engPill.className = 'pr-pill pr-k-' + v.k;
		E.engT.textContent = v.t;
		E.engD.textContent = v.d;
		if (!E.sw.classList.contains('pr-busy')) E.swInp.checked = !!g.enabled;

		wans.forEach(function(w) {
			if (w.state === 'online' && w.enabled !== false) on++;
			if (w.state === 'absent') ab++;
		});
		t = e.applied_ts > 0;
		E.tApply.v.textContent = t ? fmt.since(e.applied_ts, now()) : '尚未应用';
		E.tApply.s.textContent = t ? fmt.time(e.applied_ts) : '';
		E.tExits.v.textContent = on + ' 个在线';
		E.tExits.s.textContent = '共 ' + wans.length + ' 个出口' + (ab ? ',' + ab + ' 个设备未接入' : '');
		E.tRules.v.textContent = (e.rules_active || 0) + ' 条生效';
		E.tRules.s.textContent = '共 ' + (e.rules_total || 0) + ' 条' + (e.rules_error > 0 ? ',' + e.rules_error + ' 条有错误' : '');
		E.tProbe.v.textContent = e.health_source === 'probe' ? '运行中' : '未启用';
		E.tProbe.s.textContent = e.health_source === 'probe' ? ctx.copy.probe : '出口状态只依据链路与路由';
		E.tErr.v.textContent = e.error ? e.error : '无';
		E.tErr.v.title = e.error || '';
		E.tErr.v.classList.toggle('pr-ov-clamp', !!e.error);
	}
	function onSwitch(want) {
		var e = obj(S.engine);
		var ask = (want ? e.rules_total > 0 : e.rules_active > 0) ? ctx.confirmBox(want ? {
			title: '开启策略路由?',
			body: '开启后会立即应用已启用的规则,只影响之后新建立的连接;已建立的连接不会迁移。',
			okText: '开启并应用'
		} : {
			title: '关闭策略路由?',
			body: '关闭后会清理已下发的策略路由规则,流量全部按系统默认路由走;正在使用非默认出口的连接可能中断,需要重新建立。',
			okText: '关闭并清理', danger: true
		}) : Promise.resolve(true);
		return ask.then(function(ok) {
			if (!ok) return false;
			return api('global_save', { json: { enabled: want } }, { timeout: APPLY_MS }).then(function(r) {
				reportSaved(r, true, want ? '策略路由已开启并应用' : '策略路由已关闭');
				ctx.store.load(true).then(noop, noop);
				return true;
			});
		});
	}

	/* ---------- 快速入口 ---------- */
	function buildQuick() {
		E.quick = h('div', { class: 'pr-toolbar pr-ov-quick' },
			ui.btn({ kind: 'primary', icon: 'plus', text: '新建规则', onClick: go('rules', 'edit=new') }),
			ui.btn({ kind: 'ghost', text: '出口与出口组', onClick: go('wans') }),
			ui.btn({ kind: 'ghost', text: '诊断', onClick: go('diag') }));
	}

	/* ---------- 需要注意 ---------- */
	function buildAttn() {
		E.attnSub = h('span', { class: 'pr-sub' });
		E.attnRe = ui.btn({ kind: 'text', size: 'sm', icon: 'refresh', text: '重新检查', onClick: runDiag });
		E.attnBody = h('div', { class: 'pr-ov-attn' });
		E.attn = h('section', { class: 'pr-card', 'aria-labelledby': 'pr-ov-h-attn' },
			h('div', { class: 'pr-sec' },
				h('div', null, h('h2', { class: 'pr-h2', id: 'pr-ov-h-attn' }, '需要注意'), E.attnSub),
				E.attnRe),
			E.attnBody);
	}
	function v6Rules() {
		var wans = arr(S.wans), groups = arr(S.groups), out = [];
		function lacks(id) {
			var w = byKey(wans, 'wan_id', id);
			return !!w && w.state !== 'absent' && obj(w.caps).v6 !== true;
		}
		arr(S.rules).forEach(function(r) {
			var t = splitTarget(r.target), g, bad = false;
			if (r.enabled === false) return;
			if (t.type === 'wan') bad = lacks(t.id);
			else if (t.type === 'group') {
				g = byKey(groups, 'id', t.id);
				if (g) arr(g.members).forEach(function(m) { if (lacks(m)) bad = true; });
			}
			if (bad) out.push(r);
		});
		return out;
	}
	function stateItems() {
		var e = obj(S.engine), g = obj(S.global), out = [], v6;
		if (e.failed_open) {
			out.push({ k: 'err', t: '策略路由规则已被清除(故障保护)', b: e.error || '引擎应用失败,且无法恢复上一次配置。', hint: '期间流量按系统默认路由走。修正问题后点击「应用」重新下发规则。' });
		} else if (e.ok === false) {
			out.push({ k: 'err', t: '上次应用失败', b: e.error || '引擎没有给出原因。', hint: '已保持上一次有效配置。修正问题后点击「应用」重试。' });
		}
		arr(S.warnings).forEach(function(w) { out.push({ k: 'warn', t: '读取状态时出现问题', b: String(w), hint: '' }); });
		if (!D.checks && e.rules_error > 0) {
			out.push({ k: 'warn', t: e.rules_error + ' 条规则有错误,没有生效', b: '打开「策略规则」页查看每条规则的原因。', hint: '' });
		}
		v6 = v6Rules();
		if (g.ipv6 !== false && v6.length) {
			out.push({ k: 'info', t: 'IPv6 受出口限制', b: v6.length + ' 条规则指向的出口(或出口组成员)不发放 LAN 前缀,例如「' + (v6[0].name || v6[0].id) + '」。' + ctx.copy.ipv6, hint: '' });
		}
		return out;
	}
	function diagItems() {
		var list = [];
		arr(D.checks).forEach(function(c, i) {
			if (DIAG_LV[c.level]) list.push({ k: DIAG_LV[c.level], t: c.label || c.id || '检查项', b: c.detail || '', hint: c.hint || '', ord: (c.level === 'fail' ? 0 : 1) * 10000 + i });
		});
		list.sort(function(a, b) { return a.ord - b.ord; });
		return list;
	}
	function noteEl(it) {
		return h('div', { class: 'pr-note pr-k-' + it.k + ' pr-ov-item' },
			icon(it.k === 'err' ? 'x-circle' : it.k === 'warn' ? 'warn' : 'info'),
			h('div', { class: 'pr-grow' },
				h('div', { class: 'pr-ov-item-t' }, ui.badge(LV_TXT[it.k], it.k), h('b', null, it.t)),
				it.b ? h('div', { class: 'pr-ov-item-b' }, it.b) : null,
				it.hint ? h('div', { class: 'pr-dim pr-ov-item-h' }, it.hint) : null));
	}
	function renderAttn() {
		var items = stateItems(), dg = diagItems(), more = Math.max(0, dg.length - ATTN_MAX), shown = items.concat(dg.slice(0, ATTN_MAX));
		var sig = JSON.stringify([shown, more, D.loading, D.error ? D.error.message : '', !!D.checks]);
		E.attnRe.disabled = D.loading;
		E.attnSub.textContent = D.loading ? '正在检查…' : D.at ? '诊断检查于 ' + fmt.since(D.at, now()) : '';
		swap(E.attnBody, sig, function(host) {
			shown.forEach(function(it) { host.appendChild(noteEl(it)); });
			if (D.loading && !D.checks) host.appendChild(ui.loading({ rows: 2, text: '正在运行诊断检查…' }));
			else if (D.error && !D.checks) {
				host.appendChild(h('div', { class: 'pr-note pr-k-off pr-ov-item' }, icon('info'),
					h('div', { class: 'pr-grow' }, '诊断检查没有完成:' + D.error.message)));
			} else if (!shown.length) {
				host.appendChild(h('div', { class: 'pr-note pr-k-ok pr-ov-item' }, icon('check-circle'),
					h('div', { class: 'pr-grow' }, h('b', null, '没有发现需要处理的问题'), h('div', { class: 'pr-dim' }, '诊断页还有更详细的检查项。'))));
			}
			if (more > 0 || dg.length) {
				host.appendChild(h('div', { class: 'pr-ov-more' },
					more > 0 ? h('span', { class: 'pr-dim' }, '还有 ' + more + ' 项注意或失败未列出') : null,
					ui.btn({ kind: 'text', size: 'sm', text: '查看全部检查结果', onClick: go('diag') })));
			}
		});
	}
	function runDiag() {
		var my;
		if (D.loading || dead) return;
		my = ++D.seq;
		D.loading = true;
		D.error = null;
		renderAttn();
		api('diag').then(function(j) {
			if (dead || my !== D.seq) return;
			D.checks = arr(j.checks);
			D.at = now();
		}, function(e) {
			if (dead || my !== D.seq) return;
			D.error = e;
		}).then(function() {
			if (dead || my !== D.seq) return;
			D.loading = false;
			renderAttn();
		});
	}

	/* ---------- 当前出口 ---------- */
	function buildExits() {
		E.exSub = h('p', { class: 'pr-hint pr-ov-exh' });
		E.exGrid = h('div', { class: 'pr-grid pr-ov-exits' });
		E.exits = h('section', { class: 'pr-ov-sec', 'aria-labelledby': 'pr-ov-h-ex' },
			h('div', { class: 'pr-sec' },
				h('h2', { class: 'pr-h2', id: 'pr-ov-h-ex' }, '当前出口'),
				ui.btn({ kind: 'text', size: 'sm', text: '管理出口', onClick: go('wans') })),
			E.exSub, E.exGrid);
	}
	function healthNode(w) {
		var hl = obj(w.health);
		if (hl.source !== 'probe') return h('span', { class: 'pr-dim' }, '未启用主动探测');
		if (hl.state === 'up') return ui.dot('ok', '连通' + (hl.rtt_ms != null ? ' · ' + fmt.rtt(hl.rtt_ms) : ''));
		if (hl.state === 'down') return ui.dot('err', '探测不通' + (hl.fails > 0 ? '(连续 ' + hl.fails + ' 次)' : ''));
		return ui.dot('off', '探测结果未知');
	}
	function exitCard(w, uses) {
		var st = WAN_ST[w.state] || ['off', String(w.state || '未知')];
		var caps = obj(w.caps), idle = w.state === 'absent' || w.state === 'disabled' || w.enabled === false;
		var kv = h('dl', { class: 'pr-kv pr-ov-kv' }), v6, warns = arr(w.warnings).slice(0, 2);
		function row(k, v) {
			kv.appendChild(h('dt', null, k));
			kv.appendChild(h('dd', null, v == null || v === '' ? '—' : v));
		}
		if (w.enabled === false && w.state !== 'absent') st = ['off', '已停用'];
		v6 = caps.v6 === true ? '可用' + (w.prefix6 ? '(发放 LAN 前缀)' : '') : '不可用(不发放 LAN 前缀)';
		row('设备', w.dev ? h('span', { class: 'pr-mono' }, w.dev_label || w.dev) : '');
		row('协议', PROTO[w.proto] || w.proto || '');
		row('IPv4 地址', arr(w.addr4)[0] ? h('span', { class: 'pr-mono' }, arr(w.addr4)[0]) : '');
		row('网关', w.gw4 ? h('span', { class: 'pr-mono' }, w.gw4) : '');
		row('IPv6', v6);
		row('规则', uses ? uses + ' 条规则指向此出口' : '没有规则指向此出口');
		return h('article', { class: ['pr-card', 'pr-ov-ex', idle ? 'pr-ov-idle' : ''], 'aria-label': wanTitle(w) },
			h('div', { class: 'pr-ov-ex-h' },
				h('h3', { class: 'pr-ov-ex-n pr-ellip', title: wanTitle(w) }, wanTitle(w)),
				ui.badge(fmt.kind_label(w.kind), 'off'),
				w.is_default ? ui.badge('系统默认路由', 'info') : null),
			h('div', { class: 'pr-ov-ex-s' }, ui.dot(st[0], st[1]), idle ? null : healthNode(w)),
			kv,
			caps.policy === false ? h('div', { class: 'pr-note pr-k-warn pr-ov-ex-w' }, icon('warn'), h('div', { class: 'pr-grow' }, '不能作为策略路由出口' + (caps.reason ? ':' + caps.reason : ''))) : null,
			warns.length ? h('ul', { class: 'pr-ov-ex-l' }, warns.map(function(x) { return h('li', null, String(x)); })) : null);
	}
	function renderExits() {
		var wans = arr(S.wans).slice(), uses = {}, e = obj(S.engine), sig;
		arr(S.rules).forEach(function(r) {
			var t = splitTarget(r.target);
			if (r.enabled !== false && t.type === 'wan') uses[t.id] = (uses[t.id] || 0) + 1;
		});
		wans.sort(function(a, b) {
			var ra = WAN_RANK[a.state], rb = WAN_RANK[b.state];
			ra = ra == null ? 9 : ra;
			rb = rb == null ? 9 : rb;
			return ra !== rb ? ra - rb : (a.prio || 99) - (b.prio || 99);
		});
		E.exSub.textContent = (S.default_dev ? '系统默认路由经 ' + S.default_dev + '。' : '') +
			(e.health_source === 'probe' ? ctx.copy.probe : '') + ' ' + ctx.copy.failover;
		sig = JSON.stringify([wans, uses]);
		swap(E.exGrid, sig, function(host) {
			if (!wans.length) {
				host.appendChild(ui.empty({ icon: 'globe', title: '没有发现出口', body: '出口按真实的网络状态自动发现。接入网线、蜂窝模组或 USB 网卡后会出现在这里。' }));
				return;
			}
			wans.forEach(function(w) { host.appendChild(exitCard(w, uses[w.wan_id] || 0)); });
		});
	}

	/* ---------- 规则命中概览 ---------- */
	function buildRules() {
		E.rulesSub = h('span', { class: 'pr-sub' });
		E.rulesBody = h('div', { class: 'pr-ov-rules' });
		E.rules = h('section', { class: 'pr-card', 'aria-labelledby': 'pr-ov-h-rules' },
			h('div', { class: 'pr-sec' },
				h('div', null, h('h2', { class: 'pr-h2', id: 'pr-ov-h-rules' }, '规则命中'), E.rulesSub),
				h('div', { class: 'pr-row' },
					ui.btn({ kind: 'text', size: 'sm', text: '全部规则', onClick: go('rules') }),
					ui.btn({ kind: 'ghost', size: 'sm', icon: 'plus', text: '新建规则', onClick: go('rules', 'edit=new') }))),
			E.rulesBody);
	}
	function th(text, hidden) {
		return h('th', { role: 'columnheader', scope: 'col' }, hidden ? h('span', { class: 'pr-sr-only' }, text) : text);
	}
	function td(label, kid) { return h('td', { role: 'cell', 'data-label': label || null }, kid); }
	function topTable(list) {
		var rows = list.map(function(r) {
			var hc = td('命中次数', fmt.pkts(obj(r.hits).pkts));
			hitCells[r.id] = hc;
			var rs = RULE_ST.filter(function(x) { return x[0] === r.status; })[0] || ['', 'off', String(r.status || '未知')];
			return h('tr', { role: 'row' },
				td('序号', '#' + r.index),
				td('规则', h('b', { class: 'pr-ov-rn' }, r.name || r.id)),
				td('走向', targetLabel(r.target)),
				hc,
				td('状态', h('span', { title: r.reason || '' }, ui.dot(rs[1], rs[2]))),
				td('', ui.btn({ kind: 'text', size: 'sm', text: '编辑', label: '编辑规则 ' + (r.name || r.id), onClick: go('rules', 'edit=' + encodeURIComponent(r.id)) })));
		});
		return h('table', { class: 'pr-tbl pr-ov-tbl', role: 'table', 'aria-label': '命中次数最多的规则' },
			h('thead', { role: 'rowgroup' }, h('tr', { role: 'row' }, th('序号'), th('规则'), th('走向'), th('命中次数'), th('状态'), th('操作', true))),
			h('tbody', { role: 'rowgroup' }, rows));
	}
	function renderRules() {
		var rules = arr(S.rules), e = obj(S.engine), g = obj(S.global);
		var top = rules.filter(function(r) { return obj(r.hits).pkts > 0; });
		top.sort(function(a, b) { return obj(b.hits).pkts - obj(a.hits).pkts || a.index - b.index; });
		top = top.slice(0, TOP_N);
		/* 签名不含命中数:包数每次轮询都涨,整表重建会丢键盘焦点;命中数改在原位更新 */
		var sig = JSON.stringify([rules.map(function(r) { return [r.id, r.name, r.index, r.target, r.status, r.reason, r.enabled]; }), top.map(function(r) { return r.id; }), arr(S.wans).map(function(w) { return [w.wan_id, w.alias, w.name, w.state]; }), arr(S.groups).map(function(x) { return [x.id, x.name]; }), !!g.enabled, e.rules_active]);
		var sub = '';
		if (e.applied_ts > 0) sub = '最近应用 ' + fmt.since(e.applied_ts, now());
		if (e.counters_since > 0) sub += (sub ? ';' : '') + '计数从 ' + fmt.since(e.counters_since, now()) + ' 开始累计';
		E.rulesSub.textContent = sub;
		swap(E.rulesBody, sig, function(host) {
			var cnt = {}, badges, msg;
			hitCells = {};
			if (!rules.length) {
				host.appendChild(ui.empty({
					icon: 'plus', title: '还没有策略规则',
					body: '策略规则决定哪些设备、应用或目的地的新连接走哪个出口。新建第一条规则后,点「应用」立即生效,或等待 1 分钟内的下次对账自动生效。',
					actions: [{ kind: 'primary', icon: 'plus', text: '新建第一条规则', onClick: go('rules', 'edit=new') }]
				}));
				return;
			}
			rules.forEach(function(r) { cnt[r.status] = (cnt[r.status] || 0) + 1; });
			badges = RULE_ST.filter(function(x) { return cnt[x[0]]; }).map(function(x) { return ui.badge(cnt[x[0]] + ' 条' + x[2], x[1]); });
			host.appendChild(h('div', { class: 'pr-row pr-ov-cnt' }, badges));
			if (top.length) host.appendChild(topTable(top));
			else {
				msg = !g.enabled ? '总开关关闭时规则不会生效,所以没有命中记录。' :
					!e.rules_active ? '还没有生效的规则。' : '暂时没有命中记录。计数只统计应用之后新建立的连接。';
				host.appendChild(h('p', { class: 'pr-dim pr-ov-none' }, msg));
			}
			host.appendChild(h('p', { class: 'pr-hint' }, '命中次数统计被规则匹配的新建连接的数据包,不是该规则转发的总流量。'));
		});
		top.forEach(function(r) {
			var c = hitCells[r.id], t = fmt.pkts(obj(r.hits).pkts);
			if (c && c.textContent !== t) c.textContent = t;
		});
	}

	/* ---------- 全局设置 ---------- */
	function swRow(title, hint, tg, extra) {
		return h('div', { class: 'pr-field pr-ov-sw-row' },
			h('div', { class: 'pr-grow' }, h('span', { class: 'pr-lab' }, title), hint ? h('div', { class: 'pr-hint' }, hint) : null, extra || null), tg);
	}
	function txtRow(id, title, hint, input, err) {
		return h('div', { class: 'pr-field' }, h('label', { class: 'pr-lab', 'for': id }, title), input, h('div', { class: 'pr-hint' }, hint), err);
	}
	function swInput(label) {
		var t = ui.toggle({ checked: false, label: label });
		return { el: t, inp: t.querySelector('input') };
	}
	function buildGlobal() {
		var s1 = swInput('处理 IPv6'), s2 = swInput('域名解析集合'), s3 = swInput('应用后清空加速表');
		F.ipv6 = s1.inp;
		F.dns = s2.inp;
		F.flush = s3.inp;
		F.dnsTtl = h('input', { type: 'text', id: 'pr-ov-f-dnsttl', class: 'pr-in', autocomplete: 'off', spellcheck: 'false', placeholder: '1d' });
		F.learnTtl = h('input', { type: 'text', id: 'pr-ov-f-learnttl', class: 'pr-in', autocomplete: 'off', spellcheck: 'false', placeholder: '30m' });
		F.zones = h('input', { type: 'text', id: 'pr-ov-f-zones', class: 'pr-in', autocomplete: 'off', spellcheck: 'false', placeholder: 'lan' });
		F.eTtl = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		F.eLearn = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		F.eZones = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		F.dnsWarn = h('div', { class: 'pr-fe', hidden: true }, 'dnsmasq 当前不支持 nftset,域名规则不可用。');
		F.flushRow = swRow('应用后清空加速表', ctx.copy.offload + '清除命令沿用「行为管控」的做法,具体清除范围尚未证实,且有短暂改走软件转发的副作用。', s3.el);
		E.gSave = ui.btn({ kind: 'ghost', text: '仅保存', title: '保存到配置,将在 1 分钟内的下次对账自动生效,也可稍后点击「应用」立即生效', onClick: function() { saveGlobal(false); } });
		E.gApply = ui.btn({ kind: 'primary', text: '保存并应用', onClick: function() { saveGlobal(true); } });
		E.gNote = h('span', { class: 'pr-ov-gnote' });
		E.gBody = h('div', { class: 'pr-ov-gbody', id: 'pr-ov-gb', hidden: true },
			swRow('处理 IPv6', ctx.copy.ipv6, s1.el),
			swRow('域名解析集合', '域名规则依赖 dnsmasq 把解析结果写入集合,关闭后域名维度不可用。', s2.el, F.dnsWarn),
			txtRow('pr-ov-f-dnsttl', 'DNS 集合寿命', '域名解析结果在集合里保留的时间,1 小时到 7 天,例如 12h、1d;不要小于 dnsmasq 的缓存时间。', F.dnsTtl, F.eTtl),
			txtRow('pr-ov-f-learnttl', '应用识别地址寿命', '识别出应用后,记住其目的地址的时间,1 分钟到 7 天,例如 30m、2h。', F.learnTtl, F.eLearn),
			txtRow('pr-ov-f-zones', '参与流量的区域', '防火墙区域名,多个用空格或逗号分隔,默认 lan。只有来自这些区域的新连接才会被策略路由评估。', F.zones, F.eZones),
			F.flushRow,
			h('div', { class: 'pr-ov-gact' }, E.gNote, E.gSave, E.gApply));
		E.gBtn = h('button', { type: 'button', class: 'pr-btn pr-text pr-ov-gtog', 'aria-expanded': 'false', 'aria-controls': 'pr-ov-gb', onclick: toggleGlobal },
			h('span', null, '展开'), icon('chevron-down'));
		E.glob = h('section', { class: 'pr-card', 'aria-labelledby': 'pr-ov-h-glob' },
			h('div', { class: 'pr-sec pr-ov-gh' },
				h('div', null, h('h2', { class: 'pr-h2', id: 'pr-ov-h-glob' }, '全局设置'), h('span', { class: 'pr-sub' }, 'IPv6、域名集合、寿命与参与区域')),
				E.gBtn),
			E.gBody);
		E.gBody.addEventListener('input', recheck);
		E.gBody.addEventListener('change', recheck);
	}
	function toggleGlobal() {
		G.open = !G.open;
		E.gBody.hidden = !G.open;
		E.gBtn.setAttribute('aria-expanded', G.open ? 'true' : 'false');
		E.gBtn.firstChild.textContent = G.open ? '收起' : '展开';
	}
	function normGlobal(g) {
		var z = arr(g.src_zones);
		return { ipv6: g.ipv6 !== false, dns_sets: g.dns_sets !== false, flush_accel: g.flush_accel === true,
			dns_ttl: String(g.dns_ttl || '1d'), learn_ttl: String(g.learn_ttl || '30m'), src_zones: z.length ? z.slice() : ['lan'] };
	}
	function readForm() {
		return { ipv6: F.ipv6.checked, dns_sets: F.dns.checked, flush_accel: F.flush.checked,
			dns_ttl: F.dnsTtl.value.trim().toLowerCase(), learn_ttl: F.learnTtl.value.trim().toLowerCase(), src_zones: parseZones(F.zones.value) };
	}
	function fillForm(n) {
		F.ipv6.checked = n.ipv6;
		F.dns.checked = n.dns_sets;
		F.flush.checked = n.flush_accel;
		F.dnsTtl.value = n.dns_ttl;
		F.learnTtl.value = n.learn_ttl;
		F.zones.value = n.src_zones.join(' ');
	}
	function sameForm(a, b) {
		return a.ipv6 === b.ipv6 && a.dns_sets === b.dns_sets && a.flush_accel === b.flush_accel &&
			a.dns_ttl === b.dns_ttl && a.learn_ttl === b.learn_ttl && sameList(a.src_zones, b.src_zones);
	}
	function checkForm(o) {
		var errs = {}, s, i;
		s = ttlSec(o.dns_ttl);
		if (s == null || s < 3600 || s > 604800) errs.dns_ttl = 'DNS 集合寿命要在 1 小时到 7 天之间,例如 12h、1d';
		s = ttlSec(o.learn_ttl);
		if (s == null || s < 60 || s > 604800) errs.learn_ttl = '应用识别地址寿命要在 1 分钟到 7 天之间,例如 30m、2h';
		if (!o.src_zones.length) errs.src_zones = '至少填写一个防火墙区域,例如 lan';
		else if (o.src_zones.length > 16) errs.src_zones = '最多填写 16 个区域';
		else for (i = 0; i < o.src_zones.length; i++) {
			if (o.src_zones[i].length > 64 || !/^[\w-]+$/.test(o.src_zones[i])) { errs.src_zones = '区域名只能包含字母、数字、下划线和连字符:' + o.src_zones[i]; break; }
		}
		return errs;
	}
	function showErrs(errs) {
		[['dns_ttl', F.eTtl, F.dnsTtl], ['learn_ttl', F.eLearn, F.learnTtl], ['src_zones', F.eZones, F.zones]].forEach(function(x) {
			x[1].hidden = !errs[x[0]];
			x[1].textContent = errs[x[0]] || '';
			if (errs[x[0]]) x[2].setAttribute('aria-invalid', 'true'); else x[2].removeAttribute('aria-invalid');
		});
	}
	function recheck() {
		var o = readForm();
		G.dirty = !!G.base && !sameForm(o, G.base);
		ctx.dirty.set('ov-global', G.dirty);
		E.gSave.disabled = G.busy || !G.dirty;
		E.gApply.disabled = G.busy || !G.dirty;
		E.gNote.textContent = G.dirty ? '有未保存的修改' : '';
		if (!G.dirty) showErrs({});
	}
	function syncGlobal() {
		var g = obj(S.global), c = obj(S.caps);
		F.flushRow.hidden = !(c.hnat === true || g.flush_accel === true);
		F.dnsWarn.hidden = c.dns_sets !== false;
		if (G.dirty || G.busy) return;
		G.base = normGlobal(g);
		fillForm(G.base);
		recheck();
	}
	function saveGlobal(apply) {
		var o = readForm(), errs = checkForm(o), first = null;
		if (G.busy) return;
		showErrs(errs);
		if (errs.dns_ttl) first = F.dnsTtl; else if (errs.learn_ttl) first = F.learnTtl; else if (errs.src_zones) first = F.zones;
		if (first) {
			ctx.toast('请先修正标红的设置项', 'warn');
			first.focus();
			return;
		}
		G.busy = true;
		recheck();
		api('global_save', { json: o, apply: apply ? undefined : 0 }, { timeout: APPLY_MS }).then(function(r) {
			G.base = o;
			reportSaved(r, apply, '设置已保存并应用');
		}, function(e) {
			ctx.toast('保存失败:' + e.message, 'err');
		}).then(function() {
			if (dead) return;
			G.busy = false;
			recheck();
			ctx.store.load(true).then(noop, noop);
		});
	}

	/* ---------- 组装 ---------- */
	function update(st) {
		S = st;
		renderBanner();
		renderEngine();
		renderAttn();
		renderExits();
		renderRules();
		syncGlobal();
	}
	function build(st) {
		S = st;
		root.textContent = '';
		if (st.installed === false) {
			root.appendChild(ui.empty({ icon: 'info', title: '策略路由组件未安装', body: '没有找到策略路由引擎(mwrt-pbr),请确认固件包含该组件。' }));
			return;
		}
		buildBanner();
		buildEngine();
		buildQuick();
		buildAttn();
		buildExits();
		buildRules();
		buildGlobal();
		[E.pend, E.eng, E.quick, E.attn, E.exits, E.rules, E.glob].forEach(function(n) { root.appendChild(n); });
		ctx.store.on('change', function(s) { if (!dead && s) update(s); });
		offPend = ctx.pending.on(function() { if (!dead && S) renderBanner(); });
		update(st);
		runDiag();
	}

	start();
	return {
		destroy: function() {
			dead = true;
			ctx.dirty.set('ov-global', false);
			if (offPend) offPend();
		}
	};
}

M.pages.overview = { title: '总览', render: render };
})();
