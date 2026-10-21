'use strict';
'require baseclass';
'require dom';
'require network';
'require rpc';
'require uci';
'require ui';

var callFrequencies = rpc.declare({
	object: 'iwinfo', method: 'freqlist', params: [ 'device' ], expect: { results: [] }
});
var callPowers = rpc.declare({
	object: 'iwinfo', method: 'txpowerlist', params: [ 'device' ], expect: { results: [] }
});
var nextId = 0;

function textValue(value) {
	return value == null ? '' : String(value);
}

function byteLength(value) {
	return new TextEncoder().encode(value).length;
}

function encryptionMode(encryption) {
	var value = textValue(encryption);
	if (value === 'none') return value;
	// These cipher names and aliases match the existing LuCI expert editor.
	var match = /^(psk-mixed|psk2|sae-mixed|sae)(?:\+(?:ccmp|aes|ccmp256|gcmp|gcmp256|tkip|tkip\+(?:ccmp|aes)|(?:ccmp|aes)\+tkip))?$/.exec(value);
	return match ? match[1] : '';
}

function personal(encryption) {
	var mode = encryptionMode(encryption);
	return !!mode && mode !== 'none';
}

function cloneBasic(profile) {
	return {
		enabled: profile.enabled, ssid: profile.ssid,
		encryption: profile.encryption, key: profile.key
	};
}

function sameBasic(a, b) {
	return a.enabled === b.enabled && a.ssid === b.ssid &&
		a.encryption === b.encryption && a.key === b.key;
}

function bandOf(radio, device) {
	if ([ '2g', '5g', '6g', '60g' ].indexOf(radio.band) >= 0)
		return radio.band;
	var frequency = device ? Number(device.ubus('dev', 'iwinfo', 'frequency')) : 0;
	if (frequency >= 2400 && frequency < 2500) return '2g';
	if (frequency >= 4900 && frequency < 5925) return '5g';
	if (frequency >= 5925 && frequency < 7126) return '6g';
	if (frequency >= 57000) return '60g';
	if (radio.hwmode === '11g' || radio.hwmode === '11b') return '2g';
	if (radio.hwmode === '11a') return '5g';
	return '';
}

function bandLabel(band) {
	return { '2g': '2.4 GHz', '5g': '5 GHz', '6g': '6 GHz', '60g': '60 GHz' }[band] || '无线网络';
}

function frequencyBand(mhz, band) {
	if ([ 2, 5, 6, 60 ].indexOf(Number(band)) >= 0)
		return Number(band) + 'g';
	if (mhz >= 2400 && mhz < 2500) return '2g';
	if (mhz >= 4900 && mhz < 5925) return '5g';
	if (mhz >= 5925 && mhz < 7126) return '6g';
	if (mhz >= 57000) return '60g';
	return '';
}

function frequencyList(profile, frequencies) {
	if (Array.isArray(frequencies) && frequencies.length) return frequencies;
	if (profile.type !== 'mtwifi') return [];
	// Match the MTK expert view when an inactive radio cannot report its list.
	var channels = profile.band === '2g'
		? [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ]
		: profile.band === '5g'
			? [ 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165 ] : [];
	return channels.map(function(channel) {
		return { channel: channel, mhz: profile.band === '2g' ? 2407 + channel * 5 : 5000 + channel * 5 };
	});
}

function field(label, control, hint) {
	var target = control.matches('input, select, button') ? control : control.querySelector('input, select, button');
	var id = target.id || ('myos-wifi-' + (++nextId));
	target.id = id;
	var content = E('div', { 'class': 'wifi-control' }, [ control ]);
	if (hint) content.appendChild(E('p', { 'class': 'wifi-hint' }, hint));
	return E('div', { 'class': 'wifi-field' }, [
		E('label', { 'class': 'wifi-label', 'for': id }, label), content
	]);
}

function toggle(value, label, readonly, onChange) {
	var input = E('input', {
		type: 'checkbox', checked: value ? '' : null, disabled: readonly ? '' : null,
		'aria-label': label,
		change: function() { onChange(this.checked); }
	});
	return E('label', { 'class': 'wifi-switch' }, [
		input, E('span', { 'class': 'wifi-switch-track', 'aria-hidden': 'true' }),
		E('span', { 'class': 'wifi-switch-label' }, label)
	]);
}

function select(value, choices, readonly, onChange) {
	var list = choices.slice();
	if (!list.some(function(choice) { return String(choice[0]) === value; }))
		list.push([ value, value ? value + '（当前）' : '驱动默认' ]);
	return E('select', {
		'class': 'cbi-input-select', disabled: readonly ? '' : null,
		change: function() { onChange(this.value); }
	}, list.map(function(choice) {
		return E('option', { value: String(choice[0]), selected: String(choice[0]) === value ? '' : null, disabled: choice[2] ? '' : null }, choice[1]);
	}));
}

function encryptionChoices(profiles) {
	var choices = [ [ 'psk-mixed', 'WPA / WPA2 个人版' ], [ 'psk2', 'WPA2 个人版' ] ];
	var hasSAE = profiles.every(function(profile) {
		return profile.type === 'mtwifi' || L.hasSystemFeature('hostapd', 'sae');
	});
	if (hasSAE) {
		choices.push([ 'sae-mixed', 'WPA2 / WPA3 个人版' ]);
		choices.push([ 'sae', 'WPA3 个人版' ]);
	}
	choices.push([ 'none', '开放网络（无密码）' ]);
	return choices;
}

function modeLabel(mode) {
	var match = /^(EHT|HE|VHT|HT)(\d+)$/.exec(mode);
	if (!match) return mode || '传统模式';
	return { EHT: 'Wi-Fi 7', HE: 'Wi-Fi 6', VHT: 'Wi-Fi 5', HT: 'Wi-Fi 4' }[match[1]] + ' · ' + match[2] + ' MHz';
}

return baseclass.extend({
	render: function() {
		this.readonly = !L.hasViewPermission();
		this.busy = false;
		this.advanced = [];
		return network.getWifiDevices().then(L.bind(function(devices) {
			var ifaces = uci.sections('wireless', 'wifi-iface');
			var radios = uci.sections('wireless', 'wifi-device');
			this.profiles = [];
			this.unmanaged = 0;
			radios.forEach(L.bind(function(radio) {
				var candidates = ifaces.filter(function(iface) {
					return iface.device === radio['.name'] && (iface.mode === 'ap' || iface.mode === 'ap-wds');
				});
				var lan = candidates.filter(function(iface) { return L.toArray(iface.network).indexOf('lan') >= 0; });
				var iface = lan.length === 1 ? lan[0] : candidates.length === 1 ? candidates[0] : null;
				if (!iface) { this.unmanaged += candidates.length; return; }
				this.unmanaged += candidates.length - 1;
				var device = devices.filter(function(item) { return item.getName() === radio['.name']; })[0];
				var profile = {
					radio: radio['.name'], iface: iface['.name'], type: radio.type,
					band: bandOf(radio, device), device: device,
					enabled: radio.disabled !== '1' && iface.disabled !== '1',
					ssid: textValue(iface.ssid), encryption: textValue(iface.encryption) || 'none',
					key: personal(iface.encryption || '') ? textValue(iface.key) : '',
					original: {}, draft: {}, frequencies: [], powers: []
				};
				profile.original.basic = cloneBasic(profile);
				profile.draft = cloneBasic(profile);
				[ 'channel', 'htmode', 'txpower' ].forEach(function(option) {
					profile.original[option] = textValue(radio[option]);
					profile[option] = textValue(radio[option]);
				});
				[ 'hidden', 'isolate', 'maxassoc' ].forEach(function(option) {
					profile.original[option] = textValue(iface[option]);
					profile[option] = textValue(iface[option]);
				});
				this.profiles.push(profile);
			}, this));
			this.profiles.sort(function(a, b) {
				return [ '2g', '5g', '6g', '60g', '' ].indexOf(a.band) - [ '2g', '5g', '6g', '60g', '' ].indexOf(b.band);
			});
			var two = this.profiles.filter(function(p) { return p.band === '2g'; });
			var five = this.profiles.filter(function(p) { return p.band === '5g'; });
			this.pair = two.length === 1 && five.length === 1 ? [ two[0], five[0] ] : [];
			// Enterprise, WEP and mesh-specific credentials remain in the expert view.
			if (!this.pair.every(function(p) { return p.encryption === 'none' || personal(p.encryption); })) this.pair = [];
			this.merged = this.pair.length === 2 && sameBasic(this.pair[0].draft, this.pair[1].draft);
			this.shared = this.pair.length ? cloneBasic(this.pair[1].draft) : null;
			return Promise.all(this.profiles.map(function(profile) {
				return Promise.all([
					L.resolveDefault(callFrequencies(profile.radio), []),
					profile.type === 'mtwifi' ? Promise.resolve([]) : L.resolveDefault(callPowers(profile.radio), [])
				]).then(function(data) {
					profile.frequencies = frequencyList(profile, data[0]);
					profile.frequencyFallback = (!Array.isArray(data[0]) || !data[0].length) && profile.frequencies.length > 0;
					profile.powers = data[1];
				});
			}));
		}, this)).then(L.bind(this.renderPage, this));
	},

	renderPage: function() {
		var expert = L.url('admin/network/wireless') + '?expert=1';
		this.cards = E('div', { 'class': 'wifi-cards' });
		this.message = E('div', { 'class': 'wifi-feedback', role: 'status', 'aria-live': 'polite' });
		this.saveButton = E('button', {
			type: 'button', 'class': 'cbi-button cbi-button-apply wifi-save',
			disabled: this.readonly || !this.profiles.length ? '' : null,
			click: L.bind(this.save, this)
		}, '保存并应用');
		var sections = [
			E('link', { rel: 'stylesheet', href: L.resource('tools/myos-wifi.css') + '?v=20261009-2' }),
			E('div', { 'class': 'wifi-heading' }, [
				E('div', {}, [ E('h2', {}, 'Wi-Fi 设置'), E('p', {}, '轻松设置无线名称、密码和双频连接。') ]),
				E('a', { 'class': 'wifi-expert', href: expert }, '专家设置 ↗')
			])
		];
		if (this.pair.length === 2) {
			sections.push(E('section', { 'class': 'wifi-combine wifi-card' }, [
				E('div', {}, [ E('h3', {}, '双频合一'), E('p', { 'class': 'wifi-hint' }, '2.4 GHz 和 5 GHz 使用相同名称、密码，由终端选择频段。') ]),
					toggle(this.merged, '双频合一', this.readonly, L.bind(function(value) {
					if (value) {
						this.shared = cloneBasic(this.pair[1].draft);
						this.shared.ssid = this.shared.ssid.replace(/[-_ ](?:2\.4G|5G)$/i, '');
						this.shared.enabled = this.pair.some(function(profile) { return profile.draft.enabled; });
					} else {
						this.pair.forEach(L.bind(function(profile) {
							profile.draft = cloneBasic(this.shared);
							var suffix = profile.band === '2g' ? '-2.4G' : '-5G';
							var base = Array.from(this.shared.ssid.replace(/[-_ ](?:2\.4G|5G)$/i, ''));
							while (byteLength(base.join('') + suffix) > 32) base.pop();
							profile.draft.ssid = base.join('') + suffix;
						}, this));
					}
					this.merged = value;
					this.renderCards();
				}, this))
			]));
		}
		sections.push(this.cards);
		if (!this.profiles.length) sections.push(E('p', { 'class': 'wifi-empty' }, '暂无可简化配置的无线接入点，请进入专家设置。'));
		if (this.unmanaged) sections.push(E('p', { 'class': 'wifi-hint' }, '访客、多 SSID 等其他无线网络可在专家设置中管理。'));
		sections.push(this.message, E('div', { 'class': 'wifi-actions' }, [
			E('span', { 'class': 'wifi-hint' }, '修改名称或密码后，无线设备需要重新连接。'), this.saveButton
		]));
		this.root = E('div', { id: 'myos-simple-wifi' }, sections);
		this.renderCards();
		return this.root;
	},

	renderCards: function() {
		var cards = [], handled = [];
		if (this.merged) {
			cards.push(this.renderCard(this.shared, this.pair, '双频 Wi-Fi', '2.4 GHz + 5 GHz'));
			handled = this.pair;
		}
		this.profiles.forEach(L.bind(function(profile) {
			if (handled.indexOf(profile) < 0)
				cards.push(this.renderCard(profile.draft, [ profile ], bandLabel(profile.band) + ' Wi-Fi', profile.band === '2g' ? '覆盖更远' : profile.band === '5g' ? '高速连接' : '独立频段'));
		}, this));
		this.cards.classList.toggle('wifi-cards-split', !this.merged && cards.length > 1);
		dom.content(this.cards, cards);
	},

	renderCard: function(draft, profiles, title, subtitle) {
		var mode = encryptionMode(draft.encryption), supported = !!mode;
		var enabledBadge = E('span', { 'class': 'wifi-badge' + (draft.enabled ? ' is-on' : '') }, draft.enabled ? '已开启' : '已关闭');
		var enabledControl = toggle(draft.enabled, 'Wi-Fi 开关', this.readonly, function(value) {
			draft.enabled = value;
			enabledBadge.classList.toggle('is-on', value);
			enabledBadge.textContent = value ? '已开启' : '已关闭';
		});
		var ssid = E('input', {
			type: 'text', 'class': 'cbi-input-text', value: draft.ssid, autocomplete: 'off',
			disabled: this.readonly ? '' : null, 'aria-label': title + ' 名称',
			input: function() { draft.ssid = this.value; }
		});
		var password = E('input', {
			type: 'password', 'class': 'cbi-input-text', value: draft.key, autocomplete: 'new-password',
			disabled: this.readonly || !supported ? '' : null, 'aria-label': title + ' 密码',
			placeholder: draft.encryption === 'none' ? '填写密码可启用 WPA / WPA2 加密' : '输入 Wi-Fi 密码',
			input: function() {
				draft.key = this.value;
				if (draft.encryption === 'none' && this.value) {
					draft.encryption = 'psk-mixed';
					encryption.value = 'psk-mixed';
					passwordHint.textContent = '8–63 字节；支持 WPA2 的 64 位十六进制密钥。';
				}
			}
		});
		var reveal = E('button', {
			type: 'button', 'class': 'wifi-reveal', 'aria-label': '显示密码', 'aria-pressed': 'false',
			disabled: !supported ? '' : null,
			click: function() {
				var visible = password.type === 'password';
				password.type = visible ? 'text' : 'password';
				this.textContent = visible ? '隐藏' : '显示';
				this.setAttribute('aria-pressed', String(visible));
				this.setAttribute('aria-label', visible ? '隐藏密码' : '显示密码');
			}
		}, '显示');
		var passwordHint = E('p', { 'class': 'wifi-hint' }, !supported ? '当前认证方式请通过专家设置管理。' : draft.encryption === 'none' ? '当前为开放网络，填写密码后启用加密。' : '8–63 字节；支持 WPA2 的 64 位十六进制密钥。');
		var encryption = supported ? select(mode, encryptionChoices(profiles), this.readonly, function(value) {
			// Display the authentication family while preserving an unchanged cipher.
			if (value !== encryptionMode(draft.encryption)) draft.encryption = value;
			if (value === 'none') { draft.key = ''; password.value = ''; }
			passwordHint.textContent = value === 'none' ? '当前为开放网络，填写密码后启用加密。' : '8–63 字节；WPA3 密码最多 63 字节。';
		}) : select('expert', [ [ 'expert', '请通过专家设置管理' ] ], true, function() {});
		var basic = E('div', { 'class': 'wifi-basic' }, [
			field('Wi-Fi 开关', enabledControl), field('Wi-Fi 名称', ssid),
			field('Wi-Fi 密码', E('div', { 'class': 'wifi-password' }, [ password, reveal ]))
		]);
		basic.lastElementChild.lastElementChild.appendChild(passwordHint);
		var advanced = E('div', { 'class': 'wifi-advanced-content' }, [ field('加密方式', encryption) ]);
		profiles.forEach(L.bind(function(profile) { advanced.appendChild(this.renderAdvanced(profile, profiles.length > 1)); }, this));
		return E('section', { 'class': 'wifi-card' }, [
			E('div', { 'class': 'wifi-card-heading' }, [
				E('div', {}, [ E('h3', {}, title), E('p', { 'class': 'wifi-hint' }, subtitle) ]), enabledBadge
			]),
			basic,
			E('details', { 'class': 'wifi-advanced' }, [ E('summary', {}, [ E('span', {}, '无线高级设置'), E('span', { 'class': 'wifi-chevron', 'aria-hidden': 'true' }, '⌄') ]), advanced ])
		]);
	},

	renderAdvanced: function(profile, showTitle) {
		var controls = [];
		if (showTitle) controls.push(E('h4', {}, bandLabel(profile.band)));
		var channels = [ [ 'auto', '自动' ] ];
		profile.frequencies.forEach(function(item) {
			// Older iwinfo has only restricted; newer replies distinguish no-IR.
			if (!(item.restricted && (item.no_ir == null || item.no_ir)) && frequencyBand(item.mhz, item.band) === profile.band)
				channels.push([ String(item.channel), String(item.channel) + (item.mhz ? ' · ' + item.mhz + ' MHz' : '') ]);
		});
		var channelHint = profile.frequencyFallback ? '暂未读取到实时信道，显示兼容选项。' : channels.length === 1 ? '暂未读取到可用信道，可在专家设置中查看。' : null;
		controls.push(field('信道', select(profile.channel || 'auto', channels, this.readonly, function(value) { profile.channel = value; }), channelHint));
		var modes = profile.device ? L.toArray(profile.device.getHTModes()) : [];
		var maxWidth = profile.band === '2g' ? 40 : profile.band === '5g' ? 160 : null;
		modes = modes.filter(function(mode) {
			var width = /^(EHT|HE|VHT|HT)(\d+)$/.exec(mode);
			return !width || !maxWidth || Number(width[2]) <= maxWidth;
		});
		var modeChoices = modes.map(function(mode) { return [ mode, modeLabel(mode) ]; });
		if (!modeChoices.some(function(choice) { return choice[0] === profile.htmode; })) {
			var configured = /^(EHT|HE|VHT|HT)(\d+)$/.exec(profile.htmode);
			var unsupported = configured && maxWidth && Number(configured[2]) > maxWidth;
			modeChoices.push([ profile.htmode, unsupported ? '当前配置（请重新选择带宽）' : modeLabel(profile.htmode) + '（当前）', unsupported ]);
		}
		controls.push(field('模式与带宽', select(profile.htmode, modeChoices, this.readonly, function(value) { profile.htmode = value; })));
		var powers = [ [ '', '驱动默认' ] ];
		if (profile.type === 'mtwifi')
			[ 100, 75, 50, 25, 10 ].forEach(function(value) { powers.push([ String(value), value + '%' ]); });
		else profile.powers.forEach(function(item) { powers.push([ String(item.dbm), item.dbm + ' dBm · ' + item.mw + ' mW' ]); });
		controls.push(field('发射功率', select(profile.txpower, powers, this.readonly, function(value) { profile.txpower = value; })));
		controls.push(field('隐藏名称', toggle(profile.hidden === '1', '隐藏 Wi-Fi 名称', this.readonly, function(value) { profile.hidden = value ? '1' : '0'; })));
		controls.push(field('终端隔离', toggle(profile.isolate === '1', '隔离无线终端', this.readonly, function(value) { profile.isolate = value ? '1' : '0'; })));
		controls.push(field('最大接入数', E('input', {
			type: 'number', min: '0', step: '1', 'class': 'cbi-input-text', value: profile.maxassoc,
			placeholder: '驱动默认', disabled: this.readonly ? '' : null,
			input: function() { profile.maxassoc = this.value; }
		}), '留空使用驱动默认值。'));
		return E('div', { 'class': 'wifi-radio-advanced' }, controls);
	},

	validate: function(profile, draft) {
		if (draft.enabled || draft.ssid !== profile.original.basic.ssid) {
			if (byteLength(draft.ssid) < 1 || byteLength(draft.ssid) > 32 || /[\x00-\x1f\x7f]/.test(draft.ssid))
				throw new Error(bandLabel(profile.band) + '：Wi-Fi 名称须为 1–32 字节，且不能包含控制字符。');
		}
		if (personal(draft.encryption) && (draft.enabled || draft.key !== profile.original.basic.key || draft.encryption !== profile.original.basic.encryption)) {
			var bytes = byteLength(draft.key), rawPSK = /^[0-9a-f]{64}$/i.test(draft.key) && draft.encryption.indexOf('sae') !== 0;
			if ((!rawPSK && (bytes < 8 || bytes > 63)) || /[\x00-\x1f\x7f]/.test(draft.key))
				throw new Error(bandLabel(profile.band) + '：Wi-Fi 密码须为 8–63 字节；WPA2 也支持 64 位十六进制密钥。');
		}
		if (profile.maxassoc !== profile.original.maxassoc && profile.maxassoc !== '' && !/^\d+$/.test(profile.maxassoc))
			throw new Error(bandLabel(profile.band) + '：最大接入数须为非负整数。');
	},

	stage: function(profile, draft) {
		var original = profile.original.basic, changes = 0;
		function set(section, option, value) {
			uci.set('wireless', section, option, value === '' ? null : value);
			changes++;
		}
		if (draft.enabled !== original.enabled) {
			set(profile.radio, 'disabled', draft.enabled ? '0' : '1');
			if (draft.enabled) set(profile.iface, 'disabled', '0');
		}
		if (draft.ssid !== original.ssid) set(profile.iface, 'ssid', draft.ssid);
		if (draft.encryption !== original.encryption) {
			set(profile.iface, 'encryption', draft.encryption);
			if (draft.encryption === 'sae') set(profile.iface, 'ieee80211w', '2');
			else if (draft.encryption === 'sae-mixed' && uci.get('wireless', profile.iface, 'ieee80211w') !== '2') set(profile.iface, 'ieee80211w', '1');
			else if (original.encryption.indexOf('sae') === 0 && draft.encryption.indexOf('sae') !== 0) set(profile.iface, 'ieee80211w', '');
		}
		if (personal(draft.encryption) && (draft.key !== original.key || draft.encryption !== original.encryption)) set(profile.iface, 'key', draft.key);
		else if (draft.encryption === 'none' && original.encryption !== 'none') set(profile.iface, 'key', '');
		[ 'channel', 'htmode', 'txpower', 'hidden', 'isolate', 'maxassoc' ].forEach(function(option) {
			if (profile[option] !== profile.original[option])
				set([ 'channel', 'htmode', 'txpower' ].indexOf(option) >= 0 ? profile.radio : profile.iface, option, profile[option]);
		});
		return changes;
	},

	setBusy: function(value) {
		this.busy = value;
		if (value) {
			this.disabledStates = Array.from(this.root.querySelectorAll('input, select, button')).map(function(control) {
				var state = [ control, control.disabled ];
				control.disabled = true;
				return state;
			});
		} else {
			(this.disabledStates || []).forEach(function(state) { state[0].disabled = state[1]; });
		}
		this.saveButton.classList.toggle('spinning', value);
	},

	save: function() {
		if (this.busy || this.readonly) return;
		this.message.textContent = '';
		var drafts = this.profiles.map(L.bind(function(profile) {
			return cloneBasic(this.merged && this.pair.indexOf(profile) >= 0 ? this.shared : profile.draft);
		}, this));
		var snapshots = this.profiles.map(function(profile) { return Object.assign({}, profile); });
		try {
			snapshots.forEach(L.bind(function(profile, index) { this.validate(profile, drafts[index]); }, this));
		} catch (error) {
			this.message.classList.add('is-error');
			this.message.textContent = error.message;
			return;
		}
		var count = 0;
		snapshots.forEach(L.bind(function(profile, index) { count += this.stage(profile, drafts[index]); }, this));
		this.setBusy(true);
		if (!count) {
			return uci.changes().then(L.bind(function(changes) {
				if (changes.wireless && changes.wireless.length) {
					return ui.changes.init().then(function() { ui.changes.apply(true); });
				}
				this.message.classList.remove('is-error');
				this.message.textContent = '当前设置未更改。';
			}, this)).catch(L.bind(function(error) {
				this.message.classList.add('is-error');
				this.message.textContent = '读取待应用设置失败：' + error.message;
			}, this)).finally(L.bind(function() { this.setBusy(false); }, this));
		}
		return uci.save().then(L.bind(function() {
			// Native LuCI apply retains connectivity checks and automatic rollback.
			this.profiles.forEach(function(profile, index) {
				profile.original.basic = cloneBasic(drafts[index]);
				[ 'channel', 'htmode', 'txpower', 'hidden', 'isolate', 'maxassoc' ].forEach(function(option) { profile.original[option] = snapshots[index][option]; });
			});
			return ui.changes.init();
		}, this)).then(function() {
			ui.changes.apply(true);
		}).catch(L.bind(function(error) {
			this.message.classList.add('is-error');
			this.message.textContent = '保存失败：' + error.message;
		}, this)).finally(L.bind(function() {
			// apply() owns the modal and returns void in this LuCI build.
			this.setBusy(false);
		}, this));
	}
});
