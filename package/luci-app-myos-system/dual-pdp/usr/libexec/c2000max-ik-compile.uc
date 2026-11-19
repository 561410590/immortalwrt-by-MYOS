#!/usr/bin/ucode

'use strict';

/*
 * Compile an administrator-supplied IKprotocol "extracted edition" into the
 * OAF v4 runtime grammar. Only decoded JSON/JSONL is consumed; app5.dat and
 * uploaded executables are never interpreted or run.
 */

import { open, error } from 'fs';

const MAX_APPS = 16384;
const MAX_CLASSES = 32;
const MAX_APPS_PER_CLASS = 512;
const MAX_RULES_PER_APP = 512;
const MAX_PATTERN_BYTES = 124;
const MAX_FEATURE_BYTES = 511;
const MAX_JSONL_BYTES = 16384;
const MAX_HTTP_CLAUSES = 4;

let supported_http_fields = {
	'0': true,  /* request target */
	'1': true,  /* Host */
	'2': true,  /* User-Agent */
	'3': true,  /* Referer */
	'5': true,  /* Cache-Control */
	'7': true,  /* Cookie */
	'9': true,  /* Pragma */
	'12': true, /* Content-Type */
	'13': true, /* Range */
	'15': true  /* Connection */
};

let input_dir = ARGV[0];
let output_dir = ARGV[1];
let skipped = {};
let source_ikapp_rules = 0;
let source_http_rules = 0;
let candidate_rules = 0;
let ik_context = {};
let http_context = {};
let ik_context_counts = {};
let http_context_counts = {};
let ik_context_methods = {};
let http_context_methods = {};
let compiled_ik_contexts = {};
let compiled_http_contexts = {};
let compiled_match_kinds = {};
let ik_constraints = {};
let predicate_owner = {};
let ambiguous_predicates = {};
let domain_owner = {};
let ambiguous_domains = {};

/* These entries describe transport protocols rather than a concrete
 * application.  They remain useful as a last-resort label, but must never
 * win before a lower-priority specific application has had its DPI window. */
let fallback_source_apps = {
	'1010000': true, /* 网页浏览 */
	'1060000': true, /* 其它HTTP */
	'1080000': true, /* HTTP404错误 */
	'1080001': true, /* HTTPS */
	'1080002': true, /* HTTP1 */
	'1080003': true  /* QUIC */
};

function fatal(message) {
	warn(`ik-native-v1: ${message}\n`);
	exit(1);
}

function skip(reason) {
	skipped[reason] = (skipped[reason] ?? 0) + 1;
}

function path_join(dir, name) {
	return dir + '/' + name;
}

function open_read(path) {
	let fp = open(path, 'r');
	if (!fp)
		fatal(`cannot open ${path}: ${error()}`);
	return fp;
}

function open_write(path) {
	let fp = open(path, 'w', 0600);
	if (!fp)
		fatal(`cannot create ${path}: ${error()}`);
	return fp;
}

function read_all(path) {
	let fp = open_read(path);
	let data = fp.read('all');
	fp.close();
	if (type(data) != 'string')
		fatal(`cannot read ${path}`);
	return data;
}

function require_int(obj, key, minimum, maximum, where) {
	let value = obj[key];
	if (type(value) != 'int' || value < minimum || value > maximum)
		fatal(`${where}.${key} is not an integer in ${minimum}..${maximum}`);
	return value;
}

function require_string(obj, key, where) {
	let value = obj[key];
	if (type(value) != 'string')
		fatal(`${where}.${key} is not a string`);
	return value;
}

function clean_name(value, fallback) {
	if (type(value) != 'string')
		value = fallback;
	value = trim(value);
	value = replace(value, /[[:cntrl:][:space:]]+/g, '_');
	value = replace(value, /[#:;,]+/g, '_');
	value = replace(value, /\[/g, '_');
	value = replace(value, /\]/g, '_');
	if (!length(value))
		value = fallback;
	if (length(value) > 63)
		fatal(`application name exceeds 63 bytes: ${value}`);
	return value;
}

function clean_class(value, suffix) {
	let name = clean_name(value, '未分类');
	if (suffix)
		name += suffix;
	if (length(name) > 31)
		fatal(`class name exceeds 31 bytes: ${name}`);
	return name;
}

function valid_sha256(value) {
	return type(value) == 'string' && length(value) == 64 &&
		match(value, /^[0-9a-f]{64}$/) != null;
}

function protocol_name(value) {
	if (value == 0)
		return 'any';
	if (value == 6)
		return 'tcp';
	if (value == 17)
		return 'udp';
	return null;
}

function ports_string(ranges, where) {
	if (type(ranges) != 'array')
		fatal(`${where}.port_range is not an array`);
	if (length(ranges) > 9)
		return { reason: 'too_many_port_ranges' };

	let values = [];
	for (let i = 0; i < length(ranges); i++) {
		let pair = ranges[i];
		if (type(pair) != 'array' || length(pair) != 2 ||
		    type(pair[0]) != 'int' || type(pair[1]) != 'int')
			fatal(`${where}.port_range[${i}] is invalid`);
		if (pair[0] < 1 || pair[1] < pair[0] || pair[1] > 65535)
			return { reason: 'invalid_port_range' };
		push(values, pair[0] == pair[1] ? `${pair[0]}` : `${pair[0]}-${pair[1]}`);
	}
	return { value: join('|', values) };
}

/*
 * The v4 matcher supports grouping, alternation, byte escapes and character
 * classes. Counted quantifiers, backreferences and lookaround are deliberately
 * unsupported, so reject them per rule instead of rejecting the whole upload.
 */
function regex_unsupported(raw) {
	let escaped = false;
	let in_class = false;
	let depth = 0;

	for (let i = 0; i < length(raw); i++) {
		let byte = ord(substr(raw, i, 1));
		if (escaped) {
			if (byte == 120) {
				if (i + 2 >= length(raw) ||
				    match(substr(raw, i + 1, 2), /^[0-9A-Fa-f]{2}$/) == null)
					return 'regex_invalid_hex_escape';
				i += 2;
			}
			else if ((byte >= 48 && byte <= 57) ||
			         (byte >= 65 && byte <= 90) ||
			         (byte >= 97 && byte <= 122)) {
				/* The kernel deliberately implements no PCRE character,
				 * word-boundary, control or numeric escape shorthand. */
				return 'regex_character_escape_unsupported';
			}
			escaped = false;
			continue;
		}
		if (byte == 92) {
			escaped = true;
			continue;
		}
		if (in_class) {
			if (byte == 93)
				in_class = false;
			continue;
		}
		if (byte == 91) {
			in_class = true;
			continue;
		}
		if (byte == 40) {
			if (i + 1 < length(raw) && ord(substr(raw, i + 1, 1)) == 63)
				return 'regex_group_extension';
			depth++;
			if (depth > 32)
				return 'regex_nesting_too_deep';
			continue;
		}
		if (byte == 41) {
			if (--depth < 0)
				return 'regex_unbalanced';
			continue;
		}
		if (byte == 123 && match(substr(raw, i), /^\{[0-9]+(,[0-9]*)?\}/) != null)
			return 'regex_counted_quantifier';
	}

	if (escaped || in_class || depth != 0)
		return 'regex_unbalanced';
	return null;
}

function make_feature(proto, dport, dir, pkt_seq, kind, offset, pattern,
		      priority, payload_lengths, server_addr, server_mask,
		      fallback) {
	let fields = [
		proto, '', dport, '', '', '', '', '0', `${dir}`, `${pkt_seq}`,
		kind, offset == null ? '' : `${offset}`, pattern, `${priority}`,
		payload_lengths ?? '', server_addr ?? '', server_mask ?? '',
		fallback ? '1' : '0'
	];
	let feature = join(';', fields);
	return length(feature) <= MAX_FEATURE_BYTES ? feature : null;
}

/* Priority controls search order; it is not additional evidence.  Likewise,
 * the fallback bit controls publication after a match.  Remove both when
 * comparing predicates so two concrete applications cannot keep the same
 * observable test merely because their source priorities differ. */
function feature_predicate_key(feature) {
	let fields = split(feature, ';');
	if (length(fields) != 18)
		fatal('internal feature does not contain 18 fields');
	fields[13] = ''; /* priority */
	fields[17] = ''; /* fallback */
	return join(';', fields);
}

function feature_as_fallback(feature) {
	let fields = split(feature, ';');
	if (length(fields) != 18)
		fatal('internal feature does not contain 18 fields');
	fields[17] = '1';
	return join(';', fields);
}

function domain_label_valid(label) {
	return (length(label) >= 2 && length(label) <= 63 &&
		match(label, /^[a-z0-9][a-z0-9-]*[a-z0-9]$/) != null) ||
		(length(label) == 1 && match(label, /^[a-z0-9]$/) != null);
}

/* Extract only a literal DNS suffix from SNI or a standalone Host predicate.
 * Regex support is deliberately narrow: anchors, an optional leading wildcard
 * and escaped dots are accepted, while alternation/classes/groups remain OAF-
 * only evidence. This avoids turning a multi-domain regex into an overbroad
 * dnsmasq policy. */
function dns_domain_suffix(raw, regex_mode) {
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
		if (!domain_label_valid(label))
			return null;
	return value;
}

function http_field_supported(index) {
	return supported_http_fields[`${index}`] === true;
}

function encode_http_clauses(clauses) {
	if (!length(clauses) || length(clauses) > MAX_HTTP_CLAUSES)
		return null;
	let packed = chr(length(clauses));
	for (let clause in clauses) {
		if (!http_field_supported(clause.field) || clause.method < 0 ||
		    clause.method > 2 || !length(clause.pattern) ||
		    length(clause.pattern) > 255)
			return null;
		packed += chr(clause.field) + chr(clause.method) +
			chr(length(clause.pattern)) + clause.pattern;
	}
	return length(packed) <= MAX_PATTERN_BYTES ? hexenc(packed) : null;
}

function parse_json_line(line, where) {
	if (type(line) != 'string' || !length(line) || length(line) > MAX_JSONL_BYTES)
		fatal(`${where} has an invalid line length`);
	try {
		let value = json(line);
		if (type(value) != 'object')
			fatal(`${where} is not a JSON object`);
		return value;
	}
	catch (e) {
		fatal(`${where} is invalid JSON: ${e}`);
	}
}

function increment(counter, key, amount) {
	counter[key] = (counter[key] ?? 0) + (amount ?? 1);
}

function ik_context_name(top) {
	if (top == 1)
		return 'slot';
	if (top == 17)
		return 'offset';
	if (top == 7)
		return 'destination';
	if (top == 9)
		return 'subnet';
	if (top == 11)
		return 'sequence_length';
	return null;
}

function http_context_name(top) {
	if (top == 2)
		return 'direct';
	if (top == 13)
		return 'exact_host';
	if (top == 14)
		return 'prefix';
	if (top == 15)
		return 'one_header';
	if (top == 16)
		return 'postfix';
	if (top == 18)
		return 'multifast';
	return null;
}

/*
 * app5.decode_raw.txt is protoc --decode_raw output.  The JSONL exporter
 * flattened recursive EXTEND containers and thereby lost the parent which
 * carries safety-critical context.  Recover only the small numeric stack we
 * need: every IK rule_id (field 20), every HTTP rule_id (field 8), the direct
 * child of root field 5, and that child's scalar field 1.  Pattern bytes still
 * come from JSONL, so no protobuf string unescaping is attempted here.
 */
function parse_raw_context(expected_ikapp, expected_http) {
	let name = 'app5.decode_raw.txt';
	let fp = open_read(path_join(input_dir, name));
	let stack = [];
	let current_top = null;
	let parent_field1 = null;
	let current_rule = null;
	let current_rule_depth = -1;
	let current_length_min = null;
	let current_subnet_addr = null;
	let line_no = 0;
	let ik_count = 0;
	let http_count = 0;

	for (;;) {
		let line = fp.read('line');
		if (line == null)
			break;
		line_no++;
		if (length(line) > MAX_JSONL_BYTES) {
			fp.close();
			fatal(`${name}:${line_no} exceeds the line limit`);
		}
		line = replace(line, /[\r\n]+$/, '');
		let spaces = 0;
		while (spaces < length(line) && substr(line, spaces, 1) == ' ')
			spaces++;
		if (spaces % 2) {
			fp.close();
			fatal(`${name}:${line_no} has invalid indentation`);
		}
		let depth = spaces / 2;
		let body = substr(line, spaces);
		let opened = match(body, /^([0-9]+) \{$/);
		if (opened) {
			let opened_field = int(opened[1]);
			stack = slice(stack, 0, depth);
			push(stack, opened_field);
			if (depth == 1) {
				current_top = opened_field;
				parent_field1 = null;
			}
			if (current_rule && depth == current_rule_depth + 1) {
				if (opened_field == 11)
					current_length_min = null;
				else if (opened_field == 18)
					current_subnet_addr = null;
			}
			continue;
		}
		let scalar = match(body, /^([0-9]+): ([0-9]+)$/);
		if (!scalar)
			continue;
		let field = int(scalar[1]);
		let value = int(scalar[2]);
		if (depth == 2 && field == 1)
			parent_field1 = value;
		let ik_top = current_top == 1 || current_top == 7 ||
			current_top == 9 || current_top == 11 || current_top == 17;
		/* Slot/prefix groups contain one or more hash buckets, so rule depth
		 * is not fixed.  A direct protobuf field 2 (protocol) below a field-1
		 * message is the unambiguous start of an IKAPP record. */
		if (!current_rule && ik_top && field == 2 && depth >= 3 &&
		    stack[depth - 1] == 1) {
			current_rule = {
				lengths: [], sequences: [], servers: [],
				explicit_offset: null
			};
			current_rule_depth = depth - 1;
			current_length_min = null;
			current_subnet_addr = null;
		}

		if (current_rule && depth == current_rule_depth + 2) {
			let parent = stack[current_rule_depth + 1];
			if (parent == 11) {
				if (field == 1)
					current_length_min = value;
				else if (field == 2) {
					if (current_length_min == null || value < current_length_min || value > 65535)
						fatal(`${name}:${line_no} has an invalid payload length range`);
					push(current_rule.lengths, [ current_length_min, value ]);
				}
			}
			else if (parent == 16 && field == 1) {
				if (value < 1 || value > 7)
					fatal(`${name}:${line_no} has an unsupported payload packet ordinal`);
				push(current_rule.sequences, value);
			}
			else if (parent == 17 && field == 1) {
				if (value < 0 || value > 4294967295)
					fatal(`${name}:${line_no} has an invalid destination IPv4 address`);
				push(current_rule.servers, [ value, 4294967295 ]);
			}
			else if (parent == 18) {
				if (field == 1)
					current_subnet_addr = value;
				else if (field == 2) {
					if (current_subnet_addr == null || value < 1 || value > 4294967295)
						fatal(`${name}:${line_no} has an invalid destination subnet`);
					push(current_rule.servers, [ current_subnet_addr, value ]);
				}
			}
		}

		if (field == 20) {
			let context_name = ik_context_name(current_top);
			if (!context_name) {
				fp.close();
				fatal(`${name}:${line_no} has IK rule ${value} in an unknown parent context`);
			}
			if (ik_context[`${value}`]) {
				fp.close();
				fatal(`${name}:${line_no} duplicates IK rule_id ${value}`);
			}
			if (!current_rule || depth != current_rule_depth + 1) {
				fp.close();
				fatal(`${name}:${line_no} cannot associate IK rule_id ${value} with its raw rule`);
			}
			ik_context[`${value}`] = {
				top: current_top,
				name: context_name,
				prefix_width: current_top == 17 ? parent_field1 : null,
				seen: false
			};
			ik_constraints[`${value}`] = current_rule;
			current_rule = null;
			current_rule_depth = -1;
			increment(ik_context_counts, context_name, 1);
			ik_count++;
			continue;
		}

		let http_name = http_context_name(current_top);
		if (field == 8 && http_name) {
			if (http_context[`${value}`]) {
				fp.close();
				fatal(`${name}:${line_no} duplicates HTTP rule_id ${value}`);
			}
			if (current_top != 13 && current_top != 2 &&
			    (type(parent_field1) != 'int' || parent_field1 < 0 || parent_field1 > 255)) {
				fp.close();
				fatal(`${name}:${line_no} has an invalid HTTP parent header index`);
			}
			http_context[`${value}`] = {
				top: current_top,
				name: http_name,
				header_index: current_top == 13 || current_top == 2 ? null : parent_field1,
				seen: false
			};
			increment(http_context_counts, http_name, 1);
			http_count++;
		}
	}
	fp.close();
	if (current_rule)
		fatal(`${name} ended inside an IK rule`);
	if (ik_count != expected_ikapp || http_count != expected_http)
		fatal(`${name} context counts ${ik_count}/${http_count} do not match README ${expected_ikapp}/${expected_http}`);
}

if (!input_dir || !output_dir || length(ARGV) != 2)
	fatal('usage: c2000max-ik-compile.uc <input-directory> <output-directory>');

let readme = read_all(path_join(input_dir, 'README.txt'));
let version_match = match(readme, /^IKprotocol ([0-9]+([.][0-9]+)*) extracted edition/);
if (!version_match)
	fatal('README.txt does not identify an IKprotocol extracted edition');
let version = version_match[1];

let map_fp = open_read(path_join(input_dir, 'appid-map.json'));
let app_list;
try {
	app_list = json(map_fp);
}
catch (e) {
	map_fp.close();
	fatal(`appid-map.json is invalid JSON: ${e}`);
}
map_fp.close();
if (type(app_list) != 'array' || !length(app_list) || length(app_list) > MAX_APPS)
	fatal('appid-map.json has an invalid application count');

let apps_by_source = {};
let apps_by_oaf = {};
let class_major = {};
let class_ids = [];
for (let map_index = 0; map_index < length(app_list); map_index++) {
	let row = app_list[map_index];
	let where = `appid-map.json[${map_index}]`;
	if (type(row) != 'object')
		fatal(`${where} is not an object`);
	/* The upstream catalog deliberately contains APPID 0 ("undefined").  It
	 * has no rules, but retaining it keeps source accounting complete. */
	let source_id = require_int(row, 'appid', 0, 999999999, where);
	let oaf_id = require_int(row, 'oaf_appid', 1001, 32512, where);
	let class_id = int(oaf_id / 1000);
	let sequence = oaf_id % 1000;
	if (class_id < 1 || class_id > MAX_CLASSES ||
	    sequence < 1 || sequence > MAX_APPS_PER_CLASS)
		fatal(`${where}.oaf_appid is outside the OAF class/sequence range`);
	if (apps_by_source[`${source_id}`] || apps_by_oaf[`${oaf_id}`])
		fatal(`${where} duplicates an application id`);

	let name = clean_name(require_string(row, 'name', where), `App_${source_id}`);
	let major = clean_class(require_string(row, 'major_category', where), '');
	let category = clean_name(require_string(row, 'category', where), major);
	if (class_major[`${class_id}`] && class_major[`${class_id}`] != major)
		fatal(`${where} changes the major category inside one OAF class`);
	if (!class_major[`${class_id}`]) {
		class_major[`${class_id}`] = major;
		push(class_ids, class_id);
	}
	let app = {
		source_id,
		oaf_id,
		class_id,
		name,
		major,
		category,
		fallback: fallback_source_apps[`${source_id}`] === true,
		features: [],
		feature_seen: {},
		domains: [],
		domain_seen: {}
	};
	apps_by_source[`${source_id}`] = app;
	apps_by_oaf[`${oaf_id}`] = app;
}

sort(class_ids, (a, b) => a - b);
sort(app_list, (a, b) => a.oaf_appid - b.oaf_appid);

let major_class_ids = {};
for (let class_id in class_ids) {
	let major = class_major[`${class_id}`];
	major_class_ids[major] ??= [];
	push(major_class_ids[major], class_id);
}

let class_names = {};
for (let major, ids in major_class_ids) {
	sort(ids, (a, b) => a - b);
	for (let index, class_id in ids) {
		let suffix = length(ids) > 1 ? `${index + 1}` : '';
		class_names[`${class_id}`] = clean_class(major, suffix);
	}
}

/* Runtime classes are only 32x512 storage shards.  Present the semantic leaf
 * category from the uploaded IK catalog instead of leaking shards such as
 * “网络游戏1/2” and “其它应用1..4” into LuCI. */
let category_names = [];
let category_seen = {};
for (let row in app_list) {
	let category = apps_by_source[`${row.appid}`].category;
	if (!category_seen[category]) {
		category_seen[category] = true;
		push(category_names, category);
	}
}
sort(category_names, (a, b) => a < b ? -1 : (a > b ? 1 : 0));
let category_ids = {};
for (let index = 0; index < length(category_names); index++)
	category_ids[category_names[index]] = index + 1;

function add_feature(source_id, feature, priority, rule_id, source_kind,
		     source_context, match_kind) {
	let app = apps_by_source[`${source_id}`];
	let predicate;
	if (!app) {
		skip('appid_not_catalog');
		return;
	}
	if (!feature) {
		skip('feature_too_long');
		return;
	}
	if (app.feature_seen[feature]) {
		skip('duplicate_feature');
		return;
	}
	predicate = feature_predicate_key(feature);
	app.feature_seen[feature] = true;
	push(app.features, {
		feature, predicate, priority, rule_id, source_kind, source_context,
		match_kind
	});
	/* Generic protocol labels are deliberately allowed to overlap a concrete
	 * application because they remain non-terminal until the DPI window ends.
	 * Two concrete owners, however, make the predicate intrinsically unable to
	 * identify either application and would turn file order into a false hit. */
	if (!app.fallback) {
		let owner = predicate_owner[predicate];
		if (owner == null)
			predicate_owner[predicate] = source_id;
		else if (owner != source_id)
			ambiguous_predicates[predicate] = true;
	}
	candidate_rules++;
}

function add_domain_feature(source_id, raw, regex_mode, priority, rule_id,
			    source_kind, source_context) {
	let domain = dns_domain_suffix(raw, regex_mode);
	let app = apps_by_source[`${source_id}`];
	if (!domain || !app || app.domain_seen[domain])
		return;
	app.domain_seen[domain] = true;
	push(app.domains, domain);
	let owner = domain_owner[domain];
	if (owner == null)
		domain_owner[domain] = source_id;
	else if (owner != source_id)
		ambiguous_domains[domain] = true;
}

function unique_numeric(values, minimum, maximum, where) {
	let seen = {};
	let output = [];
	for (let value in values) {
		if (type(value) != 'int' || value < minimum || value > maximum)
			fatal(`${where} contains a value outside ${minimum}..${maximum}`);
		if (!seen[`${value}`]) {
			seen[`${value}`] = true;
			push(output, value);
		}
	}
	sort(output, (a, b) => a - b);
	return output;
}

function payload_length_string(ranges, where) {
	if (!length(ranges))
		return '';
	if (length(ranges) > 5)
		fatal(`${where} contains more than five payload length ranges`);
	let output = [];
	for (let range in ranges) {
		if (type(range) != 'array' || length(range) != 2 ||
		    type(range[0]) != 'int' || type(range[1]) != 'int' ||
		    range[0] < 0 || range[1] < range[0] || range[1] > 65535)
			fatal(`${where} contains an invalid payload length range`);
		push(output, range[0] == range[1] ? `${range[0]}` :
			`${range[0]}-${range[1]}`);
	}
	return join('|', output);
}

function compile_ikapp(rec, where) {
	let source_id = require_int(rec, 'appid', 1, 999999999, where);
	if (require_string(rec, 'kind', where) != 'IKAPP')
		fatal(`${where}.kind is not IKAPP`);
	let proto_value = require_int(rec, 'proto', 0, 255, where);
	let proto = protocol_name(proto_value);
	if (!proto) {
		skip('unsupported_proto');
		return;
	}
	let dir = require_int(rec, 'dir', 0, 255, where);
	if (dir > 2) {
		skip('unsupported_direction');
		return;
	}
	let pkt_seq = require_int(rec, 'pkt_seq', 0, 255, where);
	if (pkt_seq > 7) {
		skip('unsupported_packet_sequence');
		return;
	}
	let match_flag = require_int(rec, 'match_flag', 0, 255, where);
	if (match_flag != 0) {
		skip('unsupported_match_flag');
		return;
	}
	let method = require_int(rec, 'match_method', 0, 255, where);
	let tls = require_int(rec, 'https_tls', 0, 255, where);
	if (tls > 2) {
		skip('unsupported_tls_mode');
		return;
	}
	let priority = require_int(rec, 'priority', 0, 255, where);
	let rule_id = require_int(rec, 'rule_id', 1, 999999999, where);
	if (!valid_sha256(require_string(rec, 'sha256', where)))
		fatal(`${where}.sha256 is invalid`);
	let context = ik_context[`${rule_id}`];
	if (!context)
		fatal(`${where} has no app5.decode_raw parent context`);
	if (context.seen)
		fatal(`${where} duplicates rule_id ${rule_id}`);
	context.seen = true;
	let constraints = ik_constraints[`${rule_id}`];
	if (!constraints)
		fatal(`${where} has no raw safety constraints`);
	increment(ik_context_methods,
		`${context.name}_method_${method}_tls_${tls}`, 1);
	if ((context.top == 7 || context.top == 9) &&
	    !length(constraints.servers))
		fatal(`${where} lost its destination constraint`);
	if (context.top == 11 && !length(constraints.lengths))
		fatal(`${where} lost its payload length constraint`);
	let sequence_values = unique_numeric(constraints.sequences, 1, 7,
		`${where}.raw_packet_sequences`);
	/* app5.decode_raw stores the accelerator table buckets used to reach an
	 * IKAPP row.  For payload signatures those buckets also preserve useful
	 * packet-ordinal constraints.  For reconstructed TLS ClientHello/SNI they
	 * do not: every https_tls=1 rule in IKprotocol 2.0.476 declares pkt_seq=0,
	 * while the parent table commonly says 1|2.  Applying that parent mask to
	 * the packet which completes a split ClientHello made a valid SNI visible
	 * to the parser but ineligible for matching.  Keep the source SNI semantic
	 * here; the kernel also ignores legacy SNI masks from an already imported
	 * R20.5 feature.cfg. */
	let pkt_seq_spec = tls == 1 ? `${pkt_seq}` :
		(length(sequence_values) ? join('|', sequence_values) : `${pkt_seq}`);
	let payload_lengths = payload_length_string(constraints.lengths,
		`${where}.raw_payload_lengths`);

	let encoded = require_string(rec, 'data_b64', where);
	let raw = b64dec(encoded);
	if (type(raw) != 'string' || b64enc(raw) != encoded)
		fatal(`${where}.data_b64 is not canonical base64`);
	let data_len = require_int(rec, 'data_len', 0, 65535, where);
	if (length(raw) != data_len)
		fatal(`${where}.data_len does not match data_b64`);
	if (data_len > MAX_PATTERN_BYTES) {
		skip('pattern_too_long');
		return;
	}

	let port_result = ports_string(rec.port_range, where);
	if (port_result.reason) {
		skip(port_result.reason);
		return;
	}
	let dport = port_result.value;
	let offset = rec.offset;
	if (offset != null && (type(offset) != 'int' || offset < -3000 || offset > 2999)) {
		skip('unsupported_offset');
		return;
	}
	/* EXTEND field 17 is prefix_match_apps: its parent field 1 is the
	 * prefix/hash width (2 or 4), not a payload byte offset.  These rules
	 * start at byte zero; applying 2/4 here was the primary false-negative
	 * bug in the previous adapter. */
	if (offset == null && context.top == 17)
		offset = 0;

	let kind;
	if (method == 2) {
		if (!length(dport) && !length(payload_lengths) &&
		    !length(constraints.servers)) {
			skip('unbounded_no_fixed_data');
			return;
		}
		kind = 'port';
		offset = null;
		raw = '';
	}
	else if (method == 0 || method == 4) {
		if (tls == 1)
			kind = 'sni_exact';
		else {
			/* A slot-context exact rule with neither its own offset nor a raw
			 * OffsetMatchAPP parent has no safe placement. Never guess it. */
			if (offset == null) {
				skip('exact_context_missing');
				return;
			}
			kind = tls == 2 ? 'tls_exact' : 'exact';
		}
	}
	else if (method == 5) {
		kind = tls == 1 ? 'sni_bm' : (tls == 2 ? 'tls_bm' : 'bm');
	}
	else if (method == 1) {
		let reason = regex_unsupported(raw);
		if (reason) {
			skip(reason);
			return;
		}
		kind = tls == 1 ? 'sni_regex' : (tls == 2 ? 'tls_regex' : 'regex');
	}
	else {
		skip('unsupported_match_method');
		return;
	}

	if (kind != 'port' && !length(raw)) {
		skip('empty_pattern');
		return;
	}
	let servers = constraints.servers;
	if (!length(servers))
		servers = [ [ null, null ] ];
	let server_seen = {};
	for (let server in servers) {
		let server_addr = server[0] == null ? '' : sprintf('%08x', server[0]);
		let server_mask = server[1] == null ? '' : sprintf('%08x', server[1]);
		let server_key = `${server_addr}/${server_mask}`;
		if (server_seen[server_key])
			continue;
		server_seen[server_key] = true;
		let feature = make_feature(proto, dport, dir, pkt_seq_spec, kind,
			kind == 'port' ? null : offset,
			kind == 'port' ? '' : hexenc(raw), priority,
			payload_lengths, server_addr, server_mask,
			apps_by_source[`${source_id}`].fallback);
		add_feature(source_id, feature, priority, rule_id, 'IKAPP',
			context.name, kind);
	}
	if (tls == 1 && kind != 'port')
		add_domain_feature(source_id, raw, method == 1, priority, rule_id,
			'IKAPP', context.name);
}

/* Compile the original HTTPINFO header list as one bounded AND expression.
 * Flattening each clause into an independent OAF rule caused the first broad
 * Host/URL expression to label unrelated applications.  The raw parent tells
 * us which index was used to build iKuai's prefix/postfix/multifast table; it
 * is an accelerator key, not permission to discard the other headers.
 *
 * In this library match_method 0 carries the regexp used by direct,
 * prefix/postfix, one-header and multifast tables.  exact_host is the one
 * exception: its field-1 key is a literal Host value.  Methods 4 and 5 are
 * bounded substring matches.  Unknown header indexes are skipped as a whole
 * rule instead of weakening an AND expression into an unsafe partial match. */
function compile_http(rec, where) {
	let source_id = require_int(rec, 'appid', 1, 999999999, where);
	if (require_string(rec, 'kind', where) != 'IKHTTPAPP')
		fatal(`${where}.kind is not IKHTTPAPP`);
	let dir = require_int(rec, 'dir', 0, 255, where);
	if (dir > 2) {
		skip('unsupported_direction');
		return;
	}
	let pkt_seq = require_int(rec, 'pkt_seq', 0, 255, where);
	if (pkt_seq > 7) {
		skip('unsupported_packet_sequence');
		return;
	}
	let match_flag = require_int(rec, 'match_flag', 0, 255, where);
	if (match_flag != 0) {
		skip('unsupported_match_flag');
		return;
	}
	let priority = require_int(rec, 'priority', 0, 255, where);
	let rule_id = require_int(rec, 'rule_id', 1, 999999999, where);
	if (!valid_sha256(require_string(rec, 'sha256', where)))
		fatal(`${where}.sha256 is invalid`);
	let context = http_context[`${rule_id}`];
	if (!context)
		fatal(`${where} has no app5.decode_raw parent context`);
	if (context.seen)
		fatal(`${where} duplicates rule_id ${rule_id}`);
	context.seen = true;
	if (type(rec.headers) != 'array')
		fatal(`${where}.headers is not an array`);
	if (!length(rec.headers) || length(rec.headers) > MAX_HTTP_CLAUSES) {
		skip('http_missing_header');
		return;
	}

	let clauses = [];
	let parent_index_seen = context.header_index == null;
	let exact_host_seen = context.top != 13;
	for (let i = 0; i < length(rec.headers); i++) {
		let header = rec.headers[i];
		let header_where = `${where}.headers[${i}]`;
		if (type(header) != 'object')
			fatal(`${header_where} is not an object`);
		let index = require_int(header, 'index', 0, 255, header_where);
		let method = require_int(header, 'match_method', 0, 255,
			header_where);
		let raw = require_string(header, 'data', header_where);
		increment(http_context_methods,
			`${context.name}_method_${method}`, 1);
		if (index == context.header_index)
			parent_index_seen = true;
		if (!http_field_supported(index)) {
			skip(`http_header_${index}_unsupported`);
			return;
		}
		if (!length(raw)) {
			skip('empty_pattern');
			return;
		}
		if (length(raw) > MAX_PATTERN_BYTES) {
			skip('pattern_too_long');
			return;
		}

		let clause_method;
		if (context.top == 13 && index == 1) {
			if (method != 0 || match(raw, /^[A-Za-z0-9._:-]+$/) == null) {
				skip('http_exact_host_key_unsupported');
				return;
			}
			clause_method = 0; /* full-length literal equality */
			exact_host_seen = true;
		}
		else if (method == 0) {
			let reason = regex_unsupported(raw);
			if (reason) {
				skip(reason);
				return;
			}
			clause_method = 2;
		}
		else if (method == 4 || method == 5) {
			clause_method = 1;
		}
		else {
			skip(`http_match_method_${method}_unsupported`);
			return;
		}
		push(clauses, {
			field: index,
			method: clause_method,
			pattern: raw
		});
	}
	if (!parent_index_seen)
		fatal(`${where} does not contain its raw parent header index`);
	if (!exact_host_seen) {
		skip('http_exact_host_key_missing');
		return;
	}
	let packed = encode_http_clauses(clauses);
	if (!packed) {
		skip('http_compound_pattern_too_long');
		return;
	}
	let feature = make_feature('tcp', '', dir, pkt_seq, 'http_multi', null,
		packed, priority, '', '', '',
		apps_by_source[`${source_id}`].fallback);
	add_feature(source_id, feature, priority, rule_id, 'HTTP', context.name,
		'http_multi');
	if (length(clauses) == 1 && clauses[0].field == 1)
		add_domain_feature(source_id, clauses[0].pattern,
			clauses[0].method == 2, priority, rule_id, 'HTTP', context.name);
}

function walk_jsonl(name, expected, callback) {
	let path = path_join(input_dir, name);
	let fp = open_read(path);
	let line_no = 0;
	for (;;) {
		let line = fp.read('line');
		if (line == null)
			break;
		line_no++;
		line = trim(line);
		if (!length(line)) {
			fp.close();
			fatal(`${name}:${line_no} is blank`);
		}
		callback(parse_json_line(line, `${name}:${line_no}`),
			`${name}:${line_no}`);
	}
	fp.close();
	if (line_no != expected)
		fatal(`${name} contains ${line_no} records but README.txt declares ${expected}`);
	return line_no;
}

let declared_apps_match = match(readme,
	/Leaf applications\/protocols:[[:space:]]*([0-9]+)/);
let declared_ikapp_match = match(readme,
	/IKAPP rules decoded:[[:space:]]*([0-9]+)/);
let declared_http_match = match(readme,
	/HTTP rules decoded:[[:space:]]*([0-9]+)/);
if (!declared_apps_match || !declared_ikapp_match || !declared_http_match)
	fatal('README.txt is missing source counts');
let declared_apps = int(declared_apps_match[1]);
let declared_ikapp = int(declared_ikapp_match[1]);
let declared_http = int(declared_http_match[1]);
if (declared_apps != length(app_list) || declared_ikapp < 0 || declared_http < 0)
	fatal('README.txt source counts do not match appid-map.json');

parse_raw_context(declared_ikapp, declared_http);
source_ikapp_rules = walk_jsonl('ikapp-rules.jsonl', declared_ikapp,
	(rec, where) => compile_ikapp(rec, where));
source_http_rules = walk_jsonl('http-rules.jsonl', declared_http,
	(rec, where) => compile_http(rec, where));
for (let rule_id, context in ik_context)
	if (!context.seen)
		fatal(`app5.decode_raw IK rule_id ${rule_id} is absent from ikapp-rules.jsonl`);
for (let rule_id, context in http_context)
	if (!context.seen)
		fatal(`app5.decode_raw HTTP rule_id ${rule_id} is absent from http-rules.jsonl`);

/* A NO_FIXED_DATA_MATCH row is weak evidence when the same application also
 * has a payload, SNI or HTTP signature.  Publishing that port/IP candidate on
 * the first packet made it terminal before a later, stronger packet could be
 * inspected, which is the main cross-application false-positive mode when the
 * source tables are flattened into OAF.  Keep it as a connection candidate:
 * a concrete signature may replace it, otherwise the kernel publishes it at
 * the bounded DPI window.  Port-only protocols (DNS/DHCP/SSH/etc.) remain
 * terminal so one-packet services do not disappear from the audit. */
let deferred_weak_features = 0;
for (let row in app_list) {
	let app = apps_by_source[`${row.appid}`];
	let has_specific = false;
	for (let item in app.features) {
		if (item.match_kind != 'port') {
			has_specific = true;
			break;
		}
	}
	if (!has_specific || app.fallback)
		continue;
	for (let item in app.features) {
		if (item.match_kind != 'port')
			continue;
		item.feature = feature_as_fallback(item.feature);
		deferred_weak_features++;
	}
}

/* An identical runtime predicate owned by more than one concrete application
 * cannot determine which application generated the flow.  Keeping whichever
 * row happens to be loaded first turns this source ambiguity into a stable
 * false positive, so remove every such predicate from all concrete owners.
 * Priority and fallback are deliberately excluded from identity; all actual
 * v4.2 packet constraints remain included.  Generic fallback applications
 * remain eligible because they are never terminal while DPI is pending. */
let ambiguous_runtime_features_removed = 0;
for (let row in app_list) {
	let app = apps_by_source[`${row.appid}`];
	let retained = [];
	let retained_domains = [];
	for (let item in app.features) {
		if (!app.fallback && ambiguous_predicates[item.predicate]) {
			ambiguous_runtime_features_removed++;
			skip('ambiguous_cross_app_feature');
			continue;
		}
		push(retained, item);
	}
	app.features = retained;
	for (let domain in app.domains)
		if (!ambiguous_domains[domain])
			push(retained_domains, domain);
	app.domains = retained_domains;
}

/* Retain at most the kernel's bounded per-application limit. Prefer original
 * higher-priority rules, then the stable upstream rule id. */
let compiled_apps = 0;
let compiled_rules = 0;
let compiled_classes = {};
let compiled_source_rule_seen = {};
let compiled_source_rules = 0;
for (let row in app_list) {
	let app = apps_by_source[`${row.appid}`];
	sort(app.features, (a, b) => {
		if (b.priority != a.priority)
			return b.priority - a.priority;
		if (a.rule_id != b.rule_id)
			return a.rule_id - b.rule_id;
		return a.feature < b.feature ? -1 : (a.feature > b.feature ? 1 : 0);
	});
	if (length(app.features) > MAX_RULES_PER_APP) {
		let dropped = length(app.features) - MAX_RULES_PER_APP;
		skipped.rule_limit = (skipped.rule_limit ?? 0) + dropped;
		app.features = slice(app.features, 0, MAX_RULES_PER_APP);
	}
	if (!length(app.features))
		continue;
	compiled_apps++;
	compiled_rules += length(app.features);
	compiled_classes[`${app.class_id}`] = true;
	for (let item in app.features) {
		let source_key = `${item.source_kind}:${item.rule_id}`;
		if (!compiled_source_rule_seen[source_key]) {
			compiled_source_rule_seen[source_key] = true;
			compiled_source_rules++;
		}
		increment(compiled_match_kinds, item.match_kind, 1);
		if (item.source_kind == 'IKAPP')
			increment(compiled_ik_contexts, item.source_context, 1);
		else
			increment(compiled_http_contexts, item.source_context, 1);
	}
}

let source_rules = source_ikapp_rules + source_http_rules;
let runtime_features = compiled_rules;
let skipped_rules = source_rules - compiled_source_rules;
let dns_domains = 0;
for (let row in app_list) {
	let app = apps_by_source[`${row.appid}`];
	if (!length(app.features))
		continue;
	sort(app.domains, (a, b) => a < b ? -1 : (a > b ? 1 : 0));
	dns_domains += length(app.domains);
}
if (runtime_features < 1 || compiled_apps < 1 || skipped_rules < 0)
	fatal('no safe native rules could be compiled');

let feature_fp = open_write(path_join(output_dir, 'feature.cfg'));
feature_fp.write(`#version v${version}-ikuai\n`);
feature_fp.write('#format v4.2\n');
feature_fp.write('# source_format ik-native-v1; one native feature per application line\n');
for (let class_id in class_ids) {
	if (!compiled_classes[`${class_id}`])
		continue;
	feature_fp.write(sprintf('#class ik_%02d %d %s\n', class_id,
		class_id, class_names[`${class_id}`]));
	for (let row in app_list) {
		let app = apps_by_source[`${row.appid}`];
		if (app.class_id != class_id || !length(app.features))
			continue;
		for (let item in app.features)
			feature_fp.write(`${app.oaf_id} ${app.name}:[${item.feature}]\n`);
	}
}
feature_fp.close();

/* A separate, immutable domain index lets dnsmasq populate policy-specific
 * IPv4 and IPv6 nft sets from real DNS answers. It is intentionally limited
 * to unique literal suffixes derived from safe SNI or standalone Host rules. */
let domains_fp = open_write(path_join(output_dir, 'domains.tsv'));
domains_fp.write('oaf_appid\tdomain\tsource_appid\n');
for (let row in app_list) {
	let app = apps_by_source[`${row.appid}`];
	if (!length(app.features))
		continue;
	for (let domain in app.domains)
		domains_fp.write(`${app.oaf_id}\t${domain}\t${app.source_id}\n`);
}
domains_fp.close();

/* The first four columns are the user-facing semantic catalog. Runtime OAF
 * class shards are retained in later columns solely for diagnostics. */
let catalog_fp = open_write(path_join(output_dir, 'catalog.tsv'));
catalog_fp.write('oaf_appid\tname\tcategory_id\tcategory_name\tsource_appid\tcompiled\truntime_class_id\truntime_class_name\n');
for (let row in app_list) {
	let app = apps_by_source[`${row.appid}`];
	catalog_fp.write(`${app.oaf_id}\t${app.name}\t${category_ids[app.category]}\t` +
		`${app.category}\t${app.source_id}\t${length(app.features) ? 1 : 0}\t` +
		`${app.class_id}\t${class_names[`${app.class_id}`]}\n`);
}
catalog_fp.close();

let report = {
	source_format: 'ik-native-v1',
	runtime_format: 'v4.2',
	source_version: version,
	source_apps: length(app_list),
	compiled_apps,
	skipped_apps: length(app_list) - compiled_apps,
	source_rules,
	source_ikapp_rules,
	source_http_rules,
	candidate_rules,
	compiled_rules: compiled_source_rules,
	runtime_features,
	dns_domains,
	skipped_rules,
	flattened_context_unavailable: false,
	flattened_context_missing_in_jsonl: true,
	raw_context_recovered: true,
	flattened_context_note: 'parent context is absent from JSONL and was recovered by rule_id from app5.decode_raw.txt',
	ik_parent_contexts: ik_context_counts,
	http_parent_contexts: http_context_counts,
	ik_context_methods,
	http_context_methods,
	compiled_ik_contexts,
	compiled_http_contexts,
	compiled_match_kinds,
	deferred_weak_features,
	ambiguous_runtime_features_removed,
	skipped_reasons: skipped
};
let report_fp = open_write(path_join(output_dir, 'conversion-report.json'));
report_fp.write(sprintf('%J\n', report));
report_fp.close();

let meta_fp = open_write(path_join(output_dir, 'native.meta'));
meta_fp.write('source_format=ik-native-v1\n');
meta_fp.write(`source_version=${version}\n`);
meta_fp.write('runtime_format=v4.2\n');
meta_fp.write(`source_apps=${length(app_list)}\n`);
meta_fp.write(`compiled_apps=${compiled_apps}\n`);
meta_fp.write(`skipped_apps=${length(app_list) - compiled_apps}\n`);
meta_fp.write(`source_rules=${source_rules}\n`);
meta_fp.write(`compiled_rules=${compiled_source_rules}\n`);
meta_fp.write(`runtime_features=${runtime_features}\n`);
meta_fp.write(`dns_domains=${dns_domains}\n`);
meta_fp.write(`skipped_rules=${skipped_rules}\n`);
meta_fp.write('flattened_context_unavailable=0\n');
meta_fp.write('flattened_context_missing_in_jsonl=1\n');
meta_fp.write('raw_context_recovered=1\n');
meta_fp.close();

print(sprintf('%J\n', {
	success: true,
	source_format: 'ik-native-v1',
	format: 'v4.2',
	version: `v${version}-ikuai`,
	source_apps: length(app_list),
	compiled_apps,
	source_rules,
	compiled_rules: compiled_source_rules,
	runtime_features,
	dns_domains,
	skipped_rules
}));
