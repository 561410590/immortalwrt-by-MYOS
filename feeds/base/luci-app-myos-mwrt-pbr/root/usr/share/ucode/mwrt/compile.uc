'use strict';
// mwrt-pbr 编译器:规则 + 出口快照 -> nft / ip rule / 出口表路由 / dnsmasq 映射 的期望状态。
// 纯函数:不读 UCI、不 fork、不写文件;"装载"由 pbr.uc 负责。

import { MAX_RULES, to_bool, list_of, merge_addrs, merge_ports, ranges_text, validate_rule_ex, build_ctx } from 'mwrt.validate';

const TBL = 'inet mwrt_pr';
const PRI_PRE = -152;
const PRI_LEARN = -148;
// Bits 16-22 leave MYOS dual-PDP and its flow bypass bit intact.
const MARK_MASK_TXT = '0x7f0000';
const MARK_KEEP_TXT = '0xff80ffff';
const FAKEIP = '198.18.0.0/15';
const DNS_SIZE = 65536;
const LEARN_SIZE = 8192;
const SIG_VER = 'v1';
const MAX_EXIT_ID = 127;
const V6_BLOCK_REASON = '该出口不发放 LAN 前缀,IPv6 流量将被阻断(客户端回落 IPv4)';

// ---------------------------------------------------------------- 小工具

// 12 位十六进制短哈希:两路模运算多项式哈希,只用于"内容是否变化"的比较,不做安全用途
function hash_str(s) {
	let h1 = 5381;
	let h2 = 52711;
	let n = length(s);
	for (let i = 0; i < n; i++) {
		let c = ord(s, i);
		h1 = (h1 * 33 + c) % 2147483647;
		h2 = (h2 * 131 + c) % 2147483629;
	}
	return sprintf('%08x%04x', h1, h2 & 0xffff);
}

function mark_txt(id) {
	return sprintf('0x%08x', id * 65536);
}

function mark_short(id) {
	return sprintf('0x%x', id * 65536);
}

function parse_ttl(s) {
	if (s == null)
		return null;
	let t = lc(trim('' + s));
	if (!match(t, /^([0-9]{1,6}[dhms])+$/))
		return null;
	let total = 0;
	for (let m in match(t, /[0-9]{1,6}[dhms]/g)) {
		let u = substr(m[0], -1);
		let v = int(substr(m[0], 0, length(m[0]) - 1));
		let unit = 1;
		if (u == 'd')
			unit = 86400;
		else if (u == 'h')
			unit = 3600;
		else if (u == 'm')
			unit = 60;
		total += v * unit;
	}
	return total;
}

function ttl_text(secs) {
	if (secs % 86400 == 0)
		return sprintf('%dd', secs / 86400);
	if (secs % 3600 == 0)
		return sprintf('%dh', secs / 3600);
	if (secs % 60 == 0)
		return sprintf('%dm', secs / 60);
	return sprintf('%ds', secs);
}

// 寿命取值:无法识别或越界时按默认/边界处理并给出警告
function norm_ttl(v, dflt, lo, hi, label, warns) {
	let secs = parse_ttl(v);
	if (secs == null) {
		if (v != null && trim('' + v) != '')
			push(warns, sprintf('%s「%s」无法识别(应写成 30m、1h、1d 这类),已按默认值 %s 处理', label, v, ttl_text(dflt)));
		secs = dflt;
	}
	else if (secs < lo) {
		push(warns, sprintf('%s「%s」低于下限 %s,已按下限处理', label, v, ttl_text(lo)));
		secs = lo;
	}
	else if (secs > hi) {
		push(warns, sprintf('%s「%s」高于上限 %s,已按上限处理', label, v, ttl_text(hi)));
		secs = hi;
	}
	return { secs: secs, text: ttl_text(secs) };
}

function safe_ifname(s) {
	return (type(s) == 'string' && length(s) <= 15 && match(s, /^[A-Za-z0-9._@:-]+$/)) ? true : false;
}

// ---------------------------------------------------------------- 出口与成员

function wan_enabled(w, cfg) {
	let over = (cfg.wan_over ?? {})[w.wan_id];
	if (over != null && over.enabled != null)
		return to_bool(over.enabled, true);
	return to_bool(w.enabled, true);
}

function wan_label(wid, w, cfg) {
	let over = (cfg.wan_over ?? {})[wid];
	if (over != null && over.alias != null && trim('' + over.alias) != '')
		return trim('' + over.alias);
	if (w != null && w.alias != null && trim('' + w.alias) != '')
		return trim('' + w.alias);
	if (w != null && w.name != null && trim('' + w.name) != '')
		return trim('' + w.name);
	return wid;
}

// 一个出口成员的可用性。usable 按 §13.3:state=online && up && route4 && enabled && health!=down;
// usable6 另需它发放 LAN 前缀(caps.v6 && prefix6)且有 v6 默认路由。
function member_info(wid, w, cfg, group_off) {
	let m = {
		wan_id: wid, dev: '', iface: '', gw4: '', gw6: '', ptp: false, usable: false, reason: '',
		u6: false, carrier: false, present: false, prefix6: ''
	};
	if (w == null) {
		m.reason = group_off ? '出口组已停用' : '设备未接入';
		return m;
	}
	m.present = true;
	m.dev = w.dev ?? '';
	m.iface = w.iface ?? '';
	m.gw4 = w.gw4 ?? '';
	m.gw6 = w.gw6 ?? '';
	m.ptp = (w.ptp == true);
	let caps = w.caps ?? {};
	let health = w.health ?? {};
	if (w.prefix6 != null && w.prefix6 != '' && caps.v6 == true) {
		m.carrier = true;
		m.prefix6 = w.prefix6;
	}
	let enabled = wan_enabled(w, cfg);
	let linked = (w.state == 'online' && w.up == true);
	let hdown = (health.state == 'down');
	let alive = (!group_off && enabled && linked && !hdown);
	m.usable = (alive && w.route4 == true);
	m.u6 = (alive && m.carrier && w.route6 == true);
	if (group_off)
		m.reason = '出口组已停用';
	else if (!enabled)
		m.reason = '被用户停用';
	else if (w.state == 'absent')
		m.reason = '设备未接入';
	else if (w.state == 'disabled')
		m.reason = '接口已停用';
	else if (!linked)
		m.reason = '链路断开';
	else if (hdown)
		m.reason = '探测失败';
	else if (w.route4 != true)
		m.reason = '无默认路由';
	return m;
}

// ---------------------------------------------------------------- 规则 -> 匹配行

function port_set(items) {
	let m = merge_ports(items);
	let t = ranges_text(m);
	return (length(m) > 1) ? '{ ' + t + ' }' : t;
}

// 服务限定片段;fam=0 表示同一行覆盖 v4 与 v6
function svc_frags(n, p, fam, v6on) {
	let out = [];
	if (n.proto == 'tcp' || n.proto == 'udp')
		push(out, 'meta l4proto ' + n.proto);
	else if (n.proto == 'tcp_udp')
		push(out, 'meta l4proto { tcp, udp }');
	else if (n.proto == 'icmp') {
		if (fam == 6)
			push(out, 'meta l4proto ipv6-icmp');
		else if (fam == 0 && v6on)
			push(out, 'meta l4proto { icmp, ipv6-icmp }');
		else
			push(out, 'meta l4proto icmp');
	}
	if (length(p.dport) > 0)
		push(out, 'th dport ' + port_set(p.dport));
	if (length(p.sport) > 0)
		push(out, 'th sport ' + port_set(p.sport));
	return out;
}

function has_entries(p, dim, fam) {
	if (dim == 'src')
		return length(fam == 4 ? p.src.v4 : p.src.v6) > 0;
	if (dim == 'dst')
		return length(fam == 4 ? p.dst.v4 : p.dst.v6) > 0;
	return true;
}

// 维度片段;同时记录该规则用到了哪些集合
function dim_frag(dim, fam, rid, uses) {
	if (dim == 'mac') {
		uses.mac = true;
		return 'ether saddr @mac_' + rid;
	}
	if (dim == 'src') {
		uses['s' + fam] = true;
		return (fam == 4) ? 'ip saddr @s4_' + rid : 'ip6 saddr @s6_' + rid;
	}
	if (dim == 'dst') {
		uses['d' + fam] = true;
		return (fam == 4) ? 'ip daddr @d4_' + rid : 'ip6 daddr @d6_' + rid;
	}
	if (dim == 'dom') {
		uses['dns' + fam] = true;
		// OpenClash fake-ip 段的地址不是真实目的地,不能按它选路
		return (fam == 4) ? 'ip daddr @dns4_' + rid + ' ip daddr != ' + FAKEIP : 'ip6 daddr @dns6_' + rid;
	}
	uses['l' + fam] = true;
	return (fam == 4) ? 'ip daddr . meta l4proto . th dport @l4_' + rid : 'ip6 daddr . meta l4proto . th dport @l6_' + rid;
}

function mk_line(fam, dimfams, n, p, rid, v6on, uses) {
	let frags = [];
	for (let df in dimfams)
		push(frags, dim_frag(df[0], df[1], rid, uses));
	for (let s in svc_frags(n, p, fam, v6on))
		push(frags, s);
	return { fam: fam, pre: (fam == 0 && !v6on) ? 'meta nfproto ipv4' : '', frags: frags };
}

// 一条规则 -> 若干 classify 行(与出口编号无关,编号在渲染时填入)
function build_rule_lines(n, p, v6on, rid) {
	let present = {
		mac: length(p.mac) > 0,
		src: length(p.src.v4) + length(p.src.v6) > 0,
		dst: length(p.dst.v4) + length(p.dst.v6) > 0,
		dom: length(p.dom) > 0,
		app: length(p.app) > 0
	};
	let dims = [];
	let specific = false;
	for (let d in [ 'mac', 'src', 'dst', 'dom', 'app' ]) {
		if (present[d]) {
			push(dims, d);
			if (d != 'mac')
				specific = true;
		}
	}
	let fams = v6on ? [ 4, 6 ] : [ 4 ];
	let uses = {};
	let lines = [];

	if (!specific) {
		// 没有带地址族的维度:一行同时覆盖 v4 与 v6
		push(lines, mk_line(0, present.mac ? [ [ 'mac', 0 ] ] : [], n, p, rid, v6on, uses));
	}
	else if (n.match == 'all') {
		for (let fam in fams) {
			let ok = true;
			let dimfams = [];
			for (let d in dims) {
				if (!has_entries(p, d, fam))
					ok = false;
				push(dimfams, [ d, fam ]);
			}
			if (ok)
				push(lines, mk_line(fam, dimfams, n, p, rid, v6on, uses));
		}
	}
	else {
		for (let d in dims) {
			if (d == 'mac') {
				push(lines, mk_line(0, [ [ 'mac', 0 ] ], n, p, rid, v6on, uses));
				continue;
			}
			for (let fam in fams) {
				if (has_entries(p, d, fam))
					push(lines, mk_line(fam, [ [ d, fam ] ], n, p, rid, v6on, uses));
			}
		}
	}

	let reason = '';
	if (length(lines) == 0) {
		if (!v6on && length(p.src.v6) + length(p.dst.v6) > 0)
			reason = '该规则只有 IPv6 条目,而 IPv6 处理已关闭';
		else
			reason = '地址维度的 IPv4/IPv6 条目没有交集,没有可生成的匹配';
	}
	return { lines: lines, uses: uses, reason: reason };
}

// ---------------------------------------------------------------- 集合与元素文本

function set_decl(name, body) {
	return sprintf('add set %s %s { %s }', TBL, name, body);
}

function elem_text(items) {
	let t = [];
	for (let it in items)
		push(t, it.text);
	return join(', ', t);
}

// 垃圾回收文本(第二个事务):只删除本插件命名规则内、且已不在新规划里的对象
function gc_text(objects, existing) {
	if (existing == null)
		return '';
	let keep_c = {};
	let keep_s = {};
	let keep_n = {};
	for (let c in objects.chains)
		keep_c[c] = true;
	for (let s in objects.sets)
		keep_s[s] = true;
	for (let c in objects.counters)
		keep_n[c] = true;
	let out = [];
	for (let c in (existing.chains ?? [])) {
		if (!keep_c[c] && match(c, /^x_[0-9]+$/)) {
			push(out, sprintf('flush chain %s %s', TBL, c));
			push(out, sprintf('delete chain %s %s', TBL, c));
		}
	}
	for (let s in (existing.sets ?? [])) {
		if (!keep_s[s] && match(s, /^(mac|s4|s6|d4|d6|dns4|dns6|l4|l6)_[a-z0-9_]+$/)) {
			push(out, sprintf('flush set %s %s', TBL, s));
			push(out, sprintf('delete set %s %s', TBL, s));
		}
	}
	for (let c in (existing.counters ?? [])) {
		if (!keep_n[c] && match(c, /^c_[a-z0-9_]+$/))
			push(out, sprintf('delete counter %s %s', TBL, c));
	}
	return length(out) > 0 ? join('\n', out) + '\n' : '';
}

// ---------------------------------------------------------------- 主入口

// opts: { lan_ifs, apps, dns_capable, now(目前不用), prev_ids, existing }
//   prev_ids  : { "<target>|<on_fail>": id } 上次的出口编号,给出时同键沿用原编号(规则重排不换号)
//   existing  : { chains, sets, counters } 当前内核表里的对象名,给出时才生成 nft_gc
function compile(cfg, snap, opts) {
	let res = {
		ok: true, errors: [], warnings: [],
		nft: '', nft_gc: '', nft_rebuild: '', nft_sig: '',
		exits: [], ip_rules: [], routes: [],
		dns: { lines: [], sig: '', domains: 0 },
		rules: [], hash: '', objects: { chains: [], sets: [], counters: [] }
	};
	if (type(cfg) != 'object') {
		res.ok = false;
		push(res.errors, '配置无效:缺少 cfg 对象');
		return res;
	}
	opts = opts ?? {};
	snap = snap ?? {};
	let warns = res.warnings;
	let g = cfg.global ?? {};
	let rules_in = cfg.rules ?? [];
	let v6on = to_bool(g.ipv6, true);
	let dns_ttl = norm_ttl(g.dns_ttl, 86400, 3600, 604800, 'DNS 集合寿命', warns);
	let learn_ttl = norm_ttl(g.learn_ttl, 1800, 60, 604800, '应用学习寿命', warns);

	let ctx = build_ctx(cfg, snap, opts.apps, { dns_capable: opts.dns_capable });
	ctx.no_shadow = true;
	ctx.no_total = true;   // 规则总数超限由 compile 统一报一次
	let dns_on = ctx.caps.dns_sets;

	// 入口设备
	let lan_ifs = [];
	let seen_if = {};
	for (let d in (opts.lan_ifs ?? [])) {
		if (!safe_ifname(d)) {
			push(warns, sprintf('入口设备名 %s 不合法,已忽略', d));
			continue;
		}
		if (seen_if[d])
			continue;
		seen_if[d] = true;
		push(lan_ifs, d);
	}
	if (length(lan_ifs) == 0)
		push(warns, '没有任何参与策略路由的入口设备(lan_ifs 为空),规则不会生效');

	// ---- 逐条规则:校验 + 生成匹配行
	let items = [];
	let seen_rid = {};
	if (length(rules_in) > MAX_RULES)
		push(warns, sprintf('规则共 %d 条,超过上限 %d,排在后面的规则不会生效', length(rules_in), MAX_RULES));
	for (let i = 0; i < length(rules_in); i++) {
		let raw = rules_in[i];
		let it = { idx: i, id: '', rid: '', name: '', label: sprintf('第 %d 条', i + 1), status: 'active', reason: '', exit: null, counter: null, n: null, p: null, lines: [], uses: {}, key: '' };
		push(items, it);
		if (type(raw) != 'object') {
			it.status = 'error';
			it.reason = '规则内容无效';
			continue;
		}
		it.id = (raw.id != null) ? '' + raw.id : '';
		it.name = trim('' + (raw.name ?? ''));
		if (it.name != '')
			it.label = it.name;
		else if (it.id != '')
			it.label = it.id;
		if (!to_bool(raw.enabled, true)) {
			it.status = 'disabled';
			continue;
		}
		let rid = lc(it.id);
		if (!match(rid, /^[a-z0-9_]{1,24}$/)) {
			it.status = 'error';
			it.reason = '规则编号缺失、超过 24 个字符或含有不允许的字符(只允许字母、数字、下划线)';
			continue;
		}
		if (seen_rid[rid]) {
			it.status = 'error';
			it.reason = '规则编号与前面的规则重复';
			continue;
		}
		seen_rid[rid] = true;
		it.rid = rid;
		if (i >= MAX_RULES) {
			it.status = 'error';
			it.reason = sprintf('超过 %d 条规则上限,本条未生效', MAX_RULES);
			continue;
		}
		ctx.self_id = it.id;
		let v = validate_rule_ex(raw, ctx);
		if (!v.ok) {
			it.status = 'error';
			it.reason = v.errors[0].msg;
			continue;
		}
		for (let w in v.warnings)
			push(warns, sprintf('规则「%s」:%s', it.label, w.msg));
		it.n = v.normalized;
		it.p = v.parsed;
		let bl = build_rule_lines(it.n, it.p, v6on, rid);
		if (length(bl.lines) == 0) {
			it.status = 'error';
			it.reason = bl.reason;
			continue;
		}
		if (!v6on && length(it.p.src.v6) + length(it.p.dst.v6) > 0)
			push(warns, sprintf('规则「%s」的 IPv6 地址条目因 IPv6 处理已关闭而不会生效', it.label));
		it.lines = bl.lines;
		it.uses = bl.uses;
		it.key = it.n.target + '|' + it.n.on_fail;
	}

	// ---- 出口编号:规则顺序里首次出现的 (target,on_fail) 取一个 id(1..253)
	let keys = [];
	let kfirst = {};
	for (let it in items) {
		if (it.status == 'active' && kfirst[it.key] == null) {
			kfirst[it.key] = it;
			push(keys, it.key);
		}
	}
	let prev = opts.prev_ids ?? {};
	let used = {};
	let prev_vals = {};
	let kid = {};
	// 被删出口释放的 id 也算"旧 id":新出口优先取从未用过的号,避免劫持残留 ct mark
	for (let k, v in prev) {
		if (type(v) == 'int' && v >= 1 && v <= MAX_EXIT_ID)
			prev_vals['' + v] = true;
	}
	for (let k in keys) {
		let v = prev[k];
		if (type(v) == 'int' && v >= 1 && v <= MAX_EXIT_ID && !used['' + v]) {
			kid[k] = v;
			used['' + v] = true;
		}
	}
	for (let k in (keys)) {
		if (kid[k] != null)
			continue;
		let pick = 0;
		for (let c = 1; c <= MAX_EXIT_ID && pick == 0; c++) {
			if (!used['' + c] && !prev_vals['' + c])
				pick = c;
		}
		for (let c = 1; c <= MAX_EXIT_ID && pick == 0; c++) {
			if (!used['' + c])
				pick = c;
		}
		kid[k] = pick;
		used['' + pick] = true;
	}

	// ---- 出口对象、成员可用性、出口表路由
	let gmap = ctx._gmap;
	let wmap = ctx._wmap;
	let exits = [];
	let exit_by_id = {};
	let routes = [];
	let ip_rules = [];
	for (let k in keys) {
		let it0 = kfirst[k];
		let id = kid[k];
		let tg = match(it0.n.target, /^(wan|group):(.+)$/);
		let mis = [];
		let group_off = false;
		let label = '';
		if (tg[1] == 'wan') {
			mis = [ member_info(tg[2], wmap[tg[2]], cfg, false) ];
			label = wan_label(tg[2], wmap[tg[2]], cfg);
		}
		else {
			let grp = gmap[tg[2]] ?? {};
			group_off = !to_bool(grp.enabled, true);
			label = (grp.name != null && trim('' + grp.name) != '') ? trim('' + grp.name) : tg[2];
			let ms = list_of(grp.members);
			for (let j = 0; j < length(ms) && j < 8; j++)
				push(mis, member_info(ms[j], wmap[ms[j]], cfg, group_off));
		}
		let table = 4000 + id;
		let ex = { id: id, key: k, target: it0.n.target, on_fail: it0.n.on_fail, table: table, mark: mark_short(id),
			members: [], v4: false, v6: false, v6_reason: '' };
		let present = 0;
		let carriers = 0;
		let reasons = [];
		for (let j = 0; j < length(mis); j++) {
			let m = mis[j];
			push(ex.members, { wan_id: m.wan_id, dev: m.dev, iface: m.iface, gw4: m.gw4, gw6: m.gw6, ptp: m.ptp, usable: m.usable, reason: m.reason });
			if (m.present)
				present++;
			if (m.carrier)
				carriers++;
			if (m.usable) {
				ex.v4 = true;
				push(routes, { family: 4, table: table, dev: m.dev, via: m.ptp ? '' : m.gw4, metric: 1 + j, from: '' });
			}
			else if (m.reason != '')
				push(reasons, sprintf('%s:%s', wan_label(m.wan_id, wmap[m.wan_id], cfg), m.reason));
			if (v6on && m.u6)
				push(routes, { family: 6, table: table, dev: m.dev, via: m.ptp ? '' : m.gw6, metric: 1 + j, from: m.prefix6 });
		}
		// 成员全部在场且没有一个发放 LAN 前缀 -> v6 无法按该出口选路;含未接入成员时情况未知,不强制阻断
		let forced6 = (v6on && length(mis) > 0 && present == length(mis) && carriers == 0);
		ex.v6 = (v6on && !forced6);
		if (!v6on)
			ex.v6_reason = 'IPv6 处理已关闭';
		else if (forced6)
			ex.v6_reason = V6_BLOCK_REASON;
		ex.forced6 = forced6;
		ex.label = label;
		push(exits, ex);
		exit_by_id['' + id] = ex;

		if (!ex.v4) {
			let rs = length(reasons) > 0 ? join(';', reasons) : '没有成员';
			push(warns, sprintf('出口「%s」当前没有可用成员(%s),%s', label, rs,
				(ex.on_fail == 'block') ? '匹配的流量将被阻断' : '匹配的流量回落到系统默认路由'));
		}
	}
	exits = sort(exits, function(a, b) { return a.id - b.id; });

	// ---- ip rule 期望集(按出口结构生成,不随成员可用性变化)
	for (let ex in exits) {
		let spec = sprintf('fwmark %s/%s', ex.mark, MARK_MASK_TXT);
		let fams = v6on ? [ 4, 6 ] : [ 4 ];
		for (let fam in fams) {
			push(ip_rules, { family: fam, pref: 5001 + ex.id, spec: spec + ' lookup main suppress_prefixlength 0' });
			push(ip_rules, { family: fam, pref: 5301 + ex.id, spec: sprintf('%s lookup %d', spec, ex.table) });
			if (ex.on_fail == 'block' || (fam == 6 && ex.forced6))
				push(ip_rules, { family: fam, pref: 5601 + ex.id, spec: spec + ' unreachable' });
		}
	}
	ip_rules = sort(ip_rules, function(a, b) { return (a.family != b.family) ? (a.family - b.family) : (a.pref - b.pref); });

	// ---- 渲染 nft(事务一)
	let objects = { chains: [ 'prerouting', 'classify', 'learn' ], sets: [ 'lan_ifs' ], counters: [] };
	let decl_chain = [];
	let decl_set = [];
	let flush_set = [];
	let decl_elem = [];
	let decl_counter = [];
	let classify_rules = [];
	let learn_rules = [];
	let dns_rules = [];   // 域名规则:{ order, sets:[…], domains:[…] }

	for (let it in items) {
		if (it.status != 'active')
			continue;
		let id = kid[it.key];
		let rid = it.rid;
		it.exit = id;
		it.counter = 'c_' + rid;
		push(objects.counters, it.counter);
		push(decl_counter, sprintf('add counter %s %s', TBL, it.counter));
		let u = it.uses;
		let p = it.p;
		// 静态集合:先 flush 再写元素,同一事务内原子替换
		let stat = [];
		if (u.mac)
			push(stat, [ 'mac_' + rid, 'type ether_addr;', elem_text(p.mac) ]);
		if (u.s4)
			push(stat, [ 's4_' + rid, 'type ipv4_addr; flags interval;', elem_text(merge_addrs(p.src.v4)) ]);
		if (u.s6)
			push(stat, [ 's6_' + rid, 'type ipv6_addr; flags interval;', elem_text(merge_addrs(p.src.v6)) ]);
		if (u.d4)
			push(stat, [ 'd4_' + rid, 'type ipv4_addr; flags interval;', elem_text(merge_addrs(p.dst.v4)) ]);
		if (u.d6)
			push(stat, [ 'd6_' + rid, 'type ipv6_addr; flags interval;', elem_text(merge_addrs(p.dst.v6)) ]);
		for (let s in stat) {
			push(objects.sets, s[0]);
			push(decl_set, set_decl(s[0], s[1]));
			push(flush_set, sprintf('flush set %s %s', TBL, s[0]));
			push(decl_elem, sprintf('add element %s %s { %s }', TBL, s[0], s[2]));
		}
		// 动态集合:DNS 解析集合由 dnsmasq 写入,学习集合由 learn 链写入,绝不 flush
		let dsets = [];
		for (let fam in [ 4, 6 ]) {
			if (u['dns' + fam]) {
				let nmset = sprintf('dns%d_%s', fam, rid);
				push(objects.sets, nmset);
				push(decl_set, set_decl(nmset, sprintf('type ipv%d_addr; flags timeout; timeout %s; size %d; gc-interval 1h;', fam, dns_ttl.text, DNS_SIZE)));
				push(dsets, sprintf('%d#inet#mwrt_pr#%s', fam, nmset));
			}
			if (u['l' + fam]) {
				let nmset = sprintf('l%d_%s', fam, rid);
				push(objects.sets, nmset);
				push(decl_set, set_decl(nmset, sprintf('type ipv%d_addr . inet_proto . inet_service; flags dynamic,timeout; timeout %s; size %d;', fam, learn_ttl.text, LEARN_SIZE)));
				push(learn_rules, sprintf('meta nfproto ipv%d meta l4proto { tcp, udp } ct mark & 0xffff { %s } update @%s { ip%s daddr . meta l4proto . th dport timeout %s }',
					fam, ranges_text(merge_ports(p.app)), nmset, (fam == 6) ? '6' : '', learn_ttl.text));
			}
		}
		if (length(dsets) > 0 && dns_on) {
			let doms = [];
			for (let d in p.dom)
				push(doms, d.text);
			push(dns_rules, { sets: dsets, domains: doms });
		}
		// classify 行
		for (let ln in it.lines) {
			let parts = [];
			if (ln.pre != '')
				push(parts, ln.pre);
			for (let f in ln.frags)
				push(parts, f);
			push(parts, sprintf('counter name "%s" goto x_%d', it.counter, id));
			push(classify_rules, sprintf('add rule %s classify %s', TBL, join(' ', parts)));
		}
	}

	// 出口链
	let xchains = [];
	let vmap = [];
	for (let ex in exits) {
		let nmx = 'x_' + ex.id;
		push(objects.chains, nmx);
		push(xchains, sprintf('add rule %s %s ct mark set ct mark & %s | %s', TBL, nmx, MARK_KEEP_TXT, mark_txt(ex.id)));
		push(xchains, sprintf('add rule %s %s meta mark set meta mark & %s | %s', TBL, nmx, MARK_KEEP_TXT, mark_txt(ex.id)));
		push(vmap, sprintf('%s : goto %s', mark_txt(ex.id), nmx));
	}

	let L = [];
	push(L, sprintf('add table %s', TBL));
	push(L, sprintf('add chain %s prerouting { type filter hook prerouting priority %d; policy accept; }', TBL, PRI_PRE));
	push(L, sprintf('add chain %s classify', TBL));
	for (let ex in exits)
		push(L, sprintf('add chain %s x_%d', TBL, ex.id));
	push(L, sprintf('add chain %s learn { type filter hook forward priority %d; policy accept; }', TBL, PRI_LEARN));
	for (let c in objects.chains)
		push(L, sprintf('flush chain %s %s', TBL, c));
	push(L, set_decl('lan_ifs', 'type ifname;'));
	push(L, sprintf('flush set %s lan_ifs', TBL));
	if (length(lan_ifs) > 0) {
		let q = [];
		for (let d in lan_ifs)
			push(q, '"' + d + '"');
		push(L, sprintf('add element %s lan_ifs { %s }', TBL, join(', ', q)));
	}
	for (let s in decl_set)
		push(L, s);
	for (let s in flush_set)
		push(L, s);
	for (let s in decl_elem)
		push(L, s);
	for (let s in decl_counter)
		push(L, s);
	push(L, sprintf('add rule %s prerouting iifname != @lan_ifs return', TBL));
	if (length(exits) > 0) {
		push(L, sprintf('add rule %s prerouting ct state new jump classify', TBL));
		push(L, sprintf('add rule %s prerouting ct mark & %s vmap { %s }', TBL, MARK_MASK_TXT, join(', ', vmap)));
	}
	for (let s in classify_rules)
		push(L, s);
	for (let s in xchains)
		push(L, s);
	if (length(learn_rules) > 0) {
		// OAF 在 forward(-149) 识别,学习链排在它之后(-148);只看 LAN 来向、原方向、已被识别的包
		push(L, sprintf('add rule %s learn iifname != @lan_ifs return', TBL));
		push(L, sprintf('add rule %s learn ct direction reply return', TBL));
		push(L, sprintf('add rule %s learn ct mark & 0xffff == 0 return', TBL));
		for (let s in learn_rules)
			push(L, sprintf('add rule %s learn %s', TBL, s));
	}
	res.nft = join('\n', L) + '\n';
	res.nft_rebuild = sprintf('add table %s\ndelete table %s\n', TBL, TBL) + res.nft;
	res.objects = objects;
	res.nft_gc = gc_text(objects, opts.existing);
	res.nft_sig = sprintf('%s;dns_ttl=%s;learn_ttl=%s;dns_size=%d;learn_size=%d;pri=%d/%d',
		SIG_VER, dns_ttl.text, learn_ttl.text, DNS_SIZE, LEARN_SIZE, PRI_PRE, PRI_LEARN);

	// ---- DNS 映射:dnsmasq 每个问题名只取最长后缀那一条,所以每个域名一行,集合取所有后缀规则的并集
	let by_dom = {};
	let all_doms = [];
	for (let i = 0; i < length(dns_rules); i++) {
		for (let d in dns_rules[i].domains) {
			if (by_dom[d] == null) {
				by_dom[d] = [];
				push(all_doms, d);
			}
			push(by_dom[d], i);
		}
	}
	all_doms = sort(all_doms);
	let dlines = [];
	for (let d in all_doms) {
		let idxs = [];
		let have = {};
		let labels = split(d, '.');
		for (let s = 0; s < length(labels); s++) {
			let suf = join('.', slice(labels, s));
			let lst = by_dom[suf];
			if (lst == null)
				continue;
			for (let ri in lst) {
				if (!have['' + ri]) {
					have['' + ri] = true;
					push(idxs, ri);
				}
			}
		}
		idxs = sort(idxs, function(a, b) { return a - b; });
		let sets = [];
		for (let ri in idxs) {
			for (let st in dns_rules[ri].sets)
				push(sets, st);
		}
		push(dlines, sprintf('nftset=/%s/%s', d, join(',', sets)));
	}
	res.dns = { lines: dlines, sig: hash_str(join('\n', dlines)), domains: length(dlines) };

	// ---- 已有出口的 v6 提示:强制阻断只对可能承载 v6 的行有意义
	for (let ex in exits) {
		if (!ex.forced6)
			continue;
		let carry6 = false;
		for (let it in items) {
			if (it.status != 'active' || kid[it.key] != ex.id)
				continue;
			for (let ln in it.lines) {
				if (ln.fam != 4)
					carry6 = true;
			}
		}
		if (carry6)
			push(warns, sprintf('出口「%s」不发放 LAN 前缀:匹配到它的 IPv6 流量会被阻断(客户端回落 IPv4),不会回落系统默认路由', ex.label));
	}

	// ---- 汇总
	let out_exits = [];
	for (let ex in exits) {
		push(out_exits, { id: ex.id, key: ex.key, target: ex.target, on_fail: ex.on_fail, table: ex.table, mark: ex.mark,
			members: ex.members, v4: ex.v4, v6: ex.v6, v6_reason: ex.v6_reason });
	}
	res.exits = out_exits;
	res.ip_rules = ip_rules;
	res.routes = routes;
	for (let it in items) {
		push(res.rules, { id: it.id, name: it.name, status: it.status, reason: it.reason, exit: it.exit, counter: it.counter });
		if (it.status == 'error')
			push(warns, sprintf('规则「%s」未生效:%s', it.label, it.reason));
	}
	let ipt = [];
	for (let r in ip_rules)
		push(ipt, sprintf('%d %d %s', r.family, r.pref, r.spec));
	res.hash = hash_str(res.nft + '\n' + join('\n', ipt));
	return res;
}

export { compile, gc_text, hash_str };
