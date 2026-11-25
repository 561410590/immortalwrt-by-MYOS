'use strict';
// mwrt-pbr 配置读取:/etc/config/mwrt_pbr ⇄ cfg 对象(SPEC §13.1)。只读;写配置只在 Lua 控制器与 mwrt-wans 里做。
// 这里只做"形状校正"(类型、默认值、历史值),取值是否合法由 validate.uc 判断。

import { uci_cursor } from 'mwrt.lib';
import { to_bool, list_of } from 'mwrt.validate';

const CONF = 'mwrt_pbr';

// 缺省值见 SPEC §3.1;每次返回新对象,调用方可随意改
function defaults() {
	return {
		global: {
			enabled: false, ipv6: true, dns_sets: true, dns_ttl: '1d', learn_ttl: '30m',
			src_zones: [ 'lan' ], flush_accel: false
		},
		wan_over: {},
		groups: [],
		rules: []
	};
}

function str_of(v, dflt) {
	if (v == null)
		return dflt;
	let s = trim('' + v);
	return (s == '') ? dflt : s;
}

// src_zones 既可能是 list 也可能是空格分隔的 option;去重保序,空则回到 lan
function zones_of(v) {
	let out = [];
	let seen = {};
	for (let item in list_of(v)) {
		for (let z in split(item, /[ \t,]+/)) {
			if (z == '' || seen[z] == true || !match(z, /^[A-Za-z0-9_.-]+$/))
				continue;
			seen[z] = true;
			push(out, z);
		}
	}
	if (length(out) == 0)
		push(out, 'lan');
	return out;
}

function norm_global(raw) {
	let d = defaults().global;
	if (type(raw) != 'object')
		return d;
	return {
		enabled: to_bool(raw.enabled, d.enabled),
		ipv6: to_bool(raw.ipv6, d.ipv6),
		dns_sets: to_bool(raw.dns_sets, d.dns_sets),
		dns_ttl: str_of(raw.dns_ttl, d.dns_ttl),
		learn_ttl: str_of(raw.learn_ttl, d.learn_ttl),
		src_zones: zones_of(raw.src_zones),
		flush_accel: to_bool(raw.flush_accel, d.flush_accel)
	};
}

// 缺编号或编号含非法字符时按位置生成,保证编译器拿到的 id 一定能当 nft 名字用
function norm_id(v, prefix, idx) {
	let s = str_of(v, '');
	if (s != '' && match(s, /^[A-Za-z0-9_]+$/))
		return s;
	return sprintf('%s%08x', prefix, idx + 1);
}

function norm_rule(raw, idx) {
	let on_fail = lc(str_of(raw.on_fail, 'default'));
	if (on_fail == 'backup')
		on_fail = 'default';
	return {
		id: norm_id(raw.id, 'r_', idx),
		name: str_of(raw.name, ''),
		enabled: to_bool(raw.enabled, true),
		note: str_of(raw.note, ''),
		match: lc(str_of(raw.match, 'all')),
		target: str_of(raw.target, ''),
		on_fail: on_fail,
		proto: lc(str_of(raw.proto, 'any')),
		dport: list_of(raw.dport),
		sport: list_of(raw.sport),
		app: list_of(raw.app),
		src_dev: list_of(raw.src_dev),
		src_ip: list_of(raw.src_ip),
		dst_ip: list_of(raw.dst_ip),
		domain: list_of(raw.domain)
	};
}

// UCI 里成员是 list member,输入对象里写 members 或 member 都认
function norm_group(raw, idx) {
	let id = norm_id(raw.id, 'g_', idx);
	return {
		id: id,
		name: str_of(raw.name, id),
		enabled: to_bool(raw.enabled, true),
		members: list_of(raw.members ?? raw.member),
		note: str_of(raw.note, '')
	};
}

function norm_over(raw) {
	if (type(raw) != 'object')
		return { alias: '', enabled: true };
	return { alias: str_of(raw.alias, ''), enabled: to_bool(raw.enabled, true) };
}

// 读 UCI。配置文件不存在不算错(给默认值);拿不到 cursor 才抛错,让调用方保持上一次状态而不是误当成"没有规则"
function load(opts) {
	let cur = (type(opts) == 'object' && opts.cursor != null) ? opts.cursor : uci_cursor();
	if (cur == null)
		die('无法读取配置:uci 游标创建失败');
	let cfg = defaults();

	// 以名为 global 的段为准;没有这个名字的段时取第一个 global 段
	let named = null;
	let first = null;
	cur.foreach(CONF, 'global', function(s) {
		if (s['.name'] == 'global')
			named = s;
		else if (first == null)
			first = s;
		return true;
	});
	cfg.global = norm_global(named ?? first);

	cur.foreach(CONF, 'wan', function(s) {
		let wid = str_of(s.wan_id, '');
		if (wid != '')
			cfg.wan_over[wid] = norm_over(s);
		return true;
	});

	let gi = 0;
	cur.foreach(CONF, 'group', function(s) {
		let raw = {};
		for (let k, v in s)
			raw[k] = v;
		raw.id = s['.name'];
		push(cfg.groups, norm_group(raw, gi));
		gi++;
		return true;
	});

	let ri = 0;
	cur.foreach(CONF, 'rule', function(s) {
		let raw = {};
		for (let k, v in s)
			raw[k] = v;
		raw.id = s['.name'];
		push(cfg.rules, norm_rule(raw, ri));
		ri++;
		return true;
	});
	return cfg;
}

// preview --input 的对象:形状同 cfg;缺的部分按默认值补
function from_input(obj) {
	let cfg = defaults();
	if (type(obj) != 'object')
		return cfg;
	cfg.global = norm_global(obj.global);
	if (type(obj.wan_over) == 'object') {
		for (let wid, v in obj.wan_over) {
			if (str_of(wid, '') != '')
				cfg.wan_over[wid] = norm_over(v);
		}
	}
	let i = 0;
	for (let g in (type(obj.groups) == 'array' ? obj.groups : [])) {
		if (type(g) == 'object') {
			push(cfg.groups, norm_group(g, i));
			i++;
		}
	}
	i = 0;
	for (let r in (type(obj.rules) == 'array' ? obj.rules : [])) {
		if (type(r) == 'object') {
			push(cfg.rules, norm_rule(r, i));
			i++;
		}
	}
	return cfg;
}

export { load, from_input, defaults };
