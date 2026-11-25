'use strict';
// 出口(WAN)发现:把 uci、ubus、sysfs、内核路由整理成统一快照,键名见 SPEC §2.4 / §13.5。
// 只读系统状态;只有 set_* 函数会写 UCI。模块顶层只放常量与函数,每次调用用独立的 ctx,不留模块级可变状态。

import * as fs from 'fs';
import * as ubus from 'ubus';
// s297c 只认 C8 三机型(board_name 含 c8-668/c8-688, 或 model 含 c5800), 其余机型返回空
let _xfer = null;
function xfer_dev() {
	if (_xfer == null) {
		let b = lc(trim(fs.readfile('/tmp/sysinfo/board_name') ?? ''));
		let m = lc(trim(fs.readfile('/tmp/sysinfo/model') ?? ''));
		_xfer = (index(b, 'c8-668') >= 0 || index(b, 'c8-688') >= 0 || index(m, 'c5800') >= 0) ? 'eth1' : '';
	}
	return _xfer;
}

import { run, sh_quote, read_json, write_json_atomic, now, uci_cursor, wan_section, RUN_DIR } from 'mwrt.lib';

const SNAP_VER = 1;
const HEALTH_FILE = '/tmp/mwrt-wan.json';
const HEALTH_INTERVAL = 10;
const USBNIC_DIR = '/tmp/mwrt-usbnic.d';
const USBINFO = '/usr/libexec/mwrt-usbinfo';
const SYS_NET = '/sys/class/net';
const ASSURANCE = 'Mwrt-Assurance';
const FAILOVER_SEC = 'wan_failover';
const PBR_CFG = 'mwrt_pbr';
// 回落标识(if:<接口>)到上次读到的真实 wan_id 的映射,放 tmpfs 不写 flash
const WID_CACHE = RUN_DIR + '/wanid.cache';

const KIND_LABEL = {
	wired: '有线', cellular: '蜂窝', usbnic: 'USB 网卡', tether: '手机共享', wlbridge: '无线网桥',
	pppoe: 'PPPoE', tunnel: '隧道', wireless: '无线', other: '其他'
};

// §6.3 默认序的类别序:有线/PPPoE 0 < USB 网卡/手机共享 1 < 无线网桥 2 < 蜂窝 3 < 其他 4
const KIND_CLASS = {
	wired: 0, pppoe: 0, usbnic: 1, tether: 1, wlbridge: 2, cellular: 3, tunnel: 4, wireless: 4, other: 4
};

// IPv6 配套逻辑接口(与同设备上的 IPv4 接口合并成一个出口)
const V6_PROTOS = ['dhcpv6', '6in4', '6to4', '6rd'];

const TUNNEL_PROTOS = [
	'wireguard', 'gre', 'gretap', 'grev6', 'grev6tap', 'vti', 'vti6', 'vxlan', 'ipip', 'sit',
	'l2tp', 'pptp', 'openvpn', '6in4', '6to4', '6rd', 'dslite'
];

// 这些协议本身就是蜂窝拨号协议(SPEC §2.2 第 1 条的扩展:没有 modem_config 的机型)
const CELL_PROTOS = ['qmi', 'mbim', 'ncm', '3g', 'modemmanager', 'directip', 'hso'];

const CELL_DRIVERS = ['qmi_wwan', 'cdc_mbim', 'huawei_cdc_ncm', 'option', 'qcserial', 'usb_wwan', 'mhi_net', 'mtk_t7xx'];

// sysfs type:ipip/gre/sit/ip6gre 等隧道设备;65534(ARPHRD_NONE)只在无 device 链接的虚拟设备上才算隧道,
// 因为 qmi_wwan 的 raw-ip 网卡也是 65534
const TUNNEL_TYPES = [768, 769, 776, 778, 823];

// ---------- 小工具 ----------

function is_obj(v) {
	return type(v) == 'object';
}

function as_list(v) {
	if (v == null)
		return [];
	if (type(v) == 'array')
		return v;
	return [v];
}

function as_str(v) {
	return (v == null) ? '' : sprintf('%s', v);
}

function first_str(v) {
	let l = as_list(v);
	if (length(l) == 0 || l[0] == null)
		return '';
	return as_str(l[0]);
}

// uci option 可能是 "eth0 eth1" 这种空白分隔,取第一个词
function first_word(v) {
	let p = split(trim(first_str(v)), /[ \t]+/);
	return (length(p) > 0) ? p[0] : '';
}

// fw4 的 option network/device 允许空白分隔多个名字,展平成单个名字的数组
function split_words(v) {
	let out = [];
	for (let item in as_list(v)) {
		for (let w in split(trim(as_str(item)), /[ \t]+/)) {
			if (w != '')
				push(out, w);
		}
	}
	return out;
}

function has(list, v) {
	return index(list, v) >= 0;
}

// UCI 布尔:0/false/off/no/空 为假,其余为真
function is_true(v) {
	if (v == null)
		return false;
	let s = lc(sprintf('%s', v));
	return !(s == '' || s == '0' || s == 'false' || s == 'off' || s == 'no');
}

function to_int(v, d) {
	if (v == null)
		return d;
	let n = int(v);
	if (n != n)
		return d;
	return n;
}

function rd(path) {
	let s = fs.readfile(path);
	if (s == null)
		return null;
	return trim(s);
}

// 不叫 exists:那是 ucode 内置函数名
function path_exists(p) {
	return fs.stat(p) != null;
}

// 设备名进入 sysfs 路径前必须过滤,uci 里的值不可信
function safe_dev(name) {
	if (type(name) != 'string' || name == '')
		return false;
	return match(name, /^[A-Za-z0-9_][A-Za-z0-9_.:-]*$/) != null;
}

function parse_kv(text) {
	let o = {};
	if (text == null)
		return o;
	for (let line in split(text, "\n")) {
		let p = index(line, '=');
		if (p > 0)
			o[trim(substr(line, 0, p))] = trim(substr(line, p + 1));
	}
	return o;
}

function note_error(ctx, label, e) {
	let msg = (is_obj(e) && e.message != null) ? e.message : sprintf('%s', e);
	push(ctx.errors, label + ': ' + msg);
}

// 每个采集阶段独立 try/catch:一个来源失败只进 errors[],其余照常出结果
function stage(ctx, label, fn) {
	try {
		return fn();
	}
	catch (e) {
		note_error(ctx, label, e);
		return null;
	}
}

// ip route show default 的一行 → { dev, via, metric, from };不是 default 行返回 null
function parse_route_line(line) {
	let t = split(trim(line), /[ \t]+/);
	if (length(t) == 0 || t[0] != 'default')
		return null;
	let r = { dev: '', via: '', metric: 0, from: '' };
	for (let i = 1; i < length(t); i++) {
		let v = (i + 1 < length(t)) ? t[i + 1] : '';
		if (t[i] == 'dev') {
			r.dev = v;
			i++;
		}
		else if (t[i] == 'via') {
			r.via = v;
			i++;
		}
		else if (t[i] == 'from') {
			r.from = v;
			i++;
		}
		else if (t[i] == 'metric') {
			r.metric = to_int(v, 0);
			i++;
		}
	}
	if (r.dev == '')
		return null;
	return r;
}

function is_ula(addr) {
	let p = lc(substr(as_str(addr), 0, 2));
	return p == 'fc' || p == 'fd';
}

// ---------- 采集 ----------

function new_ctx(opts) {
	return {
		use_health: !(is_obj(opts) && opts.health == false),
		errors: [],
		cur: null,
		nets: {},          // uci network 的 interface 段
		net_order: [],
		zones: [],         // [{ name, masq, nets, devs }]
		over: {},          // wan_id → { alias, enabled, section }
		sta: {},           // 接口名 → wifi 设备名(sta 模式 vif 的 network)
		dump: {},          // ubus network.interface dump,按接口名
		dump_order: [],
		kroutes4: [],
		kroutes6: [],
		kroutes_ok: true,
		usbnic: {},        // /tmp/mwrt-usbnic.d/<dev> 记录
		wid_cache: {},     // 回落标识 → 上次读到的真实 wan_id(落盘缓存)
		wid_seen: {},      // 本次读到真实设备标识的 回落标识 → wan_id
		usbnic_kind: {},
		usbrows: null,
		dinfo: {},
		health: { fresh: false, ts: null, members: {} },
		lan_names: {},
		wan_zone_names: {},
		zone_of: {},
		lan_prefix6: null,
		default_dev: ''
	};
}

function load_uci(ctx) {
	let cur = uci_cursor();
	if (cur == null) {
		push(ctx.errors, 'uci: 无法创建游标');
		return;
	}
	ctx.cur = cur;

	stage(ctx, 'uci network', function() {
		let ok = cur.foreach('network', 'interface', function(s) {
			let n = s['.name'];
			if (n != null) {
				ctx.nets[n] = s;
				push(ctx.net_order, n);
			}
			return true;
		});
		if (ok == null)
			push(ctx.errors, 'uci network: 读取失败');
	});

	stage(ctx, 'uci firewall', function() {
		let ok = cur.foreach('firewall', 'zone', function(z) {
			push(ctx.zones, {
				name: as_str(z.name), masq: is_true(z.masq),
				nets: split_words(z.network), devs: split_words(z.device)
			});
			return true;
		});
		if (ok == null)
			push(ctx.errors, 'uci firewall: 读取失败');
	});

	// 覆盖段按 option wan_id 建索引,不靠段名,这样读取不需要 fork md5sum;配置文件可以不存在
	stage(ctx, 'uci mwrt_pbr', function() {
		cur.foreach(PBR_CFG, 'wan', function(s) {
			let id = as_str(s.wan_id);
			if (id != '')
				ctx.over[id] = {
					alias: as_str(s.alias),
					enabled: (s.enabled == null) ? true : is_true(s.enabled),
					section: s['.name']
				};
			return true;
		});
	});
}

function derive_zones(ctx) {
	for (let z in ctx.zones) {
		if (z.masq || z.name == 'wan')
			ctx.wan_zone_names[z.name] = true;
	}
	for (let z in ctx.zones) {
		let wan_like = (ctx.wan_zone_names[z.name] == true);
		for (let n in z.nets) {
			let name = as_str(n);
			if (wan_like) {
				if (ctx.zone_of[name] == null)
					ctx.zone_of[name] = z.name;
			}
			else if (z.name == 'lan')
				ctx.lan_names[name] = true;
		}
	}
	// 协议脚本通过 data.zone 动态入区(fw4 同样读取它),uci 的 network 列表里看不到
	for (let n in ctx.dump_order) {
		let d = ctx.dump[n];
		let dz = (is_obj(d) && is_obj(d.data)) ? as_str(d.data.zone) : '';
		if (dz == '' || ctx.zone_of[n] != null)
			continue;
		if (ctx.wan_zone_names[dz] == true)
			ctx.zone_of[n] = dz;
		else if (dz == 'lan')
			ctx.lan_names[n] = true;
	}
	// 同时在 wan 区的接口不算 LAN
	for (let n in keys(ctx.lan_names)) {
		if (ctx.zone_of[n] != null)
			delete ctx.lan_names[n];
	}
	if (ctx.nets['lan'] != null && ctx.zone_of['lan'] == null)
		ctx.lan_names['lan'] = true;
}

// ubus 不可用时的后备:读 wireless 配置里 mode=sta 的 wifi-iface
function load_sta_from_uci(ctx) {
	if (ctx.cur == null)
		return;
	ctx.cur.foreach('wireless', 'wifi-iface', function(s) {
		if (as_str(s.mode) != 'sta' || is_true(s.disabled))
			return true;
		for (let item in as_list(s.network)) {
			for (let n in split(trim(as_str(item)), /[ \t]+/)) {
				if (n != '')
					ctx.sta[n] = as_str(s.device);
			}
		}
		return true;
	});
}

function load_ubus(ctx) {
	let ub = ubus.connect();
	if (ub == null) {
		push(ctx.errors, 'ubus: 无法连接 (' + as_str(ubus.error()) + ')');
		return;
	}

	stage(ctx, 'network.interface dump', function() {
		let r = ub.call('network.interface', 'dump');
		if (!is_obj(r) || type(r.interface) != 'array') {
			push(ctx.errors, 'network.interface dump: 调用失败 (' + as_str(ubus.error()) + ')');
			return;
		}
		for (let it in r.interface) {
			if (is_obj(it) && it.interface != null) {
				ctx.dump[it.interface] = it;
				push(ctx.dump_order, it.interface);
			}
		}
	});

	stage(ctx, 'network.wireless status', function() {
		let w = ub.call('network.wireless', 'status');
		if (!is_obj(w)) {
			// 没有任何无线配置时 netifd 不注册该对象(Not found),这不是错误;其它失败才记录,并退回读 uci
			let e = as_str(ubus.error());
			if (index(lc(e), 'not found') < 0) {
				push(ctx.errors, 'network.wireless status: 调用失败 (' + e + ')');
				load_sta_from_uci(ctx);
			}
			return;
		}
		for (let radio, rv in w) {
			if (!is_obj(rv))
				continue;
			for (let vif in as_list(rv.interfaces)) {
				let c = is_obj(vif) ? vif.config : null;
				if (!is_obj(c) || c.mode != 'sta')
					continue;
				for (let n in as_list(c.network))
					ctx.sta[as_str(n)] = radio;
			}
		}
	});
}

// 至多两次 ip route show(v4、v6 各一次)
function load_kroutes(ctx) {
	for (let fam in [4, 6]) {
		let r = run(sprintf('ip -%d route show default', fam));
		if (r.rc != 0) {
			push(ctx.errors, sprintf('ip -%d route show default: 执行失败', fam));
			ctx.kroutes_ok = false;
			continue;
		}
		for (let line in split(r.out, "\n")) {
			let rt = parse_route_line(line);
			if (rt != null)
				push((fam == 4) ? ctx.kroutes4 : ctx.kroutes6, rt);
		}
	}
}

// mwrt-usbnic 的每设备记录:key=value 文本;另兼容 <dev>.kind 单值文件(SPEC §2.2 第 2 条)
function load_usbnic(ctx) {
	let ents = fs.lsdir(USBNIC_DIR);
	if (ents == null)
		return;
	for (let f in ents) {
		if (!safe_dev(f))
			continue;
		let txt = rd(USBNIC_DIR + '/' + f);
		if (txt == null)
			continue;
		if (length(f) > 5 && substr(f, length(f) - 5) == '.kind')
			ctx.usbnic_kind[substr(f, 0, length(f) - 5)] = txt;
		else
			ctx.usbnic[f] = parse_kv(txt);
	}
}

// mwrt-usbinfo 会 fork 很多次 cat,只在 sysfs 判不出类型的 USB 设备上按需调用一次
function usbinfo_rows(ctx) {
	if (ctx.usbrows != null)
		return ctx.usbrows;
	ctx.usbrows = [];
	if (!path_exists(USBINFO))
		return ctx.usbrows;
	let r = run('sh ' + sh_quote(USBINFO));
	if (r.rc != 0) {
		push(ctx.errors, 'mwrt-usbinfo: 执行失败');
		return ctx.usbrows;
	}
	for (let line in split(r.out, "\n")) {
		let c = split(trim(line), '|');
		if (length(c) >= 5)
			push(ctx.usbrows, { bus: c[0], vid: c[1], pid: c[2], type: c[4] });
	}
	return ctx.usbrows;
}

function load_health(ctx) {
	let h = read_json(HEALTH_FILE, null);
	if (!is_obj(h))
		return;
	let iv = to_int(h.interval, HEALTH_INTERVAL);
	if (iv < 1)
		iv = HEALTH_INTERVAL;
	let ts = to_int(h.ts, 0);
	let d = now() - ts;
	if (d < 0)
		d = -d;
	// 板子可能没有 RTC,时钟会跳变,前后偏差都按过期处理
	if (d > 3 * iv)
		return;
	for (let m in as_list(h.members)) {
		if (is_obj(m) && m.wan_id != null)
			ctx.health.members[m.wan_id] = m;
	}
	ctx.health.fresh = true;
	ctx.health.ts = ts;
}

// ---------- 设备信息(sysfs,无 fork) ----------

function devinfo(ctx, dev) {
	if (ctx.dinfo[dev] != null)
		return ctx.dinfo[dev];
	let di = {
		exists: false, real: '', driver: '', type: null, flags: 0, carrier: null, operstate: '',
		wireless: false, phydev: false, dsa: false, bridge: false, tun: false, of_name: '',
		usb: false, bus: '', vid: '', pid: '', serial: '', pci: '', lower: ''
	};
	ctx.dinfo[dev] = di;
	if (!safe_dev(dev))
		return di;
	let base = SYS_NET + '/' + dev;
	if (!path_exists(base))
		return di;
	di.exists = true;

	let t = rd(base + '/type');
	if (t != null)
		di.type = to_int(t, null);
	let f = rd(base + '/flags');
	if (f != null) {
		let fv = hex(f);
		if (fv == fv)
			di.flags = fv;
	}
	di.carrier = rd(base + '/carrier');
	di.operstate = as_str(rd(base + '/operstate'));
	di.wireless = path_exists(base + '/wireless') || path_exists(base + '/phy80211');
	di.phydev = path_exists(base + '/phydev');
	di.bridge = path_exists(base + '/bridge');
	di.tun = path_exists(base + '/tun_flags');
	let sw = rd(base + '/phys_switch_id');
	di.dsa = (sw != null && sw != '');

	let real = fs.realpath(base + '/device');
	if (real != null) {
		di.real = real;
		let drv = fs.realpath(base + '/device/driver');
		if (drv != null)
			di.driver = as_str(fs.basename(drv));
		let dkv = parse_kv(rd(base + '/device/uevent'));
		di.of_name = as_str(dkv['OF_NAME']);

		// USB 网口的 device 链接指向接口目录 N-N(.N)*:C.I,其上一级是 USB 设备目录
		let m = match(as_str(fs.basename(real)), /^([0-9]+-[0-9]+(\.[0-9]+)*):[0-9]+\.[0-9]+$/);
		if (m != null) {
			let usbdir = fs.dirname(real);
			di.usb = true;
			di.bus = m[1];
			di.vid = lc(as_str(rd(usbdir + '/idVendor')));
			di.pid = lc(as_str(rd(usbdir + '/idProduct')));
			di.serial = as_str(rd(usbdir + '/serial'));
		}
		for (let seg in split(real, '/')) {
			if (match(seg, /^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9a-fA-F]$/) != null)
				di.pci = lc(seg);
		}
	}
	else {
		// 虚拟设备(VLAN/桥/bond):lower_<底层> 链接指向下层设备
		let ents = fs.lsdir(base);
		for (let e in as_list(ents)) {
			if (substr(e, 0, 6) == 'lower_') {
				di.lower = substr(e, 6);
				break;
			}
		}
	}
	return di;
}

function is_tunnel_dev(di) {
	if (di.tun)
		return true;
	if (di.type != null && has(TUNNEL_TYPES, di.type))
		return true;
	return (di.type == 65534 && di.real == '' && di.lower == '');
}

// SPEC §2.2 第 6-10 步:只看设备本身
function dev_kind(ctx, dev, depth) {
	let di = devinfo(ctx, dev);
	if (!di.exists)
		return 'other';
	if (is_tunnel_dev(di))
		return 'tunnel';
	if (di.wireless)
		return 'wireless';
	if (has(CELL_DRIVERS, di.driver))
		return 'cellular';
	if (di.driver == 'ipheth')
		return 'tether';
	if (di.usb) {
		for (let row in usbinfo_rows(ctx)) {
			if (row.bus == di.bus) {
				if (row.type == 'modem')
					return 'cellular';
				if (row.type == 'tether')
					return 'tether';
				break;
			}
		}
		return 'usbnic';
	}
	if (di.of_name == 'mac' || di.phydev || di.dsa)
		return 'wired';
	if (di.real != '' && di.driver != '' && di.type == 1)
		return 'wired';
	if (di.lower != '' && depth < 3)
		return dev_kind(ctx, di.lower, depth + 1);
	return 'other';
}

// ---------- 接口与分组 ----------

// 接口对应的网络设备名;'' 表示无法确定(例如 qmi 的控制节点 /dev/cdc-wdm0)
function resolve_dev(ctx, name, depth) {
	if (depth > 4)
		return '';
	let d = ctx.dump[name];
	let s = ctx.nets[name];
	let proto = '';
	if (is_obj(d) && d.proto != null)
		proto = as_str(d.proto);
	else if (is_obj(s))
		proto = as_str(s.proto);

	let dev = '';
	if (is_obj(d))
		dev = as_str((d.l3_device != null) ? d.l3_device : d.device);
	// pppoe 的 uci device 是底层网卡,出口设备是 pppoe-<接口名>;down 时 ubus 没有 l3_device,按 netifd 的命名补上
	if (proto == 'pppoe' && (!is_obj(d) || d.l3_device == null))
		return 'pppoe-' + name;
	if (dev == '' && is_obj(s))
		dev = first_word((s.device != null) ? s.device : s.ifname);
	if (substr(dev, 0, 1) == '@')
		return resolve_dev(ctx, substr(dev, 1), depth + 1);
	if (substr(dev, 0, 1) == '/')
		return '';
	return dev;
}

function iface_proto(ctx, name) {
	let d = ctx.dump[name];
	let s = ctx.nets[name];
	if (is_obj(d) && d.proto != null)
		return as_str(d.proto);
	return is_obj(s) ? as_str(s.proto) : '';
}

function has_default_route(d) {
	let lists = [as_list(d.route)];
	if (is_obj(d.inactive))
		push(lists, as_list(d.inactive.route));
	for (let l in lists) {
		for (let rt in l) {
			if (is_obj(rt) && rt.mask == 0 && (rt.target == '0.0.0.0' || rt.target == '::'))
				return true;
		}
	}
	return false;
}

// SPEC §2.1:uci wan 区成员 ∪ ubus 有默认路由的接口 ∪ 内核默认路由的 dev;再按设备合并;排除 LAN
function discover(ctx) {
	let order = [];
	let seen = {};
	let add = function(n) {
		if (seen[n] == true)
			return;
		if (substr(n, 0, 4) != 'dev:' && ctx.nets[n] == null && ctx.dump[n] == null)
			return;
		seen[n] = true;
		push(order, n);
	};

	for (let z in ctx.zones) {
		if (ctx.wan_zone_names[z.name] != true)
			continue;
		for (let n in z.nets)
			add(as_str(n));
	}
	for (let n in ctx.dump_order) {
		if (ctx.zone_of[n] != null || (is_obj(ctx.dump[n]) && has_default_route(ctx.dump[n])))
			add(n);
	}

	// 内核默认路由兜底:proto none 的蜂窝口不在 ubus 路由里
	let kdevs = [];
	for (let r in ctx.kroutes4)
		if (!has(kdevs, r.dev))
			push(kdevs, r.dev);
	for (let r in ctx.kroutes6)
		if (!has(kdevs, r.dev))
			push(kdevs, r.dev);
	let known_names = [];
	for (let n in ctx.net_order)
		push(known_names, n);
	for (let n in ctx.dump_order)
		if (!has(known_names, n))
			push(known_names, n);
	let owned = {};
	for (let n in known_names)
		owned[n] = resolve_dev(ctx, n, 0);
	for (let dev in kdevs) {
		let found = false;
		for (let n in known_names) {
			if (owned[n] == dev) {
				add(n);
				found = true;
			}
		}
		if (!found)
			add('dev:' + dev);
	}

	// LAN 设备:LAN 接口解析出的设备都不能当出口
	let lan_devs = {};
	for (let n in keys(ctx.lan_names)) {
		let dv = resolve_dev(ctx, n, 0);
		if (dv != '')
			lan_devs[dv] = true;
	}

	let groups = [];
	let by_key = {};
	for (let n in order) {
		let synth = (substr(n, 0, 4) == 'dev:' && ctx.nets[n] == null && ctx.dump[n] == null);
		let dev = synth ? substr(n, 4) : resolve_dev(ctx, n, 0);
		let in_wan = (ctx.zone_of[n] != null);
		if (!synth) {
			if (ctx.lan_names[n] == true)
				continue;
			if (!in_wan && dev != '' && lan_devs[dev] == true)
				continue;
		}
		else if (lan_devs[dev] == true)
			continue;
		if (dev == 'lo')
			continue;
		let key = (dev != '') ? 'dev:' + dev : 'if:' + n;
		if (by_key[key] == null) {
			by_key[key] = { key: key, dev: dev, ifaces: [], synth: synth };
			push(groups, by_key[key]);
		}
		if (!synth)
			push(by_key[key].ifaces, n);
	}
	return groups;
}

// ---------- 出口对象 ----------

function iface_info(ctx, n) {
	let s = ctx.nets[n];
	let d = ctx.dump[n];
	let proto = iface_proto(ctx, n);
	return {
		name: n, sect: s, d: d, proto: proto,
		is6: has(V6_PROTOS, proto),
		up: is_obj(d) && d.up == true,
		disabled: is_obj(s) && is_true(s.disabled)
	};
}

// 在 route[] 里挑某一族的默认路由(最低 metric);ubus 的 v4 路由不带 metric 字段,用接口 metric
function scan_routes(list, ifmetric, target) {
	let best = null;
	for (let rt in as_list(list)) {
		if (!is_obj(rt) || rt.mask != 0 || rt.target != target)
			continue;
		let m = to_int(rt.metric, to_int(ifmetric, 0));
		if (best != null && best.metric <= m)
			continue;
		let gw = as_str(rt.nexthop);
		if (gw == '0.0.0.0' || gw == '::')
			gw = '';
		best = { gw: gw, metric: m, from: '' };
	}
	return best;
}

function pick_kernel(routes, dev) {
	if (dev == '')
		return null;
	let best = null;
	for (let r in routes) {
		if (r.dev != dev)
			continue;
		if (best == null || r.metric < best.metric)
			best = { gw: r.via, metric: r.metric, from: r.from };
	}
	return best;
}

function addr_text(a) {
	return sprintf('%s/%d', as_str(a.address), to_int(a.mask, 0));
}

// 点对点设备:路由不写 via。接口带 POINTOPOINT 标志、PPP 类型,或默认路由没有网关
function ptp_of(di, r4, r6) {
	if (di.exists && ((di.flags & 16) != 0 || di.type == 512))
		return true;
	if (r4 != null)
		return r4.gw == '';
	if (r6 != null)
		return r6.gw == '';
	return false;
}

// 整理一个设备分组为出口对象(health/alias/enabled/prio 由 snapshot 补)
function build_wan(ctx, g) {
	let infos = [];
	for (let n in g.ifaces)
		push(infos, iface_info(ctx, n));
	let dev = g.dev;
	let warnings = [];

	let iface = '';
	let iface6 = '';
	for (let it in infos) {
		if (!it.is6 && iface == '')
			iface = it.name;
		else if (it.is6 && iface6 == '')
			iface6 = it.name;
	}
	let ifaces = [];
	if (iface != '')
		push(ifaces, iface);
	if (iface6 != '')
		push(ifaces, iface6);
	for (let it in infos) {
		if (!has(ifaces, it.name))
			push(ifaces, it.name);
	}
	let proto = '';
	if (iface != '')
		proto = iface_proto(ctx, iface);
	else if (iface6 != '')
		proto = iface_proto(ctx, iface6);

	// ---- 分类(SPEC §2.2;按真实配置与设备状态,不看接口名) ----
	let kind = '';
	let radio = '';
	for (let it in infos) {
		if (is_obj(it.sect) && as_str(it.sect.modem_config) != '') {
			kind = 'cellular';
			break;
		}
	}
	if (kind == '') {
		for (let it in infos) {
			if (has(CELL_PROTOS, it.proto)) {
				kind = 'cellular';
				break;
			}
		}
	}
	if (kind == '') {
		for (let it in infos) {
			if (is_obj(it.sect) && is_true(it.sect.mwrt_usbnic)) {
				kind = 'usbnic';
				break;
			}
		}
	}
	if (kind == '') {
		for (let it in infos) {
			if (ctx.sta[it.name] != null) {
				kind = 'wlbridge';
				radio = as_str(ctx.sta[it.name]);
				break;
			}
		}
	}
	if (kind == '') {
		for (let it in infos) {
			if (it.proto == 'pppoe') {
				kind = 'pppoe';
				break;
			}
		}
	}
	if (kind == '') {
		for (let it in infos) {
			if (has(TUNNEL_PROTOS, it.proto)) {
				kind = 'tunnel';
				break;
			}
		}
	}
	if (kind == '')
		kind = (dev != '') ? dev_kind(ctx, dev, 0) : 'other';
	// mwrt-usbnic 记录声明为手机共享的,归 tether
	if (kind == 'usbnic' && dev != '') {
		let rec = ctx.usbnic[dev];
		if ((is_obj(rec) && rec.kind == 'tether') || ctx.usbnic_kind[dev] == 'tether')
			kind = 'tether';
	}

	// pppoe 的底层设备
	let under = '';
	if (kind == 'pppoe') {
		for (let it in infos) {
			if (it.proto != 'pppoe')
				continue;
			let u = '';
			if (is_obj(it.sect))
				u = first_word((it.sect.device != null) ? it.sect.device : it.sect.ifname);
			if (substr(u, 0, 1) == '@')
				u = resolve_dev(ctx, substr(u, 1), 1);
			if (u == '' && is_obj(it.d))
				u = as_str(it.d.device);
			if (substr(u, 0, 6) == 'pppoe-')
				u = '';
			under = u;
			break;
		}
	}

	// ---- 标识(SPEC §2.3) ----
	let iddev = (kind == 'pppoe') ? under : dev;
	let di = devinfo(ctx, iddev);
	let dd = devinfo(ctx, dev);
	let rec_vid = '';
	let rec_pid = '';
	let rec_ser = '';
	let rec_bus = '';
	if (!di.usb && iddev != '' && is_obj(ctx.usbnic[iddev])) {
		// 设备目录读不到时,用 mwrt-usbnic 的记录兜底:id=总线位置/vid:pid/序列号
		let rec = ctx.usbnic[iddev];
		let idp = split(as_str(rec.id), '/');
		let vp = (length(idp) >= 2) ? split(idp[1], ':') : [];
		rec_bus = (length(idp) >= 1) ? idp[0] : '';
		rec_vid = lc((length(vp) == 2) ? vp[0] : as_str(rec.vid));
		rec_pid = lc((length(vp) == 2) ? vp[1] : as_str(rec.pid));
		rec_ser = (length(idp) >= 3) ? idp[2] : '';
	}
	let u_vid = di.usb ? di.vid : rec_vid;
	let u_pid = di.usb ? di.pid : rec_pid;
	let u_ser = di.usb ? di.serial : rec_ser;
	let u_bus = di.usb ? di.bus : rec_bus;

	let wid = '';
	if (kind == 'cellular') {
		if (u_vid != '' && u_pid != '') {
			if (u_ser != '')
				wid = sprintf('cell:%s:%s:%s', u_vid, u_pid, u_ser);
			else if (u_bus != '') {
				wid = sprintf('cell:%s:%s:%s', u_vid, u_pid, u_bus);
				push(warnings, '无序列号,换口可能改变标识');
			}
		}
		else if (di.pci != '')
			wid = 'cell:pci:' + di.pci;
	}
	else if (kind == 'usbnic' || kind == 'tether') {
		if (u_vid != '' && u_pid != '') {
			if (u_ser != '')
				wid = sprintf('usb:%s:%s:%s', u_vid, u_pid, u_ser);
			else if (u_bus != '') {
				wid = sprintf('usb:%s:%s:%s', u_vid, u_pid, u_bus);
				push(warnings, '无序列号,换口可能改变标识');
			}
		}
	}
	else if (kind == 'wlbridge' && radio != '')
		wid = 'wlb:' + radio;
	else if (kind == 'pppoe' && under != '')
		wid = 'pppoe:' + under;

	let fb = (iface != '') ? 'if:' + iface : ((iface6 != '') ? 'if:' + iface6 : 'dev:' + dev);
	let hw_kind = (kind == 'cellular' || kind == 'usbnic' || kind == 'tether');
	if (wid != '' && hw_kind)
		ctx.wid_seen[fb] = wid;
	if (wid == '') {
		wid = fb;
		if (hw_kind) {
			// 设备重新枚举期间读不到标识,沿用上次的真实 wan_id,避免别名/顺序/规则对不上
			let old = ctx.wid_cache[fb];
			if (type(old) == 'string' && old != '') {
				wid = old;
				push(warnings, '暂时读不到 USB/PCIe 设备标识,沿用上次记录的出口标识');
			}
			else
				push(warnings, '读不到 USB/PCIe 设备标识,出口标识回落为接口名,设备重新插拔后标识可能变化');
		}
	}
	wid = replace(wid, /[^A-Za-z0-9._:-]/g, '_');

	// ---- 地址、默认路由、LAN 前缀 ----
	let a4 = [];
	let a6 = [];
	let r4 = null;
	let r6 = null;
	let i4 = null;
	let i6 = null;
	let pfx = null;
	let if_metric = null;
	for (let it in infos) {
		let d = it.d;
		if (is_obj(d)) {
			for (let a in as_list(d['ipv4-address'])) {
				if (is_obj(a))
					push(a4, addr_text(a));
			}
			for (let a in as_list(d['ipv6-address'])) {
				if (is_obj(a))
					push(a6, addr_text(a));
			}
			let b4 = scan_routes(d.route, d.metric, '0.0.0.0');
			if (b4 != null && (r4 == null || b4.metric < r4.metric))
				r4 = b4;
			let b6 = scan_routes(d.route, d.metric, '::');
			if (b6 != null && (r6 == null || b6.metric < r6.metric))
				r6 = b6;
			if (is_obj(d.inactive)) {
				let c4 = scan_routes(d.inactive.route, d.metric, '0.0.0.0');
				if (c4 != null && (i4 == null || c4.metric < i4.metric))
					i4 = c4;
				let c6 = scan_routes(d.inactive.route, d.metric, '::');
				if (c6 != null && (i6 == null || c6.metric < i6.metric))
					i6 = c6;
			}
			// 上游委派给 LAN 的前缀:ipv6-prefix[].assigned.<lan 接口>
			if (pfx == null) {
				for (let p in as_list(d['ipv6-prefix'])) {
					if (!is_obj(p) || !is_obj(p.assigned))
						continue;
					for (let k, av in p.assigned) {
						if (ctx.lan_names[k] == true && is_obj(av) && av.address != null && !is_ula(av.address)) {
							pfx = addr_text(av);
							break;
						}
					}
					if (pfx != null)
						break;
				}
			}
			if (if_metric == null && d.metric != null)
				if_metric = to_int(d.metric, null);
		}
		if (if_metric == null && is_obj(it.sect) && it.sect.metric != null)
			if_metric = to_int(it.sect.metric, null);
	}
	// proto none 的蜂窝口等不在 ubus 路由里,用内核默认路由补
	let k4 = pick_kernel(ctx.kroutes4, dev);
	let k6 = pick_kernel(ctx.kroutes6, dev);
	if (r4 == null)
		r4 = k4;
	if (r6 == null)
		r6 = k6;

	let gw4 = '';
	if (r4 != null)
		gw4 = r4.gw;
	else if (i4 != null)
		gw4 = i4.gw;
	let gw6 = '';
	if (r6 != null)
		gw6 = r6.gw;
	else if (i6 != null)
		gw6 = i6.gw;
	let metric = 0;
	if (r4 != null)
		metric = r4.metric;
	else if (i4 != null)
		metric = i4.metric;
	else if (r6 != null)
		metric = r6.metric;
	else if (if_metric != null)
		metric = if_metric;

	// ---- 状态 ----
	let up = false;
	let all_disabled = (length(infos) > 0);
	for (let it in infos) {
		if (it.up)
			up = true;
		if (!it.disabled)
			all_disabled = false;
	}
	let link = false;
	if (dd.exists)
		link = (dd.carrier != null) ? (dd.carrier == '1') : up;
	if (length(infos) == 0)
		up = dd.exists && ((dd.flags & 1) != 0) && link;

	let base_ok = dd.exists;
	if (!base_ok && kind == 'pppoe' && under != '')
		base_ok = devinfo(ctx, under).exists;
	if (!base_ok && dev == '') {
		for (let it in infos) {
			let u = is_obj(it.sect) ? first_word((it.sect.device != null) ? it.sect.device : it.sect.ifname) : '';
			if (substr(u, 0, 1) == '/' && path_exists(u))
				base_ok = true;
		}
	}
	let route4 = (r4 != null);
	let route6 = (r6 != null);
	let state = 'offline';
	if (all_disabled)
		state = 'disabled';
	else if (!up && !base_ok)
		state = 'absent';
	else if (up && (route4 || route6))
		state = 'online';

	// ---- 防火墙区 ----
	let zone = '';
	for (let it in infos) {
		if (ctx.zone_of[it.name] != null) {
			zone = ctx.zone_of[it.name];
			break;
		}
	}
	if (zone == '') {
		for (let z in ctx.zones) {
			let hit = has(z.devs, dev);
			for (let it in infos) {
				if (has(z.nets, it.name))
					hit = true;
			}
			if (hit) {
				zone = z.name;
				break;
			}
		}
	}
	let in_zone = (zone != '' && ctx.wan_zone_names[zone] == true);
	if (!in_zone)
		push(warnings, '未加入防火墙的 wan 区(masq 区),经此出口转发的流量可能被拒绝且没有地址转换');
	if (up && !route4 && !route6)
		push(warnings, '接口已启动但没有默认路由');

	// ---- 能力 ----
	let v4 = (iface != '') || route4;
	let v6 = (pfx != null);
	if (!v6 && r6 != null && k6 != null && k6.from != '' && k6.from == ctx.lan_prefix6)
		v6 = true;
	let policy = (dev != '');

	let label = KIND_LABEL[kind];
	let shown = dev;
	if (shown == '')
		shown = (iface != '') ? iface : iface6;
	// s297c 668/688/5800 的 eth1 是机内接模组的 2.5G PHY, 页面上叫「模组转网口PHY」(其余机型 dev_label 为空)
	let dev_label = (dev != '' && dev == xfer_dev()) ? '模组转网口PHY' : '';
	if (dev_label != '')
		shown = dev_label;

	let drv = devinfo(ctx, iddev);
	return {
		wan_id: wid,
		name: label + ' ' + shown,
		alias: '',
		kind: kind,
		kind_label: label,
		iface: iface,
		iface6: iface6,
		ifaces: ifaces,
		dev: dev,
		dev_label: dev_label,
		under: under,
		proto: proto,
		state: state,
		up: up,
		link: link,
		addr4: a4,
		gw4: gw4,
		metric: metric,
		is_default: (dev != '' && dev == ctx.default_dev),
		addr6: a6,
		gw6: gw6,
		prefix6: pfx,
		in_zone: in_zone,
		zone: zone,
		caps: { v4: v4, v6: v6, policy: policy, reason: policy ? '' : '尚未识别到出口设备' },
		health: null,
		prio: 0,
		enabled: true,
		warnings: warnings,
		route4: route4,
		route6: route6,
		ptp: ptp_of(dd, r4, r6),
		bus: drv.usb ? drv.bus : '',
		vidpid: (drv.usb && drv.vid != '' && drv.pid != '') ? (drv.vid + ':' + drv.pid) : '',
		driver: drv.driver
	};
}

// ---------- 健康、排序 ----------

function health_for(ctx, wan_id) {
	let h = { state: 'unknown', rtt_ms: null, fails: 0, since: null, source: 'none', ts: null };
	if (!ctx.use_health || !ctx.health.fresh)
		return h;
	let m = ctx.health.members[wan_id];
	if (!is_obj(m))
		return h;
	let st = as_str((m.health != null) ? m.health : m.state);
	if (!has(['up', 'down', 'unknown'], st))
		st = 'unknown';
	h.state = st;
	h.source = 'probe';
	h.rtt_ms = to_int(m.rtt_ms, null);
	h.fails = to_int(m.fails, 0);
	h.since = to_int(m.since, null);
	h.ts = to_int(m.last_probe, ctx.health.ts);
	return h;
}

// §6.3 默认序:类别序,再 metric,再 wan_id
function default_before(a, b) {
	let ca = KIND_CLASS[a.kind];
	let cb = KIND_CLASS[b.kind];
	if (ca == null)
		ca = 4;
	if (cb == null)
		cb = 4;
	if (ca != cb)
		return ca < cb;
	let ma = to_int(a.metric, 0);
	let mb = to_int(b.metric, 0);
	if (ma != mb)
		return ma < mb;
	return as_str(a.wan_id) < as_str(b.wan_id);
}

// 稳定插入排序,成员很少;不依赖 sort() 比较函数的返回值约定
function sorted_by_default(list) {
	let res = [];
	for (let w in list) {
		let i = length(res);
		while (i > 0 && default_before(w, res[i - 1]))
			i--;
		splice(res, i, 0, w);
	}
	return res;
}

function order_ids(wans, saved) {
	let by_id = {};
	for (let w in wans) {
		if (is_obj(w) && w.wan_id != null)
			by_id[w.wan_id] = w;
	}
	let res = [];
	let used = {};
	for (let id in saved) {
		let s = as_str(id);
		if (by_id[s] != null && used[s] != true) {
			used[s] = true;
			push(res, s);
		}
	}
	let rest = [];
	for (let w in wans) {
		if (is_obj(w) && used[w.wan_id] != true)
			push(rest, w);
	}
	for (let w in sorted_by_default(rest))
		push(res, w.wan_id);
	return res;
}

function get_order(cur) {
	if (cur == null)
		cur = uci_cursor();
	let res = [];
	if (cur == null)
		return res;
	for (let id in as_list(cur.get(PBR_CFG, 'global', 'wan_order'))) {
		let s = trim(as_str(id));
		if (s != '' && !has(res, s))
			push(res, s);
	}
	return res;
}

// 已保存的顺序里不在快照中的 wan_id 不参与;快照里多出来的按默认序追加到末尾(SPEC §6.3)
// cfg 可以是 wan_order 数组、含 wan_order 的对象,或 null(此时读 UCI)
function effective_order(snap, cfg) {
	let saved = [];
	if (type(cfg) == 'array')
		saved = cfg;
	else if (is_obj(cfg))
		saved = as_list(cfg.wan_order);
	else if (cfg == null) {
		try {
			saved = get_order(null);
		}
		catch (e) {
			saved = [];
		}
	}
	return order_ids(is_obj(snap) ? as_list(snap.wans) : [], saved);
}

function resolve(snap, wan_id) {
	if (!is_obj(snap))
		return null;
	for (let w in as_list(snap.wans)) {
		if (is_obj(w) && w.wan_id == wan_id)
			return w;
	}
	return null;
}

// ---------- 快照 ----------

function compute_lan_prefix6(ctx) {
	for (let n in keys(ctx.lan_names)) {
		let d = ctx.dump[n];
		if (!is_obj(d))
			continue;
		for (let p in as_list(d['ipv6-prefix-assignment'])) {
			if (is_obj(p) && p.address != null && !is_ula(p.address))
				return addr_text(p);
		}
	}
	return null;
}

// 系统默认出口设备:内核主表 metric 最低的默认路由;内核读取失败时才退回 ubus
function compute_default_dev(ctx) {
	let best = null;
	for (let r in ctx.kroutes4) {
		if (best == null || r.metric < best.metric)
			best = r;
	}
	if (best == null) {
		for (let r in ctx.kroutes6) {
			if (best == null || r.metric < best.metric)
				best = r;
		}
	}
	if (best != null)
		return best.dev;
	if (ctx.kroutes_ok)
		return '';
	let bm = null;
	let bd = '';
	for (let n in ctx.dump_order) {
		let d = ctx.dump[n];
		let r = scan_routes(d.route, d.metric, '0.0.0.0');
		if (r != null && (bm == null || r.metric < bm)) {
			bm = r.metric;
			bd = resolve_dev(ctx, n, 0);
		}
	}
	return bd;
}

function snapshot(opts) {
	let ctx = new_ctx(opts);
	stage(ctx, 'uci', function() {
		load_uci(ctx);
	});
	stage(ctx, 'ubus', function() {
		load_ubus(ctx);
	});
	// data.zone 来自 ubus,所以区的归属要在 ubus 之后推导
	stage(ctx, '防火墙区', function() {
		derive_zones(ctx);
	});
	stage(ctx, '内核路由', function() {
		load_kroutes(ctx);
	});
	stage(ctx, 'mwrt-usbnic', function() {
		load_usbnic(ctx);
	});
	if (ctx.use_health) {
		stage(ctx, '健康数据', function() {
			load_health(ctx);
		});
	}
	ctx.default_dev = stage(ctx, '默认出口设备', function() {
		return compute_default_dev(ctx);
	});
	if (ctx.default_dev == null)
		ctx.default_dev = '';
	ctx.lan_prefix6 = stage(ctx, 'LAN 前缀', function() {
		return compute_lan_prefix6(ctx);
	});

	stage(ctx, '出口标识缓存', function() {
		let c = read_json(WID_CACHE, {});
		if (is_obj(c))
			ctx.wid_cache = c;
	});

	let groups = stage(ctx, '出口发现', function() {
		return discover(ctx);
	});
	let wans = [];
	for (let g in as_list(groups)) {
		let w = stage(ctx, '出口 ' + g.key, function() {
			return build_wan(ctx, g);
		});
		if (w != null)
			push(wans, w);
	}

	let cache_dirty = false;
	for (let k, v in ctx.wid_seen) {
		if (ctx.wid_cache[k] != v) {
			ctx.wid_cache[k] = v;
			cache_dirty = true;
		}
	}
	if (cache_dirty)
		write_json_atomic(WID_CACHE, ctx.wid_cache);

	// 标识重复(例如两个同型号无序列号的设备)时加后缀,保证 wan_id 唯一
	let seen = {};
	for (let w in wans) {
		if (seen[w.wan_id] == true) {
			let n = 2;
			while (seen[sprintf('%s-%d', w.wan_id, n)] == true)
				n++;
			w.wan_id = sprintf('%s-%d', w.wan_id, n);
			push(w.warnings, '出口标识与另一个出口重复,已加后缀以区分');
		}
		seen[w.wan_id] = true;
	}

	let saved = [];
	if (ctx.cur != null) {
		saved = stage(ctx, '出口顺序', function() {
			return get_order(ctx.cur);
		});
		if (saved == null)
			saved = [];
	}
	let ids = order_ids(wans, saved);
	let by_id = {};
	for (let w in wans)
		by_id[w.wan_id] = w;
	let out = [];
	for (let i = 0; i < length(ids); i++) {
		let w = by_id[ids[i]];
		w.prio = i + 1;
		let ov = ctx.over[w.wan_id];
		if (is_obj(ov)) {
			w.alias = ov.alias;
			w.enabled = ov.enabled;
		}
		w.health = health_for(ctx, w.wan_id);
		push(out, w);
	}

	return {
		ver: SNAP_VER,
		ts: now(),
		default_dev: ctx.default_dev,
		lan_prefix6: ctx.lan_prefix6,
		wans: out,
		errors: ctx.errors
	};
}

// ---------- 读写 UCI(只由 mwrt-wans 的 set 子命令使用) ----------

function get_mode(cur) {
	if (cur == null)
		cur = uci_cursor();
	if (cur == null)
		return 'pair';
	let m = lc(first_str(cur.get(ASSURANCE, FAILOVER_SEC, 'mode')));
	return (m == 'multi') ? 'multi' : 'pair';
}

function valid_wan_id(id) {
	return type(id) == 'string' && length(id) > 0 && length(id) <= 128 && match(id, /^[A-Za-z0-9._:-]+$/) != null;
}

function uci_fail(cur, what) {
	let e = (cur != null) ? as_str(cur.error()) : '';
	return { ok: false, error: what + (e != '' ? ': ' + e : '') };
}

// Mwrt-Assurance 属于网关存活页,文件不存在就不替它创建
function assurance_ready(cur) {
	if (fs.stat('/etc/config/' + ASSURANCE) == null)
		return false;
	if (cur.get(ASSURANCE, FAILOVER_SEC) == null)
		return cur.set(ASSURANCE, FAILOVER_SEC, 'settings') == true;
	return true;
}

function set_order(ids) {
	let list = [];
	for (let id in as_list(ids)) {
		if (!valid_wan_id(id))
			return { ok: false, error: '出口标识不合法: ' + as_str(id) };
		if (!has(list, id))
			push(list, id);
	}
	try {
		let cur = uci_cursor();
		if (cur == null)
			return { ok: false, error: '无法打开 UCI' };
		// 已保存的顺序里允许含当前不在线的 wan_id,设备拔掉再插回来顺序不丢
		if (length(list) == 0)
			cur.delete(PBR_CFG, 'global', 'wan_order');
		else if (cur.set(PBR_CFG, 'global', 'wan_order', list) != true)
			return uci_fail(cur, '写入出口顺序失败');
		if (cur.commit(PBR_CFG) != true)
			return uci_fail(cur, '提交出口顺序失败');
		return { ok: true, order: list };
	}
	catch (e) {
		return { ok: false, error: '保存出口顺序失败: ' + as_str(e.message) };
	}
}

function set_mode(mode) {
	if (mode != 'pair' && mode != 'multi')
		return { ok: false, error: '运行模式只能是 pair 或 multi' };
	try {
		let cur = uci_cursor();
		if (cur == null)
			return { ok: false, error: '无法打开 UCI' };
		if (!assurance_ready(cur))
			return { ok: false, error: '网关存活配置(Mwrt-Assurance)不存在,无法保存运行模式' };
		if (cur.set(ASSURANCE, FAILOVER_SEC, 'mode', mode) != true)
			return uci_fail(cur, '写入运行模式失败');
		if (cur.commit(ASSURANCE) != true)
			return uci_fail(cur, '提交运行模式失败');
		return { ok: true, mode: mode };
	}
	catch (e) {
		return { ok: false, error: '保存运行模式失败: ' + as_str(e.message) };
	}
}

// 控制字符换成空格,避免写进 UCI 后破坏文件格式
function clean_text(s) {
	let out = '';
	let t = as_str(s);
	for (let i = 0; i < length(t); i++) {
		let c = ord(t, i);
		out += (c < 32 || c == 127) ? ' ' : substr(t, i, 1);
	}
	return trim(out);
}

// 找 wan_id 对应的覆盖段;不存在且 create 为真时新建(段名见 lib.wan_section)
function over_section(cur, wan_id, create) {
	let found = null;
	cur.foreach(PBR_CFG, 'wan', function(s) {
		if (found == null && as_str(s.wan_id) == wan_id)
			found = s['.name'];
		return true;
	});
	if (found != null || !create)
		return found;
	let sec = wan_section(wan_id);
	if (sec == null)
		return null;
	if (cur.set(PBR_CFG, sec, 'wan') != true)
		return null;
	if (cur.set(PBR_CFG, sec, 'wan_id', wan_id) != true)
		return null;
	return sec;
}

function pbr_ready() {
	let path = '/etc/config/' + PBR_CFG;
	if (fs.stat(path) != null)
		return true;
	// 出厂文件缺失时建一个空文件,uci 才能往里写
	return fs.writefile(path, '') != null;
}

function set_alias(wan_id, text) {
	if (!valid_wan_id(wan_id))
		return { ok: false, error: '出口标识不合法' };
	let alias = clean_text(text);
	if (length(alias) > 96)
		return { ok: false, error: '别名太长(最多 96 字节,约 32 个汉字)' };
	try {
		if (!pbr_ready())
			return { ok: false, error: '无法创建 /etc/config/' + PBR_CFG };
		let cur = uci_cursor();
		if (cur == null)
			return { ok: false, error: '无法打开 UCI' };
		// 清空别名且没有覆盖段时无事可做
		let sec = over_section(cur, wan_id, alias != '');
		if (sec == null) {
			if (alias == '')
				return { ok: true, wan_id: wan_id, alias: '' };
			return uci_fail(cur, '创建别名配置失败');
		}
		if (alias == '')
			cur.delete(PBR_CFG, sec, 'alias');
		else if (cur.set(PBR_CFG, sec, 'alias', alias) != true)
			return uci_fail(cur, '写入别名失败');
		if (cur.commit(PBR_CFG) != true)
			return uci_fail(cur, '提交别名失败');
		return { ok: true, wan_id: wan_id, alias: alias };
	}
	catch (e) {
		return { ok: false, error: '保存别名失败: ' + as_str(e.message) };
	}
}

function set_enabled(wan_id, on) {
	if (!valid_wan_id(wan_id))
		return { ok: false, error: '出口标识不合法' };
	try {
		if (!pbr_ready())
			return { ok: false, error: '无法创建 /etc/config/' + PBR_CFG };
		let cur = uci_cursor();
		if (cur == null)
			return { ok: false, error: '无法打开 UCI' };
		let sec = over_section(cur, wan_id, true);
		if (sec == null)
			return uci_fail(cur, '创建出口配置失败');
		if (cur.set(PBR_CFG, sec, 'enabled', on ? '1' : '0') != true)
			return uci_fail(cur, '写入启用开关失败');
		if (cur.commit(PBR_CFG) != true)
			return uci_fail(cur, '提交启用开关失败');
		return { ok: true, wan_id: wan_id, enabled: on ? true : false };
	}
	catch (e) {
		return { ok: false, error: '保存启用开关失败: ' + as_str(e.message) };
	}
}

export { snapshot, resolve, effective_order, get_order, set_order, get_mode, set_mode, set_alias, set_enabled };
