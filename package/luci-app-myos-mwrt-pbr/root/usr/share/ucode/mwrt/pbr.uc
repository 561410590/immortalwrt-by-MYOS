'use strict';
// mwrt-pbr 引擎(SPEC §5、§13.4):apply / reconcile / flush / status / counters / kick / validate / preview,
// 以及 diag、trace 的入口。只管理自己的东西:nft 表 inet mwrt_pr、出口路由表 4001-4253、
// ip rule 优先级 5001-5899 里带 0x1fff0000 掩码的规则、dnsmasq conf-dir 里的 mwrt-pbr.conf;绝不碰主表默认路由。
// 模块顶层只定义函数与常量,不读配置、不起进程;重模块(config/compile/validate/wans/diag/trace)按需加载,
// 这样 kick、status、counters 这类轻命令不用为解析整套编译器付启动开销。

import * as fs from 'fs';
import * as ubus from 'ubus';
import { sh_quote, run, log, read_json, write_json_atomic, write_atomic, now, uci_cursor, RUN_DIR } from 'mwrt.lib';

const TBL = 'inet mwrt_pr';
const CLI = '/usr/libexec/mwrt-pbr';
const LOCK_FILE = '/var/lock/mwrt-pbr.lock';
const LOCK_WAIT_MS = 10000;
const CFG_FILE = '/etc/config/mwrt_pbr';
const F_STATE = RUN_DIR + '/state.json';
const F_GOOD_NFT = RUN_DIR + '/last-good.nft';
const F_GOOD_RULES = RUN_DIR + '/last-good.rules';
const F_COUNTERS = RUN_DIR + '/counters.json';
const F_DIRTY = RUN_DIR + '/dirty';
const F_KICK_LOCK = RUN_DIR + '/kick.lock';
const F_CAND = RUN_DIR + '/cand.nft';
const F_CAND_GC = RUN_DIR + '/cand-gc.nft';
const F_CAP = RUN_DIR + '/dnsmasq.cap';
// dnsmasq 重启失败后留标记,否则映射文件内容已一致,之后的对账不会再重启
const F_DNS_PENDING = RUN_DIR + '/dns_pending';
const F_APPLIED = '/etc/mwrt-pbr-applied.json';
// 服务被 stop 后留标记,阻止 hotplug 去抖对账把规则装回去
const F_STOPPED = RUN_DIR + '/stopped';
const F_WANMON = '/tmp/mwrt-wan.json';
const DNS_FILE = 'mwrt-pbr.conf';
const DNS_BIN = '/usr/sbin/dnsmasq';
const HNAT_ENTRY = '/sys/kernel/debug/hnat/hnat_entry';
const BAND_LO = 5001;
const BAND_HI = 5899;
const TABLE_LO = 4001;
const TABLE_HI = 4127;
const NFT_EOF = 'MWRT_PBR_EOF';
const HEREDOC_MAX = 100000;
const RETRY_SECS = 300;
const SPEC_RE = /^fwmark 0x[0-9a-f]+\/0x7f0000 (lookup main suppress_prefixlength 0|lookup [0-9]+|unreachable)$/;

// 持锁期间的运行时信息:lock_fd 是引擎锁、kick_fd 是去抖 worker 的唯一性锁。
// fs.open 的描述符会被子进程继承,而 flock 跟着打开的文件描述走,dnsmasq 这类常驻进程若继承了它们,锁就永远放不掉
const ENG = { lock_fd: -1, kick_fd: -1 };
const LAZY = {};

// ---------------------------------------------------------------- 通用小工具

// require() 对 export 风格模块不可用,用 loadstring 里的 import 取命名空间对象
function lazy(name) {
	if (LAZY[name] != null)
		return LAZY[name];
	if (!match(name, /^[a-z]+$/))
		die('内部错误:模块名无效 ' + name);
	let fn = loadstring("'use strict'; import * as m from 'mwrt." + name + "'; return m;");
	if (fn == null)
		die('内部错误:无法加载模块 ' + name);
	LAZY[name] = fn();
	return LAZY[name];
}

function msg_of(e) {
	return (e != null && e.message != null) ? '' + e.message : '' + e;
}

function as_int(v, d) {
	if (type(v) == 'int')
		return v;
	if (type(v) == 'double')
		return int(v);
	if (type(v) == 'string' && match(v, /^-?[0-9]+$/))
		return int(v);
	return d;
}

function clip(s, n) {
	let t = trim('' + (s ?? ''));
	return (length(t) > n) ? substr(t, 0, n) + '…' : t;
}

function ensure_dir(d) {
	if (fs.stat(d) == null)
		fs.mkdir(d);
	return fs.stat(d) != null;
}

function self_pid() {
	return fs.readlink('/proc/self') ?? '0';
}

function tokens_of(v) {
	let out = [];
	if (v == null)
		return out;
	let items = (type(v) == 'array') ? v : [ v ];
	for (let it in items) {
		for (let t in split(trim('' + it), /[ \t]+/)) {
			if (t != '')
				push(out, t);
		}
	}
	return out;
}

function state_read() {
	let s = read_json(F_STATE, null);
	return (type(s) == 'object') ? s : null;
}

function cfg_mtime() {
	let st = fs.stat(CFG_FILE);
	return (st != null && type(st.mtime) == 'int') ? st.mtime : 0;
}

// 会拉起常驻进程的命令(如 dnsmasq 重启)不能继承引擎锁的描述符,否则锁会被子进程一直占着
function nolock() {
	let s = '';
	// ash 的重定向只认单个数字的描述符
	for (let fd in [ ENG.lock_fd, ENG.kick_fd ]) {
		if (fd >= 0 && fd < 10)
			s += sprintf(' %d>&-', fd);
	}
	return s;
}

// ---------------------------------------------------------------- 引擎锁

function lock_take(wait_ms) {
	ensure_dir(fs.dirname(LOCK_FILE));
	let fh = fs.open(LOCK_FILE, 'a');
	if (fh == null)
		return null;
	let waited = 0;
	while (true) {
		if (fh.lock('xn') == true)
			return fh;
		if (waited >= wait_ms)
			break;
		sleep(100);
		waited += 100;
	}
	fh.close();
	return null;
}

// ucode 没有 try/finally:异常先记下,解锁后再返回错误
function with_lock(fn) {
	let fh = lock_take(LOCK_WAIT_MS);
	if (fh == null)
		return { ok: false, error: '另一个应用正在进行,请稍后重试', busy: true };
	ENG.lock_fd = fh.fileno();
	let res = null;
	let ex = null;
	try {
		res = fn();
	}
	catch (e) {
		ex = e;
	}
	ENG.lock_fd = -1;
	fh.lock('u');
	fh.close();
	if (ex != null)
		return { ok: false, error: '内部错误: ' + msg_of(ex) };
	return res;
}

// ---------------------------------------------------------------- nft

// 读表里已有的链/集合/计数器。-t 省略集合元素,DNS 集合再大也不会拖慢;表不存在返回 exists=false
function nft_read() {
	let res = { exists: false, chains: [], sets: [], counters: [], cvals: {} };
	let r = run('nft -j -t list table ' + TBL);
	if (r.rc != 0)
		return res;
	res.exists = true;
	let txt = r.out;
	let i = index(txt, '{');
	if (i > 0)
		txt = substr(txt, i);
	let o = null;
	try {
		o = json(txt);
	}
	catch (e) {
		return res;
	}
	if (type(o) != 'object' || type(o.nftables) != 'array')
		return res;
	for (let it in o.nftables) {
		if (type(it) != 'object')
			continue;
		if (type(it.chain) == 'object' && it.chain.name != null) {
			push(res.chains, '' + it.chain.name);
		}
		else if (type(it.set) == 'object' && it.set.name != null) {
			push(res.sets, '' + it.set.name);
		}
		else if (type(it.counter) == 'object' && it.counter.name != null) {
			let nm = '' + it.counter.name;
			push(res.counters, nm);
			res.cvals[nm] = { pkts: as_int(it.counter.packets, 0), bytes: as_int(it.counter.bytes, 0) };
		}
	}
	return res;
}

// 只读命名计数器;表不存在返回 null
function cnt_live() {
	let r = run('nft -j list counters table ' + TBL);
	if (r.rc != 0)
		return null;
	let txt = r.out;
	let i = index(txt, '{');
	if (i > 0)
		txt = substr(txt, i);
	let o = null;
	try {
		o = json(txt);
	}
	catch (e) {
		return null;
	}
	let res = {};
	if (type(o) != 'object' || type(o.nftables) != 'array')
		return res;
	for (let it in o.nftables) {
		if (type(it) == 'object' && type(it.counter) == 'object' && it.counter.name != null)
			res['' + it.counter.name] = { pkts: as_int(it.counter.packets, 0), bytes: as_int(it.counter.bytes, 0) };
	}
	return res;
}

function nft_check_file(path) {
	let r = run('nft -c -f ' + sh_quote(path));
	return { ok: (r.rc == 0), error: (r.rc == 0) ? '' : clip(r.out, 800) };
}

// 文本直接经 heredoc 喂给 nft -c,干跑不落盘;文本太长超过命令行上限时借 /tmp 下的临时文件,检查完立即删除
function nft_check_text(text) {
	if (text == null || text == '')
		return { ok: true, error: '' };
	if (index(text, NFT_EOF) >= 0)
		return { ok: false, error: 'nft 文本含有保留的结束标记,无法检查' };
	if (substr(text, length(text) - 1) != '\n')
		text += '\n';
	if (length(text) > HEREDOC_MAX) {
		let tmp = sprintf('/tmp/.mwrt-pbr-chk.%s.nft', self_pid());
		if (fs.writefile(tmp, text) == null) {
			fs.unlink(tmp);
			return { ok: false, error: '无法写入临时文件做语法检查' };
		}
		let res = nft_check_file(tmp);
		fs.unlink(tmp);
		return res;
	}
	let r = run("nft -c -f - <<'" + NFT_EOF + "'\n" + text + NFT_EOF);
	return { ok: (r.rc == 0), error: (r.rc == 0) ? '' : clip(r.out, 800) };
}

function nft_load_file(path) {
	let r = run('nft -f ' + sh_quote(path));
	return { ok: (r.rc == 0), error: (r.rc == 0) ? '' : clip(r.out, 800) };
}

// ---------------------------------------------------------------- ip rule

// 本插件的规则:优先级在 5001-5899 带内,且选择器是 fwmark …/0x1fff0000;带内别人的规则一律不动
function rules_kernel() {
	let out = [];
	for (let fam in [ 4, 6 ]) {
		let r = run(sprintf('ip -%d rule show', fam));
		if (r.rc != 0)
			continue;
		for (let ln in split(r.out, '\n')) {
			let m = match(ln, /^([0-9]+):[ \t]+from all (.+)$/);
			if (m == null)
				continue;
			let pref = int(m[1]);
			if (pref < BAND_LO || pref > BAND_HI)
				continue;
			// 新版 iproute2 可能追加 proto 字段,不影响比较
			let spec = join(' ', split(trim(replace(m[2], /[ \t]+proto[ \t]+[^ \t]+/, '')), /[ \t]+/));
			if (match(spec, SPEC_RE) == null)
				continue;
			push(out, { family: fam, pref: pref, spec: spec, key: sprintf('%d|%d|%s', fam, pref, spec) });
		}
	}
	return out;
}

function rule_cmd(op, r) {
	let toks = [];
	for (let t in split(r.spec, ' '))
		push(toks, sh_quote(t));
	return sprintf('ip -%d rule %s pref %d %s', r.family, op, r.pref, join(' ', toks));
}

function rules_diff(want, have) {
	let hv = {};
	let wv = {};
	for (let r in have)
		hv[r.key] = true;
	let add = [];
	let del = [];
	for (let r in want) {
		let key = sprintf('%d|%d|%s', r.family, r.pref, r.spec);
		wv[key] = true;
		if (!hv[key])
			push(add, { family: r.family, pref: r.pref, spec: r.spec, key: key });
	}
	for (let r in have) {
		if (!wv[r.key])
			push(del, r);
	}
	return { add: add, del: del };
}

function rule_valid(r) {
	return (r.family == 4 || r.family == 6) && type(r.pref) == 'int' && r.pref >= BAND_LO && r.pref <= BAND_HI
		&& match('' + r.spec, SPEC_RE) != null;
}

// ---------------------------------------------------------------- 出口表路由

function table_of_spec(spec) {
	let m = match(spec, /lookup ([0-9]+)$/);
	if (m == null)
		return 0;
	let t = int(m[1]);
	return (t >= TABLE_LO && t <= TABLE_HI) ? t : 0;
}

// IP/前缀转成与内核输出一致的规范写法(压缩、小写),这样"期望值"和"内核现状"才能逐字比较;
// 两边写法不一致的话,对账会把刚写入的路由当成多余的删掉
function canon_ip(s) {
	if (s == null || s == '')
		return '';
	let t = lc('' + s);
	let parts = split(t, '/');
	let a = iptoarr(parts[0]);
	if (a == null)
		return t;
	let c = arrtoip(a);
	if (c == null)
		return t;
	return (length(parts) > 1) ? c + '/' + parts[1] : c;
}

// 参数名 → 取值;未知的单词(onlink、linkdown、dead 等标志)直接跳过
function route_parse(fam, text) {
	let out = [];
	for (let ln in split(text, '\n')) {
		let tk = split(trim(ln), /[ \t]+/);
		if (length(tk) < 2 || tk[0] != 'default')
			continue;
		let r = { family: fam, via: '', dev: '', metric: 0, from: '' };
		let i = 1;
		while (i < length(tk)) {
			let k = tk[i];
			if ((k == 'via' || k == 'dev' || k == 'metric' || k == 'from' || k == 'proto' || k == 'src' || k == 'pref'
				|| k == 'scope' || k == 'table' || k == 'weight' || k == 'expires') && i + 1 < length(tk)) {
				if (k == 'via')
					r.via = canon_ip(tk[i + 1]);
				else if (k == 'dev')
					r.dev = tk[i + 1];
				else if (k == 'metric')
					r.metric = as_int(tk[i + 1], 0);
				else if (k == 'from')
					r.from = canon_ip(tk[i + 1]);
				i += 2;
			}
			else
				i++;
		}
		push(out, r);
	}
	return out;
}

// 表不存在时 ip 返回非 0,按空表处理
function route_list(fam, table) {
	let r = run(sprintf('ip -%d route show table %d', fam, table));
	if (r.rc != 0)
		return [];
	return route_parse(fam, r.out);
}

function route_valid(r, table) {
	if (r.family != 4 && r.family != 6)
		return false;
	if (type(table) != 'int' || table < TABLE_LO || table > TABLE_HI)
		return false;
	if (match('' + r.dev, /^[A-Za-z0-9_.@:-]{1,15}$/) == null)
		return false;
	if ((r.via ?? '') != '' && match('' + r.via, /^[0-9A-Fa-f:.]+$/) == null)
		return false;
	if ((r.from ?? '') != '' && match('' + r.from, /^[0-9A-Fa-f:.\/]+$/) == null)
		return false;
	return type(r.metric) == 'int' && r.metric >= 1;
}

function route_cmd(op, table, r) {
	let p = [ 'ip', '-' + r.family, 'route', op, 'default' ];
	if ((r.from ?? '') != '')
		push(p, 'from', sh_quote(r.from));
	if ((r.via ?? '') != '')
		push(p, 'via', sh_quote(r.via));
	push(p, 'dev', sh_quote(r.dev), 'metric', sprintf('%d', r.metric), 'table', sprintf('%d', table));
	return join(' ', p);
}

// 把出口表里的默认路由同步成期望值:先 replace 补/改,再删多余的。tables 是需要检查的全部表号;
// 不在 routes 里出现的表会被清空(出口消失、成员全部不可用)。返回 { changed, errors }
function routes_sync(routes, tables) {
	let want = {};
	for (let r in routes) {
		let k = sprintf('%d|%d', r.family, r.table);
		if (want[k] == null)
			want[k] = [];
		push(want[k], r);
	}
	let changed = 0;
	let errors = [];
	for (let t in tables) {
		for (let fam in [ 4, 6 ]) {
			let wl = want[sprintf('%d|%d', fam, t)] ?? [];
			let cur = route_list(fam, t);
			for (let w in wl) {
				let found = false;
				for (let c in cur) {
					if (c.metric == w.metric && c.dev == w.dev && c.via == canon_ip(w.via) && c.from == canon_ip(w.from)) {
						found = true;
						break;
					}
				}
				if (found)
					continue;
				if (!route_valid(w, t)) {
					push(errors, sprintf('出口表 %d 的路由参数无效(设备 %s),已跳过', t, w.dev));
					continue;
				}
				let r = run(route_cmd('replace', t, w));
				if (r.rc != 0)
					push(errors, sprintf('出口表 %d 路由写入失败(设备 %s):%s', t, w.dev, clip(r.out, 200)));
				else
					changed++;
			}
			for (let c in cur) {
				let keep = false;
				for (let w in wl) {
					if (c.metric == w.metric && c.from == canon_ip(w.from)) {
						keep = true;
						break;
					}
				}
				if (keep)
					continue;
				if (!route_valid(c, t))
					continue;
				let r = run(route_cmd('del', t, c));
				if (r.rc != 0)
					push(errors, sprintf('出口表 %d 路由删除失败(设备 %s):%s', t, c.dev, clip(r.out, 200)));
				else
					changed++;
			}
		}
	}
	return { changed: changed, errors: errors };
}

// 需要检查路由的全部表:本次计划 ∪ 上次状态里的出口 ∪ 内核 ip rule 里引用到的表(状态文件丢失也能清干净)
function tables_known(plan, prev, krules) {
	let seen = {};
	let out = [];
	let add = function(t) {
		if (type(t) == 'int' && t >= TABLE_LO && t <= TABLE_HI && !seen['' + t]) {
			seen['' + t] = true;
			push(out, t);
		}
	};
	if (plan != null) {
		for (let e in plan.exits)
			add(e.table);
	}
	for (let e in ((prev != null && type(prev.exits) == 'array') ? prev.exits : []))
		add(as_int(e.table, 0));
	for (let r in krules)
		add(table_of_spec(r.spec));
	return sort(out, function(a, b) { return a - b; });
}

// 清空一张出口表(两个地址族);只在表非空时才 flush,避免无谓的 fork
function table_flush(t) {
	let n = 0;
	for (let fam in [ 4, 6 ]) {
		if (length(route_list(fam, t)) > 0) {
			run(sprintf('ip -%d route flush table %d', fam, t));
			n++;
		}
	}
	return n;
}

// ---------------------------------------------------------------- dnsmasq

// conf-dir 从 dnsmasq 的运行配置里解析(在 tmpfs 里,每次开机可能变),多实例时取全部去重
function dns_dirs() {
	let dirs = [];
	for (let f in (fs.glob('/var/etc/dnsmasq.conf.*') ?? [])) {
		let t = fs.readfile(f);
		if (t == null)
			continue;
		for (let ln in split(t, '\n')) {
			let m = match(ln, /^conf-dir=([^,]+)/);
			if (m != null) {
				let d = trim(m[1]);
				if (d != '' && !(d in dirs))
					push(dirs, d);
			}
		}
	}
	return dirs;
}

// 能力判定:dnsmasq --version 的编译选项里含 nftset。结果按二进制的 mtime:size 缓存,只在会写状态的路径落盘
function dnsmasq_capable(write_cache) {
	let st = fs.stat(DNS_BIN);
	let key = (st != null) ? sprintf('%d:%d', st.mtime, st.size) : '';
	if (key != '') {
		let c = read_json(F_CAP, null);
		if (type(c) == 'object' && c.key == key && type(c.nftset) == 'bool')
			return c.nftset;
	}
	let r = run('dnsmasq --version');
	let ok = false;
	if (r.rc == 0) {
		for (let ln in split(r.out, '\n')) {
			if (match(ln, /(^|[ \t])nftset([ \t]|$)/) != null) {
				ok = true;
				break;
			}
		}
	}
	if (write_cache == true && key != '')
		write_json_atomic(F_CAP, { key: key, nftset: ok });
	return ok;
}

// 期望的映射文件与现状。dir 为空表示找不到 conf-dir(dnsmasq 没在跑),此时无法安装,按"无需同步"处理
function dns_view(plan) {
	let dirs = dns_dirs();
	let v = { dir: (length(dirs) > 0) ? dirs[0] : '', path: '', want: null, cur: null, same: true, dirs: dirs };
	if (v.dir == '')
		return v;
	v.path = v.dir + '/' + DNS_FILE;
	if (plan != null && length(plan.dns.lines) > 0)
		v.want = '# mwrt-pbr 自动生成,请勿手工修改\n' + join('\n', plan.dns.lines) + '\n';
	v.cur = fs.readfile(v.path);
	v.same = (v.want == v.cur && fs.stat(F_DNS_PENDING) == null);
	return v;
}

function dnsmasq_restart() {
	let r = run('/etc/init.d/dnsmasq restart' + nolock());
	if (r.rc == 0) {
		fs.unlink(F_DNS_PENDING);
		return true;
	}
	write_atomic(F_DNS_PENDING, '1
');
	return false;
}

// 写入或清除映射文件;只在内容变化时重启 dnsmasq(SIGHUP 不会重读 conf-dir)。返回 { changed, restarted, warn }
function dns_sync(plan) {
	let res = { changed: false, restarted: false, warn: '' };
	let v = dns_view(plan);
	if (v.dir == '') {
		if (plan != null && length(plan.dns.lines) > 0)
			res.warn = '没有找到 dnsmasq 的 conf-dir(dnsmasq 未运行?),域名规则暂时不会生效';
		return res;
	}
	if (v.same)
		return res;
	if (v.want == null) {
		fs.unlink(v.path);
		res.changed = true;
	}
	else {
		let tmp = v.dir + '/.' + DNS_FILE + '.cand';
		if (fs.writefile(tmp, v.want) == null) {
			fs.unlink(tmp);
			res.warn = '无法写入 dnsmasq 映射文件,域名规则暂时不会生效';
			return res;
		}
		// 先让 dnsmasq 自己检查语法再换上,坏文件会让 dnsmasq 起不来
		let t = run('dnsmasq --test -C ' + sh_quote(tmp));
		if (t.rc != 0) {
			fs.unlink(tmp);
			res.warn = 'dnsmasq 检查映射文件未通过,已放弃写入:' + clip(t.out, 200);
			return res;
		}
		if (!fs.rename(tmp, v.path)) {
			fs.unlink(tmp);
			res.warn = '无法替换 dnsmasq 映射文件,域名规则暂时不会生效';
			return res;
		}
		res.changed = true;
	}
	if (res.changed) {
		if (dnsmasq_restart())
			res.restarted = true;
		else
			res.warn = 'dnsmasq 重启失败,请手动检查 dnsmasq 服务';
	}
	return res;
}

// 关闭时清掉所有 conf-dir 里的映射文件;有文件被删才重启
function dns_remove() {
	let n = 0;
	for (let d in dns_dirs()) {
		let p = d + '/' + DNS_FILE;
		if (fs.stat(p) != null) {
			fs.unlink(p);
			n++;
		}
	}
	if (n > 0)
		dnsmasq_restart();
	return n > 0;
}

// ---------------------------------------------------------------- 输入:入口设备、应用库、配置/快照

// 入口设备 = src_zones 各区的网络对应的 l3_device(snapshot 里没有 lan_ifs,引擎自己解析);
// 快照里若直接带 lan_ifs 数组则以它为准(静态检查用)
function resolve_lan_ifs(cfg, snap, warns) {
	let out = [];
	let add = function(d) {
		d = trim('' + d);
		if (d != '' && !(d in out))
			push(out, d);
	};
	if (type(snap.lan_ifs) == 'array') {
		for (let d in snap.lan_ifs)
			add(d);
		return out;
	}
	let zones = (type(cfg.global.src_zones) == 'array') ? cfg.global.src_zones : [ 'lan' ];
	let nets = [];
	let cur = uci_cursor();
	if (cur != null) {
		cur.foreach('firewall', 'zone', function(z) {
			let zn = trim('' + (z.name ?? ''));
			if (!(zn in zones))
				return true;
			let any = false;
			for (let n in tokens_of(z.network)) {
				push(nets, n);
				any = true;
			}
			for (let d in tokens_of(z.device)) {
				add(d);
				any = true;
			}
			if (z.subnet != null)
				any = true;
			// 区里什么都没写时,fw4 按区名当作网络名
			if (!any)
				push(nets, zn);
			return true;
		});
	}
	if (length(nets) > 0) {
		let ub = ubus.connect();
		let dump = (ub != null) ? ub.call('network.interface', 'dump') : null;
		if (type(dump) == 'object' && type(dump.interface) == 'array') {
			for (let it in dump.interface) {
				if (type(it) == 'object' && (it.interface in nets))
					add(it.l3_device ?? it.device ?? '');
			}
		}
	}
	if (length(out) == 0 && fs.stat('/sys/class/net/br-lan') != null) {
		add('br-lan');
		push(warns, sprintf('没能从防火墙区域(%s)解析出入口设备,已回落为 br-lan', join('、', zones)));
	}
	return out;
}

// feature.cfg:`#class <key> <分类号> <名称>` 与 `<应用号> <名称>:[…]`,键一律是字符串
function load_apps() {
	let res = { available: false, ids: {}, classes: {} };
	let text = null;
	for (let p in [ '/etc/mwrt_behav/feature.cfg', '/usr/share/mwrt-behav/feature.cfg' ]) {
		text = fs.readfile(p);
		if (text != null)
			break;
	}
	if (text == null)
		return res;
	for (let ln in split(text, '\n')) {
		if (substr(ln, 0, 1) == '#') {
			let m = match(ln, /^#class[ \t]+[^ \t]+[ \t]+([0-9]+)[ \t]+/);
			if (m != null)
				res.classes[m[1]] = true;
			continue;
		}
		let m2 = match(ln, /^([0-9]+)[ \t]+[^:]+:/);
		if (m2 != null)
			res.ids[m2[1]] = true;
	}
	res.available = (length(res.ids) > 0);
	return res;
}

function read_input_file(path, what) {
	let t = fs.readfile(path);
	if (t == null)
		return { ok: false, error: sprintf('无法读取 %s 文件 %s', what, path) };
	let o = null;
	try {
		o = json(t);
	}
	catch (e) {
		return { ok: false, error: sprintf('%s 文件 %s 不是合法的 JSON', what, path) };
	}
	if (type(o) != 'object')
		return { ok: false, error: sprintf('%s 文件 %s 的内容必须是 JSON 对象', what, path) };
	return { ok: true, v: o };
}

// 汇总编译所需输入。cfg 可由调用方给出(引擎已读过配置);opts.input / opts.snapshot 用于静态检查,代替 UCI 与实时发现
function gather(opts, cfg0, write_cache) {
	opts = opts ?? {};
	let cm = lazy('config');
	let cfg = cfg0;
	if (cfg == null) {
		if (opts.input != null) {
			let f = read_input_file(opts.input, '--input');
			if (!f.ok)
				return f;
			cfg = cm.from_input(f.v);
		}
		else
			cfg = cm.load({});
	}
	let snap = null;
	if (opts.snapshot != null) {
		let f = read_input_file(opts.snapshot, '--snapshot');
		if (!f.ok)
			return f;
		snap = f.v;
		if (type(snap.wans) != 'array')
			return { ok: false, error: sprintf('--snapshot 文件 %s 缺少 wans 数组', opts.snapshot) };
	}
	else
		snap = lazy('wans').snapshot({ health: (opts.health == false) ? false : true });
	let warns = [];
	let n = 0;
	for (let e in (type(snap.errors) == 'array' ? snap.errors : [])) {
		if (n++ < 5)
			push(warns, '出口发现:' + e);
	}
	let lan_ifs = resolve_lan_ifs(cfg, snap, warns);
	return { ok: true, cfg: cfg, snap: snap, apps: load_apps(), dns_capable: dnsmasq_capable(write_cache),
		lan_ifs: lan_ifs, warnings: warns };
}

// existing 非 null 时 compile 才生成 nft_gc;prev_ids 让规则重排后出口编号不变
function compile_plan(g, prev, existing) {
	let prev_ids = {};
	if (prev != null && type(prev.exits) == 'array') {
		for (let e in prev.exits) {
			if (type(e) == 'object' && e.key != null && type(e.id) == 'int')
				prev_ids[e.key] = e.id;
		}
	}
	return lazy('compile').compile(g.cfg, g.snap, { lan_ifs: g.lan_ifs, apps: g.apps, dns_capable: g.dns_capable,
		now: now(), prev_ids: prev_ids, existing: existing });
}

// ---------------------------------------------------------------- 计数器累计偏移

function cj_pair(v) {
	return { pkts: as_int(v.pkts, 0), bytes: as_int(v.bytes, 0) };
}

function cj_load() {
	let o = read_json(F_COUNTERS, null);
	let cj = { ver: 1, since: 0, offsets: {}, last: {}, last_ts: 0 };
	if (type(o) != 'object')
		return cj;
	cj.since = as_int(o.since, 0);
	cj.last_ts = as_int(o.last_ts, 0);
	for (let sect in [ 'offsets', 'last' ]) {
		if (type(o[sect]) == 'object') {
			for (let k, v in o[sect]) {
				if (type(v) == 'object')
					cj[sect][k] = cj_pair(v);
			}
		}
	}
	return cj;
}

function cj_save(cj) {
	if (cj.since <= 0)
		cj.since = now();
	return write_json_atomic(F_COUNTERS, cj);
}

// 表要被重建/删除之前,把当前内核计数并入偏移,累计值才不会随计数器清零而倒退。调用方在动作成功后再调用
function cj_fold(live_vals) {
	let cj = cj_load();
	for (let name, v in live_vals) {
		let o = cj.offsets[name] ?? { pkts: 0, bytes: 0 };
		o.pkts += v.pkts;
		o.bytes += v.bytes;
		cj.offsets[name] = o;
	}
	cj.last = {};
	cj_save(cj);
}

// 应用成功后:删掉已不存在的规则的偏移;重建时并入旧计数
function cj_after_apply(live, rebuild, names) {
	let cj = cj_load();
	if (rebuild && live.exists) {
		for (let name, v in live.cvals) {
			let o = cj.offsets[name] ?? { pkts: 0, bytes: 0 };
			o.pkts += v.pkts;
			o.bytes += v.bytes;
			cj.offsets[name] = o;
		}
	}
	let keep = {};
	for (let n in names)
		keep[n] = true;
	for (let name in keys(cj.offsets)) {
		if (!keep[name])
			delete cj.offsets[name];
	}
	cj.last = {};
	cj_save(cj);
}

// 计数器名 → 规则编号(用上次应用的状态,状态没有时按名字去掉 c_ 前缀)
function counter_names(st) {
	let map = {};
	if (st != null && type(st.rules) == 'array') {
		for (let r in st.rules) {
			if (type(r) == 'object' && r.id != null && r.status == 'active')
				map['c_' + lc('' + r.id)] = '' + r.id;
		}
	}
	return map;
}

function cnt_totals(cj, live) {
	let tot = {};
	for (let name, o in cj.offsets) {
		let lv = (live != null && live[name] != null) ? live[name] : { pkts: 0, bytes: 0 };
		tot[name] = { pkts: (o.pkts + lv.pkts > 0) ? o.pkts + lv.pkts : 0, bytes: (o.bytes + lv.bytes > 0) ? o.bytes + lv.bytes : 0 };
	}
	if (live != null) {
		for (let name, lv in live) {
			if (tot[name] == null)
				tot[name] = { pkts: lv.pkts, bytes: lv.bytes };
		}
	}
	return tot;
}

// 状态文件里的命中数是应用时的快照;表在时把生效规则的命中数换成实时累计值(内核计数 + 跨重建偏移)
function with_live_hits(rules, cl) {
	if (cl == null || type(rules) != 'array')
		return rules;
	let tot = cnt_totals(cj_load(), cl);
	let out = [];
	for (let r in rules) {
		let x = {};
		if (type(r) == 'object') {
			for (let k, v in r)
				x[k] = v;
			let t = tot['c_' + lc('' + r.id)];
			if (r.status == 'active' && t != null)
				x.hits = t;
		}
		push(out, x);
	}
	return out;
}

// ---------------------------------------------------------------- 状态文件

function state_exits(plan) {
	let out = [];
	for (let e in plan.exits) {
		let ms = [];
		for (let m in e.members)
			push(ms, { wan_id: m.wan_id, dev: m.dev, usable: (m.usable == true), reason: m.reason ?? '' });
		push(out, { id: e.id, key: e.key, target: e.target, on_fail: e.on_fail, table: e.table, mark: e.mark,
			members: ms, v4: e.v4, v6: e.v6, v6_reason: e.v6_reason });
	}
	return out;
}

function health_source(snap) {
	for (let w in (type(snap.wans) == 'array' ? snap.wans : [])) {
		if (type(w) == 'object' && type(w.health) == 'object' && w.health.source == 'probe')
			return 'probe';
	}
	return 'none';
}

function build_state(plan, g, prev, applied_ts, totals, warns) {
	let rules = [];
	for (let r in plan.rules) {
		let h = { pkts: 0, bytes: 0 };
		if (r.counter != null && totals != null && totals[r.counter] != null)
			h = totals[r.counter];
		else if (prev != null && type(prev.rules) == 'array') {
			for (let pr in prev.rules) {
				if (type(pr) == 'object' && pr.id == r.id && type(pr.hits) == 'object')
					h = pr.hits;
			}
		}
		push(rules, { id: r.id, name: r.name, status: r.status, reason: r.reason, exit: r.exit, hits: h });
	}
	let all = [];
	for (let w in plan.warnings)
		push(all, w);
	for (let w in g.warnings)
		push(all, w);
	for (let w in warns)
		push(all, w);
	return {
		ver: 1, ts: now(), applied_ts: applied_ts, enabled: true, ok: true, error: '', failed_open: false,
		nft_hash: plan.hash, nft_sig: plan.nft_sig, dns_sig: plan.dns.sig, dns_domains: plan.dns.domains,
		lan_ifs: g.lan_ifs, n_ip_rules: length(plan.ip_rules), health_source: health_source(g.snap),
		exits: state_exits(plan), rules: rules, warnings: all
	};
}

// 比较两份状态是否"实质相同":时间戳与命中数每次都在变,不该触发写文件
function state_sig(st) {
	let c = {};
	for (let k, v in st) {
		if (k == 'ts')
			continue;
		if (k == 'rules' && type(v) == 'array') {
			let rr = [];
			for (let r in v) {
				let x = {};
				if (type(r) == 'object') {
					for (let kk, vv in r) {
						if (kk != 'hits')
							x[kk] = vv;
					}
				}
				push(rr, x);
			}
			c.rules = rr;
		}
		else
			c[k] = v;
	}
	return sprintf('%J', c);
}

function applied_ts_for(prev, structural) {
	let t = now();
	let cm = cfg_mtime();
	let p = (prev != null) ? as_int(prev.applied_ts, 0) : 0;
	if (structural || cm > p)
		return (cm > t) ? cm : t;
	return p;
}

// 记录"本次失败、保持上一次有效状态不动"。没有上一次状态时写一份未启用的最小状态,让界面能显示错误
function state_fail(prev, errtext, plan_hash, failed_open) {
	let st = null;
	if (prev != null) {
		st = {};
		for (let k, v in prev)
			st[k] = v;
	}
	else
		st = { ver: 1, applied_ts: 0, enabled: false, nft_hash: '', nft_sig: '', dns_sig: '', dns_domains: 0, lan_ifs: [],
			n_ip_rules: 0, health_source: 'none', exits: [], rules: [], warnings: [] };
	st.ts = now();
	st.ok = false;
	st.error = errtext;
	st.fail_hash = plan_hash ?? '';
	st.failed_open = (failed_open == true);
	if (failed_open == true) {
		st.enabled = true;
		st.nft_hash = '';
	}
	write_json_atomic(F_STATE, st);
	return st;
}

function log_failure(prev, errtext) {
	if (prev == null || prev.error != errtext)
		log('err', '策略路由应用失败:' + errtext);
}

function log_diff(prev, st) {
	let oldm = {};
	let oldr = {};
	if (prev != null) {
		for (let e in (type(prev.exits) == 'array' ? prev.exits : [])) {
			for (let m in (type(e.members) == 'array' ? e.members : []))
				oldm[sprintf('%s|%s', e.key, m.wan_id)] = m.usable;
		}
		for (let r in (type(prev.rules) == 'array' ? prev.rules : [])) {
			if (type(r) == 'object')
				oldr['' + r.id] = sprintf('%s|%s', r.status, r.reason);
		}
	}
	for (let e in st.exits) {
		for (let m in e.members) {
			let k = sprintf('%s|%s', e.key, m.wan_id);
			if (oldm[k] != null && oldm[k] != m.usable)
				log('notice', sprintf('出口 %s 的成员 %s(%s)%s%s', e.key, m.wan_id, m.dev, m.usable ? '恢复可用' : '不可用',
					(m.reason != '') ? ':' + m.reason : ''));
		}
	}
	for (let r in st.rules) {
		if (r.status == 'error' && oldr['' + r.id] != sprintf('error|%s', r.reason))
			log('warn', sprintf('规则 %s 未生效:%s', (r.name != '') ? r.name : r.id, r.reason));
	}
}

function result_of(st, changed, extra) {
	let act = 0;
	let err = 0;
	for (let r in st.rules) {
		if (r.status == 'active')
			act++;
		else if (r.status == 'error')
			err++;
	}
	let res = { ok: (st.ok == true), changed: changed, rules_active: act, rules_error: err, exits: length(st.exits),
		warnings: st.warnings ?? [], failed_open: (st.failed_open == true) };
	if (extra != null) {
		for (let k, v in extra)
			res[k] = v;
	}
	return res;
}

// ---------------------------------------------------------------- 关闭

function hnat_clear() {
	if (fs.stat(HNAT_ENTRY) == null)
		return false;
	// 与 s275 的 mwadv-apply 写法一致:每条命令单独打开、不带换行;PPE2(7)在 MT7987 上是否存在未验证
	let n = 0;
	for (let c in [ '3 -1', '5 -1', '7 -1' ]) {
		let h = fs.open(HNAT_ENTRY, 'w');
		if (h != null) {
			h.write(c);
			h.close();
			n++;
		}
	}
	return n > 0;
}

// 关闭的实际动作:删 ip rule、清出口表、删 nft 表、删 dnsmasq 映射、写"未启用"状态。
// o.failed_open 表示应用失败后的兜底关闭:状态里 enabled 仍为 true,并记录失败原因
function flush_core(prev, o) {
	o = o ?? {};
	let changed = false;
	let live = nft_read();
	let kr = rules_kernel();
	for (let r in kr) {
		run(rule_cmd('del', r));
		changed = true;
	}
	for (let t in tables_known(null, prev, kr)) {
		if (table_flush(t) > 0)
			changed = true;
	}
	if (live.exists) {
		let d = run('nft delete table ' + TBL);
		if (d.rc == 0) {
			cj_fold(live.cvals);
			changed = true;
		}
	}
	if (dns_remove())
		changed = true;
	fs.unlink(F_GOOD_NFT);
	fs.unlink(F_GOOD_RULES);
	let st = {
		ver: 1, ts: now(), applied_ts: now(), enabled: (o.failed_open == true), ok: (o.failed_open != true),
		error: o.error ?? '', failed_open: (o.failed_open == true), nft_hash: '', nft_sig: '', dns_sig: '', dns_domains: 0,
		lan_ifs: [], n_ip_rules: 0, health_source: (prev != null) ? (prev.health_source ?? 'none') : 'none',
		exits: [], rules: [], warnings: []
	};
	if (o.failed_open == true)
		st.fail_hash = o.fail_hash ?? '';
	let was_on = (prev != null && prev.enabled == true);
	write_json_atomic(F_STATE, st);
	if (changed || was_on) {
		if (o.failed_open != true)
			log('info', '策略路由已关闭:ip rule、出口表路由、nft 表与 dnsmasq 映射已清理');
	}
	return result_of(st, changed || was_on, null);
}

function flush(opts) {
	return with_lock(function() {
		return flush_core(state_read(), {});
	});
}

// ---------------------------------------------------------------- 应用 / 对账

function undo_rules(added) {
	for (let r in added)
		run(rule_cmd('del', r));
}

// 应用失败的善后:撤销本次新加的 ip rule,按上次有效配置还原出口表路由并重放 last-good.nft;
// 再失败(或从未有过有效配置)就整体关闭,宁可走默认路由也不黑洞
function recover(prev, added, errtext, plan_hash, tables) {
	undo_rules(added);
	let good = read_json(F_GOOD_RULES, null);
	let ok = false;
	if (type(good) == 'object' && fs.stat(F_GOOD_NFT) != null) {
		routes_sync(type(good.routes) == 'array' ? good.routes : [], tables);
		ok = nft_load_file(F_GOOD_NFT).ok;
	}
	if (ok) {
		let st = state_fail(prev, '规则应用失败,已恢复到上一次有效配置:' + errtext, plan_hash, false);
		log_failure(prev, st.error);
		return result_of(st, false, { error: st.error, restored: true });
	}
	let e2 = '规则应用失败,且无法恢复上一次配置,已整体关闭(出口表与规则已清理,流量走系统默认路由):' + errtext;
	let r = flush_core(prev, { failed_open: true, error: e2, fail_hash: plan_hash });
	log('err', '策略路由应用失败并已整体关闭:' + errtext);
	r.ok = false;
	r.error = e2;
	r.failed_open = true;
	return r;
}

function objects_present(objs, live) {
	let have = {};
	for (let c in live.chains)
		have['c|' + c] = true;
	for (let s in live.sets)
		have['s|' + s] = true;
	for (let n in live.counters)
		have['n|' + n] = true;
	for (let c in objs.chains) {
		if (!have['c|' + c])
			return false;
	}
	for (let s in objs.sets) {
		if (!have['s|' + s])
			return false;
	}
	for (let n in objs.counters) {
		if (!have['n|' + n])
			return false;
	}
	return true;
}

// 完整应用。流程见 SPEC §5.2;与规格的差别(记入报告):删多余 ip rule/旧出口表放在 nft 事务成功之后,
// 这样事务失败时只需撤销"新加的",不会留下半拆的状态
function apply_plan(plan, g, prev, live, kr, df, opts) {
	ensure_dir(RUN_DIR);
	let pst = (prev != null) ? prev : {};
	let tables = tables_known(plan, prev, kr);
	let rebuild = (live.exists && plan.nft_sig != (pst.nft_sig ?? ''));
	let text = rebuild ? plan.nft_rebuild : plan.nft;
	if (!write_atomic(F_CAND, text)) {
		let e0 = '无法写入候选规则文件 ' + F_CAND;
		log_failure(prev, e0);
		let st0 = state_fail(prev, e0, plan.hash, false);
		return result_of(st0, false, { error: e0 });
	}
	let chk = nft_check_file(F_CAND);
	if (!chk.ok) {
		let e1 = '规则语法检查未通过,已保持上一次状态:' + chk.error;
		log_failure(prev, e1);
		let st1 = state_fail(prev, e1, plan.hash, false);
		return result_of(st1, false, { error: e1 });
	}

	// 阶段 A:先加新的 ip rule、再同步出口表路由,nft 事务之前路由就位
	let added = [];
	for (let r in df.add) {
		if (!rule_valid(r)) {
			undo_rules(added);
			let e2 = sprintf('ip rule 参数无效(%s),已保持上一次状态', r.spec);
			let st2 = state_fail(prev, e2, plan.hash, false);
			log_failure(prev, e2);
			return result_of(st2, false, { error: e2 });
		}
		let ar = run(rule_cmd('add', r));
		if (ar.rc != 0) {
			undo_rules(added);
			let e3 = 'ip rule 添加失败,已保持上一次状态:' + clip(ar.out, 300);
			let st3 = state_fail(prev, e3, plan.hash, false);
			log_failure(prev, e3);
			return result_of(st3, false, { error: e3 });
		}
		push(added, r);
	}
	let rs = routes_sync(plan.routes, tables);

	// 阶段 B:nft 事务
	let nr = nft_load_file(F_CAND);
	if (!nr.ok)
		return recover(prev, added, nr.error, plan.hash, tables);

	// 成功之后:删多余 ip rule、清已消失出口的路由表、回收孤儿对象、DNS、HNAT、last-good、计数偏移、状态
	let warns = [];
	for (let e in rs.errors)
		push(warns, e);
	for (let r in df.del) {
		let dr = run(rule_cmd('del', r));
		if (dr.rc != 0)
			push(warns, '删除多余的 ip rule 失败:' + clip(dr.out, 200));
	}
	let in_plan = {};
	for (let e in plan.exits)
		in_plan['' + e.table] = true;
	for (let t in tables) {
		if (!in_plan['' + t])
			table_flush(t);
	}
	if (plan.nft_gc != '' && !rebuild) {
		if (write_atomic(F_CAND_GC, plan.nft_gc)) {
			let gr = nft_load_file(F_CAND_GC);
			if (!gr.ok)
				push(warns, '回收孤儿规则对象失败(不影响当前规则):' + gr.error);
		}
	}
	let dres = dns_sync(plan);
	if (dres.warn != '')
		push(warns, dres.warn);
	let structural_flush = false;
	// 计划、ip rule 都没变的重复 apply(例如无关的 network/firewall 提交)不清 HNAT
	let plan_changed = !(prev != null && prev.ok == true && prev.enabled == true && prev.failed_open != true
		&& prev.nft_hash == plan.hash && length(df.add) == 0 && length(df.del) == 0);
	if (g.cfg.global.flush_accel == true && plan_changed) {
		structural_flush = hnat_clear();
		if (structural_flush)
			log('info', '已按设置清除硬件加速(HNAT)表项');
	}
	write_atomic(F_GOOD_NFT, plan.nft);
	write_json_atomic(F_GOOD_RULES, { ts: now(), hash: plan.hash, nft_sig: plan.nft_sig, ip_rules: plan.ip_rules, routes: plan.routes });
	cj_after_apply(live, rebuild, plan.objects.counters);
	let totals = cnt_live();
	let cj = cj_load();
	totals = cnt_totals(cj, totals);
	let st = build_state(plan, g, prev, applied_ts_for(prev, true), totals, warns);
	write_json_atomic(F_STATE, st);

	let act = 0;
	let err = 0;
	for (let r in st.rules) {
		if (r.status == 'active')
			act++;
		else if (r.status == 'error')
			err++;
	}
	log('info', sprintf('策略路由已应用:规则生效 %d 条、有误 %d 条,出口 %d 个%s%s', act, err, length(st.exits),
		rebuild ? '(集合属性变化,已重建表)' : '', dres.restarted ? ',dnsmasq 已重启' : ''));
	log_diff(prev, st);
	return result_of(st, true, null);
}

// 结构没变:只同步出口表路由并刷新状态(无变化时不写文件)
function sync_only(plan, g, prev, kr, opts) {
	let tables = tables_known(plan, prev, kr);
	let rs = routes_sync(plan.routes, tables);
	let in_plan = {};
	for (let e in plan.exits)
		in_plan['' + e.table] = true;
	let flushed = 0;
	for (let t in tables) {
		if (!in_plan['' + t])
			flushed += table_flush(t);
	}
	let st = build_state(plan, g, prev, applied_ts_for(prev, false), null, rs.errors);
	let differs = (state_sig(st) != state_sig(prev));
	if (differs) {
		write_json_atomic(F_STATE, st);
		log_diff(prev, st);
	}
	return result_of(st, (rs.changed > 0 || flushed > 0 || differs), null);
}

// apply 与 reconcile 的共同入口,调用方已持有引擎锁
function engine(mode, opts) {
	opts = opts ?? {};
	let cfg = mode == 'apply' ? lazy('config').load({}) :
		read_json(F_APPLIED, { global: { enabled: false } });
	let prev = state_read();
	if (cfg.global.enabled != true) {
		if (mode == 'reconcile' && prev != null && prev.enabled == false)
			return { ok: true, changed: false, rules_active: 0, rules_error: 0, exits: 0, warnings: [], failed_open: false, disabled: true };
		let res = flush_core(prev, {});
		if (res.ok && mode == 'apply')
			write_json_atomic(F_APPLIED, cfg);
		return res;
	}
	ensure_dir(RUN_DIR);
	let g = gather(opts, cfg, true);
	if (!g.ok)
		return g;
	let live = nft_read();
	let plan = compile_plan(g, prev, { chains: live.chains, sets: live.sets, counters: live.counters });
	if (!plan.ok) {
		let e = '编译失败:' + join(';', plan.errors);
		log_failure(prev, e);
		let st = state_fail(prev, e, plan.hash, false);
		return result_of(st, false, { error: e });
	}
	let pst = (prev != null) ? prev : {};
	// 同一份计划刚失败过:对账不反复重试(每分钟一次会让规则来回抖动),等配置/出口变化或过了一段时间再试;显式 apply 总是重试
	if (mode == 'reconcile' && pst.ok == false && (pst.fail_hash ?? '') == plan.hash && (now() - as_int(pst.ts, 0)) < RETRY_SECS)
		return result_of(pst, false, { error: pst.error ?? '', skipped: true });

	let kr = rules_kernel();
	let df = rules_diff(plan.ip_rules, kr);
	if (mode == 'reconcile' && prev != null && pst.ok == true && pst.enabled == true && pst.failed_open != true
		&& pst.nft_hash == plan.hash && live.exists && objects_present(plan.objects, live)
		&& length(df.add) == 0 && length(df.del) == 0 && dns_view(plan).same)
		return sync_only(plan, g, prev, kr, opts);
	let res = apply_plan(plan, g, prev, live, kr, df, opts);
	if (res.ok && mode == 'apply')
		write_json_atomic(F_APPLIED, cfg);
	return res;
}

function reconcile(opts) {
	return with_lock(function() {
		return engine('reconcile', opts ?? {});
	});
}

// ---------------------------------------------------------------- 预览(干跑,绝不写系统状态)

function preview(opts) {
	opts = opts ?? {};
	let g = null;
	let plan = null;
	let prev = null;
	try {
		g = gather(opts, null, false);
		if (!g.ok)
			return g;
		let static_in = (opts.input != null || opts.snapshot != null);
		prev = (opts.input == null) ? state_read() : null;
		let existing = null;
		if (!static_in) {
			let live = nft_read();
			existing = { chains: live.chains, sets: live.sets, counters: live.counters };
		}
		plan = compile_plan(g, prev, existing);
	}
	catch (e) {
		return { ok: false, error: '内部错误: ' + msg_of(e) };
	}
	let warns = [];
	for (let w in plan.warnings)
		push(warns, w);
	for (let w in g.warnings)
		push(warns, w);
	if (g.cfg.global.enabled != true)
		push(warns, '总开关当前是关闭的:真正应用时会执行"关闭"(清理),不会下发下面的计划');
	let ipc = [];
	for (let r in plan.ip_rules) {
		if (rule_valid(r))
			push(ipc, rule_cmd('add', r));
	}
	let rtc = [];
	for (let r in plan.routes) {
		if (route_valid(r, r.table))
			push(rtc, route_cmd('replace', r.table, r));
	}
	let res = {
		ok: (plan.ok == true), enabled: (g.cfg.global.enabled == true), nft: plan.nft, nft_gc: plan.nft_gc, nft_rebuild: plan.nft_rebuild,
		nft_sig: plan.nft_sig, hash: plan.hash, ip_rules: plan.ip_rules, routes: plan.routes, dns: plan.dns, rules: plan.rules,
		exits: plan.exits, lan_ifs: g.lan_ifs, ip_cmds: ipc, route_cmds: rtc, warnings: warns,
		nft_check: { ok: null, error: '' }
	};
	if (plan.ok != true) {
		res.error = '编译失败:' + join(';', plan.errors);
		return res;
	}
	res.nft_check = nft_check_text(plan.nft);
	return res;
}

// 函数不会提升,apply 要放在它调用的 preview 之后
function apply(opts) {
	opts = opts ?? {};
	if (opts.dry_run == true)
		return preview(opts);
	return with_lock(function() {
		return engine('apply', opts);
	});
}

// ---------------------------------------------------------------- 状态 / 计数器

function pid_alive(pid) {
	return type(pid) == 'int' && pid > 0 && fs.stat('/proc/' + pid) != null;
}

function status() {
	let st = state_read();
	let live = { table: false, ip_rules: { want: 0, have: 0 }, dnsmasq_map: false, wanmon: false, cron: false };
	// 一次 nft 调用同时拿到"表是否存在"和各规则的实时命中数
	let cl = cnt_live();
	live.table = (cl != null);
	live.ip_rules.have = length(rules_kernel());
	if (st != null)
		live.ip_rules.want = as_int(st.n_ip_rules, 0);
	for (let d in dns_dirs()) {
		let t = fs.stat(d + '/' + DNS_FILE);
		if (t != null && t.size > 0) {
			live.dnsmasq_map = true;
			break;
		}
	}
	let wm = read_json(F_WANMON, null);
	if (type(wm) == 'object') {
		let iv = as_int(wm.interval, 10);
		if (iv < 1)
			iv = 10;
		let d = now() - as_int(wm.ts, 0);
		if (d < 0)
			d = -d;
		live.wanmon = pid_alive(as_int(wm.pid, 0)) && d <= 3 * iv;
	}
	let cron = fs.readfile('/etc/crontabs/root');
	live.cron = (cron != null && index(cron, 'mwrt-pbr reconcile') >= 0);
	let res = null;
	if (st == null) {
		res = { ok: true, enabled: false, never_applied: true };
	}
	else {
		res = {};
		for (let k, v in st)
			res[k] = v;
		// 状态文件里的 ok 表示"上次应用是否成功",直接透传;请求本身成功与否看是否拿到 JSON
		if (type(res.ok) != 'bool')
			res.ok = true;
		res.rules = with_live_hits(res.rules, cl);
	}
	res.live = live;
	return res;
}

// 计算累计值;锁拿得到时顺手把"最近一次内核计数"落盘(至多每 60 秒一次),这样表被外部重建后偏移还能补上
function counters(opts) {
	let st = state_read();
	let live = cnt_live();
	let fh = lock_take(300);
	let cj = cj_load();
	let need_save = false;
	if (live != null) {
		for (let name, v in live) {
			let l = cj.last[name];
			if (l != null && (v.pkts < l.pkts || v.bytes < l.bytes)) {
				let o = cj.offsets[name] ?? { pkts: 0, bytes: 0 };
				o.pkts += l.pkts;
				o.bytes += l.bytes;
				cj.offsets[name] = o;
				need_save = true;
			}
		}
		let nl = {};
		for (let name, v in live)
			nl[name] = { pkts: v.pkts, bytes: v.bytes };
		cj.last = nl;
		if (now() - cj.last_ts >= 60)
			need_save = true;
	}
	let since = (cj.since > 0) ? cj.since : as_int((st != null) ? st.applied_ts : 0, 0);
	if (fh != null) {
		if (need_save) {
			cj.last_ts = now();
			ensure_dir(RUN_DIR);
			cj_save(cj);
		}
		fh.lock('u');
		fh.close();
	}
	let tot = cnt_totals(cj, live);
	let rules = {};
	let names = counter_names(st);
	if (st != null) {
		for (let name, rid in names)
			rules[rid] = tot[name] ?? { pkts: 0, bytes: 0 };
	}
	else {
		for (let name, v in tot) {
			let m = match(name, /^c_(.+)$/);
			if (m != null)
				rules[m[1]] = v;
		}
	}
	return { ok: true, ts: now(), since: since, table: (live != null), rules: rules };
}

function counters_reset() {
	return with_lock(function() {
		ensure_dir(RUN_DIR);
		let live = cnt_live();
		let cj = cj_load();
		cj.offsets = {};
		cj.last = {};
		if (live != null) {
			for (let name, v in live) {
				cj.offsets[name] = { pkts: -v.pkts, bytes: -v.bytes };
				cj.last[name] = { pkts: v.pkts, bytes: v.bytes };
			}
		}
		cj.since = now();
		cj.last_ts = now();
		if (!cj_save(cj))
			return { ok: false, error: '无法写入计数器状态文件 ' + F_COUNTERS };
		return { ok: true, since: cj.since };
	});
}

// ---------------------------------------------------------------- 去抖触发

// 写脏标记并在没有 worker 时后台拉起一个;全程不做重活,供 hotplug 调用(要求 100ms 内返回)
function kick() {
	ensure_dir(RUN_DIR);
	let tok = trim(fs.readfile('/proc/sys/kernel/random/uuid') ?? '');
	if (tok == '')
		tok = sprintf('%d', time());
	if (!write_atomic(F_DIRTY, tok + '\n'))
		return { ok: false, error: '无法写入去抖标记 ' + F_DIRTY };
	let started = false;
	let fh = fs.open(F_KICK_LOCK, 'a');
	if (fh != null) {
		if (fh.lock('xn') == true) {
			// 锁是空的说明没有 worker;放掉并关闭后再拉起,worker 自己去抢锁,多拉起的会因抢不到而退出
			fh.lock('u');
			fh.close();
			let cmd = 'if command -v setsid >/dev/null 2>&1; then setsid ' + CLI + ' kick-worker >/dev/null 2>&1 </dev/null & '
				+ 'else ' + CLI + ' kick-worker >/dev/null 2>&1 </dev/null & fi';
			system([ '/bin/sh', '-c', cmd ], 2000);
			started = true;
		}
		else
			fh.close();
	}
	return { ok: true, started: started };
}

// 去抖 worker:持有 kick.lock(保证唯一);静默满 2 秒(最长等 30 秒)后清标记并对账,期间又被 kick 就再来一轮
function kick_worker() {
	ensure_dir(RUN_DIR);
	let fh = fs.open(F_KICK_LOCK, 'a');
	if (fh == null)
		return { ok: false, error: '无法打开去抖锁文件 ' + F_KICK_LOCK };
	if (fh.lock('xn') != true) {
		fh.close();
		return { ok: true, started: false };
	}
	ENG.kick_fd = fh.fileno();
	let rounds = 0;
	while (rounds < 1000) {
		let tok = fs.readfile(F_DIRTY);
		if (tok == null) {
			// 收尾:放锁后再看一眼标记,堵住"kick 写完标记时锁还被我占着"的空档
			fh.lock('u');
			if (fs.stat(F_DIRTY) == null)
				break;
			if (fh.lock('xn') != true)
				break;
			continue;
		}
		let quiet = 0;
		let ticks = 0;
		while (quiet < 2 && ticks < 30) {
			sleep(1000);
			ticks++;
			let t2 = fs.readfile(F_DIRTY);
			if (t2 == null)
				break;
			if (t2 == tok)
				quiet++;
			else {
				tok = t2;
				quiet = 0;
			}
		}
		fs.unlink(F_DIRTY);
		if (fs.stat(F_STOPPED) != null)
			continue;
		let r = reconcile({ quiet: true });
		// 没抢到引擎锁就把标记写回去,下一轮再试
		if (type(r) == 'object' && r.busy == true)
			write_atomic(F_DIRTY, tok);
		rounds++;
	}
	fh.close();
	return { ok: true, started: true, rounds: rounds };
}

// ---------------------------------------------------------------- 校验(stdin)

function validate_stdin() {
	let raw = fs.stdin.read('all');
	if (raw == null || trim(raw) == '')
		return { ok: false, error: '标准输入为空,需要 {"rule":{…}} 或 {"group":{…}}' };
	let o = null;
	try {
		o = json(raw);
	}
	catch (e) {
		return { ok: false, error: '标准输入不是合法的 JSON' };
	}
	if (type(o) != 'object' || (type(o.rule) != 'object' && type(o.group) != 'object'))
		return { ok: false, error: '标准输入必须是 {"rule":{…}} 或 {"group":{…}}' };
	let vm = lazy('validate');
	let cfg = lazy('config').load({});
	let snap = lazy('wans').snapshot({ health: false });
	let ctx = vm.build_ctx(cfg, snap, load_apps(), { dns_capable: dnsmasq_capable(false) });
	ctx.self_id = (o.id != null && trim('' + o.id) != '') ? '' + o.id : null;
	if (type(o.rule) == 'object') {
		let r = vm.validate_rule(o.rule, ctx);
		return { ok: r.ok, errors: r.errors, warnings: r.warnings, normalized: r.normalized, shadow: r.shadow };
	}
	let gr = vm.validate_group(o.group, ctx);
	return { ok: gr.ok, errors: gr.errors, warnings: gr.warnings, normalized: gr.normalized };
}

// ---------------------------------------------------------------- diag / trace

function diag_opts() {
	let g = gather({}, null, false);
	if (!g.ok)
		return g;
	let prev = state_read();
	let plan = compile_plan(g, prev, null);
	let st = {};
	if (prev != null) {
		for (let k, v in prev)
			st[k] = v;
		// 诊断里"没有命中过的规则"要看实时计数,不能用应用时的快照
		st.rules = with_live_hits(st.rules, cnt_live());
	}
	return { ok: true, o: { cfg: g.cfg, snap: g.snap, plan: plan, state: st, apps: g.apps,
		dns_capable: g.dns_capable, lan_ifs: g.lan_ifs } };
}

function diag() {
	let d = diag_opts();
	if (!d.ok)
		return d;
	let m = null;
	try {
		m = lazy('diag');
	}
	catch (e) {
		return { ok: false, error: '诊断模块不可用: ' + msg_of(e) };
	}
	return m.run_diag(d.o);
}

function trace(params) {
	let d = diag_opts();
	if (!d.ok)
		return d;
	let m = null;
	try {
		m = lazy('trace');
	}
	catch (e) {
		return { ok: false, error: '试算模块不可用: ' + msg_of(e) };
	}
	return m.run_trace(params ?? {}, d.o);
}

export { apply, reconcile, flush, status, counters, counters_reset, kick, kick_worker, validate_stdin, preview, diag, trace };
