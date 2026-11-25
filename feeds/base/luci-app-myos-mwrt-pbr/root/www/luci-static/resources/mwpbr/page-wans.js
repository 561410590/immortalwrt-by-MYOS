/*
 * WAN 出口页:出口列表(别名、启停、优先级排序)、运行模式、出口组管理。
 * 动态文本只经 ctx.h 与 textContent 写入;出口、出口组与规则全部来自 state。
 */
(function() {
'use strict';

var M = window.MwrtPbr;
if (!M || !M.pages) return;

var APPLY_MS = 60000;
var MAX_GROUPS = 16;
var MAX_MEMBERS = 8;
var ALIAS_BYTES = 96;
var RULE_SHOW = 5;
var WAN_ST = { online: ['ok', '在线'], offline: ['err', '离线'], disabled: ['off', '接口已停用'], absent: ['off', '设备未接入'] };
var PROTO = { dhcp: 'DHCP', 'static': '静态地址', pppoe: 'PPPoE', dhcpv6: 'DHCPv6' };
var MODE_ASK = {
	multi: {
		title: '切换为多出口模式?',
		body: '多出口模式下,健康监视进程按本页的优先级,在在线可用的出口里选排在最前的一个,维护主路由表的 IPv4 默认路由;它不可用时切到下一个。\n' +
			'只管理主路由表的 IPv4 默认路由,不管理 IPv6;只影响之后新建立的连接,已建立的连接不会迁移。\n' +
			'「网关存活」页里的主口/备口设置在此模式下被忽略。该模式还没有在多条真实 WAN 上实机验证过,请确认你有其它方式登录路由器。',
		okText: '切换为多出口'
	},
	pair: {
		title: '切换回双出口模式?',
		body: '切回后由原有的网关存活守护进程,按「网关存活」页里的主口/备口设置管理默认路由,本页的优先级不再起作用。出口组和策略规则不受影响。',
		okText: '切换回双出口'
	}
};
var OFF_REASON = { '出口组已停用': 1, '被用户停用': 1, '设备未接入': 1, '接口已停用': 1 };

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
function utf8Len(s) {
	var n = 0, i, c;
	for (i = 0; i < s.length; i++) {
		c = s.charCodeAt(i);
		if (c < 0x80) n += 1;
		else if (c < 0x800) n += 2;
		else if (c >= 0xD800 && c <= 0xDBFF) { n += 4; i++; }
		else n += 3;
	}
	return n;
}
function charLen(s) { return s.replace(/[\uD800-\uDBFF][\uDC00-\uDFFF]/g, 'x').length; }
function cleanText(s) { return String(s).replace(/[\u0000-\u001f\u007f]/g, ' ').trim(); }

function render(root, ctx) {
	var h = ctx.h, ui = ctx.ui, fmt = ctx.fmt, api = ctx.api, icon = ctx.icon;
	var dead = false;
	var S = null;
	var E = {};
	var R = {};
	var GE = null;
	var modeBusy = false;
	var offPend = null;

	function now() { return ctx.store.now(); }
	function go(page, query) { return function() { ctx.nav(page, query); }; }
	function swap(host, sig, fill) {
		if (host.__sig === sig) return false;
		host.__sig = sig;
		host.textContent = '';
		fill(host);
		return true;
	}
	function focusNode(n) {
		if (!n) return;
		try { n.focus({ preventScroll: true }); } catch (e) { try { n.focus(); } catch (x) {} }
	}
	function activeNode() {
		var r = root.getRootNode ? root.getRootNode() : null;
		return r && r.activeElement ? r.activeElement : null;
	}
	/* wan_save(别名)/wan_order 不会应用,但内核 api 会把"未应用"标记清掉;已有标记时原样恢复 */
	function apiKeep(fn, data, opts) {
		var had = ctx.pending.has();
		return api(fn, data, opts).then(function(r) {
			if (had) ctx.pending.set(true);
			return r;
		});
	}
	function wanById(id) { return byKey(arr(S.wans), 'wan_id', id); }
	function mode() { return obj(S.engine).mode === 'multi' ? 'multi' : 'pair'; }

	/* 谁在用这个出口/出口组 */
	function usage() {
		var u = { wr: {}, wg: {}, gr: {} };
		arr(S.rules).forEach(function(r) {
			var t = splitTarget(r.target);
			if (t.type === 'wan') (u.wr[t.id] = u.wr[t.id] || []).push(r);
			else if (t.type === 'group') (u.gr[t.id] = u.gr[t.id] || []).push(r);
		});
		arr(S.groups).forEach(function(g) {
			arr(g.members).forEach(function(m) { (u.wg[m] = u.wg[m] || []).push(g); });
		});
		return u;
	}
	/* 与引擎的成员可用性判断一致(compile.uc member_info);"可用"只表示满足选用条件,不代表当前正在使用 */
	function memberInfo(wid, grpOff) {
		var w = wanById(wid), hd, linked, alive, enabled, reason = '', usable;
		if (!w) return { w: null, usable: false, reason: grpOff ? '出口组已停用' : '设备未接入' };
		enabled = w.enabled !== false;
		linked = w.state === 'online' && w.up === true;
		hd = obj(w.health).state === 'down';
		alive = !grpOff && enabled && linked && !hd;
		usable = alive && w.route4 === true;
		if (grpOff) reason = '出口组已停用';
		else if (!enabled) reason = '被用户停用';
		else if (w.state === 'absent') reason = '设备未接入';
		else if (w.state === 'disabled') reason = '接口已停用';
		else if (!linked) reason = '链路断开';
		else if (hd) reason = '探测失败';
		else if (w.route4 !== true) reason = '无默认路由';
		return { w: w, usable: usable, reason: reason };
	}
	function memberDot(mi) {
		return mi.usable ? ui.dot('ok', '可用') : ui.dot(OFF_REASON[mi.reason] ? 'off' : 'err', '不可用:' + mi.reason);
	}
	function memberName(id) {
		var w = wanById(id);
		return w ? wanTitle(w) : id + '(设备未接入)';
	}

	/* ---------- 加载与错误 ---------- */
	function showLoading() {
		root.textContent = '';
		root.appendChild(ui.loading({ rows: 4, text: '正在读取出口列表…' }));
	}
	function showError(e) {
		root.textContent = '';
		root.appendChild(ui.error({ title: '无法读取出口列表', message: e && e.message, onRetry: start }));
	}
	function start() {
		showLoading();
		ctx.store.load().then(function(st) {
			if (!dead) build(st);
		}, function(e) {
			if (!dead) showError(e);
		});
	}

	/* ---------- 未应用提示 ---------- */
	function buildBanner() {
		E.pendMsg = h('div', { class: 'pr-grow' });
		E.pendBtn = ui.btn({ kind: 'primary', size: 'sm', text: '应用', onClick: doApply });
		E.pend = h('div', { class: 'pr-note pr-k-warn pr-wn-pend', role: 'status', hidden: true }, icon('warn'), E.pendMsg, E.pendBtn);
	}
	function renderBanner() {
		var has = ctx.pending.has(), e = obj(S.engine), np = 0, msg = '';
		if (obj(S.global).enabled === true) arr(S.rules).forEach(function(r) { if (r.status === 'pending') np++; });
		if (has) msg = '有更改已保存但尚未应用,规则还没有作用到网络。';
		else if (np > 0) msg = np + ' 条规则尚未应用。';
		else if (e.pending === true) msg = '已保存的配置可能比运行中的规则新(按配置文件时间判断,路由器时钟不准时可能误报)。点击应用可让两者一致。';
		E.pend.hidden = !msg;
		E.pendMsg.textContent = msg;
	}
	function doApply() {
		E.pendBtn.disabled = true;
		return ctx.apply().then(function() {
			if (!dead) E.pendBtn.disabled = false;
		});
	}
	/* 保存后的统一反馈;applied 是控制器随响应带回的引擎应用结果,extra 是校验给出的提示 */
	function reportSaved(r, wantApply, okText, extra) {
		var a = obj(r.applied), w, tail = extra ? '(提示:' + extra + ')' : '';
		if (!wantApply) {
			ctx.toast('已保存,将在 1 分钟内的下次对账自动生效,也可点「应用」立即生效。' + tail, 'info');
			return;
		}
		if (a.ok === false) {
			ctx.pending.set(true);
			ctx.toast('已保存,但应用失败:' + (a.error || '引擎没有给出原因') + ' ' +
				(a.failed_open ? '恢复上一次配置也失败,已清除策略路由规则,流量按系统默认路由走。' : '已保持上一次有效配置。'), 'err');
			return;
		}
		w = arr(a.warnings);
		ctx.toast(okText + tail + (w.length ? '。注意:' + w[0] + (w.length > 1 ? '(另有 ' + (w.length - 1) + ' 条提示)' : '') : ''), w.length || tail ? 'warn' : 'ok');
	}

	/* ---------- WAN 出口:运行模式 ---------- */
	function buildMode() {
		E.mPair = h('button', { type: 'button', 'aria-pressed': 'false', onclick: function() { askMode('pair'); } }, '双出口');
		E.mMulti = h('button', { type: 'button', 'aria-pressed': 'false', onclick: function() { askMode('multi'); } }, '多出口');
		E.mDesc = h('p', { class: 'pr-wn-md-d' });
		E.mWarnT = h('div', { class: 'pr-grow' });
		E.mWarn = h('div', { class: 'pr-note pr-k-warn', role: 'status', hidden: true }, icon('warn'), E.mWarnT);
		E.mode = h('div', { class: 'pr-wn-mode' },
			h('div', { class: 'pr-wn-mode-h' },
				h('span', { class: 'pr-lab', id: 'pr-wn-mode-l' }, '运行模式'),
				h('div', { class: 'pr-seg', role: 'group', 'aria-labelledby': 'pr-wn-mode-l' }, E.mPair, E.mMulti)),
			E.mDesc, E.mWarn);
	}
	function syncMode() {
		var m = mode(), e = obj(S.engine);
		E.mPair.setAttribute('aria-pressed', m === 'pair' ? 'true' : 'false');
		E.mMulti.setAttribute('aria-pressed', m === 'multi' ? 'true' : 'false');
		E.mPair.disabled = modeBusy;
		E.mMulti.disabled = modeBusy;
		E.mDesc.textContent = m === 'multi' ?
			'多出口(优先级):按下方的优先级,在在线可用的出口里选排在最前的一个,维护主路由表的 IPv4 默认路由。只管理 IPv4 的主表默认路由,不管理 IPv6;只影响之后新建立的连接,已建立的连接不会迁移;不是负载均衡,也不叠加带宽。' :
			'双出口(主备):由原有的网关存活守护进程按「网关存活」页的主口/备口设置管理默认路由。下方的优先级在这个模式下暂不起作用,切换到多出口后才用来决定默认路由。';
		E.mWarn.hidden = !(m === 'multi' && e.wanmon === false);
		E.mWarnT.textContent = '已选择多出口模式,但健康监视进程没有在运行,默认路由不会按优先级调整。请确认「网关存活」总开关已开启;仍不运行请到诊断页查看。';
	}
	function askMode(next) {
		if (next === mode() || modeBusy) return;
		ctx.confirmBox(MODE_ASK[next]).then(function(ok) {
			if (!ok || dead) return null;
			modeBusy = true;
			syncMode();
			return api('wan_mode', { mode: next }, { timeout: APPLY_MS }).then(function(r) {
				var ap = obj(obj(r).applied);
				if (ap.ok === false) ctx.toast('运行模式已保存,但策略规则应用失败:' + (ap.error || '引擎没有给出原因'), 'err');
				else ctx.toast(next === 'multi' ? '已切换为多出口模式。只影响之后新建立的连接。' : '已切换为双出口模式。', 'ok');
			}, function(e) {
				ctx.toast('切换运行模式失败:' + e.message, 'err');
			}).then(function() {
				modeBusy = false;
				if (dead) return;
				syncMode();
				ctx.store.load(true).then(noop, noop);
			});
		});
	}

	/* ---------- WAN 出口:列表行 ---------- */
	function healthView(w) {
		var hl = obj(w.health), t;
		if (w.state === 'absent') return ['off', '—'];
		if (hl.source !== 'probe') return ['off', '未探测,只依据链路与路由判断'];
		t = hl.ts ? ' · ' + fmt.since(hl.ts, now()) + '探测' : '';
		if (hl.state === 'up') return ['ok', '探测通过' + (hl.rtt_ms != null ? ' · ' + fmt.rtt(hl.rtt_ms) : '') + t];
		if (hl.state === 'down') return ['err', '探测失败' + (hl.fails > 0 ? '(连续 ' + hl.fails + ' 次)' : '') + t];
		return ['off', '探测结果未知' + t];
	}
	function stateHint(w) {
		if (w.state === 'absent') return '设备当前没有接入(未插入、未被识别或驱动未加载)。接入后会自动恢复,别名与顺序会保留。';
		if (w.state === 'disabled') return '对应的网络接口在系统配置里被停用。';
		if (w.state === 'offline') return w.up !== true ? '链路断开,或接口还没有连上。' : '接口已启动,但没有默认路由。';
		return '';
	}
	function routeText(w) {
		if (w.state === 'absent') return '—';
		if (w.route4 === true) return 'IPv4 默认路由 · metric ' + (w.metric != null ? w.metric : 0);
		if (w.route6 === true) return '只有 IPv6 默认路由 · metric ' + (w.metric != null ? w.metric : 0);
		return '没有默认路由';
	}
	function zoneText(w) {
		if (w.state === 'absent') return '—';
		if (w.in_zone === true) return 'wan 区' + (w.zone ? '(' + w.zone + ')' : '');
		return w.zone ? '在「' + w.zone + '」区,不在 wan 区' : '不属于任何防火墙区';
	}
	function v6Text(w) {
		if (w.state === 'absent') return '—';
		if (obj(w.caps).v6 === true) return '可用,发放 LAN 前缀' + (w.prefix6 ? ' ' + w.prefix6 : '');
		return '不可用(不发放 LAN 前缀)';
	}
	function useText(id, u) {
		var nr = arr(u.wr[id]).length, ng = arr(u.wg[id]).length, p = [];
		if (nr) p.push(nr + ' 条规则');
		if (ng) p.push(ng + ' 个出口组');
		return p.length ? p.join('、') : '没有被引用';
	}

	function domRows() {
		var out = [], c = E.list.children, i;
		for (i = 0; i < c.length; i++) if (c[i].hasAttribute('data-id')) out.push(c[i]);
		return out;
	}
	function renumber() {
		var rows = domRows(), n = rows.length;
		rows.forEach(function(el, i) {
			var r = R[el.getAttribute('data-id')], t;
			if (!r) return;
			t = wanTitle(r.w);
			r.rank.textContent = String(i + 1);
			r.up.disabled = i === 0;
			r.down.disabled = i === n - 1;
			r.up.setAttribute('aria-label', '上移「' + t + '」,当前第 ' + (i + 1) + ' 位,共 ' + n + ' 位');
			r.down.setAttribute('aria-label', '下移「' + t + '」,当前第 ' + (i + 1) + ' 位,共 ' + n + ' 位');
			r.grip.setAttribute('aria-label', '调整「' + t + '」的优先级,当前第 ' + (i + 1) + ' 位。拖动,或按上下方向键');
		});
	}
	/* ▲▼ 借用拖拽排序自带的键盘通道(与拖动共用同一条保存队列与失败回滚) */
	function nudge(grip, key, btn, other, after) {
		var ev;
		try { ev = new KeyboardEvent('keydown', { key: key, bubbles: true, cancelable: true }); } catch (e) { return; }
		grip.dispatchEvent(ev);
		after();
		focusNode(btn.disabled ? other : btn);
	}
	function onOrder(order) {
		renumber();
		E.ordNote.hidden = false;
		E.ordNote.textContent = '正在保存顺序…';
		return apiKeep('wan_order', { ids: order }).then(function() {
			if (dead) return;
			E.ordNote.textContent = mode() === 'multi' ?
				'顺序已保存。多出口模式的监视进程在下一轮出口发现时采用新顺序(最迟约 30 秒)。' :
				'顺序已保存。当前是双出口模式,这个顺序暂不起作用。';
		}, function(e) {
			if (!dead) E.ordNote.textContent = '顺序没有保存成功,已恢复原来的排列。';
			throw e;
		});
	}

	function makeRow(w) {
		var r = { id: w.wan_id, w: w, f: {}, busy: false, editing: false }, swl;
		function fld(key, label, cls) {
			var v = h('span', { class: 'pr-wn-fv' + (cls ? ' ' + cls : '') });
			r.f[key] = v;
			return h('div', { class: 'pr-wn-f' }, h('span', { class: 'pr-wn-fk' }, label), v);
		}
		r.rank = h('span', { class: 'pr-wn-rank', 'aria-hidden': 'true' });
		r.grip = ui.grip('调整优先级');
		r.up = ui.btn({ kind: 'icon', icon: 'arrow-up', cls: 'pr-wn-b', title: '上移', onClick: function() {
			nudge(r.grip, 'ArrowUp', r.up, r.down, renumber);
		} });
		r.down = ui.btn({ kind: 'icon', icon: 'arrow-down', cls: 'pr-wn-b', title: '下移', onClick: function() {
			nudge(r.grip, 'ArrowDown', r.down, r.up, renumber);
		} });
		r.ctl = h('div', { class: 'pr-wn-ctl' }, r.rank, r.grip, r.up, r.down);
		r.name = h('span', { class: 'pr-wn-name' });
		r.edit = ui.btn({ kind: 'icon', icon: 'edit', cls: 'pr-wn-ed', title: '修改别名', onClick: function() { startEdit(r); } });
		r.kind = h('span', { class: 'pr-badge pr-k-off' });
		r.dflt = h('span', { class: 'pr-badge pr-k-info', hidden: true }, '系统默认路由');
		r.orig = h('div', { class: 'pr-wn-orig pr-dim', hidden: true });
		r.inp = h('input', { type: 'text', class: 'pr-in', autocomplete: 'off', spellcheck: 'false', 'aria-label': '出口别名' });
		r.edErr = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		r.edSave = ui.btn({ kind: 'primary', size: 'sm', text: '保存', onClick: function() { saveAlias(r); } });
		r.edCancel = ui.btn({ kind: 'ghost', size: 'sm', text: '取消', onClick: function() { endEdit(r); focusNode(r.edit); } });
		r.ed = h('div', { class: 'pr-wn-edit', hidden: true },
			h('div', { class: 'pr-wn-edit-r' }, r.inp, r.edSave, r.edCancel),
			h('div', { class: 'pr-hint' }, '留空恢复默认名称。别名只用于显示,最多约 32 个汉字。'),
			r.edErr);
		r.inp.addEventListener('input', function() {
			ctx.dirty.set('wn-alias:' + r.id, r.inp.value.trim() !== (r.w.alias || ''));
			r.edErr.hidden = true;
		});
		r.inp.addEventListener('keydown', function(e) {
			if (e.key === 'Enter') { e.preventDefault(); saveAlias(r); }
			else if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); endEdit(r); focusNode(r.edit); }
		});
		r.stDot = h('i', { class: 'pr-dot', 'aria-hidden': 'true' });
		r.stT = h('span', null);
		r.st = h('span', { class: 'pr-st pr-k-off' }, r.stDot, r.stT);
		r.offB = h('span', { class: 'pr-badge pr-k-off', hidden: true }, '已停用');
		r.sw = ui.toggle({ checked: w.enabled !== false, label: '启用出口', onChange: function(want) { return onEnable(r, want); } });
		r.swInp = r.sw.querySelector('input');
		swl = h('span', { class: 'pr-wn-swl', 'aria-hidden': 'true' }, '启用');
		r.hDot = h('i', { class: 'pr-dot', 'aria-hidden': 'true' });
		r.hT = h('span', null);
		r.hSt = h('span', { class: 'pr-st pr-k-off' }, r.hDot, r.hT);
		r.hint = h('p', { class: 'pr-wn-hint', hidden: true });
		r.warn = h('ul', { class: 'pr-wn-warn', hidden: true });
		r.el = h('li', { class: 'pr-wn-row', 'data-id': w.wan_id },
			h('div', { class: 'pr-wn-top' },
				r.ctl,
				h('div', { class: 'pr-wn-id pr-grow' },
					h('div', { class: 'pr-wn-t' }, r.name, r.edit, r.kind, r.dflt),
					r.orig, r.ed),
				h('div', { class: 'pr-wn-sd' }, r.st, r.offB),
				h('div', { class: 'pr-wn-sw' }, swl, r.sw)),
			r.hint,
			h('div', { class: 'pr-wn-kv' },
				fld('iface', '逻辑接口', 'pr-mono'), fld('dev', '物理设备', 'pr-mono'), fld('proto', '协议'),
				fld('gw', '网关', 'pr-mono'), fld('route', '默认路由'), fld('v6', 'IPv6'),
				fld('zone', '防火墙'), h('div', { class: 'pr-wn-f' }, h('span', { class: 'pr-wn-fk' }, '健康探测'), r.hSt),
				fld('use', '被引用')),
			r.warn);
		r.swInp.setAttribute('aria-label', '启用出口');
		return r;
	}
	function patchRow(r, w, u, single) {
		var st = WAN_ST[w.state] || ['off', String(w.state || '未知')], title = wanTitle(w), caps = obj(w.caps);
		var userOff = w.enabled === false, hv = healthView(w), hint = stateHint(w), warns = [], sig, gw;
		r.w = w;
		r.el.setAttribute('data-name', title);
		r.el.classList.toggle('pr-wn-absent', w.state === 'absent');
		r.el.classList.toggle('pr-wn-idle', w.state === 'absent' || userOff);
		r.ctl.hidden = single;
		r.name.textContent = title;
		r.edit.setAttribute('aria-label', '修改「' + title + '」的别名');
		r.orig.hidden = !w.alias;
		r.orig.textContent = w.alias ? '原名:' + (w.name || w.wan_id) : '';
		r.kind.textContent = fmt.kind_label(w.kind);
		r.dflt.hidden = w.is_default !== true;
		r.st.className = 'pr-st pr-k-' + st[0];
		r.stT.textContent = st[1];
		r.offB.hidden = !userOff;
		if (!r.sw.classList.contains('pr-busy')) r.swInp.checked = !userOff;
		r.swInp.setAttribute('aria-label', '启用出口「' + title + '」');
		r.hint.hidden = !hint;
		r.hint.textContent = hint;
		gw = w.gw4 || '';
		if (w.gw6) gw += (gw ? ' · IPv6 ' : 'IPv6 ') + w.gw6;
		r.f.iface.textContent = arr(w.ifaces).join(' / ') || '—';
		r.f.dev.textContent = w.dev ? (w.dev_label || w.dev) + (w.under && w.under !== w.dev ? '(底层 ' + w.under + ')' : '') : '—';
		r.f.proto.textContent = PROTO[w.proto] || w.proto || '—';
		r.f.gw.textContent = w.state === 'absent' ? '—' : gw || '—';
		r.f.route.textContent = routeText(w);
		r.f.v6.textContent = v6Text(w);
		r.f.zone.textContent = zoneText(w);
		r.f.zone.classList.toggle('pr-wn-warnv', w.in_zone !== true && w.state !== 'absent');
		r.f.use.textContent = useText(w.wan_id, u);
		r.hSt.className = 'pr-st pr-k-' + hv[0];
		r.hT.textContent = hv[1];
		if (caps.policy === false) warns.push('不能作为策略路由出口' + (caps.reason ? ':' + caps.reason : ''));
		arr(w.warnings).forEach(function(x) { warns.push(String(x)); });
		sig = warns.join('\n');
		if (r.warnSig !== sig) {
			r.warnSig = sig;
			r.warn.textContent = '';
			warns.forEach(function(x) { r.warn.appendChild(h('li', null, icon('warn'), h('span', null, x))); });
			r.warn.hidden = !warns.length;
		}
	}

	/* 别名 */
	function startEdit(r) {
		r.editing = true;
		r.inp.value = r.w.alias || '';
		r.inp.setAttribute('placeholder', r.w.name || '');
		r.edErr.hidden = true;
		r.ed.hidden = false;
		r.el.classList.add('pr-wn-editing');
		focusNode(r.inp);
		try { r.inp.select(); } catch (e) {}
	}
	function endEdit(r) {
		r.editing = false;
		r.busy = false;
		r.ed.hidden = true;
		r.el.classList.remove('pr-wn-editing');
		r.edSave.disabled = false;
		r.edCancel.disabled = false;
		ctx.dirty.set('wn-alias:' + r.id, false);
	}
	function saveAlias(r) {
		var v = cleanText(r.inp.value);
		if (r.busy) return;
		if (utf8Len(v) > ALIAS_BYTES) {
			r.edErr.textContent = '别名太长:最多 96 字节,约 32 个汉字。';
			r.edErr.hidden = false;
			r.inp.setAttribute('aria-invalid', 'true');
			focusNode(r.inp);
			return;
		}
		r.inp.removeAttribute('aria-invalid');
		if (v === (r.w.alias || '')) { endEdit(r); focusNode(r.edit); return; }
		r.busy = true;
		r.edSave.disabled = true;
		r.edCancel.disabled = true;
		apiKeep('wan_save', { wan_id: r.id, alias: v }).then(function() {
			if (dead) return;
			endEdit(r);
			focusNode(r.edit);
			ctx.toast(v ? '别名已保存' : '已恢复默认名称', 'ok');
			ctx.store.load(true).then(noop, noop);
		}, function(e) {
			if (dead) return;
			r.busy = false;
			r.edSave.disabled = false;
			r.edCancel.disabled = false;
			r.edErr.textContent = '保存失败:' + e.message;
			r.edErr.hidden = false;
		});
	}

	/* 启用/停用 */
	function onEnable(r, want) {
		var u = usage(), id = r.id, w = r.w, title = wanTitle(w), nr = arr(u.wr[id]).length, ng = arr(u.wg[id]).length, ask, body = [], others;
		if (!want) {
			others = arr(S.wans).filter(function(x) { return x.wan_id !== id && x.enabled !== false && x.state === 'online'; }).length;
			if (nr + ng) body.push('「' + title + '」正被 ' + useText(id, u) + ' 使用。停用后它不再被选作出口;指向它的规则按各自的「不可用时」设置处理(回落到系统默认路由,或阻断)。');
			if (w.state === 'online' && !others) body.push('它是目前唯一在线可用的出口,停用后没有出口可被策略路由选用。');
			if (body.length) body.push('停用只影响策略路由(多出口模式下也包括默认路由的优先级选择),不会关闭网络接口本身。');
		}
		ask = body.length ? ctx.confirmBox({ title: '停用出口「' + title + '」?', body: body.join('\n'), okText: '停用并应用', danger: true }) : Promise.resolve(true);
		return ask.then(function(ok) {
			if (!ok) return false;
			return api('wan_save', { wan_id: id, enabled: want }, { timeout: APPLY_MS }).then(function(res) {
				reportSaved(res, true, want ? '已启用「' + title + '」并应用' : '已停用「' + title + '」并应用');
				ctx.store.load(true).then(noop, noop);
				return true;
			});
		});
	}

	/* ---------- WAN 出口:区块 ---------- */
	function buildWans() {
		E.wSub = h('span', { class: 'pr-sub' });
		E.ordHint = h('p', { class: 'pr-hint pr-wn-ordhint' });
		E.ordNote = h('p', { class: 'pr-hint pr-wn-ordnote', role: 'status' });
		E.single = h('div', { class: 'pr-note pr-k-info', hidden: true }, icon('info'),
			h('div', { class: 'pr-grow' }, '目前只有一个出口,不需要设置优先级和运行模式。接入第二个出口(网线、蜂窝模组、USB 网卡等)后,这里会出现排序与运行模式设置。'));
		E.empty = ui.empty({ icon: 'globe', title: '没有发现出口', body: '出口按设备和接口的真实状态自动发现。接入网线、蜂窝模组或 USB 网卡后会出现在这里。' });
		E.list = h('ol', { class: 'pr-wn-list', role: 'list', 'aria-label': 'WAN 出口,按优先级从高到低' });
		buildMode();
		ctx.dragSort(E.list, {
			onOrder: onOrder,
			disabled: function() { return domRows().length < 2; },
			disabledMsg: '只有一个出口,不需要调整顺序'
		});
		E.wans = h('section', { class: 'pr-card pr-wn-card', 'aria-labelledby': 'pr-wn-h-wan' },
			h('div', { class: 'pr-sec' },
				h('div', null, h('h2', { class: 'pr-h2', id: 'pr-wn-h-wan' }, 'WAN 出口'), E.wSub)),
			E.mode, E.single, E.empty, E.ordHint, E.list, E.ordNote);
	}
	function renderWans() {
		var wans = arr(S.wans), single = wans.length < 2, u = usage(), seen = {}, ref, on = 0, ab = 0, m = mode(), txt;
		if (E.list.classList.contains('pr-dragging')) return;
		wans.forEach(function(w) {
			var r = R[w.wan_id];
			if (!r) { r = R[w.wan_id] = makeRow(w); }
			seen[w.wan_id] = 1;
			patchRow(r, w, u, single);
			if (w.state === 'online' && w.enabled !== false) on++;
			if (w.state === 'absent') ab++;
		});
		Object.keys(R).forEach(function(id) {
			if (seen[id]) return;
			ctx.dirty.set('wn-alias:' + id, false);
			if (R[id].el.parentNode) R[id].el.parentNode.removeChild(R[id].el);
			delete R[id];
		});
		ref = E.list.firstChild;
		wans.forEach(function(w) {
			var el = R[w.wan_id].el;
			if (el === ref) ref = ref.nextSibling;
			else E.list.insertBefore(el, ref);
		});
		renumber();
		E.empty.hidden = wans.length > 0;
		E.list.hidden = !wans.length;
		E.single.hidden = wans.length !== 1;
		E.mode.hidden = single && m !== 'multi';
		E.ordHint.hidden = single;
		E.ordNote.hidden = single || !E.ordNote.textContent;
		E.ordHint.textContent = m === 'multi' ?
			'优先级从上到下由高到低:排在前面且可用的出口承担默认路由。拖动左侧手柄、点上移/下移,或在手柄上按上下方向键调整。' :
			'优先级从上到下由高到低。当前是双出口模式,这个顺序暂不起作用,但会先保存下来;切换到多出口模式后才用来决定默认路由。拖动左侧手柄、点上移/下移,或在手柄上按上下方向键调整。';
		txt = '共 ' + wans.length + ' 个出口,' + on + ' 个在线可用' + (ab ? ',' + ab + ' 个设备未接入' : '');
		if (S.default_dev) txt += ';系统默认路由经 ' + S.default_dev;
		E.wSub.textContent = txt;
		syncMode();
	}

	/* ---------- 出口组:列表 ---------- */
	function buildGroups() {
		E.gSub = h('span', { class: 'pr-sub' });
		E.gNew = ui.btn({ kind: 'primary', size: 'sm', icon: 'plus', text: '新建出口组', onClick: function() { openGroup(null); } });
		E.gList = h('div', { class: 'pr-gp-list' });
		E.groups = h('section', { class: 'pr-card pr-gp-card-sec', 'aria-labelledby': 'pr-wn-h-gp' },
			h('div', { class: 'pr-sec' },
				h('div', null, h('h2', { class: 'pr-h2', id: 'pr-wn-h-gp' }, '出口组'), E.gSub),
				E.gNew),
			h('p', { class: 'pr-hint pr-gp-sem' },
				'出口组是一组有序的出口:按成员顺序选用第一个可用的成员,第一个为主,其余为备用;主出口恢复后会回到主出口。' +
				'可用指:在线、链路正常、有 IPv4 默认路由、没有被停用、探测没有判定失败。全部成员都不可用时,按规则里的「不可用时」设置处理:回落到系统默认路由,或阻断。' + ctx.copy.failover),
			E.gList);
	}
	function focusKey() {
		var a = activeNode(), n = a, card;
		while (n && n !== E.gList) {
			if (n.getAttribute && n.getAttribute('data-gid')) { card = n; break; }
			n = n.parentNode;
		}
		return card && a.getAttribute && a.getAttribute('data-act') ? card.getAttribute('data-gid') + '|' + a.getAttribute('data-act') : '';
	}
	function ruleLink(r) {
		return ui.btn({ kind: 'text', size: 'sm', text: r.name || r.id, title: '打开规则「' + (r.name || r.id) + '」', onClick: go('rules', 'edit=' + encodeURIComponent(r.id)) });
	}
	function groupCard(g, rules) {
		var off = g.enabled === false, members = arr(g.members), first = null, mem, sum, use, note = String(g.note || '').trim(), mi, shown;
		var name = g.name || g.id, bEdit, bDel;
		mem = h('ol', { class: 'pr-gp-mem', role: 'list', 'aria-label': '成员,按优先级从高到低' });
		members.forEach(function(id, idx) {
			mi = memberInfo(id, off);
			if (!first && mi.usable) first = id;
			mem.appendChild(h('li', { class: 'pr-gp-mi' },
				h('span', { class: 'pr-gp-role' + (idx === 0 ? ' pr-gp-main' : '') }, idx === 0 ? '主' : '备 ' + idx),
				h('span', { class: 'pr-gp-mn' }, memberName(id)),
				memberDot(mi)));
		});
		if (!members.length) mem.appendChild(h('li', { class: 'pr-dim' }, '没有成员'));
		if (off) sum = h('div', { class: 'pr-gp-sum' }, ui.dot('off', '出口组已停用,所有成员都视为不可用'));
		else if (first) sum = h('div', { class: 'pr-gp-sum' }, ui.dot('ok', '首个可用成员:' + memberName(first)));
		else sum = h('div', { class: 'pr-gp-sum' }, ui.dot('err', '当前没有可用成员'),
			h('span', { class: 'pr-dim' }, rules.length ? ',指向本组的规则按各自的「不可用时」设置处理' : ''));
		if (rules.length) {
			shown = rules.slice(0, RULE_SHOW);
			use = h('div', { class: 'pr-gp-use' }, h('span', { class: 'pr-dim' }, '被 ' + rules.length + ' 条规则使用:'),
				shown.map(ruleLink),
				rules.length > shown.length ? h('span', { class: 'pr-dim' }, '等共 ' + rules.length + ' 条') : null);
		} else use = h('div', { class: 'pr-gp-use pr-dim' }, '没有规则使用这个出口组');
		bEdit = ui.btn({ kind: 'ghost', size: 'sm', icon: 'edit', text: '编辑', label: '编辑出口组 ' + name, onClick: function() { openGroup(g.id); } });
		bDel = ui.btn({ kind: 'text', size: 'sm', icon: 'trash', text: '删除', label: '删除出口组 ' + name, onClick: function() { delGroup(g.id); } });
		bEdit.setAttribute('data-act', 'edit');
		bDel.setAttribute('data-act', 'del');
		return h('article', { class: ['pr-gp-card', off ? 'pr-gp-off' : ''], 'data-gid': g.id, 'aria-label': '出口组 ' + name },
			h('div', { class: 'pr-gp-h' },
				h('h3', { class: 'pr-gp-n' }, name),
				off ? ui.badge('已停用', 'off') : null,
				h('div', { class: 'pr-gp-act' }, bEdit, bDel)),
			mem, sum, note ? h('p', { class: 'pr-gp-note pr-dim' }, note) : null, use);
	}
	function renderGroups() {
		var groups = arr(S.groups), u = usage(), key = focusKey(), sig, full = groups.length >= MAX_GROUPS, k;
		sig = JSON.stringify([groups.map(function(g) {
			return [g.id, g.name, g.enabled, g.note, arr(g.members).map(function(id) {
				var mi = memberInfo(id, g.enabled === false);
				return [id, mi.w ? wanTitle(mi.w) : '', mi.usable, mi.reason];
			}), arr(u.gr[g.id]).map(function(r) { return [r.id, r.name]; })];
		})]);
		E.gSub.textContent = groups.length ? '共 ' + groups.length + ' 个,最多 ' + MAX_GROUPS + ' 个' : '';
		E.gNew.disabled = full;
		E.gNew.title = full ? '出口组已达上限(' + MAX_GROUPS + ' 个)' : '';
		if (!swap(E.gList, sig, function(host) {
			if (!groups.length) {
				host.appendChild(ui.empty({
					icon: 'plus', title: '还没有出口组',
					body: '出口组把几个出口排成主备:主出口可用就走主出口,不可用时按顺序换到备用出口,只影响之后新建立的连接。规则指向出口组后才会用到它。',
					actions: [{ kind: 'primary', icon: 'plus', text: '新建第一个出口组', onClick: function() { openGroup(null); } }]
				}));
				return;
			}
			groups.forEach(function(g) { host.appendChild(groupCard(g, arr(u.gr[g.id]))); });
		}) || !key) return;
		/* 列表被重建后把焦点还给原来的按钮 */
		k = key.split('|');
		E.gList.querySelectorAll('.pr-gp-card').forEach(function(c) {
			if (c.getAttribute('data-gid') === k[0]) focusNode(c.querySelector('[data-act="' + k[1] + '"]'));
		});
	}

	/* 删除 */
	function blockedModal(g, rules) {
		var list = rules.map(function(r) {
			return h('li', { class: 'pr-gp-rli' }, h('span', { class: 'pr-gp-rn' }, r.name || r.id),
				ui.btn({ kind: 'text', size: 'sm', text: '去编辑规则', label: '编辑规则 ' + (r.name || r.id), onClick: go('rules', 'edit=' + encodeURIComponent(r.id)) }));
		});
		ctx.modal({
			title: '无法删除出口组', size: 'sm',
			body: h('div', null,
				h('p', { class: 'pr-p' }, '出口组「' + (g.name || g.id) + '」仍被 ' + rules.length + ' 条规则使用。请先把这些规则改到其它出口或出口组(或删除规则),再删除出口组:'),
				h('ul', { class: 'pr-gp-rl' }, list)),
			actions: [{ text: '知道了', kind: 'primary', autofocus: true }]
		});
	}
	function delGroup(id) {
		var g = byKey(arr(S.groups), 'id', id), u = usage(), rules;
		if (!g) return;
		rules = arr(u.gr[id]);
		if (rules.length) { blockedModal(g, rules); return; }
		ctx.confirmBox({
			title: '删除出口组「' + (g.name || g.id) + '」?',
			body: '删除后不能恢复。没有规则使用它,现有规则不受影响。',
			okText: '删除并应用', danger: true
		}).then(function(ok) {
			if (!ok || dead) return null;
			return api('group_del', { id: id }, { timeout: APPLY_MS }).then(function(r) {
				reportSaved(r, true, '已删除出口组「' + (g.name || g.id) + '」并应用');
			}, function(e) {
				if (e.in_use && e.in_use.length) blockedModal(g, e.in_use);
				else ctx.toast('删除失败:' + e.message, 'err');
			}).then(function() {
				if (!dead) ctx.store.load(true).then(noop, noop);
			});
		});
	}

	/* ---------- 出口组:编辑弹层 ---------- */
	function geOrder() {
		var c = GE.list.children, out = [], i;
		for (i = 0; i < c.length; i++) if (c[i].hasAttribute('data-id')) out.push(c[i].getAttribute('data-id'));
		return out;
	}
	function geSnap() { return JSON.stringify([GE.name.value.trim(), GE.note.value.trim(), GE.en.checked, GE.members]); }
	function geDirty() {
		var d = geSnap() !== GE.base;
		GE.dirty = d;
		ctx.dirty.set('wn-group', d);
	}
	function geRelabel() {
		var ids = geOrder(), n = ids.length;
		GE.members = ids;
		ids.forEach(function(id, i) {
			var row = GE.rows[id], t = memberName(id);
			if (!row) return;
			row.role.textContent = i === 0 ? '主' : '备 ' + i;
			row.role.className = 'pr-gp-role' + (i === 0 ? ' pr-gp-main' : '');
			row.up.disabled = i === 0;
			row.down.disabled = i === n - 1;
			row.up.setAttribute('aria-label', '上移「' + t + '」,当前第 ' + (i + 1) + ' 位');
			row.down.setAttribute('aria-label', '下移「' + t + '」,当前第 ' + (i + 1) + ' 位');
			row.grip.setAttribute('aria-label', '调整「' + t + '」的顺序,当前第 ' + (i + 1) + ' 位。拖动,或按上下方向键');
		});
		GE.cnt.textContent = n + ' / ' + MAX_MEMBERS;
		geDirty();
	}
	function geMemberRow(id) {
		var mi = memberInfo(id, false), t = memberName(id), row = { id: id };
		row.role = h('span', { class: 'pr-gp-role' });
		row.grip = ui.grip('调整顺序');
		row.up = ui.btn({ kind: 'icon', icon: 'arrow-up', cls: 'pr-wn-b', title: '上移', onClick: function() {
			nudge(row.grip, 'ArrowUp', row.up, row.down, geRelabel);
		} });
		row.down = ui.btn({ kind: 'icon', icon: 'arrow-down', cls: 'pr-wn-b', title: '下移', onClick: function() {
			nudge(row.grip, 'ArrowDown', row.down, row.up, geRelabel);
		} });
		row.rm = ui.btn({ kind: 'icon', icon: 'close', title: '移出出口组', label: '把「' + t + '」移出出口组', onClick: function() { geRemove(id); } });
		row.el = h('li', { class: 'pr-gp-mrow', 'data-id': id, 'data-name': t },
			row.role, row.grip, h('div', { class: 'pr-gp-ud' }, row.up, row.down),
			h('div', { class: 'pr-gp-mn2 pr-grow' }, h('b', null, t), memberDot(mi)),
			row.rm);
		return row;
	}
	function geRenderMembers() {
		GE.list.textContent = '';
		GE.rows = {};
		GE.members.forEach(function(id) {
			var row = geMemberRow(id);
			GE.rows[id] = row;
			GE.list.appendChild(row.el);
		});
		GE.empty.hidden = GE.members.length > 0;
		geRenderCand();
		geRelabel();
	}
	function geRenderCand() {
		var full = GE.members.length >= MAX_MEMBERS, cands = arr(S.wans).filter(function(w) { return GE.members.indexOf(w.wan_id) < 0; });
		GE.cand.textContent = '';
		if (!cands.length) {
			GE.cand.appendChild(h('li', { class: 'pr-dim pr-gp-cnone' }, arr(S.wans).length ? '所有出口都已加入这个出口组。' : '当前没有发现任何出口。'));
			return;
		}
		cands.forEach(function(w) {
			var mi = memberInfo(w.wan_id, false), t = wanTitle(w);
			GE.cand.appendChild(h('li', { class: 'pr-gp-crow' },
				h('div', { class: 'pr-gp-mn2 pr-grow' }, h('b', null, t), ui.badge(fmt.kind_label(w.kind), 'off'), memberDot(mi)),
				ui.btn({ kind: 'ghost', size: 'sm', icon: 'plus', text: '加入', label: '把「' + t + '」加入出口组', disabled: full,
					title: full ? '成员已达上限(' + MAX_MEMBERS + ' 个)' : '', onClick: function() { geAdd(w.wan_id); } })));
		});
	}
	function geAdd(id) {
		if (GE.members.length >= MAX_MEMBERS || GE.members.indexOf(id) >= 0) return;
		GE.members.push(id);
		geShowErr('members', '');
		geRenderMembers();
		focusNode(GE.rows[id] && GE.rows[id].grip);
	}
	function geRemove(id) {
		var i = GE.members.indexOf(id), nx;
		if (i < 0) return;
		GE.members.splice(i, 1);
		geRenderMembers();
		nx = GE.members[Math.min(i, GE.members.length - 1)];
		focusNode(nx && GE.rows[nx] ? GE.rows[nx].grip : GE.name);
	}
	function geShowErr(field, msg) {
		var map = { name: [GE.eName, GE.name], members: [GE.eMem, null], _rule: [GE.eTop, null] }, x = map[field] || map._rule;
		x[0].hidden = !msg;
		x[0].textContent = msg || '';
		if (x[1]) { if (msg) x[1].setAttribute('aria-invalid', 'true'); else x[1].removeAttribute('aria-invalid'); }
	}
	function geCheck() {
		var nm = GE.name.value.trim(), errs = {};
		if (!nm) errs.name = '请填写出口组名称,例如「主备上网」';
		else if (charLen(nm) > 64) errs.name = '名称不能超过 64 个字符';
		if (!GE.members.length) errs.members = '至少加入 1 个成员';
		else if (GE.members.length > MAX_MEMBERS) errs.members = '成员最多 ' + MAX_MEMBERS + ' 个';
		return errs;
	}
	function geSave(apply) {
		var errs = geCheck(), body, id = GE.id, g = GE;
		if (g.saving) return Promise.resolve(false);
		['name', 'members', '_rule'].forEach(function(k) { geShowErr(k, errs[k] || ''); });
		if (errs.name || errs.members) {
			ctx.toast('请先修正标红的项目', 'warn');
			focusNode(errs.name ? GE.name : GE.list.querySelector('.pr-grip') || GE.cand.querySelector('.pr-btn'));
			return Promise.resolve(false);
		}
		body = { name: GE.name.value.trim(), note: GE.note.value.trim(), enabled: GE.en.checked, members: GE.members.slice() };
		g.saving = true;
		return api('group_save', { json: body, id: id || undefined, apply: apply ? undefined : 0 }, { timeout: APPLY_MS }).then(function(r) {
			var w = arr(r.warnings);
			g.saving = false;
			if (!dead) {
				reportSaved(r, apply, '出口组已保存并应用', w.length ? String(obj(w[0]).msg || w[0]) : '');
				ctx.store.load(true).then(noop, noop);
			}
			return true;
		}, function(e) {
			var list = arr(e.errors), top = '';
			g.saving = false;
			if (GE !== g) return false;
			list.forEach(function(x) {
				x = obj(x);
				if (x.field === 'name' || x.field === 'members') geShowErr(x.field, String(x.msg || ''));
				else top += (top ? ';' : '') + String(x.msg || '');
			});
			if (!list.length) top = e.message;
			geShowErr('_rule', top);
			return false;
		});
	}
	function field(labelFor, label, hint, input, err) {
		return h('div', { class: 'pr-field' }, h('label', { class: 'pr-lab', 'for': labelFor }, label), input, hint ? h('div', { class: 'pr-hint' }, hint) : null, err || null);
	}
	function openGroup(id) {
		var g = id ? byKey(arr(S.groups), 'id', id) : null, sw, mod;
		if (GE) return;
		if (id && !g) { ctx.toast('这个出口组已不存在,请刷新后重试', 'warn'); return; }
		GE = { id: id || '', members: g ? arr(g.members).slice() : [], rows: {}, dirty: false, base: '' };
		GE.name = h('input', { type: 'text', id: 'pr-wn-gn', class: 'pr-in', autocomplete: 'off', spellcheck: 'false', placeholder: '例如:主备上网', dataset: { autofocus: '1' } });
		GE.note = h('input', { type: 'text', id: 'pr-wn-gnote', class: 'pr-in', autocomplete: 'off', placeholder: '可选,方便以后识别' });
		GE.name.value = g ? String(g.name || '') : '';
		GE.note.value = g ? String(g.note || '') : '';
		sw = ui.toggle({ checked: g ? g.enabled !== false : true, label: '启用出口组' });
		GE.en = sw.querySelector('input');
		GE.eName = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		GE.eMem = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		GE.eTop = h('div', { class: 'pr-note pr-k-err', role: 'alert', hidden: true });
		GE.cnt = h('span', { class: 'pr-sub' });
		GE.list = h('ol', { class: 'pr-gp-ml', role: 'list', 'aria-label': '出口组成员,按优先级从高到低' });
		GE.empty = h('p', { class: 'pr-dim pr-gp-mnone', hidden: true }, '还没有成员。从下面选择要加入的出口。');
		GE.cand = h('ul', { class: 'pr-gp-cand', role: 'list', 'aria-label': '可加入的出口' });
		GE.drag = ctx.dragSort(GE.list, { onOrder: function() { geRelabel(); }, reload: false });
		GE.base = geSnap();
		geRenderMembers();
		[GE.name, GE.note].forEach(function(n) { n.addEventListener('input', function() { geDirty(); if (n === GE.name) geShowErr('name', ''); }); });
		GE.en.addEventListener('change', geDirty);
		mod = ctx.modal({
			title: g ? '编辑出口组' : '新建出口组', sub: g ? String(g.name || g.id) : '', size: 'lg',
			canClose: function() {
				if (!GE || !GE.dirty) return true;
				return ctx.confirmBox({ title: '放弃未保存的修改?', body: '出口组的修改还没有保存,现在关闭会丢掉这些修改。', okText: '放弃修改', cancelText: '继续编辑', danger: true });
			},
			onClose: function() {
				ctx.dirty.set('wn-group', false);
				if (GE && GE.drag) GE.drag.destroy();
				GE = null;
			},
			body: h('div', { class: 'pr-gp-ed' },
				GE.eTop,
				field('pr-wn-gn', '名称', null, GE.name, GE.eName),
				field('pr-wn-gnote', '备注', null, GE.note),
				h('div', { class: 'pr-field pr-gp-en' },
					h('div', { class: 'pr-grow' }, h('span', { class: 'pr-lab' }, '启用'),
						h('div', { class: 'pr-hint' }, '停用后组内所有成员都视为不可用,指向本组的规则按各自的「不可用时」设置处理。')),
					sw),
				h('div', { class: 'pr-field' },
					h('div', { class: 'pr-gp-mh' }, h('span', { class: 'pr-lab' }, '成员(按优先级从高到低)'), GE.cnt),
					h('div', { class: 'pr-hint' }, '第一个是主出口,其余是备用。流量选用第一个可用的成员;主出口恢复后会回到主出口。拖动手柄、点上移/下移,或在手柄上按上下方向键调整顺序。成员 1 到 ' + MAX_MEMBERS + ' 个。'),
					GE.empty, GE.list, GE.eMem),
				h('div', { class: 'pr-field' },
					h('span', { class: 'pr-lab' }, '可加入的出口'),
					GE.cand)),
			actions: [
				{ text: '取消', kind: 'ghost' },
				{ text: '仅保存', kind: 'ghost', title: '保存到配置,将在 1 分钟内的下次对账自动生效,也可稍后点击「应用」立即生效', onClick: function() { return geSave(false); } },
				{ text: '保存并应用', kind: 'primary', onClick: function() { return geSave(true); } }
			]
		});
		return mod;
	}

	/* ---------- 说明与限制 ---------- */
	function buildNotes() {
		var items = [
			'出口按设备和接口的真实状态自动发现,类型徽章来自设备识别结果,不看接口名。出口标识优先取设备序列号等硬件信息;没有序列号时回落到总线位置或接口名,换口或重新插拔后标识可能变化,这类出口上会显示提示,别名、顺序和规则引用按标识保存,标识变化后需要重新设置。',
			'「设备未接入」和「离线」是两回事:前者是设备当前不在系统里(没插或没被识别),后者是设备在、但链路断开或没有默认路由。未接入的出口会保留别名、顺序和规则引用。',
			'保存与应用:别名、优先级顺序、运行模式保存后即写入配置,不需要点「应用」;出口的启用/停用和出口组的新建、修改、删除默认在保存时一并应用,出口组编辑里选「仅保存」则会在 1 分钟内的下次对账自动生效,也可稍后点「应用」立即生效,未应用时页面顶部会提示「尚未应用」。',
			'出口组只做主备,不做按比例分流,也不叠加带宽。' + ctx.copy.failover,
			'多出口模式只管理主路由表的 IPv4 默认路由,不管理 IPv6。' + ctx.copy.ipv6,
			ctx.copy.probe + '「能 ping 通」不等于业务一定可用,阈值较小时 4G/5G 抖动可能引起误切换。',
			'这里只列出系统里有对应接口配置的出口;保存顺序时,当前列表里没有的旧出口标识不会被保留。'
		];
		E.notes = h('section', { class: 'pr-card', 'aria-labelledby': 'pr-wn-h-nt' },
			h('div', { class: 'pr-sec' }, h('h2', { class: 'pr-h2', id: 'pr-wn-h-nt' }, '说明与限制')),
			h('ul', { class: 'pr-wn-notes' }, items.map(function(t) { return h('li', null, t); })));
	}

	/* ---------- 组装 ---------- */
	function update(st) {
		S = st;
		renderBanner();
		renderWans();
		renderGroups();
	}
	function build(st) {
		S = st;
		root.textContent = '';
		if (st.installed === false) {
			root.appendChild(ui.empty({ icon: 'info', title: '策略路由组件未安装', body: '没有找到策略路由引擎(mwrt-pbr),请确认固件包含该组件。' }));
			return;
		}
		buildBanner();
		buildWans();
		buildGroups();
		buildNotes();
		[E.pend, E.wans, E.groups, E.notes].forEach(function(n) { root.appendChild(n); });
		ctx.store.on('change', function(s) { if (!dead && s) update(s); });
		offPend = ctx.pending.on(function() { if (!dead && S) renderBanner(); });
		update(st);
	}

	start();
	return {
		destroy: function() {
			dead = true;
			ctx.dirty.set('wn-group', false);
			Object.keys(R).forEach(function(id) { ctx.dirty.set('wn-alias:' + id, false); });
			if (offPend) offPend();
		}
	};
}

M.pages.wans = { title: 'WAN 出口', render: render };
})();
