'use strict';
// mwrt-pbr 规则/出口组校验与各类取值的规范化(纯函数:不读 UCI、不 fork、不写文件)。
// parse_* 的第二个参数 errs 可选:传入数组时,失败原因(简体中文,以出错的取值开头)会追加进去。

const MAX_RULES = 128;
const MAX_GROUPS = 16;
const MAX_MEMBERS = 8;
const MAX_VALUES = 64;
const MAX_PORTS = 15;

const PROTOS = [ 'any', 'tcp', 'udp', 'tcp_udp', 'icmp' ];

// ---------------------------------------------------------------- 通用小工具

function to_bool(v, dflt) {
	if (v == null)
		return dflt;
	if (type(v) == 'bool')
		return v;
	if (type(v) == 'int' || type(v) == 'double')
		return v != 0;
	let s = lc(trim('' + v));
	return !(s == '' || s == '0' || s == 'false' || s == 'off' || s == 'no');
}

// 任意值 -> 去空白、去空串的字符串数组(UI 可能只传单个字符串)
function list_of(v) {
	let out = [];
	if (v == null)
		return out;
	if (type(v) != 'array')
		v = [ v ];
	for (let x in v) {
		if (x == null)
			continue;
		let s = trim('' + x);
		if (s != '')
			push(out, s);
	}
	return out;
}

function perr(errs, msg) {
	if (errs != null)
		push(errs, msg);
	return null;
}

function put(list, field, msg) {
	push(list, { field: field, msg: msg });
}

function has_non_ascii(s) {
	for (let i = 0; i < length(s); i++) {
		if (ord(s, i) > 127)
			return true;
	}
	return false;
}

// 按字符(而非字节)计长;中文名称按 3 字节算会误伤 64 字上限
function utf8_len(s) {
	let n = 0;
	for (let i = 0; i < length(s); i++) {
		if ((ord(s, i) & 0xc0) != 0x80)
			n++;
	}
	return n;
}

// ---------------------------------------------------------------- 地址

// 地址一律用 16 位字数组表示:v4 两个字,v6 八个字,便于统一比较/合并
function parse_ipv4(s) {
	let m = match(s, /^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$/);
	if (!m)
		return null;
	let o = [];
	for (let i = 1; i <= 4; i++) {
		let t = m[i];
		// 前导零(如 010)有的实现按八进制解析,一律拒绝
		if (length(t) > 1 && substr(t, 0, 1) == '0')
			return null;
		let v = int(t);
		if (v > 255)
			return null;
		push(o, v);
	}
	return [ o[0] * 256 + o[1], o[2] * 256 + o[3] ];
}

function groups_to_words(groups, v4_last) {
	let out = [];
	let n = length(groups);
	for (let i = 0; i < n; i++) {
		let g = groups[i];
		if (v4_last && i == n - 1 && index(g, '.') >= 0) {
			let w = parse_ipv4(g);
			if (!w)
				return null;
			push(out, w[0]);
			push(out, w[1]);
			continue;
		}
		if (!match(g, /^[0-9a-f]{1,4}$/))
			return null;
		push(out, hex(g));
	}
	return out;
}

function parse_ipv6(s) {
	s = lc(s);
	if (!match(s, /^[0-9a-f:.]+$/) || index(s, ':') < 0 || index(s, ':::') >= 0)
		return null;
	let dc = index(s, '::');
	if (dc >= 0 && index(substr(s, dc + 2), '::') >= 0)
		return null;
	let head = s;
	let tail = '';
	if (dc >= 0) {
		head = substr(s, 0, dc);
		tail = substr(s, dc + 2);
	}
	let hg = (head == '') ? [] : split(head, ':');
	let tg = (tail == '') ? [] : split(tail, ':');
	let hw = groups_to_words(hg, dc < 0);
	let tw = groups_to_words(tg, true);
	if (hw == null || tw == null)
		return null;
	let total = length(hw) + length(tw);
	if (dc < 0)
		return (total == 8) ? hw : null;
	if (total > 7)
		return null;
	let out = slice(hw, 0);
	for (let i = total; i < 8; i++)
		push(out, 0);
	for (let w in tw)
		push(out, w);
	return out;
}

function fmt_v4(w) {
	return sprintf('%d.%d.%d.%d', w[0] >> 8, w[0] & 255, w[1] >> 8, w[1] & 255);
}

// RFC 5952:压缩最长的连续零段(至少两个字),小写,无前导零
function fmt_v6(w) {
	let bs = -1;
	let bl = 0;
	let i = 0;
	while (i < 8) {
		if (w[i] != 0) {
			i++;
			continue;
		}
		let j = i;
		while (j < 8 && w[j] == 0)
			j++;
		if (j - i > bl) {
			bs = i;
			bl = j - i;
		}
		i = j;
	}
	if (bl < 2)
		bs = -1;
	let out = '';
	for (let k = 0; k < 8; k++) {
		if (bs >= 0 && k >= bs && k < bs + bl) {
			if (k == bs)
				out += '::';
			continue;
		}
		if (out != '' && substr(out, -1) != ':')
			out += ':';
		out += sprintf('%x', w[k]);
	}
	return out;
}

function words_fmt(w, family) {
	return (family == 4) ? fmt_v4(w) : fmt_v6(w);
}

function words_cmp(a, b) {
	for (let i = 0; i < length(a); i++) {
		if (a[i] < b[i])
			return -1;
		if (a[i] > b[i])
			return 1;
	}
	return 0;
}

// a + 1;已是全 1 时返回 null
function words_inc(a) {
	let o = slice(a, 0);
	for (let i = length(o) - 1; i >= 0; i--) {
		if (o[i] < 0xffff) {
			o[i] = o[i] + 1;
			return o;
		}
		o[i] = 0;
	}
	return null;
}

function addr_family_word(t) {
	return (index(t, ':') >= 0) ? 'IPv6' : 'IPv4';
}

function parse_one_addr(t) {
	if (index(t, ':') >= 0) {
		let w = parse_ipv6(t);
		return w ? { family: 6, w: w } : null;
	}
	let w = parse_ipv4(t);
	return w ? { family: 4, w: w } : null;
}

// 前缀 -> [lo, hi](字数组);plen 为 0..总位数
function prefix_bounds(w, plen) {
	let lo = [];
	let hi = [];
	for (let i = 0; i < length(w); i++) {
		let bits = plen - 16 * i;
		if (bits < 0)
			bits = 0;
		if (bits > 16)
			bits = 16;
		let mask = (bits == 0) ? 0 : ((0xffff << (16 - bits)) & 0xffff);
		let l = w[i] & mask;
		push(lo, l);
		push(hi, l | (~mask & 0xffff));
	}
	return [ lo, hi ];
}

// 规范化地址对象:{ family:4|6, kind:'host'|'cidr'|'range', text, lo, hi, plen }。
// /32 与 /128 视为单个主机;不接受 /0(会匹配全部地址)。
function parse_addr(s, errs) {
	if (s == null)
		return perr(errs, '地址不能为空');
	s = trim('' + s);
	if (s == '')
		return perr(errs, '地址不能为空');

	let sep = index(s, '/');
	if (sep >= 0) {
		let ipt = trim(substr(s, 0, sep));
		let nt = trim(substr(s, sep + 1));
		let a = parse_one_addr(ipt);
		if (!a)
			return perr(errs, sprintf('%s 不是合法的 %s 地址', s, addr_family_word(ipt)));
		if (!match(nt, /^[0-9]{1,3}$/))
			return perr(errs, sprintf('%s 的前缀长度不是数字', s));
		let plen = int(nt);
		let maxbits = (a.family == 4) ? 32 : 128;
		if (plen == 0)
			return perr(errs, sprintf('%s 的前缀长度不能为 0(会匹配所有地址)', s));
		if (plen > maxbits)
			return perr(errs, sprintf('%s 的前缀长度必须在 1-%d 之间', s, maxbits));
		let b = prefix_bounds(a.w, plen);
		if (words_cmp(b[0], a.w) != 0)
			return perr(errs, sprintf('%s 的主机位不为 0(应写为 %s/%d)', s, words_fmt(b[0], a.family), plen));
		if (plen == maxbits)
			return { family: a.family, kind: 'host', text: words_fmt(a.w, a.family), lo: a.w, hi: a.w, plen: maxbits };
		return { family: a.family, kind: 'cidr', text: sprintf('%s/%d', words_fmt(b[0], a.family), plen), lo: b[0], hi: b[1], plen: plen };
	}

	let dash = index(s, '-');
	if (dash >= 0) {
		let p1 = trim(substr(s, 0, dash));
		let p2 = trim(substr(s, dash + 1));
		let a = parse_one_addr(p1);
		let b = parse_one_addr(p2);
		if (!a)
			return perr(errs, sprintf('%s 不是合法的 %s 地址', p1, addr_family_word(p1)));
		if (!b)
			return perr(errs, sprintf('%s 不是合法的 %s 地址', p2, addr_family_word(p2)));
		if (a.family != b.family)
			return perr(errs, sprintf('%s 的两端不是同一种地址(IPv4/IPv6)', s));
		let c = words_cmp(a.w, b.w);
		if (c > 0)
			return perr(errs, sprintf('%s 的起始地址大于结束地址', s));
		let maxbits = (a.family == 4) ? 32 : 128;
		if (c == 0)
			return { family: a.family, kind: 'host', text: words_fmt(a.w, a.family), lo: a.w, hi: a.w, plen: maxbits };
		return { family: a.family, kind: 'range', text: sprintf('%s-%s', words_fmt(a.w, a.family), words_fmt(b.w, a.family)), lo: a.w, hi: b.w, plen: null };
	}

	let a = parse_one_addr(s);
	if (!a) {
		if (match(s, /^[0-9a-fA-F:.]+$/) && (index(s, ':') >= 0 || index(s, '.') >= 0))
			return perr(errs, sprintf('%s 不是合法的 %s 地址', s, addr_family_word(s)));
		return perr(errs, sprintf('%s 不是合法的 IPv4/IPv6 地址、网段(CIDR)或区间', s));
	}
	let maxbits = (a.family == 4) ? 32 : 128;
	return { family: a.family, kind: 'host', text: words_fmt(a.w, a.family), lo: a.w, hi: a.w, plen: maxbits };
}

// 同族地址对象合并(重叠/相邻),返回按起点排序的 [{lo,hi,text,family}]。
// nft 的 interval 集合在同一次写入里元素重叠会报 conflicting intervals,所以由这里先合并。
function merge_addrs(items) {
	let arr = [];
	for (let it in items)
		push(arr, { lo: it.lo, hi: it.hi, text: it.text, family: it.family, n: 1 });
	arr = sort(arr, function(a, b) { return words_cmp(a.lo, b.lo); });
	let out = [];
	for (let it in arr) {
		if (length(out) > 0) {
			let last = out[length(out) - 1];
			let nxt = words_inc(last.hi);
			if (nxt == null || words_cmp(it.lo, nxt) <= 0) {
				if (words_cmp(it.hi, last.hi) > 0)
					last.hi = it.hi;
				last.n = last.n + 1;
				continue;
			}
		}
		push(out, it);
	}
	for (let o in out) {
		if (o.n > 1) {
			o.text = (words_cmp(o.lo, o.hi) == 0)
				? words_fmt(o.lo, o.family)
				: sprintf('%s-%s', words_fmt(o.lo, o.family), words_fmt(o.hi, o.family));
		}
	}
	return out;
}

// ---------------------------------------------------------------- MAC / 端口 / 域名 / 应用

function parse_mac(s, errs) {
	if (s == null)
		return perr(errs, 'MAC 地址不能为空');
	let raw = trim('' + s);
	let t = lc(raw);
	if (match(t, /^[0-9a-f]{2}(-[0-9a-f]{2}){5}$/))
		t = replace(t, /-/g, ':');
	if (!match(t, /^[0-9a-f]{2}(:[0-9a-f]{2}){5}$/))
		return perr(errs, sprintf('%s 不是合法的 MAC 地址(应为 aa:bb:cc:dd:ee:ff)', raw));
	if ((hex(substr(t, 0, 2)) & 1) == 1)
		return perr(errs, sprintf('%s 是组播/广播地址,不能作为设备 MAC', raw));
	if (t == '00:00:00:00:00:00')
		return perr(errs, sprintf('%s 不是有效的设备 MAC', raw));
	return { text: t };
}

// 端口 -> { lo, hi, text }
function parse_port(s, errs) {
	if (s == null)
		return perr(errs, '端口不能为空');
	let raw = trim('' + s);
	let m = match(raw, /^([0-9]{1,5})(-([0-9]{1,5}))?$/);
	if (!m)
		return perr(errs, sprintf('%s 不是合法的端口(1-65535 的数字,或 起-止 区间)', raw));
	let lo = int(m[1]);
	let hi = (m[3] != null) ? int(m[3]) : lo;
	if (lo < 1 || lo > 65535 || hi < 1 || hi > 65535)
		return perr(errs, sprintf('%s 超出端口范围 1-65535', raw));
	if (lo > hi)
		return perr(errs, sprintf('%s 的起始端口大于结束端口', raw));
	return { lo: lo, hi: hi, text: (lo == hi) ? sprintf('%d', lo) : sprintf('%d-%d', lo, hi) };
}

// 整数区间合并(端口/应用号),返回 [{lo,hi}]
function merge_ports(items) {
	let arr = [];
	for (let it in items)
		push(arr, { lo: it.lo, hi: it.hi });
	arr = sort(arr, function(a, b) { return a.lo - b.lo; });
	let out = [];
	for (let it in arr) {
		if (length(out) > 0) {
			let last = out[length(out) - 1];
			if (it.lo <= last.hi + 1) {
				if (it.hi > last.hi)
					last.hi = it.hi;
				continue;
			}
		}
		push(out, it);
	}
	return out;
}

// [{lo,hi}] -> "80, 1000-2000"(不含花括号)
function ranges_text(list) {
	let parts = [];
	for (let r in list)
		push(parts, (r.lo == r.hi) ? sprintf('%d', r.lo) : sprintf('%d-%d', r.lo, r.hi));
	return join(', ', parts);
}

function parse_domain(s, errs) {
	if (s == null)
		return perr(errs, '域名不能为空');
	let raw = trim('' + s);
	let t = lc(raw);
	if (substr(t, 0, 2) == '*.')
		t = substr(t, 2);
	while (length(t) > 0 && substr(t, 0, 1) == '.')
		t = substr(t, 1);
	while (length(t) > 0 && substr(t, -1) == '.')
		t = substr(t, 0, length(t) - 1);
	if (t == '')
		return perr(errs, sprintf('%s 不是有效的域名', raw));
	if (has_non_ascii(t))
		return perr(errs, sprintf('%s 含有非 ASCII 字符,请使用 punycode(xn--)写法', raw));
	if (index(t, ':') >= 0)
		return perr(errs, sprintf('%s 是 IPv6 地址,请填到「目的地址」里', raw));
	if (!match(t, /^[a-z0-9.-]+$/))
		return perr(errs, sprintf('%s 含有不允许的字符(只允许字母、数字、连字符和点)', raw));
	if (length(t) > 253)
		return perr(errs, sprintf('%s 超过 253 个字符', raw));
	let labels = split(t, '.');
	if (length(labels) < 2)
		return perr(errs, sprintf('%s 至少需要两段(如 example.com)', raw));
	for (let lb in labels) {
		if (lb == '')
			return perr(errs, sprintf('%s 含有空的段(连续的点)', raw));
		if (length(lb) > 63)
			return perr(errs, sprintf('%s 的某一段超过 63 个字符', raw));
		if (!match(lb, /^[a-z0-9]([a-z0-9-]*[a-z0-9])?$/))
			return perr(errs, sprintf('%s 的段不能以连字符开头或结尾', raw));
	}
	if (match(labels[length(labels) - 1], /^[0-9]+$/))
		return perr(errs, sprintf('%s 看起来是 IP 地址,请填到「目的地址」里', raw));
	return { text: t };
}

// 应用令牌 -> { kind:'app'|'cls', id, lo, hi, text };cls 展开为 N*1000..N*1000+999
function parse_app(s, errs) {
	if (s == null)
		return perr(errs, '应用不能为空');
	let raw = trim('' + s);
	let m = match(lc(raw), /^(app|cls):([0-9]{1,6})$/);
	if (!m)
		return perr(errs, sprintf('%s 不是合法的应用令牌(应为 app:<编号> 或 cls:<分类号>)', raw));
	let id = int(m[2]);
	if (m[1] == 'app') {
		if (id < 1 || id > 65535)
			return perr(errs, sprintf('%s 的应用编号必须在 1-65535 之间', raw));
		return { kind: 'app', id: id, lo: id, hi: id, text: sprintf('app:%d', id) };
	}
	if (id < 1 || id * 1000 + 999 > 65535)
		return perr(errs, sprintf('%s 的分类号必须在 1-65 之间', raw));
	return { kind: 'cls', id: id, lo: id * 1000, hi: id * 1000 + 999, text: sprintf('cls:%d', id) };
}

// ---------------------------------------------------------------- 规则解析(与上下文无关的部分)

// 逐项解析一个维度:返回去重保序的对象数组;每个坏值各出一条错误
function parse_dim(vals, parser, label, field, limit, errs) {
	let out = [];
	let seen = {};
	if (length(vals) > limit)
		put(errs, field, sprintf('%s最多 %d 项,当前 %d 项', label, limit, length(vals)));
	for (let v in vals) {
		let e = [];
		let r = parser(v, e);
		if (r == null) {
			put(errs, field, label + ' ' + (length(e) > 0 ? e[0] : (v + ' 无效')));
			continue;
		}
		if (seen[r.text])
			continue;
		seen[r.text] = true;
		push(out, r);
	}
	return out;
}

function texts_of(objs) {
	let out = [];
	for (let o in objs)
		push(out, o.text);
	return out;
}

function parse_target(t, errs) {
	let m = match(t, /^(wan|group):(.+)$/);
	if (!m)
		return perr(errs, sprintf('出口 %s 的格式不对(应为 wan:<出口标识> 或 group:<出口组编号>)', t));
	let id = trim(m[2]);
	if (m[1] == 'wan' && !match(id, /^[A-Za-z0-9._:-]+$/))
		return perr(errs, sprintf('出口标识 %s 含有不允许的字符', id));
	if (m[1] == 'group' && !match(id, /^[A-Za-z0-9_]+$/))
		return perr(errs, sprintf('出口组编号 %s 含有不允许的字符', id));
	return { kind: m[1], id: id, text: m[1] + ':' + id };
}

// 各族条目覆盖情况:match=all 时某族每个"带族维度"都要有条目才会生成该族的行
function family_cover(p) {
	let fam = { v4: true, v6: true, specific: false };
	let dims = [ p.src, p.dst ];
	for (let d in dims) {
		if (length(d.v4) + length(d.v6) == 0)
			continue;
		fam.specific = true;
		if (length(d.v4) == 0)
			fam.v4 = false;
		if (length(d.v6) == 0)
			fam.v6 = false;
	}
	if (length(p.dom) + length(p.app) > 0)
		fam.specific = true;
	return fam;
}

// 解析并做不依赖 ctx 的检查。返回 { n:规范化规则, p:解析结果, ndims, svc, eff_all, target }
function analyze(raw, errs, warns) {
	if (type(raw) != 'object')
		raw = {};
	let n = {};
	if (raw.id != null && trim('' + raw.id) != '')
		n.id = trim('' + raw.id);

	let name = replace(trim('' + (raw.name ?? '')), /[[:cntrl:]]/g, ' ');
	n.name = name;
	if (name == '')
		put(warns, 'name', '规则没有名称,建议起一个便于识别的名字');
	else if (length(name) > 64 && utf8_len(name) > 64)
		put(errs, 'name', '规则名称不能超过 64 个字符');
	n.enabled = to_bool(raw.enabled, true);
	n.note = replace(trim('' + (raw.note ?? '')), /[[:cntrl:]]/g, ' ');

	let mt = lc(trim('' + (raw.match ?? '')));
	if (mt == '')
		mt = 'all';
	if (mt != 'all' && mt != 'any') {
		put(errs, 'match', sprintf('匹配方式 %s 无效(只能是 all 或 any)', mt));
		mt = 'all';
	}
	n.match = mt;

	let tg = trim('' + (raw.target ?? ''));
	let tobj = null;
	if (tg == '')
		put(errs, 'target', '请选择出口');
	else {
		let e = [];
		tobj = parse_target(tg, e);
		if (!tobj)
			put(errs, 'target', e[0]);
	}
	n.target = tobj ? tobj.text : tg;

	let of = lc(trim('' + (raw.on_fail ?? '')));
	if (of == '' || of == 'backup')
		of = 'default';
	if (of != 'default' && of != 'block') {
		put(errs, 'on_fail', sprintf('出口不可用时的行为 %s 无效(只能是 default 或 block)', of));
		of = 'default';
	}
	n.on_fail = of;

	let pr = lc(trim('' + (raw.proto ?? '')));
	if (pr == '')
		pr = 'any';
	if (index(PROTOS, pr) < 0) {
		put(errs, 'proto', sprintf('协议 %s 无效(只能是 any、tcp、udp、tcp_udp 或 icmp)', pr));
		pr = 'any';
	}
	n.proto = pr;

	let rd = list_of(raw.dport);
	let rs = list_of(raw.sport);
	let p = {};
	p.dport = parse_dim(rd, parse_port, '目的端口', 'dport', MAX_PORTS, errs);
	p.sport = parse_dim(rs, parse_port, '源端口', 'sport', MAX_PORTS, errs);
	p.app = parse_dim(list_of(raw.app), parse_app, '应用', 'app', MAX_VALUES, errs);
	p.mac = parse_dim(list_of(raw.src_dev), parse_mac, '设备 MAC', 'src_dev', MAX_VALUES, errs);
	let sip = parse_dim(list_of(raw.src_ip), parse_addr, '源地址', 'src_ip', MAX_VALUES, errs);
	let dip = parse_dim(list_of(raw.dst_ip), parse_addr, '目的地址', 'dst_ip', MAX_VALUES, errs);
	p.dom = parse_dim(list_of(raw.domain), parse_domain, '域名', 'domain', MAX_VALUES, errs);
	p.src = { v4: [], v6: [] };
	p.dst = { v4: [], v6: [] };
	for (let a in sip)
		push(a.family == 4 ? p.src.v4 : p.src.v6, a);
	for (let a in dip)
		push(a.family == 4 ? p.dst.v4 : p.dst.v6, a);

	n.dport = texts_of(p.dport);
	n.sport = texts_of(p.sport);
	n.app = texts_of(p.app);
	n.src_dev = texts_of(p.mac);
	n.src_ip = texts_of(sip);
	n.dst_ip = texts_of(dip);
	n.domain = texts_of(p.dom);

	// 端口只对 TCP/UDP 有意义
	if (pr == 'icmp') {
		if (length(rd) > 0)
			put(errs, 'dport', 'ICMP 没有端口,请去掉目的端口或改选其它协议');
		if (length(rs) > 0)
			put(errs, 'sport', 'ICMP 没有端口,请去掉源端口或改选其它协议');
	}
	else if (pr == 'any') {
		if (length(rd) > 0)
			put(errs, 'dport', '指定目的端口前请先选择协议(TCP、UDP 或 TCP+UDP)');
		if (length(rs) > 0)
			put(errs, 'sport', '指定源端口前请先选择协议(TCP、UDP 或 TCP+UDP)');
	}

	let ndims = 0;
	for (let k in [ 'app', 'src_dev', 'src_ip', 'dst_ip', 'domain' ]) {
		if (length(list_of(raw[k])) > 0)
			ndims++;
	}
	let svc = (pr != 'any' || length(rd) > 0 || length(rs) > 0);
	if (ndims == 0 && !svc)
		put(errs, '_rule', '规则没有任何匹配条件');

	if (mt == 'any' && ndims == 1)
		put(warns, 'match', '只有 1 个匹配维度时,「任一」与「全部」效果相同,可以不设置');

	if (mt == 'all') {
		let fc = family_cover(p);
		if (fc.specific && !fc.v4 && !fc.v6)
			put(warns, '_rule', '同时满足 IPv4 与 IPv6 地址的流量不存在,将生成空规则');
	}

	return { n: n, p: p, ndims: ndims, svc: svc, eff_all: (mt == 'all' || ndims <= 1), target: tobj };
}

// ---------------------------------------------------------------- 上下文

function ensure_ctx(ctx) {
	if (ctx._memo == null)
		ctx._memo = {};
	if (ctx._gmap == null) {
		ctx._gmap = {};
		for (let g in (ctx.groups ?? [])) {
			if (g != null && g.id != null)
				ctx._gmap[g.id] = g;
		}
	}
	if (ctx._wmap == null) {
		ctx._wmap = {};
		for (let w in (ctx.wans ?? [])) {
			if (w != null && w.wan_id != null)
				ctx._wmap[w.wan_id] = w;
		}
	}
	if (ctx.apps == null)
		ctx.apps = { available: false, ids: {}, classes: {} };
	if (ctx.caps == null)
		ctx.caps = { dns_sets: true };
}

// 把 cfg / 快照 / 应用库整理成 §13.2 的 ctx。
// caps(可选):{ dns_capable:bool };未给出时只看 cfg.global.dns_sets。
function build_ctx(cfg, snap, apps, caps) {
	cfg = cfg ?? {};
	let g = cfg.global ?? {};
	let a = { available: false, ids: {}, classes: {} };
	if (type(apps) == 'object') {
		a.available = (apps.available == true);
		a.ids = apps.ids ?? {};
		a.classes = apps.classes ?? {};
	}
	let dns_ok = to_bool(g.dns_sets, true);
	if (caps != null && caps.dns_capable === false)
		dns_ok = false;
	let ctx = {
		rules: cfg.rules ?? [],
		groups: cfg.groups ?? [],
		wans: (snap && snap.wans) ? snap.wans : [],
		self_id: null,
		apps: a,
		caps: { dns_sets: dns_ok }
	};
	ensure_ctx(ctx);
	return ctx;
}

// ---------------------------------------------------------------- 遮蔽(启发式,宁漏报不误报)

function ints_cover(plist, lo, hi) {
	for (let r in plist) {
		if (r.lo <= lo && r.hi >= hi)
			return true;
	}
	return false;
}

function words_cover(plist, lo, hi) {
	for (let r in plist) {
		if (words_cmp(r.lo, lo) <= 0 && words_cmp(r.hi, hi) >= 0)
			return true;
	}
	return false;
}

function proto_covers(pp, cp) {
	if (pp == 'any' || pp == cp)
		return true;
	return (pp == 'tcp_udp' && (cp == 'tcp' || cp == 'udp'));
}

function addrs_covered(pitems, citems) {
	if (length(citems) == 0)
		return false;
	let pm = merge_addrs(pitems);
	for (let c in citems) {
		if (!words_cover(pm, c.lo, c.hi))
			return false;
	}
	return true;
}

function ports_covered(pitems, citems) {
	if (length(pitems) == 0)
		return true;
	if (length(citems) == 0)
		return false;
	let pm = merge_ports(pitems);
	for (let c in citems) {
		if (!ints_cover(pm, c.lo, c.hi))
			return false;
	}
	return true;
}

function domains_covered(pdoms, cdoms) {
	if (length(cdoms) == 0)
		return false;
	for (let c in cdoms) {
		let ok = false;
		for (let q in pdoms) {
			if (c.text == q.text || substr(c.text, -(length(q.text) + 1)) == '.' + q.text) {
				ok = true;
				break;
			}
		}
		if (!ok)
			return false;
	}
	return true;
}

// 前者 pa(更靠前)的匹配范围是否整体包含后者 ca 的
function covers(pa, ca) {
	if (!proto_covers(pa.n.proto, ca.n.proto))
		return false;
	if (!ports_covered(pa.p.dport, ca.p.dport) || !ports_covered(pa.p.sport, ca.p.sport))
		return false;
	if (length(pa.p.mac) > 0) {
		if (length(ca.p.mac) == 0)
			return false;
		let have = {};
		for (let m in pa.p.mac)
			have[m.text] = true;
		for (let m in ca.p.mac) {
			if (!have[m.text])
				return false;
		}
	}
	let pairs = [ [ pa.p.src, ca.p.src ], [ pa.p.dst, ca.p.dst ] ];
	for (let pr in pairs) {
		let pd = pr[0];
		let cd = pr[1];
		if (length(pd.v4) + length(pd.v6) == 0)
			continue;
		if (length(cd.v4) + length(cd.v6) == 0)
			return false;
		if (length(cd.v4) > 0 && !addrs_covered(pd.v4, cd.v4))
			return false;
		if (length(cd.v6) > 0 && !addrs_covered(pd.v6, cd.v6))
			return false;
	}
	if (length(pa.p.dom) > 0 && !domains_covered(pa.p.dom, ca.p.dom))
		return false;
	if (!ports_covered(pa.p.app, ca.p.app))
		return false;
	return true;
}

function find_shadow(cur, self_id, ctx) {
	let out = [];
	// 本规则是 any 且有多个维度时,"各维度取值是超集"推不出遮蔽,不报
	if (!cur.eff_all || !cur.n.enabled)
		return out;
	let rules = ctx.rules ?? [];
	for (let i = 0; i < length(rules); i++) {
		let r = rules[i];
		if (type(r) != 'object')
			continue;
		if (self_id != null && r.id == self_id)
			break;
		let key = (r.id != null) ? '' + r.id : 'i' + i;
		let pa = ctx._memo[key];
		if (pa == null) {
			let e = [];
			let w = [];
			pa = analyze(r, e, w);
			pa.bad = (length(e) > 0);
			ctx._memo[key] = pa;
		}
		if (pa.bad || !pa.n.enabled || !pa.eff_all)
			continue;
		// 目标出口组已不存在时 compile 会把该规则判 error 不下发,不能当作会先命中
		if (pa.target && pa.target.kind == 'group' && ctx._gmap[pa.target.id] == null)
			continue;
		if (pa.n.target == cur.n.target && pa.n.on_fail == cur.n.on_fail)
			continue;
		if (!covers(pa, cur))
			continue;
		push(out, {
			by: (r.id != null) ? '' + r.id : '',
			name: pa.n.name,
			reason: sprintf('可能被前面的规则「%s」遮蔽:它的匹配范围包含本规则,且出口不同,命中后不会再评估本规则', pa.n.name)
		});
	}
	return out;
}

// ---------------------------------------------------------------- 对外:规则

// 同 validate_rule,但多返回 parsed(已解析的各维度对象,compile 直接用)
// 与 ctx.no_shadow(true 时跳过遮蔽检测)。
function validate_rule_ex(raw, ctx) {
	if (ctx == null)
		ctx = {};
	ensure_ctx(ctx);
	let errors = [];
	let warnings = [];
	let a = analyze(raw, errors, warnings);
	let n = a.n;
	if (n.id == null && ctx.self_id != null)
		n.id = '' + ctx.self_id;

	// 出口
	if (a.target) {
		if (a.target.kind == 'group') {
			let g = ctx._gmap[a.target.id];
			if (g == null)
				put(errors, 'target', sprintf('出口组 %s 不存在', a.target.id));
			else if (!to_bool(g.enabled, true))
				put(warnings, 'target', sprintf('出口组「%s」已停用,该规则的流量会按「出口不可用」处理', g.name ?? a.target.id));
		}
		else if (ctx._wmap[a.target.id] == null)
			put(warnings, 'target', sprintf('出口 %s 当前不在设备列表里(设备未接入),接入前按「出口不可用」处理', a.target.id));
	}

	// 应用库
	if (length(n.app) > 0) {
		if (!ctx.apps.available)
			put(errors, 'app', '未安装行为管控(mwrt-behav),应用维度不可用');
		else {
			for (let o in a.p.app) {
				let tbl = (o.kind == 'app') ? ctx.apps.ids : ctx.apps.classes;
				if (tbl == null || !tbl['' + o.id])
					put(warnings, 'app', sprintf('应用库里没有 %s,将不会识别到任何流量', o.text));
			}
		}
	}

	// 域名依赖 dnsmasq 的 nftset
	if (length(n.domain) > 0 && ctx.caps.dns_sets == false)
		put(errors, 'domain', '域名维度不可用:dnsmasq 不支持 nftset,或已在全局设置里关闭「域名解析集合」');

	// 数量上限
	let total = length(ctx.rules ?? []);
	if (ctx.self_id == null && total >= MAX_RULES)
		put(errors, '_rule', sprintf('规则数量已达上限 %d 条', MAX_RULES));
	else if (total > MAX_RULES && !ctx.no_total)
		put(warnings, '_rule', sprintf('规则总数 %d 条超过上限 %d,排在后面的规则不会生效', total, MAX_RULES));

	let shadow = [];
	if (!ctx.no_shadow && length(errors) == 0)
		shadow = find_shadow(a, ctx.self_id, ctx);

	return { ok: (length(errors) == 0), errors: errors, warnings: warnings, normalized: n, shadow: shadow, parsed: a.p };
}

function validate_rule(raw, ctx) {
	let r = validate_rule_ex(raw, ctx);
	return { ok: r.ok, errors: r.errors, warnings: r.warnings, normalized: r.normalized, shadow: r.shadow };
}

// ---------------------------------------------------------------- 对外:出口组

function validate_group(raw, ctx) {
	if (ctx == null)
		ctx = {};
	ensure_ctx(ctx);
	if (type(raw) != 'object')
		raw = {};
	let errors = [];
	let warnings = [];
	let n = {};

	if (raw.id != null && trim('' + raw.id) != '') {
		n.id = trim('' + raw.id);
		if (!match(n.id, /^[A-Za-z0-9_]+$/))
			put(errors, '_rule', sprintf('出口组编号 %s 含有不允许的字符', n.id));
	}
	else if (ctx.self_id != null)
		n.id = '' + ctx.self_id;

	let name = replace(trim('' + (raw.name ?? '')), /[[:cntrl:]]/g, ' ');
	n.name = name;
	if (name == '')
		put(warnings, 'name', '出口组没有名称,建议起一个便于识别的名字');
	else if (length(name) > 64 && utf8_len(name) > 64)
		put(errors, 'name', '出口组名称不能超过 64 个字符');
	n.enabled = to_bool(raw.enabled, true);
	n.note = replace(trim('' + (raw.note ?? '')), /[[:cntrl:]]/g, ' ');

	let members = [];
	let seen = {};
	for (let m in list_of(raw.members)) {
		if (!match(m, /^[A-Za-z0-9._:-]+$/)) {
			put(errors, 'members', sprintf('出口标识 %s 含有不允许的字符', m));
			continue;
		}
		if (seen[m]) {
			put(warnings, 'members', sprintf('出口 %s 重复出现,已去重', m));
			continue;
		}
		seen[m] = true;
		push(members, m);
		if (ctx._wmap[m] == null)
			put(warnings, 'members', sprintf('出口 %s 当前不在设备列表里(设备未接入)', m));
	}
	n.members = members;
	if (length(members) == 0)
		put(errors, 'members', '出口组至少需要 1 个成员');
	else if (length(members) == 1)
		put(warnings, 'members', '只有 1 个成员的出口组没有备用出口');
	if (length(members) > MAX_MEMBERS)
		put(errors, 'members', sprintf('出口组成员最多 %d 个,当前 %d 个', MAX_MEMBERS, length(members)));

	if (ctx.self_id == null && length(ctx.groups ?? []) >= MAX_GROUPS)
		put(errors, '_rule', sprintf('出口组数量已达上限 %d 个', MAX_GROUPS));

	return { ok: (length(errors) == 0), errors: errors, warnings: warnings, normalized: n };
}

export {
	MAX_RULES, MAX_GROUPS, MAX_MEMBERS, MAX_VALUES, MAX_PORTS,
	to_bool, list_of,
	parse_mac, parse_addr, parse_port, parse_domain, parse_app,
	merge_addrs, merge_ports, ranges_text, words_fmt, words_cmp,
	validate_rule, validate_rule_ex, validate_group, build_ctx
};
