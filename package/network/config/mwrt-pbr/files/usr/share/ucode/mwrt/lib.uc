'use strict';
// mwrt-pbr 公共函数。模块顶层只定义函数与常量,不读 UCI、不起进程、不写文件;
// 函数签名见 SPEC §1.3,A2/A3 直接 import。

import * as fs from 'fs';
import * as uci from 'uci';

// 运行时状态目录与出口标记位段(bit16-28,见 SPEC §4.1)
const RUN_DIR = '/var/run/mwrt-pbr';
const MARK_MASK = 0x1fff0000;

// str_hash 的结果缓存:wan_section 常被成批调用,避免重复 fork md5sum
const hash_cache = {};

// 单引号转义:整串放进单引号,内部的单引号写成 '\'';
// ucode 双引号串里反斜杠加单引号会被吞成单引号,所以用 chr(92) 拼出反斜杠
function sh_quote(s) {
	let t = (s == null) ? '' : sprintf('%s', s);
	return "'" + replace(t, "'", "'" + chr(92) + "''") + "'";
}

// stderr 并入 out;花括号分组保证管道/复合命令的 stderr 也被并入,换行结尾避免命令以 & 或 ; 结束时语法错误
function run(cmd) {
	let p = fs.popen('{ ' + cmd + "\n} 2>&1", 'r');
	if (p == null)
		return { rc: -1, out: '' };
	let out = p.read('all');
	let rc = p.close();
	return { rc: (rc == null) ? -1 : rc, out: (out == null) ? '' : out };
}

// 命令失败返回 [],调用方据此降级而不是崩溃
function run_lines(cmd) {
	let r = run(cmd);
	let res = [];
	if (r.rc != 0)
		return res;
	for (let raw in split(r.out, "\n")) {
		let l = trim(raw);
		if (l != '')
			push(res, l);
	}
	return res;
}

// busybox logger 不一定认 warn 这个别名,统一写成 warning
function log(level, msg) {
	let lv = (level in ['debug', 'info', 'notice', 'warn', 'err']) ? level : 'info';
	if (lv == 'warn')
		lv = 'warning';
	run(sprintf('logger -t mwrt-pbr -p daemon.%s -- %s', lv, sh_quote(msg)));
}

function read_json(path, dflt) {
	let s = fs.readfile(path);
	if (s == null || trim(s) == '')
		return dflt;
	let o = null;
	try {
		o = json(s);
	}
	catch (e) {
		return dflt;
	}
	return (o == null) ? dflt : o;
}

function mkdir_p(dir) {
	if (dir == null || dir == '' || dir == '/' || dir == '.')
		return true;
	if (fs.stat(dir) != null)
		return true;
	if (!mkdir_p(fs.dirname(dir)))
		return false;
	// 并发创建时 mkdir 会失败,以目录最终存在为准
	return (fs.mkdir(dir) == true) || (fs.stat(dir) != null);
}

// 先写同目录临时文件再 rename,读者永远看不到半截内容
function write_atomic(path, text) {
	if (path == null || path == '')
		return false;
	if (!mkdir_p(fs.dirname(path)))
		return false;
	let tmp = path + '.tmp';
	let n = fs.writefile(tmp, (text == null) ? '' : text);
	if (n == null) {
		fs.unlink(tmp);
		return false;
	}
	if (!fs.rename(tmp, path)) {
		fs.unlink(tmp);
		return false;
	}
	return true;
}

function write_json_atomic(path, obj) {
	return write_atomic(path, sprintf('%J', obj) + "\n");
}

// 返回 32 位十六进制 md5;文件不存在或 md5sum 失败返回 null
function file_hash(path) {
	if (path == null || fs.stat(path) == null)
		return null;
	let r = run('md5sum ' + sh_quote(path));
	if (r.rc != 0)
		return null;
	let m = match(r.out, /^[0-9a-f]+/);
	if (m == null || length(m[0]) != 32)
		return null;
	return m[0];
}

// 字符串 md5 的前 12 位;printf %s 保证不带换行,与 echo 的实现差异无关
function str_hash(s) {
	let t = (s == null) ? '' : sprintf('%s', s);
	if (hash_cache[t] != null)
		return hash_cache[t];
	let r = run('printf %s ' + sh_quote(t) + ' | md5sum');
	if (r.rc != 0)
		return null;
	let m = match(r.out, /^[0-9a-f]+/);
	if (m == null || length(m[0]) != 32)
		return null;
	let h = substr(m[0], 0, 12);
	hash_cache[t] = h;
	return h;
}

function now() {
	return time();
}

// ucode 的 sleep 以毫秒计
function sleep_s(n) {
	if (n == null || n <= 0)
		return;
	sleep(int(n * 1000));
}

// 只在函数内调用,保持模块顶层无副作用
function uci_cursor() {
	return uci.cursor();
}

// mwrt_pbr.wan 覆盖段名,wans.uc 与 config.uc 共用(SPEC §13.1);wan_id 为空或 md5sum 不可用时返回 null,
// 避免所有空 id 落到同一个段名上
function wan_section(wan_id) {
	if (wan_id == null || wan_id == '')
		return null;
	let h = str_hash(wan_id);
	if (h == null)
		return null;
	return 'w_' + substr(h, 0, 8);
}

export { sh_quote, run, run_lines, log, read_json, write_json_atomic, write_atomic, file_hash, str_hash, now, sleep_s, uci_cursor, wan_section, RUN_DIR, MARK_MASK };
