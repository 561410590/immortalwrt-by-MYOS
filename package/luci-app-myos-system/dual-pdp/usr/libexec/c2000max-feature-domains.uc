#!/usr/bin/ucode

'use strict';

import { open } from 'fs';

let feature_path = ARGV[0];
let index_path = ARGV[1];
let owners = {};
let ambiguous = {};

function valid_appid(value) {
	if (type(value) != 'string' || match(value, /^[0-9]+$/) == null)
		return false;
	let id = int(value);
	let class_id = int(id / 1000);
	let sequence = id % 1000;
	return class_id >= 1 && class_id <= 32 && sequence >= 1 && sequence <= 512;
}

function valid_label(label) {
	return (length(label) >= 2 && length(label) <= 63 &&
		match(label, /^[a-z0-9][a-z0-9-]*[a-z0-9]$/) != null) ||
		(length(label) == 1 && match(label, /^[a-z0-9]$/) != null);
}

function normalize_domain(raw, regex_mode) {
	if (type(raw) != 'string' || !length(raw) || length(raw) > 253)
		return null;
	let value = lc(trim(raw));
	if (regex_mode) {
		if (substr(value, 0, 1) == '^')
			value = substr(value, 1);
		if (length(value) && substr(value, -1) == '$')
			value = substr(value, 0, length(value) - 1);
		if (substr(value, 0, 4) == '.*\\.')
			value = substr(value, 4);
		else if (substr(value, 0, 2) == '\\.')
			value = substr(value, 2);
		value = replace(value, /\\[.]/g, '.');
		if (match(value, /[\\*+?(){}\[\]|]/) != null)
			return null;
	}
	else {
		if (substr(value, 0, 2) == '*.')
			value = substr(value, 2);
		else if (substr(value, 0, 1) == '.')
			value = substr(value, 1);
		let port = match(value, /^([^:]+):[0-9]+$/);
		if (port)
			value = port[1];
	}
	if (length(value) < 3 || length(value) > 253 ||
	    match(value, /^[a-z0-9.-]+$/) == null ||
	    substr(value, 0, 1) == '.' || substr(value, -1) == '.' ||
	    index(value, '..') >= 0)
		return null;
	let labels = split(value, '.');
	if (length(labels) < 2)
		return null;
	for (let label in labels)
		if (!valid_label(label))
			return null;
	return value;
}

function add_domain(appid, domain) {
	domain = normalize_domain(domain, false);
	if (!valid_appid(appid) || !domain)
		return;
	let old = owners[domain];
	if (old == null)
		owners[domain] = appid;
	else if (old != appid)
		ambiguous[domain] = true;
}

function decode_hex(value) {
	if (type(value) != 'string' || length(value) % 2 ||
	    match(value, /^[0-9a-fA-F]+$/) == null)
		return null;
	try {
		return hexdec(value);
	}
	catch (e) {
		return null;
	}
}

function decode_qname(raw) {
	let labels = [];
	let offset = 0;
	while (offset < length(raw)) {
		let size = ord(substr(raw, offset++, 1));
		if (size == 0)
			return offset == length(raw) ? normalize_domain(join('.', labels), false) : null;
		if (size > 63 || offset + size > length(raw))
			return null;
		push(labels, substr(raw, offset, size));
		offset += size;
	}
	return null;
}

function parse_http_multi(raw) {
	if (length(raw) < 4 || ord(substr(raw, 0, 1)) != 1)
		return null;
	let field = ord(substr(raw, 1, 1));
	let method = ord(substr(raw, 2, 1));
	let size = ord(substr(raw, 3, 1));
	if (field != 1 || method > 2 || size < 1 || 4 + size != length(raw))
		return null;
	return normalize_domain(substr(raw, 4, size), method == 2);
}

function parse_index(path) {
	if (!path)
		return;
	let fp = open(path, 'r');
	if (!fp)
		return;
	let first = true;
	for (;;) {
		let line = fp.read('line');
		if (line == null)
			break;
		line = replace(line, /[\r\n]+$/, '');
		if (first) {
			first = false;
			if (line == 'oaf_appid\tdomain\tsource_appid')
				continue;
		}
		let fields = split(line, '\t');
		if (length(fields) >= 2)
			add_domain(fields[0], fields[1]);
	}
	fp.close();
}

function parse_legacy_hosts(appid, signature) {
	// Inst's bundled OAF v3 profile stores literal hosts in column four.
	for (let rule in split(signature, ',')) {
		let fields = split(rule, ';');
		if (length(fields) >= 4 && (fields[0] == 'tcp' || fields[0] == 'udp'))
			add_domain(appid, fields[3]);
	}
}

function parse_feature(path) {
	let fp = open(path, 'r');
	if (!fp)
		exit(1);
	let legacy = false;
	for (;;) {
		let line = fp.read('line');
		if (line == null)
			break;
		line = trim(line);
		if (match(line, /^#format[[:space:]]+v[23]\./) != null) {
			legacy = true;
			continue;
		}
		let row = match(line, /^([0-9]+)[[:space:]]+[^:]+:\[(.+)\]$/);
		if (!row || !valid_appid(row[1]))
			continue;
		let fields = split(row[2], ';');
		if (legacy) {
			parse_legacy_hosts(row[1], row[2]);
			continue;
		}
		if (length(fields) != 18)
			continue;
		let kind = fields[10];
		let raw = decode_hex(fields[12]);
		if (raw == null)
			continue;
		let domain = null;
		if (kind == 'dns_bm')
			domain = decode_qname(raw);
		else if (kind == 'sni_exact' || kind == 'sni_bm' ||
		         kind == 'http_host_exact' || kind == 'http_host_bm')
			domain = normalize_domain(raw, false);
		else if (kind == 'sni_regex' || kind == 'http_host_regex')
			domain = normalize_domain(raw, true);
		else if (kind == 'http_multi')
			domain = parse_http_multi(raw);
		if (domain)
			add_domain(row[1], domain);
	}
	fp.close();
}

if (!feature_path || length(ARGV) > 2)
	exit(2);

parse_index(index_path);
parse_feature(feature_path);
for (let domain, appid in owners)
	if (!ambiguous[domain])
		print(`${appid}\t${domain}\n`);
