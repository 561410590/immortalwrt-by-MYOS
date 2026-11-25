'use strict';
// 诊断清单(只读):逐项检查引擎、系统能力、出口、与其它软件的并存、DNS 集合、流量卸载与规则状态。
// 只读系统信息(nft list / ip rule / sysfs / proc / UCI),不写任何状态,不发包;
// 读不到的信息一律降级为 info 并说明原因,不会让整份诊断失败。

import * as fs from 'fs';
import { run, sh_quote, uci_cursor, MARK_MASK } from 'mwrt.lib';
import { to_bool } from 'mwrt.validate';
import { compile } from 'mwrt.compile';

const BAND_LO = 1101;
const BAND_HI = 1953;
const WANMON_JSON = '/tmp/mwrt-wan.json';
const HNAT_DIR = '/sys/kernel/debug/hnat';
const MAP_FILE = 'mwrt-pbr.conf';

// 并存软件的判定依据:ip rule 里的标记签名、UCI 启用开关、进程名(三者组合,见 coexist_proxy)
const PRODUCTS = [
	{ key: 'passwall', name: 'PassWall', mark: '0x50535731', uci: [ 'passwall', 'global', 'enabled', '1' ],
	  procs: [ 'xray', 'v2ray', 'sing-box', 'hysteria', 'naive', 'trojan-plus', 'tuic-client', 'ipt2socks', 'ss-redir', 'ssr-redir' ] },
	{ key: 'passwall2', name: 'PassWall2', mark: '0x50535732', uci: [ 'passwall2', 'global', 'enabled', '1' ],
	  procs: [ 'xray', 'v2ray', 'sing-box', 'hysteria', 'naive', 'trojan-plus', 'tuic-client', 'ipt2socks', 'ss-redir', 'ssr-redir' ] },
	{ key: 'ssrplus', name: 'SSR-Plus', mark: '0x53535250', uci: [ 'shadowsocksr', 'global', 'global_server', '*' ],
	  procs: [ 'ssr-redir', 'ss-redir', 'ssr-local', 'xray', 'v2ray', 'sing-box', 'trojan', 'naive', 'hysteria' ] },
	{ key: 'openclash', name: 'OpenClash', mark: '0x162', uci: [ 'openclash', 'openclash', 'enable', '1' ],
	  procs: [ 'clash', 'mihomo' ] }
];

// ---------------------------------------------------------------- 只读系统访问(opts.io 仅供离线检查注入)

function sh(S, cmd) {
	if (S.io != null && S.io.run != null)
		return S.io.run(cmd);
	return run(cmd);
}

function rd(S, path) {
	if (S.io != null && S.io.read != null)
		return S.io.read(path);
	return fs.readfile(path);
}

function ex(S, path) {
	if (S.io != null && S.io.exists != null)
		return S.io.exists(path);
	return fs.stat(path) != null;
}

function gl(S, pat) {
	if (S.io != null && S.io.glob != null)
		return S.io.glob(pat) ?? [];
	return fs.glob(pat) ?? [];
}

function sv(v) {
	if (type(v) == 'array')
		v = v[0];
	return (v == null) ? '' : trim('' + v);
}

// 取某类型第一个 UCI 段的选项;配置不存在返回 null
function uget(S, pkg, typ, opt) {
	if (S.io != null && S.io.uci != null)
		return S.io.uci(pkg, typ, opt);
	if (S.cur == null)
		S.cur = uci_cursor();
	if (S.cur == null)
		return null;
	let v = null;
	try {
		S.cur.foreach(pkg, typ, function(s) {
			v = s[opt];
			return false;
		});
	}
	catch (e) {
		v = null;
	}
	return v;
}

function rj(S, path) {
	let t = rd(S, path);
	if (t == null || trim(t) == '')
		return null;
	let o = null;
	try {
		o = json(t);
	}
	catch (e) {
		o = null;
	}
	return o;
}

function first_line(s) {
	for (let ln in split('' + (s ?? ''), '\n')) {
		let t = trim(ln);
		if (t != '')
			return t;
	}
	return '';
}

// 从 dnsmasq --version 的首行取出版本号,取不到就用首行原文
function dnsmasq_ver(s) {
	let ln = first_line(s);
	let m = match(ln, /[Vv]ersion ([0-9][0-9.a-z]*)/);
	return (m != null) ? 'dnsmasq ' + m[1] : ln;
}

// ---------------------------------------------------------------- 文本小工具

function add(S, id, group, label, level, detail, hint) {
	push(S.checks, { id: id, group: group, label: label, level: level, detail: detail, hint: hint ?? '' });
}

function ago(secs) {
	if (secs < 0)
		secs = 0;
	if (secs < 60)
		return sprintf('%d 秒', secs);
	if (secs < 3600)
		return sprintf('%d 分钟', secs / 60);
	if (secs < 86400)
		return sprintf('%d 小时', secs / 3600);
	return sprintf('%d 天', secs / 86400);
}

// 最多列 max 项,其余写"等 N 项"
function names(list, max) {
	let out = [];
	for (let i = 0; i < length(list) && i < max; i++)
		push(out, list[i]);
	let t = join('、', out);
	if (length(list) > max)
		t += sprintf(' 等共 %d 项', length(list));
	return t;
}

function ttl_secs(s) {
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

function wan_name(S, wid) {
	let over = (S.cfg != null && type(S.cfg.wan_over) == 'object') ? S.cfg.wan_over[wid] : null;
	if (over != null && over.alias != null && trim('' + over.alias) != '')
		return trim('' + over.alias);
	let w = S.wmap[wid];
	if (w != null) {
		if (w.alias != null && trim('' + w.alias) != '')
			return trim('' + w.alias);
		if (w.name != null && trim('' + w.name) != '')
			return trim('' + w.name);
	}
	return wid;
}

function exit_label(S, e) {
	let m = match('' + (e.target ?? ''), /^(wan|group):(.+)$/);
	if (m == null)
		return '' + (e.target ?? '');
	if (m[1] == 'wan')
		return wan_name(S, m[2]);
	if (S.cfg != null && type(S.cfg.groups) == 'array') {
		for (let gp in S.cfg.groups) {
			if (gp.id == m[2])
				return '出口组 ' + ((gp.name != null && trim('' + gp.name) != '') ? trim('' + gp.name) : m[2]);
		}
	}
	return '出口组 ' + m[2];
}

// 规则 id(小写)-> 显示名
function rule_title(S, rid) {
	return S.rname[rid] ?? rid;
}

// ---------------------------------------------------------------- 内核与系统信息(按需读取,同一次诊断内缓存)

function nft_struct(S) {
	if (S.c.nft != null)
		return S.c.nft;
	let r = sh(S, 'nft -t list table inet mwrt_pr');
	let o = { exists: (r.rc == 0), missing_bin: false, err: '', chains: {}, sets: {}, counters: {} };
	if (r.rc != 0) {
		o.err = first_line(r.out);
		if (r.rc == 127 || index(r.out, 'not found') >= 0)
			o.missing_bin = true;
	}
	else {
		for (let ln in split(r.out, '\n')) {
			let m = match(ln, /^[ \t]*(chain|set|counter) ([A-Za-z0-9_]+) \{/);
			if (m == null)
				continue;
			if (m[1] == 'chain')
				o.chains[m[2]] = true;
			else if (m[1] == 'set')
				o.sets[m[2]] = true;
			else
				o.counters[m[2]] = true;
		}
	}
	S.c.nft = o;
	return o;
}

// 各集合的元素个数(数 expires 出现次数,只对带超时的集合有意义)与 lan_ifs 的成员;
// 用 awk 流式统计,避免把几万元素的集合整个读进内存
function nft_sets(S) {
	if (S.c.sets != null)
		return S.c.sets;
	let o = { ok: false, counts: {}, names: [], lan_ifs: null };
	if (nft_struct(S).exists) {
		let tab = "\t";
		let prog = '/^' + tab + 'set /{n=$2;c=0;next} /^' + tab + '}/{if(n!=""){print "S",n,c;n=""}next} n!=""{c+=gsub(/expires/,"&");if(n=="lan_ifs")print "L",$0}';
		let r = sh(S, 'nft list sets table inet mwrt_pr | awk ' + sh_quote(prog));
		if (r.rc == 0) {
			o.ok = true;
			for (let ln in split(r.out, '\n')) {
				let m = match(ln, /^S ([A-Za-z0-9_]+) ([0-9]+)$/);
				if (m != null) {
					o.counts[m[1]] = int(m[2]);
					push(o.names, m[1]);
					continue;
				}
				if (substr(ln, 0, 2) == 'L ') {
					let qs = match(ln, /"[^"]+"/g);
					if (o.lan_ifs == null)
						o.lan_ifs = [];
					for (let q in (qs ?? []))
						push(o.lan_ifs, substr(q[0], 1, length(q[0]) - 2));
				}
			}
			if (o.lan_ifs == null && o.counts['lan_ifs'] != null)
				o.lan_ifs = [];
		}
	}
	S.c.sets = o;
	return o;
}

// 某一族的 ip rule:[{pref, spec}],spec 去掉 "from all " 前缀并压缩空白
function ip_rules(S, fam) {
	let key = 'rules' + fam;
	if (S.c[key] != null)
		return S.c[key];
	let r = sh(S, sprintf('ip -%d rule show', fam));
	let o = { ok: (r.rc == 0), list: [] };
	if (o.ok) {
		for (let ln in split(r.out, '\n')) {
			let m = match(trim(ln), /^([0-9]+):[ \t]+(.*)$/);
			if (m == null)
				continue;
			let spec = replace(trim(m[2]), /[ \t]+/g, ' ');
			if (substr(spec, 0, 9) == 'from all ')
				spec = substr(spec, 9);
			push(o.list, { pref: int(m[1]), spec: spec });
		}
	}
	S.c[key] = o;
	return o;
}

function is_own_spec(spec) {
	return index(spec, 'fwmark') >= 0 && index(spec, sprintf('/0x%x', MARK_MASK)) >= 0;
}

// 进程名集合(读 /proc/<pid>/comm,不 fork)
function procs(S) {
	if (S.c.procs != null)
		return S.c.procs;
	let set = {};
	if (S.io != null && S.io.procs != null) {
		for (let n in S.io.procs)
			set[n] = true;
	}
	else {
		let ds = fs.lsdir('/proc');
		for (let d in (ds ?? [])) {
			if (!match(d, /^[0-9]+$/))
				continue;
			let c = fs.readfile('/proc/' + d + '/comm');
			if (c != null)
				set[trim(c)] = true;
		}
	}
	S.c.procs = set;
	return set;
}

function any_proc(S, list) {
	let ps = procs(S);
	let hit = [];
	for (let n in list) {
		if (ps[n] == true)
			push(hit, n);
	}
	return hit;
}

// 有标记签名的 ip rule(标记后必须是空格、斜杠或行尾,避免 0x162 误中 0x1620…)
function mark_in(spec, mk) {
	let key = 'fwmark ' + mk;
	let i = index(spec, key);
	if (i < 0)
		return false;
	let nx = substr(spec, i + length(key), 1);
	return (nx == '' || nx == ' ' || nx == '/');
}

function has_mark(S, mk) {
	for (let fam in [ 4, 6 ]) {
		for (let r in ip_rules(S, fam).list) {
			if (mark_in(r.spec, mk))
				return true;
		}
	}
	return false;
}

// dnsmasq 运行配置:conf-dir、min-cache-ttl、本插件的映射文件
function dnsmasq_info(S) {
	if (S.c.dm != null)
		return S.c.dm;
	let o = { confs: [], dirs: [], min_ttl: null, map_path: '', map_lines: null };
	for (let f in gl(S, '/var/etc/dnsmasq.conf.*')) {
		let t = rd(S, f);
		if (t == null)
			continue;
		push(o.confs, f);
		for (let ln in split(t, '\n')) {
			let s = trim(ln);
			let m = match(s, /^conf-dir=([^,]+)/);
			if (m != null && !(trim(m[1]) in o.dirs))
				push(o.dirs, trim(m[1]));
			let n = match(s, /^min-cache-ttl=([0-9]+)/);
			if (n != null) {
				let v = int(n[1]);
				if (o.min_ttl == null || v > o.min_ttl)
					o.min_ttl = v;
			}
		}
	}
	for (let d in o.dirs) {
		let t = rd(S, d + '/' + MAP_FILE);
		if (t == null)
			continue;
		o.map_path = d + '/' + MAP_FILE;
		o.map_lines = [];
		for (let ln in split(t, '\n')) {
			let s = trim(ln);
			if (substr(s, 0, 8) == 'nftset=/')
				push(o.map_lines, s);
		}
		break;
	}
	S.c.dm = o;
	return o;
}

// ---------------------------------------------------------------- 引擎

function check_iprules(S) {
	let r4 = ip_rules(S, 4);
	let r6 = ip_rules(S, 6);
	if (!r4.ok && !r6.ok) {
		add(S, 'engine.iprules', 'engine', '策略路由规则(ip rule)', 'info', '读不到 ip rule,无法核对', '确认已安装 ip-full');
		return;
	}
	let live = {};
	let own_live = 0;
	for (let pair in [ [ 4, r4 ], [ 6, r6 ] ]) {
		for (let r in pair[1].list) {
			if (r.pref < BAND_LO || r.pref > BAND_HI || !is_own_spec(r.spec))
				continue;
			live[sprintf('%d|%d|%s', pair[0], r.pref, r.spec)] = true;
			own_live++;
		}
	}
	if (S.plan == null) {
		add(S, 'engine.iprules', 'engine', '策略路由规则(ip rule)', 'info', sprintf('当前没有可用的编译计划,只能确认现有 %d 条本插件的 ip rule', own_live), '');
		return;
	}
	let want = {};
	let nwant = 0;
	for (let r in (S.plan.ip_rules ?? [])) {
		want[sprintf('%d|%d|%s', r.family, r.pref, replace(trim('' + r.spec), /[ \t]+/g, ' '))] = true;
		nwant++;
	}
	if (!S.enabled) {
		if (own_live > 0)
			add(S, 'engine.iprules', 'engine', '策略路由规则(ip rule)', 'warn', sprintf('总开关已关闭,但仍有 %d 条本插件的 ip rule', own_live), '执行 /usr/libexec/mwrt-pbr flush 清理');
		return;
	}
	let miss = [];
	let stale = [];
	for (let k in keys(want)) {
		if (!live[k])
			push(miss, k);
	}
	for (let k in keys(live)) {
		if (!want[k])
			push(stale, k);
	}
	miss = sort(miss);
	stale = sort(stale);
	if (length(miss) == 0 && length(stale) == 0) {
		add(S, 'engine.iprules', 'engine', '策略路由规则(ip rule)', 'ok', sprintf('期望的 %d 条 ip rule 全部在位', nwant), '');
		return;
	}
	if (length(miss) > 0) {
		let fo = (S.state.failed_open == true);
		// 首次应用就失败或已 fail-open 时 ip rule 本来就不在内核里,不是被别的程序清掉的
		let never = (S.state.ok == false && !(type(S.state.applied_ts) == 'int' && S.state.applied_ts > 0));
		let lv = (S.pending || length(S.state) == 0 || fo || never) ? 'info' : 'fail';
		let txt = [];
		for (let k in miss) {
			let p = split(k, '|');
			push(txt, sprintf('IPv%s 优先级 %s', p[0], p[1]));
		}
		if (fo || never)
			add(S, 'engine.iprules', 'engine', '策略路由规则(ip rule)', lv,
				sprintf('ip rule 尚未装入(期望 %d 条,缺少 %d 条):%s', nwant, length(miss), names(txt, 4)),
				fo ? '已按 fail-open 清空,原因见运行状态' : '首次应用未成功,原因见运行状态');
		else
			add(S, 'engine.iprules', 'engine', '策略路由规则(ip rule)', lv,
				sprintf('缺少 %d 条 ip rule(期望 %d 条):%s', length(miss), nwant, names(txt, 4)),
				'点击「应用」重建;若反复丢失,检查是否有其它程序(如 mwan3、pbr)清空或重写了 ip rule');
	}
	else
		add(S, 'engine.iprules', 'engine', '策略路由规则(ip rule)', 'warn', sprintf('有 %d 条残留的本插件 ip rule 不在当前期望里(可能是上次应用后未清理)', length(stale)), '点击「应用」清理');
}

function check_health(S) {
	let j = rj(S, WANMON_JSON);
	let fresh = false;
	let detail = '';
	if (type(j) == 'object') {
		let iv = (type(j.interval) == 'int' && j.interval > 0) ? j.interval : 10;
		let age = (type(j.ts) == 'int') ? S.now - j.ts : null;
		let alive = true;
		if (type(j.pid) == 'int' && j.pid > 0 && !ex(S, '/proc/' + j.pid))
			alive = false;
		if (age != null && age <= iv * 3 && alive) {
			fresh = true;
			detail = sprintf('健康探测运行中(每 %d 秒一次,%s前更新)', iv, ago(age));
		}
		else if (!alive)
			detail = '健康探测的数据文件还在,但守护进程已不在运行';
		else
			detail = '健康探测数据已过期,守护进程可能已停止';
	}
	else
		detail = '没有健康探测数据';
	let has_group = false;
	for (let e in ((S.plan != null) ? (S.plan.exits ?? []) : [])) {
		if (index('' + e.target, 'group:') == 0)
			has_group = true;
	}
	if (fresh)
		add(S, 'engine.health', 'engine', '出口健康探测', 'ok', detail, '');
	else if (!S.enabled)
		return;
	else if (has_group)
		add(S, 'engine.health', 'engine', '出口健康探测', 'warn', detail + ':出口组的故障切换只能依据链路和路由状态,发现不了「有链路但不通」的故障',
			'确认「策略路由」服务已启用(它会拉起探测守护进程 mwrt-wanmon),必要时重启该服务');
	else
		add(S, 'engine.health', 'engine', '出口健康探测', 'info', detail + ':出口只按链路和路由状态判断可用性', '');
}

function check_engine(S) {
	let st = S.state;
	let enabled = S.enabled;
	let applied_ts = st.applied_ts ?? null;
	let applied = (type(applied_ts) == 'int' && applied_ts > 0) || st.ok == true;
	let ns = nft_struct(S);

	// 运行状态
	if (!enabled)
		add(S, 'engine.state', 'engine', '运行状态', 'info', '策略路由总开关处于关闭状态,规则不会生效', '需要使用时,在「总览」打开总开关并点击应用');
	else if (st.failed_open == true)
		add(S, 'engine.state', 'engine', '运行状态', 'fail',
			'上次应用失败,已退回到「全部放行」(fail-open):策略路由当前不生效' + ((sv(st.error) != '') ? '。原因:' + sv(st.error) : ''),
			'用 logread -e mwrt-pbr 查看日志,修正配置后重新应用');
	else if (!applied && st.ok != false)
		add(S, 'engine.state', 'engine', '运行状态', 'warn', '总开关已打开,但还没有成功应用过', '点击「应用」,或在命令行执行 /usr/libexec/mwrt-pbr apply');
	else if (st.ok == false)
		add(S, 'engine.state', 'engine', '运行状态', 'fail',
			(!applied ? '首次应用就失败了,内核里没有生效的策略路由' : ns.exists ? '上次应用出错,内核里保留的是更早的有效状态' : '上次应用出错,内核里也没有生效的策略路由') + ((sv(st.error) != '') ? '。原因:' + sv(st.error) : ''),
			'用 logread -e mwrt-pbr 查看日志,修正配置后重新应用');
	else
		add(S, 'engine.state', 'engine', '运行状态', 'ok',
			(type(applied_ts) == 'int' && applied_ts > 0) ? sprintf('上次成功应用在 %s前', ago(S.now - applied_ts)) : '上次应用成功', '');

	// 配置是否有尚未应用的变更:只在两边都是 12 位十六进制指纹时比较
	let ph = (S.plan != null) ? S.plan.hash : null;
	let sh_ = st.nft_hash;
	S.pending = false;
	if (enabled && applied && type(ph) == 'string' && type(sh_) == 'string' && match(ph, /^[0-9a-f]{12}$/) && match(sh_, /^[0-9a-f]{12}$/)) {
		if (ph != sh_) {
			S.pending = true;
			add(S, 'engine.pending', 'engine', '配置是否已应用', 'info',
				'当前配置的编译结果与上次应用时不同,可能有尚未应用的变更(也可能是出口状态变化引起的路由调整)', '点击「应用」使其生效');
		}
		else
			add(S, 'engine.pending', 'engine', '配置是否已应用', 'ok', '当前配置与已应用的一致', '');
	}

	// 内核表
	if (ns.missing_bin)
		add(S, 'engine.table', 'engine', '内核规则表', 'fail', '找不到 nft 命令,策略路由无法工作', '确认固件包含 nftables(nft 命令)');
	else if (ns.exists) {
		let nc = 0;
		let nsets = 0;
		for (let k in keys(ns.chains))
			nc++;
		for (let k in keys(ns.sets))
			nsets++;
		if (enabled)
			add(S, 'engine.table', 'engine', '内核规则表', 'ok', sprintf('内核里存在 inet mwrt_pr 表(%d 条链、%d 个集合)', nc, nsets), '');
		else
			add(S, 'engine.table', 'engine', '内核规则表', 'warn', '总开关已关闭,但内核里仍有 inet mwrt_pr 表', '执行 /usr/libexec/mwrt-pbr flush 清理,或打开总开关后重新应用');
	}
	else if (enabled && st.failed_open == true)
		add(S, 'engine.table', 'engine', '内核规则表', 'info', '内核里没有 inet mwrt_pr 表:已按 fail-open 清空', '原因见运行状态');
	else if (enabled && applied)
		add(S, 'engine.table', 'engine', '内核规则表', 'fail',
			'内核里没有 inet mwrt_pr 表:策略路由实际没有生效(常见于防火墙被重载或停止之后)',
			'点击「应用」重建;定时对账也会尝试补建,仍不恢复请检查防火墙是否正常运行');
	else if (enabled)
		add(S, 'engine.table', 'engine', '内核规则表', 'info', '尚未创建(还没有应用过)', '');
	else
		add(S, 'engine.table', 'engine', '内核规则表', 'ok', '未启用,内核里没有该表(正常)', '');

	// 结构完整性:期望的链/计数器(与集合,若计划带 objects)是否都在
	if (ns.exists && S.plan != null) {
		let want_c = [ 'prerouting', 'classify', 'learn' ];
		let want_s = [ 'lan_ifs' ];
		let want_n = [];
		let obj = S.plan.objects;
		if (type(obj) == 'object' && type(obj.chains) == 'array') {
			want_c = obj.chains;
			want_s = obj.sets ?? [];
			want_n = obj.counters ?? [];
		}
		else {
			for (let e in (S.plan.exits ?? []))
				push(want_c, 'x_' + e.id);
			for (let r in (S.plan.rules ?? [])) {
				if (r.status == 'active' && r.id != null)
					push(want_n, 'c_' + lc('' + r.id));
			}
		}
		let miss = [];
		for (let c in want_c) {
			if (!ns.chains[c])
				push(miss, '链 ' + c);
		}
		for (let s in want_s) {
			if (!ns.sets[s])
				push(miss, '集合 ' + s);
		}
		for (let n in want_n) {
			if (!ns.counters[n])
				push(miss, '计数器 ' + n);
		}
		if (length(miss) == 0)
			add(S, 'engine.structure', 'engine', '规则表结构', 'ok', '链、集合、计数器与当前配置的计划一致', '');
		else if (S.pending || !enabled)
			add(S, 'engine.structure', 'engine', '规则表结构', 'info', sprintf('有 %d 个对象尚未创建(配置有未应用的变更):%s', length(miss), names(miss, 4)), '点击「应用」');
		else
			add(S, 'engine.structure', 'engine', '规则表结构', 'fail', sprintf('内核表缺少 %d 个对象:%s', length(miss), names(miss, 4)), '点击「应用」重建;若反复出现,检查是否有其它程序清理了 nft 规则');
	}

	// 入口设备
	if (ns.exists) {
		let ss = nft_sets(S);
		if (ss.lan_ifs == null)
			add(S, 'engine.lan_ifs', 'engine', '入口设备', 'info', '读不到 lan_ifs 集合,无法确认参与策略路由的入口设备', '');
		else if (length(ss.lan_ifs) == 0)
			add(S, 'engine.lan_ifs', 'engine', '入口设备', 'fail', '参与策略路由的入口设备集合为空:没有任何流量会被评估', '检查全局设置里「参与流量的区域」(src_zones)是否指向有设备的防火墙区域,然后重新应用');
		else
			add(S, 'engine.lan_ifs', 'engine', '入口设备', 'ok', '来自这些设备的流量参与策略路由:' + join('、', ss.lan_ifs), '');
	}

	// ip rule:期望集与现状
	check_iprules(S);

	// 健康探测
	check_health(S);

	if (!S.v6on)
		add(S, 'engine.v6', 'engine', 'IPv6 处理', 'info', 'IPv6 处理已关闭:IPv6 流量完全不受策略路由影响', '需要时在全局设置里打开「处理 IPv6」');
}

// ---------------------------------------------------------------- 能力

function check_capability(S) {
	// nft
	let ns = nft_struct(S);
	if (ns.missing_bin)
		add(S, 'cap.nft', 'capability', 'nft 能力', 'fail', '找不到 nft 命令', '确认固件包含 nftables');
	else if (ns.exists)
		add(S, 'cap.nft', 'capability', 'nft 能力', 'ok', '已有 mwrt_pr 表,说明内核接受本插件用到的 nft 语法', '');
	else {
		// 表还不存在:只做语法与内核支持检查(-c),不改动任何规则
		let probe = 'add table inet mwrt_probe\n'
			+ 'add chain inet mwrt_probe c { type filter hook prerouting priority -152; policy accept; }\n'
			+ 'add rule inet mwrt_probe c ct state new ct mark set ct mark & 0xe000ffff | 0x00010000\n'
			+ 'add rule inet mwrt_probe c ct mark & 0x1fff0000 vmap { 0x00010000 : accept }\n'
			+ 'add set inet mwrt_probe s { type ipv4_addr . inet_proto . inet_service; flags dynamic,timeout; timeout 30m; size 8192; }\n';
		let r = sh(S, 'printf %b ' + sh_quote(probe) + ' | nft -c -f -');
		if (r.rc == 0)
			add(S, 'cap.nft', 'capability', 'nft 能力', 'ok', '内核接受本插件用到的 nft 语法(仅做语法检查,没有改动任何规则)', '');
		else
			add(S, 'cap.nft', 'capability', 'nft 能力', 'fail', '内核不接受本插件需要的 nft 语法' + ((first_line(r.out) != '') ? ':' + first_line(r.out) : '(nft 没有给出错误信息)'), '检查 nftables 版本以及 nf_tables、nft_ct、nft_set_hash 等内核模块是否齐全');
	}

	// ip rule
	let r = sh(S, 'ip rule help');
	if (index(r.out, 'suppress_prefixlength') >= 0)
		add(S, 'cap.iprule', 'capability', 'ip rule 能力', 'ok', 'ip 命令支持 fwmark 与 suppress_prefixlength', '');
	else if (r.rc == 127 || index(r.out, 'not found') >= 0)
		add(S, 'cap.iprule', 'capability', 'ip rule 能力', 'fail', '找不到 ip 命令', '安装 ip-full');
	else
		add(S, 'cap.iprule', 'capability', 'ip rule 能力', 'fail', '当前 ip 命令不支持 suppress_prefixlength(可能是 BusyBox 精简版)', '安装 ip-full');

	// dnsmasq 的 nftset
	let want_dom = (S.plan != null && type(S.plan.dns) == 'object' && length(S.plan.dns.lines ?? []) > 0);
	let dr = sh(S, 'dnsmasq --version');
	let has_nftset = index(' ' + replace(replace(dr.out, "\n", ' '), "\t", ' ') + ' ', ' nftset ') >= 0;
	if (!to_bool(S.g.dns_sets, true))
		add(S, 'cap.dnsmasq', 'capability', '域名解析集合(dnsmasq nftset)', want_dom ? 'warn' : 'info', '全局设置里已关闭「域名解析集合」,域名维度不可用', '需要按域名选路时在全局设置里打开');
	else if (dr.rc == 127 || index(dr.out, 'not found') >= 0)
		add(S, 'cap.dnsmasq', 'capability', '域名解析集合(dnsmasq nftset)', want_dom ? 'fail' : 'warn', '找不到 dnsmasq 命令,域名维度不可用', '确认固件的 dnsmasq 为支持 nftset 的版本');
	else if (has_nftset)
		add(S, 'cap.dnsmasq', 'capability', '域名解析集合(dnsmasq nftset)', 'ok', dnsmasq_ver(dr.out) + ',支持 nftset', '');
	else
		add(S, 'cap.dnsmasq', 'capability', '域名解析集合(dnsmasq nftset)', want_dom ? 'fail' : 'warn', 'dnsmasq 编译选项里没有 nftset,域名维度不可用', '换用带 nftset 的 dnsmasq(如 dnsmasq-full)');

	// 应用识别库与 OAF
	let apps = S.apps;
	let avail = (type(apps) == 'object' && apps.available == true);
	let ver = trim('' + (rd(S, '/proc/sys/oaf/version') ?? ''));
	let oen = trim('' + (rd(S, '/proc/sys/oaf/enable') ?? ''));
	let want_app = false;
	for (let rl in ((S.cfg != null && type(S.cfg.rules) == 'array') ? S.cfg.rules : [])) {
		if (type(rl) == 'object' && to_bool(rl.enabled, true) && type(rl.app) == 'array' && length(rl.app) > 0)
			want_app = true;
	}
	if (!avail)
		add(S, 'cap.behav', 'capability', '应用识别库(行为管控)', want_app ? 'fail' : 'info', '未安装行为管控(mwrt-behav),应用维度不可用' + (want_app ? ',已有规则用到了应用维度' : ''), '安装 mwrt-behav 后才能按应用选路');
	else if (ver == '')
		add(S, 'cap.behav', 'capability', '应用识别库(行为管控)', want_app ? 'warn' : 'info', '应用库可用,但内核里没有应用识别模块(OAF),应用维度不会学到任何流量', '确认 kmod-oaf 已加载');
	else if (oen == '0')
		add(S, 'cap.behav', 'capability', '应用识别库(行为管控)', want_app ? 'warn' : 'info', '应用识别模块(OAF)已加载但处于关闭状态,应用维度不会学到任何流量', '在行为管控里打开应用识别');
	else
		add(S, 'cap.behav', 'capability', '应用识别库(行为管控)', 'ok', '应用库与识别模块可用(OAF ' + ver + ')。应用选路是学习式的,只对识别之后的新连接生效', '');
}

// ---------------------------------------------------------------- 出口

function check_exits(S) {
	if (S.plan == null) {
		add(S, 'exits.none', 'exits', '出口', 'info', '缺少配置或编译计划,无法检查出口', '');
		return;
	}
	let exits = S.plan.exits ?? [];
	if (length(exits) == 0) {
		add(S, 'exits.none', 'exits', '出口', 'info', '当前没有生效的规则,尚未使用任何出口', '');
		return;
	}
	let devs = {};
	let dev_list = [];
	for (let e in exits) {
		let label = exit_label(S, e);
		let usable = [];
		let bad = [];
		let first_idx = -1;
		for (let j = 0; j < length(e.members ?? []); j++) {
			let m = e.members[j];
			if (m.usable) {
				push(usable, m);
				if (first_idx < 0)
					first_idx = j;
			}
			else
				push(bad, sprintf('%s:%s', wan_name(S, m.wan_id), (m.reason != null && m.reason != '') ? m.reason : '不可用'));
			if (m.dev != null && m.dev != '' && !devs[m.dev]) {
				devs[m.dev] = true;
				push(dev_list, m.dev);
			}
		}
		let id = sprintf('exits.%d.state', e.id);
		let lab = '出口「' + label + '」';
		if (length(usable) == 0) {
			if (e.on_fail == 'block')
				add(S, id, 'exits', lab, 'warn', '没有可用成员,且「不可用时」设为阻断:匹配的流量会被丢弃。' + names(bad, 4), '检查出口链路与默认路由;若希望断线时仍能上网,把规则的「不可用时」改为回落系统默认路由');
			else
				add(S, id, 'exits', lab, 'warn', '没有可用成员:出口表里没有路由,匹配的流量会落到系统默认路由。' + names(bad, 4), '检查出口链路与默认路由;这是「不可用时回落系统默认路由」的预期行为');
		}
		else if (first_idx > 0)
			add(S, id, 'exits', lab, 'info', sprintf('主出口不可用,当前走备用「%s」(%s)。%s', wan_name(S, usable[0].wan_id), usable[0].dev, names(bad, 4)), '主出口恢复后会自动切回');
		else if (length(bad) > 0)
			add(S, id, 'exits', lab, 'ok', sprintf('当前走「%s」(%s);备用成员不可用:%s', wan_name(S, usable[0].wan_id), usable[0].dev, names(bad, 3)), '');
		else
			add(S, id, 'exits', lab, 'ok', sprintf('当前走「%s」(%s)', wan_name(S, usable[0].wan_id), usable[0].dev), '');
	}

	// 出口设备是否在 wan 区(只看快照里有的设备)
	let nzone = 0;
	let outz = [];
	for (let d in dev_list) {
		for (let w in (S.snap.wans ?? [])) {
			if (w.dev != d)
				continue;
			nzone++;
			if (w.in_zone == false)
				push(outz, d);
		}
	}
	if (length(outz) > 0)
		add(S, 'exits.zone', 'exits', '出口设备所在防火墙区域', 'fail',
			sprintf('这些出口设备不在防火墙的 wan 区域:%s。转发默认会被拒绝且没有地址转换,走这些出口的流量不通', join('、', outz)),
			'在「网络 → 防火墙」把对应接口加入 wan 区域(本插件不会自动修改防火墙)');
	else if (nzone > 0)
		add(S, 'exits.zone', 'exits', '出口设备所在防火墙区域', 'ok', '出口设备都在 wan 区域', '');

	// rp_filter:有效值 = max(all, 设备)
	let all_rp = rd(S, '/proc/sys/net/ipv4/conf/all/rp_filter');
	let strict = [];
	let seen = [];
	for (let d in dev_list) {
		let v = rd(S, '/proc/sys/net/ipv4/conf/' + d + '/rp_filter');
		if (v == null || all_rp == null)
			continue;
		let eff = int(trim(v));
		if (int(trim(all_rp)) > eff)
			eff = int(trim(all_rp));
		push(seen, sprintf('%s=%d', d, eff));
		if (eff == 1)
			push(strict, d);
	}
	if (length(strict) > 0) {
		let cmd = 'sysctl -w net.ipv4.conf.all.rp_filter=2';
		for (let d in strict)
			cmd += sprintf(' net.ipv4.conf.%s.rp_filter=2', replace(d, '.', '/'));
		add(S, 'exits.rpfilter', 'exits', '反向路径过滤(rp_filter)', 'warn',
			sprintf('出口设备 %s 处于严格模式(1)。严格模式下走第二出口的回包可能被丢弃(推断,尚未在多出口实机验证)', join('、', strict)),
			'如遇到走第二出口时连接不通,可改为宽松模式:' + cmd + '(写入 /etc/sysctl.conf 可持久化);本插件不会自动修改');
	}
	else if (length(seen) > 0)
		add(S, 'exits.rpfilter', 'exits', '反向路径过滤(rp_filter)', 'ok', '出口设备没有处于严格模式(' + join(',', seen) + ')', '');

	// IPv6:被强制阻断的出口
	if (S.v6on) {
		let blocked = [];
		for (let e in exits) {
			if (e.v6 == false)
				push(blocked, exit_label(S, e));
		}
		if (length(blocked) > 0)
			add(S, 'exits.v6', 'exits', 'IPv6 出口能力', 'info',
				sprintf('出口 %s 不发放 LAN 前缀:匹配到它的 IPv6 流量会被阻断(客户端会回落 IPv4),不会回落系统默认路由', names(blocked, 4)),
				'需要这些规则也管 IPv6 时,换用发放 LAN 前缀的出口');
	}
}

// ---------------------------------------------------------------- 并存

function coexist_proxy(S, found) {
	for (let pd in PRODUCTS) {
		let sig = has_mark(S, pd.mark);
		let uv = sv(uget(S, pd.uci[0], pd.uci[1], pd.uci[2]));
		let enabled = (pd.uci[3] == '*') ? (uv != '' && uv != 'nil') : (uv == pd.uci[3]);
		let ps = any_proc(S, pd.procs);
		let why = [];
		if (sig)
			push(why, 'ip rule 里有它的标记签名');
		if (enabled)
			push(why, 'UCI 已启用');
		if (length(ps) > 0)
			push(why, '进程 ' + names(ps, 3) + ' 在运行');
		let id = 'coexist.' + pd.key;
		if (sig || (enabled && length(ps) > 0)) {
			found.n++;
			add(S, id, 'coexist', pd.name, 'warn',
				sprintf('%s 看起来正在运行(依据:%s)。代理优先:被它接管的流量不受本插件控制,没被接管(直连/绕过)的流量仍按策略路由走', pd.name, join(';', why)),
				'需要按出口分流的流量,请在 ' + pd.name + ' 里设为直连/绕过,或按需关闭它');
		}
		else if (enabled) {
			found.n++;
			add(S, id, 'coexist', pd.name, 'info', pd.name + ' 在 UCI 里已启用,但没有检测到运行迹象(可能正在启动或启动失败)', '');
		}
	}
}

function check_coexist(S) {
	let found = { n: 0 };
	coexist_proxy(S, found);

	// mwan3:标记掩码 0x3f00(bit8-13)不与本插件重叠,但两者都在管默认出口选路
	let m3 = false;
	for (let fam in [ 4, 6 ]) {
		for (let r in ip_rules(S, fam).list) {
			if (index(r.spec, 'fwmark') >= 0 && index(r.spec, '/0x3f00') >= 0)
				m3 = true;
		}
	}
	let p3 = any_proc(S, [ 'mwan3track', 'mwan3rtmon' ]);
	if (m3 || length(p3) > 0) {
		found.n++;
		add(S, 'coexist.mwan3', 'coexist', 'mwan3', 'warn', 'mwan3 在运行:它与本插件都在决定流量走哪个出口,同一条连接可能被两边拉扯', '二选一:关闭 mwan3,或不要对同一批流量同时配置两边的选路');
	}

	// pbr:fw_mask 默认 0x00ff0000,与本插件的出口标记字段重叠
	let pbr_sig = false;
	for (let fam in [ 4, 6 ]) {
		for (let r in ip_rules(S, fam).list) {
			if (index(r.spec, 'fwmark') >= 0 && index(r.spec, '/0xff0000') >= 0)
				pbr_sig = true;
		}
	}
	let pbr_on = (sv(uget(S, 'pbr', 'pbr', 'enabled')) == '1');
	if (pbr_sig) {
		found.n++;
		add(S, 'coexist.pbr', 'coexist', 'pbr(策略路由软件包)', 'fail', 'pbr 在运行,它的标记掩码 0x00ff0000 与本插件的出口标记字段(bit16-28)重叠,两者会互相覆盖标记', '二选一:关闭 pbr;或把 pbr 的 fw_mask 改到不重叠的位段');
	}
	else if (pbr_on) {
		found.n++;
		add(S, 'coexist.pbr', 'coexist', 'pbr(策略路由软件包)', 'warn', 'pbr 在 UCI 里已启用:它默认的标记掩码 0x00ff0000 与本插件的出口标记字段重叠,运行后会互相干扰', '二选一:关闭 pbr;或把 pbr 的 fw_mask 改到不重叠的位段');
	}

	// eqos 的 loadbalance 脚本会用全掩码改写整个 ct mark
	let eq_on = (sv(uget(S, 'eqos', 'eqos', 'enabled')) == '1');
	let eq_if = sv(uget(S, 'eqos', 'eqos', 'interface'));
	if (eq_on && eq_if != '') {
		found.n++;
		add(S, 'coexist.eqos', 'coexist', 'eqos(限速)', 'warn', 'eqos 已启用且配置了 interface:它的负载均衡脚本会用全掩码改写整个连接标记,可能抹掉本插件的出口标记', '不使用 eqos 的负载均衡时,清空其 interface 选项,或关闭 eqos');
	}

	// 主表里的分半默认路由(0.0.0.0/1 + 128.0.0.0/1)比默认路由更具体,suppress_prefixlength 0 压不住它
	let rr = sh(S, 'ip -4 route show table main');
	if (rr.rc == 0) {
		let lo = '';
		let hi = '';
		for (let ln in split(rr.out, '\n')) {
			let t = trim(ln);
			if (substr(t, 0, 10) == '0.0.0.0/1 ')
				lo = t;
			else if (substr(t, 0, 12) == '128.0.0.0/1 ')
				hi = t;
		}
		if (lo != '' && hi != '') {
			found.n++;
			let md = match(lo, /(^| )dev ([^ ]+)/);
			add(S, 'coexist.def1', 'coexist', '主表分半默认路由', 'warn',
				'主路由表里有 0.0.0.0/1 与 128.0.0.0/1' + ((md != null) ? '(设备 ' + md[2] + ',常见于 VPN 的 redirect-gateway def1)' : '') + ',它们比默认路由更具体,会盖过出口表里的默认路由',
				'用到 VPN 的全局重定向时,策略路由的出口选择可能失效;请改用不接管默认路由的 VPN 配置,或把该 VPN 作为出口之一');
		}
	}

	// 本插件 ip rule 带内的外来规则
	let alien = [];
	for (let fam in [ 4, 6 ]) {
		for (let r in ip_rules(S, fam).list) {
			// mwrt-wanmon 探测临时规则(pref 5900-5915、oif … lookup 48xx)是本插件自己的,只在探测期间短暂存在,不算外来
			if (r.pref >= 5900 && r.pref <= 5915 && match(r.spec, /^oif [^ ]+ lookup 48[0-9][0-9]$/))
				continue;
			if (r.pref >= BAND_LO && r.pref <= BAND_HI && !is_own_spec(r.spec))
				push(alien, sprintf('IPv%d 优先级 %d(%s)', fam, r.pref, r.spec));
		}
	}
	if (length(alien) > 0) {
		found.n++;
		add(S, 'coexist.band', 'coexist', 'ip rule 优先级带', 'fail',
			sprintf('本插件使用的 ip rule 优先级带(%d-%d)里有别的程序添加的规则:%s', BAND_LO, BAND_HI, names(alien, 3)),
			'找出添加它的程序并改用别的优先级,否则它可能抢在或夹在本插件的规则之间');
	}

	if (found.n == 0)
		add(S, 'coexist.none', 'coexist', '并存软件', 'ok', '没有发现 PassWall / SSR-Plus / OpenClash / mwan3 / pbr / eqos 负载均衡 在运行,主表里也没有分半默认路由', '');
}

// ---------------------------------------------------------------- DNS

function check_dns(S) {
	let dom_lines = (S.plan != null && type(S.plan.dns) == 'object') ? (S.plan.dns.lines ?? []) : [];
	let has_dom = (length(dom_lines) > 0);
	if (S.plan == null) {
		add(S, 'dns.none', 'dns', 'DNS 解析集合', 'info', '缺少配置或编译计划,无法检查', '');
		return;
	}
	if (!has_dom) {
		add(S, 'dns.none', 'dns', 'DNS 解析集合', 'info', '没有生效的域名规则,未使用 DNS 解析集合', '');
		return;
	}

	// 集合里的地址数
	let ns = nft_struct(S);
	if (!ns.exists)
		add(S, 'dns.sets', 'dns', 'DNS 解析集合', 'info', '内核里还没有集合(策略路由尚未应用),无法读取地址数', '');
	else {
		let ss = nft_sets(S);
		if (!ss.ok)
			add(S, 'dns.sets', 'dns', 'DNS 解析集合', 'info', '读不到集合内容', '');
		else {
			let per = {};
			let order = [];
			let total = 0;
			let nsets = 0;
			for (let nm in ss.names) {
				let m = match(nm, /^dns([46])_(.+)$/);
				if (m == null)
					continue;
				nsets++;
				total += ss.counts[nm];
				if (per[m[2]] == null) {
					per[m[2]] = { v4: 0, v6: 0 };
					push(order, m[2]);
				}
				if (m[1] == '4')
					per[m[2]].v4 += ss.counts[nm];
				else
					per[m[2]].v6 += ss.counts[nm];
			}
			let txt = [];
			for (let rid in order)
				push(txt, sprintf('%s(v4 %d,v6 %d)', rule_title(S, rid), per[rid].v4, per[rid].v6));
			let applied_ts = S.state.applied_ts;
			let aged = (type(applied_ts) == 'int' && S.now - applied_ts >= 600);
			if (total > 0)
				add(S, 'dns.sets', 'dns', 'DNS 解析集合', 'ok', sprintf('共 %d 个集合,已收录 %d 个地址。%s', nsets, total, names(txt, 4)), '');
			else if (aged)
				add(S, 'dns.sets', 'dns', 'DNS 解析集合', 'warn', sprintf('共 %d 个集合,但一个地址也没有收录:域名规则目前不会命中任何流量', nsets),
					'确认客户端使用本机作为 DNS(没有被手动指定的 DNS、DoH、DoT 绕开),并且 dnsmasq 已加载映射文件(见下一项)');
			else
				add(S, 'dns.sets', 'dns', 'DNS 解析集合', 'info', sprintf('共 %d 个集合,暂未收录地址(刚应用不久,或还没有客户端解析过这些域名)', nsets), '');
		}
	}

	// dnsmasq 是否带映射文件
	let dm = dnsmasq_info(S);
	if (length(dm.confs) == 0)
		add(S, 'dns.map', 'dns', 'dnsmasq 映射文件', 'warn', '找不到 dnsmasq 的运行配置(/var/etc/dnsmasq.conf.*),无法确认映射是否生效', '确认 dnsmasq 服务在运行');
	else if (dm.map_lines == null)
		add(S, 'dns.map', 'dns', 'dnsmasq 映射文件', 'fail', sprintf('dnsmasq 的配置目录里没有 %s:域名规则不会生效', MAP_FILE), '点击「应用」生成映射文件(会重启 dnsmasq);若仍没有,检查 dnsmasq 的 conf-dir 是否可写');
	else {
		let want = {};
		let have = {};
		for (let l in dom_lines)
			want[l] = true;
		for (let l in dm.map_lines)
			have[l] = true;
		let diff = 0;
		for (let k in keys(want)) {
			if (!have[k])
				diff++;
		}
		for (let k in keys(have)) {
			if (!want[k])
				diff++;
		}
		if (diff == 0)
			add(S, 'dns.map', 'dns', 'dnsmasq 映射文件', 'ok', sprintf('映射文件 %s 与当前配置一致(%d 个域名)。dnsmasq 是否已重新读取该文件,本检查无法确认', dm.map_path, length(dom_lines)), '');
		else
			add(S, 'dns.map', 'dns', 'dnsmasq 映射文件', 'warn', sprintf('映射文件 %s 与当前配置不一致(有 %d 行不同)', dm.map_path, diff), '点击「应用」更新(内容变化时会重启 dnsmasq)');
	}

	// 集合寿命与 dnsmasq 缓存
	let ttl = ttl_secs(S.g.dns_ttl ?? '1d');
	if (ttl == null)
		ttl = 86400;
	if (dm.min_ttl != null && ttl < dm.min_ttl)
		add(S, 'dns.ttl', 'dns', '集合寿命与 dnsmasq 缓存', 'warn',
			sprintf('DNS 集合寿命(%d 秒)短于 dnsmasq 的 min-cache-ttl(%d 秒):缓存命中时不会重新写入集合,元素过期后域名规则会失效', ttl, dm.min_ttl),
			'把全局设置里的「DNS 集合寿命」调到不小于 dnsmasq 的缓存时间');
	else if (dm.min_ttl != null)
		add(S, 'dns.ttl', 'dns', '集合寿命与 dnsmasq 缓存', 'ok', sprintf('DNS 集合寿命 %d 秒,不短于 dnsmasq 的 min-cache-ttl(%d 秒)', ttl, dm.min_ttl), '');

	// 解析绕过
	let risks = [];
	let hot = false;
	let redir = sv(uget(S, 'dhcp', 'dnsmasq', 'dns_redirect'));
	if (redir == '1')
		push(risks, '已开启 DNS 重定向(只重定向 UDP 53):手动指定其它 DNS 的客户端也会被送到本机;TCP 53、DoT、DoH 不受影响');
	else
		push(risks, '客户端若手动指定了其它 DNS 服务器(如 8.8.8.8),解析不经过本机 dnsmasq,域名规则不会命中');
	push(risks, '浏览器/系统自带的 DoH、DoT 同样绕开本机 DNS');
	if (sv(uget(S, 'smartdns', 'smartdns', 'enabled')) == '1' || length(any_proc(S, [ 'smartdns' ])) > 0) {
		let sp = sv(uget(S, 'smartdns', 'smartdns', 'port'));
		if (sp == '53') {
			hot = true;
			push(risks, 'smartdns 监听 53 端口:若它直接应答客户端而不经 dnsmasq,域名规则不会命中');
		}
		else
			push(risks, 'smartdns 已启用:若它作为 dnsmasq 的上游则不影响,若直接接管 53 端口则域名规则不会命中');
	}
	let oc_mode = sv(uget(S, 'openclash', 'openclash', 'en_mode'));
	let oc_run = has_mark(S, '0x162') || length(any_proc(S, [ 'clash', 'mihomo' ])) > 0;
	if (oc_run && index(oc_mode, 'fake-ip') >= 0) {
		hot = true;
		push(risks, 'OpenClash 处于 fake-ip 模式:域名被解析为 198.18.0.0/15 的假地址,本插件按设计跳过该段,域名规则对它接管的流量无效');
	}
	if (has_mark(S, '0x50535731') || has_mark(S, '0x50535732')) {
		hot = true;
		push(risks, 'PassWall 在运行:它可能重定向客户端的 53 端口解析,经它解析的域名不一定写入本插件的集合');
	}
	add(S, 'dns.bypass', 'dns', '解析绕过风险', hot ? 'warn' : 'info', join('。', risks),
		'域名选路只对「经本机 dnsmasq 解析」的流量有效;客户端使用本机 DNS 时效果最稳。域名规则只看解析出的地址,不看 HTTPS 的 URL 路径,也看不到 ECH 加密的域名');
}

// ---------------------------------------------------------------- 卸载

function check_offload(S) {
	let fo = sv(uget(S, 'firewall', 'defaults', 'flow_offloading'));
	let fh = sv(uget(S, 'firewall', 'defaults', 'flow_offloading_hw'));
	let ft = sh(S, 'nft list flowtables');
	let has_ft = (ft.rc == 0 && match(ft.out, /flowtable [A-Za-z0-9_]+ \{/) != null);
	if (fo == '1' || fh == '1')
		add(S, 'offload.flow', 'offload', '软件流量卸载', 'warn',
			'防火墙开启了流量卸载(flow_offloading)' + (has_ft ? ',内核里有 flowtable' : '') + ':被卸载的连接不再经过标记与选路,策略路由对它们无效',
			'需要策略路由对全部连接生效时,在「网络 → 防火墙」关闭「软件流量分载」;本插件不会自动改动全局卸载设置');
	else if (has_ft)
		add(S, 'offload.flow', 'offload', '软件流量卸载', 'info', 'flow_offloading 为关闭,但内核里存在 flowtable 定义(没有规则引用时不会卸载连接)', '');
	else
		add(S, 'offload.flow', 'offload', '软件流量卸载', 'ok', 'flow_offloading 为关闭,内核里没有 flowtable', '');

	if (ex(S, HNAT_DIR)) {
		let hook = trim('' + (rd(S, HNAT_DIR + '/hook_toggle') ?? ''));
		let flush = to_bool(S.g.flush_accel, false);
		let on = (hook == '' || index(hook, 'enabled') >= 0);
		add(S, 'offload.hnat', 'offload', '硬件加速(HNAT)', (on && !flush) ? 'warn' : 'info',
			'本机有联发科硬件加速(HNAT)' + (hook != '' ? ',当前状态:' + hook : '') + '。已被它绑定的流绕过防火墙,新规则和故障切换对这些存量连接不生效,新建连接不受影响'
			+ (flush ? '。已开启「应用后清空加速表」' : ''),
			flush ? '' : '需要新规则尽快对存量连接生效时,可在全局设置打开「应用后清空加速表」(清表命令沿用行为管控的做法,具体清除范围尚未证实,且有短暂重新走软件转发的副作用);本插件不会关闭硬件加速');
	}
	else
		add(S, 'offload.hnat', 'offload', '硬件加速(HNAT)', 'ok', '没有检测到联发科硬件加速(HNAT)', '');
}

// ---------------------------------------------------------------- 规则

function check_rules(S) {
	if (S.plan == null) {
		add(S, 'rules.skipped', 'rules', '规则生效情况', 'info', '缺少配置或编译计划,无法检查规则', '');
		return;
	}
	let rules = S.plan.rules ?? [];
	let nact = 0;
	let ndis = 0;
	let errs = [];
	for (let r in rules) {
		if (r.status == 'active')
			nact++;
		else if (r.status == 'disabled')
			ndis++;
		else
			push(errs, sprintf('「%s」:%s', (r.name != null && r.name != '') ? r.name : (r.id ?? '?'), r.reason ?? ''));
	}
	if (length(rules) == 0)
		add(S, 'rules.skipped', 'rules', '规则生效情况', 'info', '还没有任何规则', '在「策略规则」里新建规则');
	else if (length(errs) > 0)
		add(S, 'rules.skipped', 'rules', '规则生效情况', 'warn', sprintf('共 %d 条规则,生效 %d、停用 %d、有 %d 条被跳过:%s', length(rules), nact, ndis, length(errs), names(errs, 4)), '打开对应规则按提示修正后重新应用');
	else
		add(S, 'rules.skipped', 'rules', '规则生效情况', 'ok', sprintf('共 %d 条规则,生效 %d、停用 %d,没有被跳过的规则', length(rules), nact, ndis), '');

	// 指向设备未接入的出口
	let absent_exit = {};
	for (let e in (S.plan.exits ?? [])) {
		let all_absent = (length(e.members ?? []) > 0);
		for (let m in (e.members ?? [])) {
			if (m.reason != '设备未接入')
				all_absent = false;
		}
		if (all_absent)
			absent_exit['' + e.id] = exit_label(S, e);
	}
	let ab = [];
	for (let r in rules) {
		if (r.status == 'active' && r.exit != null && absent_exit['' + r.exit] != null)
			push(ab, sprintf('「%s」→ %s', (r.name != null && r.name != '') ? r.name : (r.id ?? '?'), absent_exit['' + r.exit]));
	}
	if (length(ab) > 0)
		add(S, 'rules.absent', 'rules', '指向未接入出口的规则', 'warn', sprintf('%d 条规则指向的出口设备当前没有接入:%s', length(ab), names(ab, 4)), '接入对应设备,或把规则改到其它出口');

	// 命中情况:只在已应用且有计数数据时给出
	let srules = S.state.rules;
	if (type(srules) == 'array' && length(srules) > 0) {
		let zero = [];
		for (let sr in srules) {
			if (sr.status == 'active' && type(sr.hits) == 'object' && sr.hits.pkts == 0)
				push(zero, '「' + ((sr.name != null && sr.name != '') ? sr.name : (sr.id ?? '?')) + '」');
		}
		if (length(zero) > 0)
			add(S, 'rules.hits', 'rules', '命中情况', 'info', sprintf('%d 条生效的规则还没有命中过流量:%s', length(zero), names(zero, 5)),
				'可能是条件写得太窄、被前面的规则抢先命中,或确实还没有对应流量;计数只在新连接建立时增加');
	}

	// 应用学习集:有应用维度的规则才有
	let ss = nft_sets(S);
	if (ss.ok) {
		let total = 0;
		let nsets = 0;
		for (let nm in ss.names) {
			if (match(nm, /^l[46]_/) != null) {
				nsets++;
				total += ss.counts[nm];
			}
		}
		if (nsets > 0)
			add(S, 'rules.learn', 'rules', '应用学习集', 'info',
				sprintf('共 %d 个应用学习集,已学到 %d 个「地址 . 协议 . 端口」。应用选路是学习式的:只对识别之后的新连接生效,首个连接和前几个包不会改道', nsets, total), '');
	}
}

// ---------------------------------------------------------------- 入口

function run_diag(opts) {
	if (type(opts) != 'object')
		opts = {};
	let S = {
		io: opts.io, now: opts.now ?? time(),
		cfg: (type(opts.cfg) == 'object') ? opts.cfg : null,
		snap: (type(opts.snap) == 'object') ? opts.snap : {},
		state: (type(opts.state) == 'object') ? opts.state : {},
		apps: opts.apps, plan: null, g: {}, enabled: false, v6on: true, pending: false,
		wmap: {}, rname: {}, checks: [], c: {}, cur: null
	};
	if (S.cfg != null && type(S.cfg.global) == 'object')
		S.g = S.cfg.global;
	for (let w in (S.snap.wans ?? [])) {
		if (w != null && w.wan_id != null)
			S.wmap[w.wan_id] = w;
	}
	S.enabled = (S.cfg != null) ? to_bool(S.g.enabled, false) : (S.state.enabled == true);
	S.v6on = to_bool(S.g.ipv6, true);

	// 编译计划:调用方没给就按当前配置现场编译(出口编号沿用上次应用的编号)
	let plan = opts.plan;
	if (!(type(plan) == 'object' && type(plan.exits) == 'array' && type(plan.rules) == 'array')) {
		plan = null;
		if (S.cfg != null) {
			let prev = {};
			for (let e in (S.state.exits ?? [])) {
				if (e.key != null && type(e.id) == 'int')
					prev[e.key] = e.id;
			}
			plan = compile(S.cfg, S.snap, { lan_ifs: (type(opts.lan_ifs) == 'array') ? opts.lan_ifs : [], apps: S.apps, dns_capable: opts.dns_capable, prev_ids: prev });
		}
	}
	S.plan = plan;
	if (S.plan != null) {
		for (let r in (S.plan.rules ?? [])) {
			if (r.id != null)
				S.rname[lc('' + r.id)] = (r.name != null && r.name != '') ? r.name : ('' + r.id);
		}
	}

	check_engine(S);
	check_capability(S);
	check_exits(S);
	check_coexist(S);
	check_dns(S);
	check_offload(S);
	check_rules(S);

	let cnt = { ok: 0, warn: 0, fail: 0, info: 0 };
	for (let c in S.checks)
		cnt[c.level] = (cnt[c.level] ?? 0) + 1;
	return { ok: true, ts: S.now, checks: S.checks, summary: cnt };
}

export { run_diag };
