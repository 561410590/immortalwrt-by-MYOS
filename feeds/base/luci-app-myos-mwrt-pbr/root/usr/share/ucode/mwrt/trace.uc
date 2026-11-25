'use strict';
// 路由试算(只读):给定一个假想的新连接,按当前配置推演它命中哪条规则、走哪个出口。
// 不发包、不写任何状态。IP/MAC/端口/协议维度按配置判定;域名与应用维度取决于运行时集合,
// 只能报告集合当前是否含目标地址,不含时记为 unknown 并说明原因。

import * as fs from 'fs';
import { run, sh_quote, MARK_MASK } from 'mwrt.lib';
import { to_bool, parse_addr, parse_mac, parse_domain, words_fmt, words_cmp, validate_rule_ex, build_ctx } from 'mwrt.validate';
import { compile } from 'mwrt.compile';

// 单次试算最多向 nft 查询的集合元素个数,防止规则很多时 fork 过多
const MAX_QUERY = 24;
// 198.18.0.0/15(OpenClash fake-ip 段)的首末地址字数组
const FAKEIP_LO = [ 0xc612, 0x0000 ];
const FAKEIP_HI = [ 0xc613, 0xffff ];

// ---------------------------------------------------------------- 只读系统访问(opts.io 仅供离线检查注入)

function sh(opts, cmd) {
	if (opts.io != null && opts.io.run != null)
		return opts.io.run(cmd);
	return run(cmd);
}

function rd(opts, path) {
	if (opts.io != null && opts.io.read != null)
		return opts.io.read(path);
	return fs.readfile(path);
}

// ---------------------------------------------------------------- 输入整理

function to_port(v, label, errs) {
	if (v == null || trim('' + v) == '')
		return 0;
	let t = trim('' + v);
	if (!match(t, /^[0-9]{1,5}$/) || int(t) < 1 || int(t) > 65535) {
		push(errs, sprintf('%s「%s」不是 1-65535 的端口', label, t));
		return 0;
	}
	return int(t);
}

// 成功返回 { P },失败返回 { error }
function prepare(params) {
	let errs = [];
	let P = { fam: 0, src: null, dst: null, mac: '', mac_auto: false, proto: '', dport: 0, sport: 0, domain: '' };
	if (type(params) != 'object')
		params = {};

	let dst_s = trim('' + (params.dst ?? ''));
	if (dst_s == '')
		push(errs, '请填写目的地址(域名规则看的是该域名解析出的 IP,试算不会发起任何解析)');
	else {
		let e = [];
		let a = parse_addr(dst_s, e);
		if (a == null)
			push(errs, '目的地址无效:' + (e[0] ?? dst_s));
		else if (a.kind != 'host')
			push(errs, '目的地址必须是单个 IP,不能是网段或区间');
		else {
			P.dst = a;
			P.fam = a.family;
		}
	}

	let src_s = trim('' + (params.src ?? ''));
	if (src_s != '') {
		let e = [];
		let a = parse_addr(src_s, e);
		if (a == null)
			push(errs, '源地址无效:' + (e[0] ?? src_s));
		else if (a.kind != 'host')
			push(errs, '源地址必须是单个 IP,不能是网段或区间');
		else if (P.dst != null && a.family != P.fam)
			push(errs, '源地址与目的地址不是同一种协议(IPv4/IPv6)');
		else
			P.src = a;
	}

	let mac_s = trim('' + (params.mac ?? ''));
	if (mac_s != '') {
		let e = [];
		let m = parse_mac(mac_s, e);
		if (m == null)
			push(errs, e[0] ?? 'MAC 地址无效');
		else
			P.mac = m.text;
	}

	let pr = lc(trim('' + (params.proto ?? '')));
	if (pr == 'any')
		pr = '';
	if (pr == 'icmpv6' || pr == 'ipv6-icmp')
		pr = 'icmp';
	if (pr != '' && pr != 'tcp' && pr != 'udp' && pr != 'icmp')
		push(errs, '协议只能是 tcp、udp 或 icmp');
	else
		P.proto = pr;

	P.dport = to_port(params.dport, '目的端口', errs);
	P.sport = to_port(params.sport, '源端口', errs);
	if (P.proto == 'icmp' && (P.dport > 0 || P.sport > 0))
		push(errs, 'ICMP 没有端口,请去掉端口或改选 tcp/udp');

	let dom_s = trim('' + (params.domain ?? ''));
	if (dom_s != '') {
		let e = [];
		let d = parse_domain(dom_s, e);
		if (d == null)
			push(errs, e[0] ?? '域名无效');
		else
			P.domain = d.text;
	}

	if (length(errs) > 0)
		return { error: join(';', errs) };
	return { P: P };
}

// ---------------------------------------------------------------- 内核侧只读信息

// 一次 nft 查询同时回答"表是否存在"与"哪些入口设备参与"
function read_lan_ifs(opts) {
	let r = sh(opts, 'nft list set inet mwrt_pr lan_ifs');
	if (r.rc != 0)
		return { table: false, ifs: [] };
	let ifs = [];
	let m = match(r.out, /elements = \{([^}]*)\}/);
	if (m != null) {
		let qs = match(m[1], /"[^"]+"/g);
		for (let q in (qs ?? []))
			push(ifs, substr(q[0], 1, length(q[0]) - 2));
	}
	return { table: true, ifs: ifs };
}

// 集合是否含某元素:true/false;表不存在或达到查询上限返回 null。
// nft 对"没有该元素"与"没有该集合"都返回失败,调用方的文案要把两种可能都说清
function set_has(T, name, elem) {
	if (!T.table)
		return null;
	let key = name + '#' + elem;
	if (T.qcache[key] != null)
		return (T.qcache[key] == 'y');
	if (T.qn >= MAX_QUERY)
		return null;
	T.qn++;
	let r = sh(T.opts, sprintf('nft get element inet mwrt_pr %s { %s }', name, elem));
	T.qcache[key] = (r.rc == 0) ? 'y' : 'n';
	return (r.rc == 0);
}

// 本机邻居表里按源地址找 MAC;找不到返回 ''
function find_mac(opts, P) {
	if (P.src == null)
		return '';
	if (P.fam == 4) {
		let sip = words_fmt(P.src.lo, 4);
		let t = rd(opts, '/proc/net/arp');
		if (t == null)
			return '';
		for (let ln in split(t, '\n')) {
			let f = split(trim(ln), /[ \t]+/);
			if (length(f) >= 4 && f[0] == sip && f[2] != '0x0' && match(lc(f[3]), /^[0-9a-f]{2}(:[0-9a-f]{2}){5}$/))
				return lc(f[3]);
		}
		return '';
	}
	let r = sh(opts, 'ip -6 neigh show');
	if (r.rc != 0)
		return '';
	for (let ln in split(r.out, '\n')) {
		let f = split(trim(ln), /[ \t]+/);
		if (length(f) < 5)
			continue;
		let i = index(f, 'lladdr');
		if (i < 0 || i + 1 >= length(f))
			continue;
		let a = parse_addr(f[0], []);
		if (a != null && a.kind == 'host' && a.family == 6 && words_cmp(a.lo, P.src.lo) == 0 && match(lc(f[i + 1]), /^[0-9a-f]{2}(:[0-9a-f]{2}){5}$/))
			return lc(f[i + 1]);
	}
	return '';
}

// 解析 `ip route get` 输出:返回 { ok, text, dev, via, table }
function route_get(opts, P, iif, mark) {
	let cmd = sprintf('ip -%d route get %s', P.fam, sh_quote(words_fmt(P.dst.lo, P.fam)));
	if (P.src != null && iif != '')
		cmd += sprintf(' from %s iif %s', sh_quote(words_fmt(P.src.lo, P.fam)), sh_quote(iif));
	if (mark != '')
		cmd += ' mark ' + mark;
	let r = sh(opts, cmd);
	let first = '';
	for (let ln in split(r.out, '\n')) {
		let t = trim(ln);
		if (t != '') {
			first = t;
			break;
		}
	}
	let res = { ok: (r.rc == 0 && first != ''), text: first, dev: '', via: '', table: '' };
	if (!res.ok)
		return res;
	let md = match(first, /(^| )dev ([^ ]+)/);
	let mv = match(first, /(^| )via ([^ ]+)/);
	let mt = match(first, /(^| )table ([^ ]+)/);
	if (md != null)
		res.dev = md[2];
	if (mv != null)
		res.via = mv[2];
	if (mt != null)
		res.table = mt[2];
	return res;
}

// 源地址所在的入口设备:`ip route get <src>` 的 dev;本机自身地址返回 local
function src_ingress(opts, P) {
	if (P.src == null)
		return { iif: '', local: false };
	let r = sh(opts, sprintf('ip -%d route get %s', P.fam, sh_quote(words_fmt(P.src.lo, P.fam))));
	if (r.rc != 0)
		return { iif: '', local: false };
	let first = '';
	for (let ln in split(r.out, '\n')) {
		let t = trim(ln);
		if (t != '') {
			first = t;
			break;
		}
	}
	if (substr(first, 0, 6) == 'local ')
		return { iif: '', local: true };
	let md = match(first, /(^| )dev ([A-Za-z0-9._@:-]{1,15})( |$)/);
	return { iif: (md != null) ? md[2] : '', local: false };
}

// 内核里是否已装出口 mark 对应的 ip rule(没装就查不出策略路由的真实结果)
function rule_installed(opts, fam, mark) {
	let r = sh(opts, sprintf('ip -%d rule show', fam));
	if (r.rc != 0)
		return false;
	return index(r.out, sprintf('fwmark %s/0x%x', mark, MARK_MASK)) >= 0;
}

// ---------------------------------------------------------------- 三值逻辑:T 成立 / F 不成立 / U 取决于运行时状态或缺少输入

function and_all(parts) {
	let first_u = null;
	let whys = [];
	for (let x in parts) {
		if (x.v == 'F')
			return x;
		if (x.v == 'U' && first_u == null)
			first_u = x;
		if (x.v == 'T' && x.why != '')
			push(whys, x.why);
	}
	if (first_u != null)
		return first_u;
	return { v: 'T', why: join(';', whys) };
}

function or_any(parts) {
	let any_t = false;
	let first_u = null;
	let first_f = null;
	let whys = [];
	for (let x in parts) {
		if (x.v == 'T') {
			any_t = true;
			if (x.why != '')
				push(whys, x.why);
		}
		else if (x.v == 'U' && first_u == null)
			first_u = x;
		else if (x.v == 'F' && first_f == null)
			first_f = x;
	}
	if (any_t)
		return { v: 'T', why: join(';', whys) };
	if (first_u != null)
		return first_u;
	return first_f ?? { v: 'F', why: '' };
}

// ---------------------------------------------------------------- 各维度判定(与 compile.uc 的 build_rule_lines 保持一致)

function dim_mac(T, p) {
	if (T.P.mac == '' && !T.mac_tried) {
		T.mac_tried = true;
		let m = find_mac(T.opts, T.P);
		if (m != '') {
			T.P.mac = m;
			T.P.mac_auto = true;
		}
	}
	if (T.P.mac == '')
		return { v: 'U', why: '没有给出设备 MAC,本机邻居表里也没有该源地址的记录,无法判断设备维度' };
	for (let m in p.mac) {
		if (m.text == T.P.mac)
			return { v: 'T', why: sprintf('设备 %s 在规则的设备列表内', T.P.mac) };
	}
	return { v: 'F', why: sprintf('设备 %s 不在规则的设备列表内', T.P.mac) };
}

function addr_hit(list, w) {
	for (let a in list) {
		if (words_cmp(a.lo, w) <= 0 && words_cmp(w, a.hi) <= 0)
			return a;
	}
	return null;
}

function dim_addr(T, list, w, label) {
	if (w == null)
		return { v: 'U', why: sprintf('没有给出%s,无法判断', label) };
	if (length(list) == 0)
		return { v: 'F', why: sprintf('规则没有适用于 %s 的%s条目', (T.P.fam == 4) ? 'IPv4' : 'IPv6', label) };
	let h = addr_hit(list, w);
	if (h == null)
		return { v: 'F', why: sprintf('%s %s 不在规则的%s范围内', label, words_fmt(w, T.P.fam), label) };
	return { v: 'T', why: sprintf('%s %s 在 %s 内', label, words_fmt(w, T.P.fam), h.text) };
}

function suffix_of(domain, list) {
	for (let d in list) {
		let n = length(d.text);
		if (domain == d.text || (length(domain) > n && substr(domain, -(n + 1)) == '.' + d.text))
			return d.text;
	}
	return null;
}

function dim_dom(T, p, rid) {
	let P = T.P;
	if (P.fam == 4 && words_cmp(P.dst.lo, FAKEIP_LO) >= 0 && words_cmp(P.dst.lo, FAKEIP_HI) <= 0)
		return { v: 'F', why: '目的地址在 198.18.0.0/15(OpenClash fake-ip 段)内,域名规则不按该段地址选路' };
	let hit = (P.domain != '') ? suffix_of(P.domain, p.dom) : null;
	let has = set_has(T, sprintf('dns%d_%s', P.fam, rid), T.dst_txt);
	if (has == true) {
		let w1 = sprintf('DNS 解析集合里已有 %s', T.dst_txt);
		if (P.domain != '' && hit == null)
			w1 += sprintf('(域名 %s 不在规则列表内,是共享地址带来的命中)', P.domain);
		return { v: 'T', why: w1 };
	}
	if (P.domain != '' && hit == null)
		return { v: 'F', why: sprintf('域名 %s 不在该规则的域名列表内,DNS 解析集合里也没有 %s', P.domain, T.dst_txt) };
	let w = '';
	if (has == null)
		w = '当前读不到 DNS 解析集合(策略路由尚未应用,或本次查询已达上限),无法确认目的地址是否已被收录';
	else if (hit != null)
		w = sprintf('域名 %s 属于规则域名 %s,但 DNS 解析集合当前不含 %s。只有客户端经本机 dnsmasq 解析过该域名、地址被写入集合之后才会命中,第一次解析前的连接走系统默认路由', P.domain, hit, T.dst_txt);
	else
		w = sprintf('DNS 解析集合当前不含 %s。规则只能命中经本机 dnsmasq 解析出来的地址', T.dst_txt);
	return { v: 'U', why: w };
}

function dim_app(T, p, rid) {
	let P = T.P;
	if ((P.proto != 'tcp' && P.proto != 'udp') || P.dport == 0)
		return { v: 'U', why: '应用学习集按「目的地址 . 协议 . 目的端口」查询,试算没有给出协议或目的端口' };
	let has = set_has(T, sprintf('l%d_%s', P.fam, rid), sprintf('%s . %s . %d', T.dst_txt, P.proto, P.dport));
	if (has == true)
		return { v: 'T', why: sprintf('应用学习集里已有 %s . %s . %d', T.dst_txt, P.proto, P.dport) };
	if (has == null)
		return { v: 'U', why: '当前读不到应用学习集(策略路由尚未应用,或本次查询已达上限),无法确认' };
	return { v: 'U', why: '应用学习集当前不含该「目的地址 . 协议 . 端口」:应用识别是学习式的,识别后只对之后的新连接生效,第一个连接和前几个包不会按应用选路' };
}

function port_in(list, port) {
	for (let r in list) {
		if (r.lo <= port && port <= r.hi)
			return r;
	}
	return null;
}

function svc_eval(T, n, p) {
	let P = T.P;
	let parts = [];
	if (n.proto != 'any') {
		let want = (n.proto == 'tcp_udp') ? 'TCP/UDP' : uc(n.proto);
		if (P.proto == '')
			push(parts, { v: 'U', why: sprintf('规则限定协议 %s,试算没有给出协议', want) });
		else if ((n.proto == 'tcp_udp') ? (P.proto == 'tcp' || P.proto == 'udp') : (P.proto == n.proto))
			push(parts, { v: 'T', why: '协议 ' + uc(P.proto) });
		else
			push(parts, { v: 'F', why: sprintf('规则限定协议 %s,而试算的协议是 %s', want, uc(P.proto)) });
	}
	if (length(p.dport) > 0) {
		if (P.dport == 0)
			push(parts, { v: 'U', why: '规则限定了目的端口,试算没有给出目的端口' });
		else if (port_in(p.dport, P.dport) != null)
			push(parts, { v: 'T', why: sprintf('目的端口 %d', P.dport) });
		else
			push(parts, { v: 'F', why: sprintf('目的端口 %d 不在规则限定的端口内', P.dport) });
	}
	if (length(p.sport) > 0) {
		if (P.sport == 0)
			push(parts, { v: 'U', why: '规则限定了源端口,试算没有给出源端口' });
		else if (port_in(p.sport, P.sport) != null)
			push(parts, { v: 'T', why: sprintf('源端口 %d', P.sport) });
		else
			push(parts, { v: 'F', why: sprintf('源端口 %d 不在规则限定的端口内', P.sport) });
	}
	return and_all(parts);
}

// 一条已校验的规则 -> { v, why }。match=all 为各维度与,match=any 为各维度或;服务限定恒与
function eval_rule(T, v, rid) {
	let n = v.normalized;
	let p = v.parsed;
	let P = T.P;
	let dims = [];
	if (length(p.mac) > 0)
		push(dims, dim_mac(T, p));
	if (length(p.src.v4) + length(p.src.v6) > 0)
		push(dims, dim_addr(T, (P.fam == 4) ? p.src.v4 : p.src.v6, (P.src == null) ? null : P.src.lo, '源地址'));
	if (length(p.dst.v4) + length(p.dst.v6) > 0)
		push(dims, dim_addr(T, (P.fam == 4) ? p.dst.v4 : p.dst.v6, P.dst.lo, '目的地址'));
	if (length(p.dom) > 0)
		push(dims, dim_dom(T, p, rid));
	if (length(p.app) > 0)
		push(dims, dim_app(T, p, rid));
	let parts = [];
	if (length(dims) > 0)
		push(parts, (n.match == 'any') ? or_any(dims) : and_all(dims));
	push(parts, svc_eval(T, n, p));
	return and_all(parts);
}

// ---------------------------------------------------------------- 计划与出口

function ensure_plan(opts, cfg, snap, lan_ifs) {
	let plan = opts.plan;
	if (type(plan) == 'object' && type(plan.exits) == 'array' && type(plan.rules) == 'array')
		return plan;
	// 调用方没给计划:按当前配置现场编译,出口编号沿用上次应用的编号
	let prev = {};
	let stt = (type(opts.state) == 'object') ? opts.state : {};
	for (let e in (stt.exits ?? [])) {
		if (e.key != null && type(e.id) == 'int')
			prev[e.key] = e.id;
	}
	return compile(cfg, snap, { lan_ifs: lan_ifs, apps: opts.apps, dns_capable: opts.dns_capable, prev_ids: prev });
}

function plan_rule(plan, raw, idx) {
	let r = plan.rules[idx];
	if (r != null && r.id == raw.id)
		return r;
	for (let x in plan.rules) {
		if (x.id == raw.id)
			return x;
	}
	return null;
}

function find_exit(plan, id) {
	for (let e in plan.exits) {
		if (e.id == id)
			return e;
	}
	return null;
}

// 计划里该出口该族的出口表路由,按 metric 取第一条
function first_route(plan, fam, table) {
	let best = null;
	for (let r in (plan.routes ?? [])) {
		if (r.family == fam && r.table == table && (best == null || r.metric < best.metric))
			best = r;
	}
	return best;
}

// 主表里覆盖目的地的最具体的非默认路由(引擎的 suppress_prefixlength 0 规则会让这类流量直接走主表,不进出口表)
function main_specific(opts, P) {
	let r = sh(opts, sprintf('ip -%d route show table main to match %s', P.fam, sh_quote(words_fmt(P.dst.lo, P.fam))));
	if (r.rc != 0)
		return null;
	let best = null;
	let bl = 0;
	for (let ln in split(r.out, '\n')) {
		if (match(ln, /^[ \t]/))
			continue;
		let t = trim(ln);
		let f = split(t, /[ \t]+/);
		if (t == '' || f[0] == 'default' || match(f[0], /^(unreachable|blackhole|prohibit|throw|local|broadcast|multicast)$/))
			continue;
		let m = match(f[0], /\/([0-9]+)$/);
		let pl = (m != null) ? +m[1] : ((P.fam == 6) ? 128 : 32);
		let md = match(t, /(^| )dev ([^ ]+)/);
		let mv = match(t, /(^| )via ([^ ]+)/);
		if (pl > bl && md != null) {
			bl = pl;
			best = { text: t, dev: md[2], via: (mv != null) ? mv[2] : '' };
		}
	}
	return best;
}

// 出口表里已经没有可用路由时,按 on_fail 说明会发生什么
function nohop_text(ex) {
	if (ex.on_fail == 'block')
		return '该出口当前没有可用成员,而「不可用时」设为阻断:流量会被丢弃';
	return '该出口当前没有可用成员,「不可用时」设为回落系统默认路由';
}

// ---------------------------------------------------------------- 入口

function run_trace(params, opts) {
	if (type(opts) != 'object')
		opts = {};
	let pr = prepare(params);
	if (pr.error != null)
		return { ok: false, error: pr.error };
	let P = pr.P;
	let cfg = opts.cfg;
	if (type(cfg) != 'object')
		return { ok: false, error: '缺少策略路由配置,无法试算' };
	let snap = (type(opts.snap) == 'object') ? opts.snap : {};
	let g = (type(cfg.global) == 'object') ? cfg.global : {};
	let v6on = to_bool(g.ipv6, true);

	let live = read_lan_ifs(opts);
	let T = { opts: opts, P: P, table: live.table, qn: 0, qcache: {}, mac_tried: false, dst_txt: words_fmt(P.dst.lo, P.fam) };
	let plan = ensure_plan(opts, cfg, snap, live.ifs);
	if (plan == null || type(plan.exits) != 'array' || type(plan.rules) != 'array')
		return { ok: false, error: '无法得到当前配置的编译计划' };

	let notes = [];
	let checked = [];
	let unknowns = 0;
	let matched = null;
	let mrule = null;
	let ing = src_ingress(opts, P);

	if (!live.table)
		push(notes, '策略路由尚未应用到内核(没有 mwrt_pr 表):以下是按当前配置的推演,不代表正在发生的转发');

	// 是否会进入规则评估(与 prerouting 的 `iifname != @lan_ifs return` 以及 IPv6 开关一致)
	let skip = '';
	if (ing.local)
		skip = '源地址是本机自己的地址:本机发出的流量不经过 LAN 入口的策略路由判定';
	else if (P.fam == 6 && !v6on)
		skip = 'IPv6 处理已关闭,IPv6 流量不受策略路由影响';
	else if (live.table && length(live.ifs) == 0)
		skip = '入口设备集合 lan_ifs 为空,规则不会评估';
	else if (live.table && ing.iif != '' && !(ing.iif in live.ifs))
		skip = sprintf('源地址所在的入口设备 %s 不在参与策略路由的设备集合(%s)内,规则不会评估', ing.iif, join('、', live.ifs));
	if (skip != '')
		push(notes, skip);

	if (skip == '') {
		T.ctx = build_ctx(cfg, snap, opts.apps, { dns_capable: opts.dns_capable });
		T.ctx.no_shadow = true;
		T.ctx.no_total = true;
		let rules = (type(cfg.rules) == 'array') ? cfg.rules : [];
		for (let i = 0; i < length(rules) && matched == null; i++) {
			let raw = rules[i];
			if (type(raw) != 'object')
				continue;
			let st = plan_rule(plan, raw, i);
			let name = trim('' + (raw.name ?? ''));
			if (name == '')
				name = '' + (raw.id ?? sprintf('第 %d 条', i + 1));
			let item = { id: '' + (raw.id ?? ''), name: name, result: 'nomatch', reason: '' };
			push(checked, item);
			if (st == null) {
				item.result = 'unknown';
				item.reason = '该规则不在当前计划里(配置可能有尚未应用的变更)';
				unknowns++;
				continue;
			}
			if (st.status == 'disabled') {
				item.reason = '规则已停用';
				continue;
			}
			if (st.status != 'active') {
				item.reason = '规则未生效:' + (st.reason ?? '');
				continue;
			}
			let rid = lc('' + raw.id);
			T.ctx.self_id = raw.id;
			let v = validate_rule_ex(raw, T.ctx);
			if (!v.ok) {
				item.reason = '规则未生效:' + ((length(v.errors) > 0) ? v.errors[0].msg : '校验未通过');
				continue;
			}
			let e = eval_rule(T, v, rid);
			if (e.v == 'T') {
				item.result = 'match';
				item.reason = (e.why != '') ? e.why : '没有需要判断的条件(全部放行)';
				matched = { id: item.id, name: name };
				mrule = st;
			}
			else if (e.v == 'U') {
				item.result = 'unknown';
				item.reason = e.why;
				unknowns++;
			}
			else
				item.reason = e.why;
		}
	}

	let uncertain = (unknowns > 0);
	if (matched != null && unknowns > 0)
		push(notes, sprintf('排在命中规则之前有 %d 条规则的结果取决于运行时集合(DNS 解析集合/应用学习集)或缺少输入;集合收录该地址后,它们会先于本规则命中', unknowns));
	if (matched == null && skip == '' && unknowns > 0)
		push(notes, sprintf('没有规则明确命中,但有 %d 条规则取决于运行时集合或缺少输入,实际走向可能不同', unknowns));
	if (matched == null && skip == '' && unknowns == 0)
		push(notes, '没有规则命中:新连接不打标,走系统默认路由');
	if (P.mac_auto)
		push(notes, sprintf('设备 MAC 取自本机邻居表(%s);设备维度只对二层直连本机的设备有效', P.mac));
	if (matched != null && length(checked) < length(cfg.rules ?? []))
		push(notes, sprintf('命中后不再评估后面的 %d 条规则(第一条命中者胜)', length(cfg.rules) - length(checked)));
	push(notes, '试算针对新建连接:已建立的连接保持原出口,不会因规则变化而迁移');
	if (rd(opts, '/sys/kernel/debug/hnat/hook_toggle') != null)
		push(notes, '本机加载了 MTK 硬件加速(HNAT):已被它绑定的流绕过防火墙,不受规则与故障切换影响');

	// ---- 出口与路由
	let exit = null;
	let route = null;
	let mark = '';
	let ex = null;
	if (matched != null && mrule != null && mrule.exit != null)
		ex = find_exit(plan, mrule.exit);
	if (ex != null) {
		mark = ex.mark;
		exit = { id: ex.id, target: ex.target, on_fail: ex.on_fail, table: ex.table, mark: ex.mark,
			usable: (first_route(plan, P.fam, ex.table) != null) };
	}

	if (ex != null && P.fam == 6 && v6on && ex.v6 == false) {
		// 出口不发放 LAN 前缀:v6 被强制 unreachable,不会回落系统默认(源前缀不属于该出口会被上游丢弃)
		route = { family: 6, dev: '', via: '', text: ex.v6_reason ?? '该出口不发放 LAN 前缀,IPv6 流量被阻断', table: '' + ex.table, failed: true };
	}
	else if (skip != '' && ing.local) {
		route = null;
	}
	else {
		let use_mark = '';
		let installed = false;
		if (ex != null) {
			installed = live.table && rule_installed(opts, P.fam, mark);
			if (installed)
				use_mark = mark;
			else
				push(notes, '该出口的 ip rule 尚未装入内核:路由是按配置推演的,不是内核实际查询的结果');
		}
		if (use_mark != '') {
			let rg = route_get(opts, P, ing.iif, use_mark);
			route = { family: P.fam, dev: rg.dev, via: rg.via, text: rg.text, table: rg.table, failed: !rg.ok };
			if (!rg.ok) {
				route.text = (rg.text != '') ? rg.text : '内核没有返回路由(无路由,或被 unreachable 规则拦下)';
			}
			let fr = first_route(plan, P.fam, ex.table);
			if (rg.ok && fr != null && rg.dev != '' && rg.dev != fr.dev) {
				let ms = main_specific(opts, P);
				if (ms != null && ms.dev == rg.dev)
					push(notes, sprintf('该目的地在主表有更具体的路由(%s),按设计直接走主表、不走出口表', ms.text));
				else
					push(notes, sprintf('内核实际查询经 %s,而按配置应经 %s:出口表路由可能尚未同步', rg.dev, fr.dev));
			}
			if (!rg.ok && ex.on_fail == 'block')
				push(notes, nohop_text(ex));
			else if (rg.ok && fr == null && ex.on_fail == 'default')
				push(notes, nohop_text(ex));
		}
		else if (ex != null) {
			// 没有装入 ip rule:按计划推演第一条可用路由
			let fr = first_route(plan, P.fam, ex.table);
			let ms = main_specific(opts, P);
			if (ms != null) {
				route = { family: P.fam, dev: ms.dev, via: ms.via, text: sprintf('按配置推演:该目的地在主表有更具体的路由,直接走主表、不走出口表(%s)', ms.text), table: 'main', failed: false };
			}
			else if (fr != null) {
				route = { family: P.fam, dev: fr.dev, via: fr.via ?? '', text: sprintf('按配置推演:经 %s%s(出口表 %d)', fr.dev, (fr.via != null && fr.via != '') ? ' via ' + fr.via : '', ex.table), table: '' + ex.table, failed: false };
			}
			else if (ex.on_fail == 'block') {
				route = { family: P.fam, dev: '', via: '', text: nohop_text(ex), table: '' + ex.table, failed: true };
			}
			else {
				push(notes, nohop_text(ex));
				let rg = route_get(opts, P, ing.iif, '');
				route = { family: P.fam, dev: rg.dev, via: rg.via, text: rg.text, table: rg.table, failed: !rg.ok };
			}
		}
		else {
			// 没有命中:系统默认路径(不带标记)
			let rg = route_get(opts, P, ing.iif, '');
			route = { family: P.fam, dev: rg.dev, via: rg.via, text: rg.text, table: rg.table, failed: !rg.ok };
			if (!rg.ok && P.fam == 6 && P.src == null)
				push(notes, '未给出源地址:IPv6 默认路由多为源地址限定的,不带 from 的查询失败不代表没有 IPv6');
		}
	}
	if (route != null && route.failed && route.text == '')
		route.text = '内核没有返回路由(无路由,或被 unreachable 规则拦下)';

	return {
		ok: true,
		matched: matched,
		exit: exit,
		route: route,
		checked: checked,
		notes: notes,
		uncertain: uncertain,
		input: { family: P.fam, src: (P.src == null) ? '' : words_fmt(P.src.lo, P.fam), dst: T.dst_txt, mac: P.mac, mac_auto: P.mac_auto,
			proto: P.proto, dport: P.dport, sport: P.sport, domain: P.domain }
	};
}

export { run_trace };
