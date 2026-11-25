/*
 * 诊断页:分组检查结果、路由试算、规则预览与维护操作。
 * 动态文本只经 ctx.h 与 textContent 写入;检查结果来自 diag,试算来自 trace。
 */
(function() {
'use strict';

var M = window.MwrtPbr;
if (!M || !M.pages) return;

var PREVIEW_MS = 40000;
var PRE_MAX = 200000;
var PRE_OPEN = 4000;
var GROUPS = [
	['engine', '引擎与内核', '总开关、应用状态、内核规则表、入口设备'],
	['capability', '系统能力', 'nft、ip rule、dnsmasq 与应用识别库'],
	['exits', '出口', '出口设备的防火墙区域、反向路径过滤、IPv6'],
	['coexist', '并存软件', '可能同时修改标记或路由的其它软件'],
	['dns', 'DNS 解析集合', '域名规则依赖的集合与 dnsmasq 映射'],
	['offload', '流量卸载', '软件流量卸载与硬件加速'],
	['rules', '规则', '规则生效情况与命中']
];
var LV = {
	ok: ['ok', '正常', 'check-circle'],
	info: ['info', '提示', 'info'],
	warn: ['warn', '注意', 'warn'],
	fail: ['err', '失败', 'x-circle']
};
var RES = { match: ['ok', '命中'], nomatch: ['off', '未命中'], unknown: ['warn', '不确定'] };
var PROTO_OPT = [['', '不限'], ['tcp', 'TCP'], ['udp', 'UDP'], ['icmp', 'ICMP']];

function arr(x) { return Array.isArray(x) ? x : []; }
function obj(x) { return x && typeof x === 'object' && !Array.isArray(x) ? x : {}; }
function byKey(list, key, val) {
	for (var i = 0; i < list.length; i++) if (list[i] && list[i][key] === val) return list[i];
	return null;
}
function splitTarget(t) {
	t = String(t || '');
	var i = t.indexOf(':');
	return i < 0 ? { type: '', id: t } : { type: t.slice(0, i), id: t.slice(i + 1) };
}
function isPort(s) { return /^\d{1,5}$/.test(s) && Number(s) >= 1 && Number(s) <= 65535; }

function render(root, ctx) {
	var h = ctx.h, ui = ctx.ui, fmt = ctx.fmt, api = ctx.api, icon = ctx.icon;
	var dead = false;
	var offRec = null;
	var S = null;
	var E = {};
	var F = {};
	var D = { checks: null, ts: 0, loading: false, error: null, seq: 0, only: false, open: {} };
	var T = { seq: 0, busy: false };
	var clients = null;
	var hid = 0;

	function noop() {}
	function go(page, query) { return function() { ctx.nav(page, query); }; }
	function note(kind, ic, text, cls) {
		return h('div', { class: 'pr-note pr-k-' + kind + (cls ? ' ' + cls : '') }, icon(ic), h('div', { class: 'pr-grow' }, text));
	}

	function targetLabel(target) {
		var t = splitTarget(target), w, g;
		if (t.type === 'wan') {
			w = byKey(arr(S && S.wans), 'wan_id', t.id);
			return w ? (w.alias || w.name || t.id) : t.id;
		}
		if (t.type === 'group') {
			g = byKey(arr(S && S.groups), 'id', t.id);
			return g ? (g.name || g.id) + '(出口组)' : t.id + '(出口组)';
		}
		return String(target || '—');
	}

	/* ---------- 加载与错误 ---------- */
	function showLoading() {
		root.textContent = '';
		root.appendChild(ui.loading({ rows: 4, text: '正在读取策略路由状态…' }));
	}
	function start() {
		if (offRec) { offRec(); offRec = null; }
		showLoading();
		ctx.store.load().then(function(st) {
			if (!dead && !S) build(st);
		}, function(e) {
			if (dead) return;
			root.textContent = '';
			root.appendChild(ui.error({ title: '无法读取策略路由状态', message: e && e.message, onRetry: start }));
			/* 顶栏轮询恢复后页面主体也要跟上;S 非空说明已经 build 过,避免重复 build */
			offRec = ctx.store.on('recover', function(st) {
				if (offRec) { offRec(); offRec = null; }
				if (!dead && !S) build(st);
			});
		});
	}

	/* ---------- 诊断检查 ---------- */
	function buildDiag() {
		E.dgSub = h('span', { class: 'pr-sub' });
		E.onlyInp = h('input', { type: 'checkbox' });
		E.onlyInp.addEventListener('change', function() {
			D.only = E.onlyInp.checked;
			renderGroups();
		});
		E.dgRe = ui.btn({ kind: 'ghost', size: 'sm', icon: 'refresh', text: '重新检查', onClick: runDiag });
		E.dgSum = h('div', { class: 'pr-dg-sum', role: 'status' });
		E.dgBody = h('div', { class: 'pr-dg-groups' });
		E.dg = h('div', { class: 'pr-dg-wrap' },
			h('section', { class: 'pr-card', 'aria-labelledby': 'pr-dg-h-chk' },
				h('div', { class: 'pr-sec pr-dg-head' },
					h('div', null, h('h2', { class: 'pr-h2', id: 'pr-dg-h-chk' }, '诊断检查'), E.dgSub),
					h('div', { class: 'pr-row' },
						h('label', { class: 'pr-chk' }, E.onlyInp, h('span', null, '只看注意与失败')),
						E.dgRe)),
				E.dgSum),
			E.dgBody);
	}
	function counts(list) {
		var c = { ok: 0, info: 0, warn: 0, fail: 0 };
		list.forEach(function(x) { if (c[x.level] != null) c[x.level]++; else c.info++; });
		return c;
	}
	function renderSummary() {
		var host = E.dgSum, list = arr(D.checks), c = counts(list), t, retry;
		host.textContent = '';
		if (!D.checks) {
			if (D.loading) host.appendChild(ui.loading({ rows: 2, text: '正在运行诊断检查…' }));
			else if (D.error) {
				retry = ui.btn({ kind: 'ghost', size: 'sm', text: '重试', onClick: runDiag });
				host.appendChild(h('div', { class: 'pr-note pr-k-err', role: 'alert' }, icon('x-circle'),
					h('div', { class: 'pr-grow' }, '诊断检查没有完成:' + D.error.message), retry));
			}
			return;
		}
		if (c.fail > 0) t = ['err', 'x-circle', '发现 ' + c.fail + ' 项失败' + (c.warn ? '、' + c.warn + ' 项注意' : '') + ',建议先处理标为「失败」的项。'];
		else if (c.warn > 0) t = ['warn', 'warn', '有 ' + c.warn + ' 项需要注意,不一定影响使用,请逐项确认。'];
		else t = ['ok', 'check-circle', '所有检查项都没有发现问题。检查只覆盖下面列出的项目,不代表已验证实际转发效果。'];
		host.appendChild(h('div', { class: 'pr-note pr-k-' + t[0] }, icon(t[1]), h('div', { class: 'pr-grow' }, t[2])));
		if (D.error) host.appendChild(note('warn', 'warn', '重新检查没有完成,下面仍是上一次的结果:' + D.error.message));
		host.appendChild(h('div', { class: 'pr-row pr-dg-cnt' },
			ui.badge('失败 ' + c.fail, c.fail ? 'err' : 'off'),
			ui.badge('注意 ' + c.warn, c.warn ? 'warn' : 'off'),
			ui.badge('提示 ' + c.info, c.info ? 'info' : 'off'),
			ui.badge('正常 ' + c.ok, c.ok ? 'ok' : 'off')));
	}
	function checkRow(c) {
		var lv = LV[c.level] || LV.info, id = String(c.id || c.label || ''), hint = String(c.hint || '');
		var open = D.open[id] != null ? D.open[id] : (c.level === 'warn' || c.level === 'fail');
		var hint_id = 'pr-dg-hn-' + (++hid), hintEl = null, tg = null;
		if (hint) {
			hintEl = h('div', { class: 'pr-dg-hint', id: hint_id, hidden: !open }, h('b', null, '怎么处理:'), ' ', hint);
			tg = ui.btn({ kind: 'text', size: 'sm', text: open ? '收起处理建议' : '查看处理建议', onClick: function() {
				var show = hintEl.hidden;
				hintEl.hidden = !show;
				D.open[id] = show;
				tg.setAttribute('aria-expanded', show ? 'true' : 'false');
				tg.lastChild.textContent = show ? '收起处理建议' : '查看处理建议';
			} });
			tg.setAttribute('aria-expanded', open ? 'true' : 'false');
			tg.setAttribute('aria-controls', hint_id);
		}
		return h('li', { class: 'pr-dg-row pr-k-' + lv[0] },
			h('span', { class: 'pr-dg-ic' }, icon(lv[2])),
			h('div', { class: 'pr-dg-main' },
				h('div', { class: 'pr-dg-top' }, h('b', { class: 'pr-dg-t' }, c.label || id || '检查项'), ui.badge(lv[1], lv[0])),
				c.detail ? h('div', { class: 'pr-dg-d' }, String(c.detail)) : null,
				tg, hintEl));
	}
	function groupCard(g, items, idx) {
		var c = counts(items), gid = 'pr-dg-h-g' + idx, badges = [];
		if (c.fail) badges.push(ui.badge(c.fail + ' 项失败', 'err'));
		if (c.warn) badges.push(ui.badge(c.warn + ' 项注意', 'warn'));
		if (!c.fail && !c.warn) badges.push(ui.badge('无异常', 'ok'));
		return h('section', { class: 'pr-card pr-dg-grp', 'aria-labelledby': gid },
			h('div', { class: 'pr-sec' },
				h('div', null, h('h2', { class: 'pr-h2', id: gid }, g[1]), g[2] ? h('span', { class: 'pr-sub' }, g[2]) : null),
				h('div', { class: 'pr-row' }, badges)),
			h('ul', { class: 'pr-dg-list', role: 'list' }, items.map(checkRow)));
	}
	function renderGroups() {
		var host = E.dgBody, list = arr(D.checks), by = {}, known = {}, defs = GROUPS.slice(), shown = 0, i, items;
		host.textContent = '';
		host.setAttribute('aria-busy', D.loading ? 'true' : 'false');
		if (!D.checks) return;
		GROUPS.forEach(function(g) { known[g[0]] = true; });
		list.forEach(function(c) {
			var g = c.group || 'other';
			if (!by[g]) {
				by[g] = [];
				if (!known[g]) {
					known[g] = true;
					defs.push([g, '其他', '']);
				}
			}
			by[g].push(c);
		});
		for (i = 0; i < defs.length; i++) {
			items = arr(by[defs[i][0]]).filter(function(c) { return !D.only || c.level === 'warn' || c.level === 'fail'; });
			if (!items.length) continue;
			shown++;
			host.appendChild(groupCard(defs[i], items, i));
		}
		if (!shown) {
			host.appendChild(h('section', { class: 'pr-card' }, ui.empty({
				compact: true, icon: D.only ? 'check-circle' : 'info',
				title: D.only ? '没有需要处理的检查项' : '没有检查结果',
				body: D.only ? '取消「只看注意与失败」可以查看全部检查项。' : '引擎没有返回任何检查项。'
			})));
		}
	}
	function renderDiag() {
		E.dgRe.disabled = D.loading;
		E.dgRe.lastChild.textContent = D.loading ? '检查中…' : '重新检查';
		E.dgSub.textContent = D.loading ? '正在检查,最长约 40 秒…' : D.ts ? '检查时间 ' + fmt.time(D.ts) : '';
		renderSummary();
		renderGroups();
	}
	function runDiag() {
		var my;
		if (D.loading || dead) return;
		my = ++D.seq;
		D.loading = true;
		D.error = null;
		renderDiag();
		api('diag').then(function(j) {
			if (dead || my !== D.seq) return;
			D.checks = arr(j.checks);
			D.ts = j.ts || ctx.store.now();
		}, function(e) {
			if (dead || my !== D.seq) return;
			D.error = e;
		}).then(function() {
			if (dead || my !== D.seq) return;
			D.loading = false;
			renderDiag();
		});
	}

	/* ---------- 路由试算 ---------- */
	function field(id, title, hint, input, extra) {
		return h('div', { class: 'pr-field' },
			h('label', { class: 'pr-lab', 'for': id }, title), input,
			hint ? h('div', { class: 'pr-hint' }, hint) : null, extra || null);
	}
	function inp(id, ph, mode) {
		return h('input', { type: 'text', id: id, class: 'pr-in', autocomplete: 'off', spellcheck: 'false', placeholder: ph, inputmode: mode || 'text' });
	}
	function buildTrace() {
		F.src = inp('pr-dg-src', '例如 192.168.1.100');
		F.mac = inp('pr-dg-mac', '例如 aa:bb:cc:dd:ee:ff');
		F.dst = inp('pr-dg-dst', '例如 1.1.1.1 或 2001:db8::1');
		F.domain = inp('pr-dg-dom', '例如 example.com', 'url');
		F.dport = inp('pr-dg-dport', '1-65535', 'numeric');
		F.sport = inp('pr-dg-sport', '1-65535', 'numeric');
		F.proto = h('select', { id: 'pr-dg-proto', class: 'pr-in pr-sel' }, PROTO_OPT.map(function(o) { return h('option', { value: o[0] }, o[1]); }));
		F.eDst = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		F.eDport = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		F.eSport = h('div', { class: 'pr-fe', role: 'alert', hidden: true });
		E.trPick = ui.btn({ kind: 'ghost', size: 'sm', text: '从设备列表选择源设备', onClick: pickClient });
		E.trBtn = ui.btn({ kind: 'primary', type: 'submit', text: '开始试算' });
		E.trClear = ui.btn({ kind: 'ghost', text: '清空', onClick: clearTrace });
		E.trRes = h('div', { class: 'pr-dg-res', role: 'region', 'aria-live': 'polite', 'aria-label': '试算结果', tabindex: '-1' });
		E.trForm = h('form', { class: 'pr-dg-form', novalidate: true, onsubmit: onTrace },
			h('div', { class: 'pr-dg-fs' },
				h('div', { class: 'pr-dg-fh' }, h('b', null, '源设备'), E.trPick),
				h('div', { class: 'pr-dg-fg' },
					field('pr-dg-src', '源地址', '选填。LAN 设备的单个 IP。', F.src),
					field('pr-dg-mac', '设备 MAC', '选填。只填源地址时,会尝试从本机邻居表里查 MAC。', F.mac))),
			h('div', { class: 'pr-dg-fs' },
				h('div', { class: 'pr-dg-fh' }, h('b', null, '目的地')),
				h('div', { class: 'pr-dg-fg' },
					field('pr-dg-dst', '目的地址(必填)', '单个 IP。域名规则看的是域名解析出的 IP,试算不会发起任何解析,请填客户端实际解析到的地址。', F.dst, F.eDst),
					field('pr-dg-dom', '域名', '选填。只用来判断域名条件是否匹配,不会解析。', F.domain))),
			h('div', { class: 'pr-dg-fs' },
				h('div', { class: 'pr-dg-fh' }, h('b', null, '协议与端口')),
				h('div', { class: 'pr-dg-fg pr-dg-fg3' },
					field('pr-dg-proto', '协议', '', F.proto),
					field('pr-dg-dport', '目的端口', '', F.dport, F.eDport),
					field('pr-dg-sport', '源端口', '', F.sport, F.eSport))),
			h('div', { class: 'pr-row pr-dg-act' }, E.trBtn, E.trClear));
		E.tr = h('section', { class: 'pr-card', 'aria-labelledby': 'pr-dg-h-tr' },
			h('div', { class: 'pr-sec' },
				h('div', null, h('h2', { class: 'pr-h2', id: 'pr-dg-h-tr' }, '路由试算'),
					h('span', { class: 'pr-sub' }, '假设一条新连接,按当前配置推演它命中哪条规则、走哪个出口。只读,不发包。'))),
			E.trForm, E.trRes);
	}
	function errAt(el, errEl, msg) {
		errEl.hidden = !msg;
		errEl.textContent = msg || '';
		if (msg) el.setAttribute('aria-invalid', 'true'); else el.removeAttribute('aria-invalid');
	}
	function readTrace() {
		return {
			src: F.src.value.trim(), mac: F.mac.value.trim(), dst: F.dst.value.trim(), domain: F.domain.value.trim(),
			proto: F.proto.value, dport: F.dport.value.trim(), sport: F.sport.value.trim()
		};
	}
	function clearTrace() {
		[F.src, F.mac, F.dst, F.domain, F.dport, F.sport].forEach(function(i) { i.value = ''; });
		F.proto.value = '';
		errAt(F.dst, F.eDst, '');
		errAt(F.dport, F.eDport, '');
		errAt(F.sport, F.eSport, '');
		T.seq++;
		T.busy = false;
		E.trBtn.disabled = false;
		E.trRes.textContent = '';
		F.src.focus();
	}
	function pickIp(c) {
		var d = F.dst.value.trim(), v6 = arr(c.ip6);
		if (d) return d.indexOf(':') >= 0 ? (v6[0] || '') : (c.ip || '');
		return c.ip || v6[0] || '';
	}
	function loadClients() {
		return api('clients').then(function(j) {
			clients = arr(j.clients);
			return clients.map(function(c) {
				var nm = c.alias || c.name || c.ip || c.mac;
				return {
					id: c.mac, name: nm,
					sub: [c.ip, arr(c.ip6)[0], c.mac].filter(function(x) { return x; }).join(' · '),
					group: c.online ? '在线设备' : '离线或历史设备',
					keys: [c.ip, c.mac, c.name, c.alias].concat(arr(c.ip6)).join(' ')
				};
			});
		});
	}
	function pickClient() {
		ctx.picker({
			title: '选择源设备', multi: false, okText: '选用', searchText: '搜索名称、IP 或 MAC',
			emptyText: '没有发现设备', emptyHint: '设备出现在 DHCP 租约、在线列表或静态主机里才会列在这里;也可以直接手填 IP 与 MAC。',
			items: loadClients
		}).then(function(ids) {
			var c, ip;
			if (dead || !ids || !ids.length) return;
			c = byKey(arr(clients), 'mac', ids[0]);
			if (!c) return;
			F.mac.value = c.mac;
			ip = pickIp(c);
			if (ip) F.src.value = ip;
			else ctx.toast('该设备没有与目的地址同类型的地址,只填入了 MAC。', 'info');
			F.dst.focus();
		});
	}
	function onTrace(ev) {
		var p, bad = null, my, q = {};
		if (ev) ev.preventDefault();
		if (T.busy) return;
		p = readTrace();
		errAt(F.dst, F.eDst, p.dst ? '' : '请填写目的地址');
		errAt(F.dport, F.eDport, !p.dport || isPort(p.dport) ? '' : '端口要在 1-65535 之间');
		errAt(F.sport, F.eSport, !p.sport || isPort(p.sport) ? '' : '端口要在 1-65535 之间');
		if (!p.dst) bad = F.dst; else if (p.dport && !isPort(p.dport)) bad = F.dport; else if (p.sport && !isPort(p.sport)) bad = F.sport;
		if (bad) {
			ctx.toast('请先修正标红的项', 'warn');
			bad.focus();
			return;
		}
		['src', 'mac', 'dst', 'domain', 'proto', 'dport', 'sport'].forEach(function(k) { if (p[k]) q[k] = p[k]; });
		my = ++T.seq;
		T.busy = true;
		E.trBtn.disabled = true;
		E.trRes.textContent = '';
		E.trRes.appendChild(ui.loading({ rows: 3, text: '正在试算…' }));
		api('trace', q).then(function(j) {
			if (dead || my !== T.seq) return;
			showTrace(j);
		}, function(e) {
			if (dead || my !== T.seq) return;
			E.trRes.textContent = '';
			E.trRes.appendChild(h('div', { class: 'pr-note pr-k-err', role: 'alert' }, icon('x-circle'),
				h('div', { class: 'pr-grow' }, h('b', null, '试算没有完成'), h('div', null, e.message))));
		}).then(function() {
			if (dead || my !== T.seq) return;
			T.busy = false;
			E.trBtn.disabled = false;
		});
	}
	function describeInput(i) {
		var parts = [], s = (i.src || '未指定源地址') + ' → ' + (i.dst || '—');
		if (i.proto) parts.push(String(i.proto).toUpperCase());
		if (i.dport) parts.push('目的端口 ' + i.dport);
		if (i.sport) parts.push('源端口 ' + i.sport);
		return (i.family === 6 ? 'IPv6:' : 'IPv4:') + s + (parts.length ? ',' + parts.join('、') : '');
	}
	function exitView(j) {
		var ex = obj(j.exit), box = [];
		box.push(h('span', null, targetLabel(ex.target)));
		if (ex.table != null) box.push(h('span', { class: 'pr-dim' }, '(出口表 ' + ex.table + ')'));
		if (ex.usable === false) {
			box.push(h('div', { class: 'pr-dg-sub' }, ui.badge('当前没有可用路由', 'warn'), ' ',
				ex.on_fail === 'block' ? '「不可用时」设为阻断:这条连接会被丢弃。' : '「不可用时」设为回落系统默认路由。'));
		}
		return h('span', { class: 'pr-dg-vals' }, box);
	}
	/* s297c 出口设备名: 转网口机型的 eth1 显示「模组转网口PHY」(取出口快照里的 dev_label) */
	function devShow(dev) {
		var a = arr(S && S.wans), i;
		for (i = 0; i < a.length; i++) if (a[i] && a[i].dev === dev && a[i].dev_label) return a[i].dev_label;
		return dev;
	}
	function routeView(j) {
		var r = obj(j.route), box = [];
		box.push(ui.badge(r.failed ? '没有可用路由' : '有路由', r.failed ? 'err' : 'ok'));
		box.push(' ');
		if (r.text) box.push(h('span', { class: 'pr-mono' }, r.text));
		if (!r.failed && r.dev) box.push(h('div', { class: 'pr-dg-sub pr-dim' }, '出口设备 ' + devShow(r.dev) + (r.via ? ',下一跳 ' + r.via : '')));
		return h('span', { class: 'pr-dg-vals' }, box);
	}
	function verdict(j) {
		var m = j.matched, cs = arr(j.checked), nm;
		if (m) {
			nm = m.name || m.id;
			return { k: j.uncertain ? 'warn' : 'ok', ic: j.uncertain ? 'warn' : 'check-circle', t: '命中规则「' + nm + '」',
				b: (j.exit ? '这条新连接会走:' + targetLabel(j.exit.target) + '。' : '') + (j.uncertain ? '排在它前面还有结果不确定的规则,见下方判定。' : '') };
		}
		if (!cs.length) return { k: 'info', ic: 'info', t: '没有规则被评估', b: '原因见下方说明。' };
		if (j.uncertain) return { k: 'warn', ic: 'warn', t: '没有规则明确命中', b: '有规则取决于运行时集合或缺少输入,实际走向可能不同,见下方判定。' };
		return { k: 'info', ic: 'info', t: '没有规则命中', b: '新连接不打标,走系统默认路由。' };
	}
	function showTrace(j) {
		var host = E.trRes, v = verdict(j), i = obj(j.input), cs = arr(j.checked), notes = arr(j.notes), kv, list;
		host.textContent = '';
		host.appendChild(h('div', { class: 'pr-note pr-k-' + v.k }, icon(v.ic),
			h('div', { class: 'pr-grow' }, h('b', null, v.t), v.b ? h('div', null, v.b) : null)));

		kv = h('dl', { class: 'pr-kv pr-dg-kv' });
		function row(k, val) {
			kv.appendChild(h('dt', null, k));
			kv.appendChild(h('dd', null, val == null || val === '' ? '—' : val));
		}
		row('试算的连接', describeInput(i));
		if (i.mac) row('设备 MAC', h('span', { class: 'pr-mono' }, i.mac + (i.mac_auto ? '(取自本机邻居表)' : '')));
		if (i.domain) row('域名', i.domain);
		row('命中规则', j.matched ? (j.matched.name || j.matched.id) : '无');
		if (j.exit) row('出口', exitView(j));
		if (j.route) row('路由', routeView(j));
		else if (j.matched || cs.length) row('路由', '不适用');
		host.appendChild(kv);

		if (cs.length) {
			list = h('ul', { class: 'pr-dg-chk', role: 'list' }, cs.map(function(c, n) {
				var rs = RES[c.result] || ['off', String(c.result || '未知')];
				return h('li', { class: 'pr-dg-ci' },
					h('span', { class: 'pr-dg-cn' }, String(n + 1)),
					h('div', { class: 'pr-dg-cm' },
						h('div', { class: 'pr-dg-top' }, h('b', null, c.name || c.id), ui.badge(rs[1], rs[0])),
						c.reason ? h('div', { class: 'pr-dim' }, String(c.reason)) : null));
			}));
			host.appendChild(h('div', { class: 'pr-dg-blk' },
				h('h3', { class: 'pr-dg-h3' }, '逐条规则判定'),
				h('p', { class: 'pr-hint' }, '按规则顺序判定,第一条命中者胜,命中后不再看后面的规则。'),
				list));
		}
		if (notes.length) {
			host.appendChild(h('div', { class: 'pr-dg-blk' },
				h('h3', { class: 'pr-dg-h3' }, '说明'),
				h('ul', { class: 'pr-dg-notes', role: 'list' }, notes.map(function(x) {
					return h('li', null, icon('info'), h('span', null, String(x)));
				}))));
		}
		try { host.scrollIntoView({ block: 'nearest' }); } catch (e) {}
	}

	/* ---------- 维护 ---------- */
	function mrow(title, desc, btn) {
		return h('div', { class: 'pr-dg-mrow' },
			h('div', { class: 'pr-grow' }, h('b', null, title), h('p', { class: 'pr-hint' }, desc)), btn);
	}
	function buildMaint() {
		E.mPrev = ui.btn({ kind: 'ghost', text: '查看', onClick: doPreview });
		E.mFlush = ui.btn({ kind: 'ghost', text: '清除', onClick: doFlush });
		E.mReset = ui.btn({ kind: 'ghost', text: '重置', onClick: doReset });
		E.mFlushRow = mrow('清除硬件加速表',
			'本机有联发科硬件加速(HNAT)。已被它绑定的连接不受新规则影响,清除后这些连接会暂时回到软件转发再重新绑定,期间速率可能下降。',
			E.mFlush);
		E.maint = h('section', { class: 'pr-card', 'aria-labelledby': 'pr-dg-h-mt' },
			h('div', { class: 'pr-sec' }, h('h2', { class: 'pr-h2', id: 'pr-dg-h-mt' }, '维护')),
			h('div', { class: 'pr-dg-mlist' },
				mrow('查看生成的规则', '按已保存的配置干跑一次编译,只读预览将下发的 nft 规则、ip rule 与出口路由命令,不会改动系统。', E.mPrev),
				E.mFlushRow,
				mrow('重置命中计数', '把各规则的命中次数清零并重新累计,只影响统计,不改规则,也不影响正在转发的连接。', E.mReset)));
	}
	function renderMaint() {
		E.mFlushRow.hidden = true;
	}
	function preBlock(title, text) {
		var t = String(text == null ? '' : text), lines = t ? t.split('\n').length : 0, cut = t.length > PRE_MAX, kids = [];
		if (cut) t = t.slice(0, PRE_MAX);
		kids.push(h('summary', null, title + '(' + lines + ' 行)'));
		if (cut) kids.push(h('p', { class: 'pr-hint' }, '内容较长,只显示前 ' + PRE_MAX + ' 个字符。'));
		kids.push(h('pre', { class: 'pr-dg-pre pr-mono', tabindex: '0', 'aria-label': title }, t || '(空)'));
		return h('details', { class: 'pr-dg-det', open: !!t && t.length <= PRE_OPEN }, kids);
	}
	function showPreview(j, err) {
		var body = h('div', { class: 'pr-dg-pv' }), chk = obj(j.nft_check), lan = arr(j.lan_ifs);
		body.appendChild(h('p', { class: 'pr-hint' }, '这是按已保存配置推演的结果,不一定是内核里正在运行的规则;尚未应用的更改也会出现在这里。'));
		if (err) body.appendChild(note('err', 'x-circle', err));
		arr(j.warnings).forEach(function(w) { body.appendChild(note('warn', 'warn', String(w))); });
		if (!err) {
			body.appendChild(h('div', { class: 'pr-row pr-dg-cnt' },
				ui.badge(arr(j.rules).length + ' 条规则', 'info'),
				ui.badge(arr(j.exits).length + ' 个出口', 'info'),
				lan.length ? ui.badge('入口设备 ' + lan.join('、'), 'off') : null,
				chk.ok === true ? ui.badge('nft 语法检查通过', 'ok') : chk.ok === false ? ui.badge('nft 语法检查未通过', 'err') : null));
		}
		if (chk.ok === false && chk.error) body.appendChild(note('err', 'x-circle', 'nft 语法检查没有通过:' + chk.error));
		if (!err) {
			body.appendChild(preBlock('nft 规则', j.nft));
			body.appendChild(preBlock('ip rule 命令', arr(j.ip_cmds).join('\n')));
			body.appendChild(preBlock('出口路由命令', arr(j.route_cmds).join('\n')));
		}
		ctx.modal({ title: '生成的规则', sub: '只读预览', size: 'lg', body: body, actions: [{ text: '关闭', kind: 'primary', autofocus: true }] });
	}
	function doPreview() {
		E.mPrev.disabled = true;
		api('preview', null, { timeout: PREVIEW_MS }).then(function(j) {
			if (!dead) showPreview(j, '');
		}, function(e) {
			if (dead) return;
			if (e.kind === 'server') showPreview(obj(e.data), e.message);
			else ctx.toast('无法生成规则预览:' + e.message, 'err');
		}).then(function() {
			if (!dead) E.mPrev.disabled = false;
		});
	}
	function doFlush() {
		ctx.confirmBox({
			title: '清除硬件加速表?',
			body: '会清空联发科硬件加速(HNAT)里已绑定的转发表项。\n' +
				'副作用:这些连接会暂时回到软件转发并重新绑定,期间速率可能下降,个别连接可能短暂卡顿或需要重连。\n' +
				'策略路由只对新建立的连接生效,清除并不保证已建立的连接改走新出口。\n' +
				'清除命令沿用「行为管控」的做法,各机型的加速引擎数量不同,是否覆盖全部尚未逐机型验证。',
			okText: '清除', danger: true
		}).then(function(ok) {
			if (!ok || dead) return;
			E.mFlush.disabled = true;
			api('accel_flush').then(function(j) {
				var n = j.written || 0;
				ctx.toast('已执行清除命令(写入 ' + n + ' 条)' + (n < 3 ? ',部分命令没有写入成功' : ''), n < 3 ? 'warn' : 'ok');
			}, function(e) {
				ctx.toast('清除失败:' + e.message, 'err');
			}).then(function() {
				if (!dead) E.mFlush.disabled = false;
			});
		});
	}
	function doReset() {
		ctx.confirmBox({
			title: '重置命中计数?',
			body: '各规则的命中次数会清零,并从现在起重新累计。只影响统计,不改动规则,也不影响正在转发的连接。',
			okText: '重置'
		}).then(function(ok) {
			if (!ok || dead) return;
			E.mReset.disabled = true;
			api('counters_reset').then(function() {
				ctx.toast('命中计数已重置,从现在起重新累计', 'ok');
				ctx.store.load(true).then(noop, noop);
			}, function(e) {
				ctx.toast('重置失败:' + e.message, 'err');
			}).then(function() {
				if (!dead) E.mReset.disabled = false;
			});
		});
	}

	/* ---------- 组装 ---------- */
	function build(st) {
		S = st;
		root.textContent = '';
		if (st.installed === false) {
			root.appendChild(ui.empty({ icon: 'info', title: '策略路由组件未安装', body: '没有找到策略路由引擎(mwrt-pbr),请确认固件包含该组件。' }));
			return;
		}
		buildDiag();
		buildTrace();
		buildMaint();
		[E.dg, E.tr, E.maint].forEach(function(n) { root.appendChild(n); });
		ctx.store.on('change', function(s) {
			if (dead || !s) return;
			S = s;
			renderMaint();
		});
		renderMaint();
		runDiag();
	}

	start();
	return {
		destroy: function() { dead = true; }
	};
}

M.pages.diag = { title: '诊断', render: render };
})();
