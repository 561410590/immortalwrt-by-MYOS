'use strict';
'require baseclass';
'require uci';
'require rpc';
'require fs';
'require ui';

/* 快速设置向导 —— 10 步首次配置, 以弹窗形式出现
 *
 * 为什么是 baseclass 而不是 view:
 *
 *   luci.js 里的 View.__init__ 在**构造的那一刻**就干了这两件事:
 *       const vp = document.getElementById('view');
 *       DOM.content(vp, E('div', { 'class': 'spinning' }, _('Loading view…')));
 *   而 L.require() 缓存的是**实例**(`const instance = new _class()`)。两条合起来
 *   意味着: 只要这个类是 view.extend, 光是 require 它就已经把右侧内容区清空并
 *   接管了 —— 做不成弹窗。所以这里是 baseclass, 自己提供 open()/close(),
 *   把 .qsw-root 挂到 document.body 上, 跟 #view 完全脱钩。
 *
 *   顺带: ui.instantiateView() 也用不了(它对 `view instanceof View` 做硬校验),
 *   所以菜单项不能再是 action.type=view 直接指这个类。见 view/mwrt/wizard.js。
 *
 * 提交分两条路, 不能混:
 *
 *   (A) network / wireless / dhcp  -> uci.set + ui.changes.apply(true)
 *       走 LuCI 自带的 90 秒确认回滚(luci.apply.rollback=90)。改 LAN IP 或者
 *       关掉当前连着的那个无线把自己关在门外时, 这条路能自己回来。后端代劳
 *       commit 就绕掉回滚了, 所以这几项坚决不交给 rpcd。
 *
 *   (B) 其余所有项 -> rpcd mwrt-wizard.apply
 *       时区、卡槽、APN、屏保、主备切换、LED、NROS。这些改错了不会断管理面,
 *       而且各有各的唯一入口(ledsched / cloudctl / qmodem_network reload),
 *       必须由后端按顺序调, 前端直接写 uci 会漏掉那些副作用。
 *
 * 先 B 后 A: B 快且安全, A 可能把页面连接打断。顺序反了的话 A 一回滚, B 还没发出去。
 */

var STEPS = [
	{ id: 'welcome',  title: '欢迎',       nav: '欢迎' },
	{ id: 'admin',    title: '管理员与地区', nav: '管理员' },
	{ id: 'wan',      title: '上网方式',     nav: '上网' },
	{ id: 'cell',     title: '蜂窝设置',     nav: '蜂窝' },
	{ id: 'wifi',     title: '无线设置',     nav: '无线' },
	{ id: 'nros',     title: 'APP 远程管理', nav: '远程' },
	{ id: 'lan',      title: '局域网',       nav: '局域网' },
	{ id: 'confirm',  title: '确认并应用',   nav: '确认' }
];

/* 屏幕时间的合法取值跟 mwrt.lua:242 的 times 表一致, 别自己编 */
var SHOWTIMES = [5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 120, 300, 600];

var WEEKNAMES = ['周日', '周一', '周二', '周三', '周四', '周五', '周六'];

var callStatus   = rpc.declare({ object: 'mwrt-wizard', method: 'status' });
var callModem    = rpc.declare({ object: 'mwrt-wizard', method: 'modem' });
var callApply    = rpc.declare({ object: 'mwrt-wizard', method: 'apply',    params: ['payload'] });
var callMarkdone = rpc.declare({ object: 'mwrt-wizard', method: 'markdone', params: ['value'] });
var callBootsw   = rpc.declare({ object: 'mwrt-wizard', method: 'bootsw',   params: ['action'] });
var callLed      = rpc.declare({
	object: 'mwrt-wizard', method: 'led',
	params: ['mode', 'starttime', 'endtime', 'weekdays']
});
var callSetPassword = rpc.declare({ object: 'luci', method: 'setPassword', params: ['username', 'password'] });
var callTimezones   = rpc.declare({ object: 'luci', method: 'getTimezones', expect: { '': {} } });

function jparse(s, dflt) {
	if (!s || typeof s != 'string') return dflt;
	try { return JSON.parse(s); } catch (e) { return dflt; }
}

/* mwrt_led 的三种状态 <-> 界面上的三个模式, 一一对应。
 * enabled=0 -> 常亮; enabled=1 && mode=always_off -> 常灭; 否则定时。
 * 「离开时恢复」就靠这个映射把原状态重新下发一遍, 因为 ledsched 的
 * on/off 会落 manual 键, 单纯不保存是回不去的。 */
/* 界面语言的选项按本机实际装了的语言来(luci.languages 段), 跟「系统 → 语言」同一个来源。
 * 以前写死成 LuCI 25.x 的语言码 zh_Hans, 这套 24.10 只有 zh_cn: ucode 找不到字典退回英文,
 * Lua 侧 setlanguage 不认四字母子码, argon 头部模板拿 nil 拼接, 应用完所有页面直接 500。 */
function langOpts() {
	var l = uci.get('luci', 'languages') || {}, zh = [], other = [];
	Object.keys(l).sort().forEach(function (k) {
		if (k.charAt(0) == '.' || k == 'en') return;
		/^zh/i.test(k) ? zh.push([k, /^zh[_-]cn$/i.test(k) ? _('简体中文') : String(l[k])]) : other.push([k, String(l[k])]);
	});
	/* luci 段读不到时按本固件的实际情况兜底 */
	if (!zh.length && !other.length) zh.push(['zh_cn', _('简体中文')]);
	return zh.concat(other, [['en', 'English'], ['auto', _('跟随浏览器')]]);
}

/* 现有值不在选项里(没设过、或者老版本写进去的 zh_Hans)时, 中文一律落到本机的中文码 */
function langSeed(v) {
	var ok = langOpts().map(function (o) { return o[0]; });
	if (v && ok.indexOf(v) >= 0) return v;
	if (!v || /^zh/i.test(v)) return ok.filter(function (k) { return /^zh/i.test(k); })[0] || 'auto';
	return 'auto';
}

function ledModeOf(cfg) {
	if (!cfg) return 'always_on';
	if (cfg.enabled != '1') return 'always_on';
	return (cfg.mode == 'always_off') ? 'always_off' : 'schedule';
}

function isIP4(s) {
	if (!/^\d{1,3}(\.\d{1,3}){3}$/.test(s)) return false;
	return s.split('.').every(function (o) { return +o >= 0 && +o <= 255; });
}

function maskToBits(m) {
	if (!isIP4(m)) return -1;
	var b = m.split('.').reduce(function (a, o) { return (a << 8) + (+o); }, 0) >>> 0;
	var n = 0, seen0 = false;
	for (var i = 31; i >= 0; i--) {
		if (b & (1 << i)) { if (seen0) return -1; n++; }
		else seen0 = true;
	}
	return n;
}

function sameSubnet(a, b, bits) {
	if (!isIP4(a) || !isIP4(b) || bits < 1) return false;
	var toN = function (s) { return s.split('.').reduce(function (x, o) { return (x << 8) + (+o); }, 0) >>> 0; };
	var m = bits == 32 ? 0xffffffff : (~((1 << (32 - bits)) - 1)) >>> 0;
	return ((toN(a) & m) >>> 0) === ((toN(b) & m) >>> 0);
}

function svgLogo() {
	return E('svg', { viewBox: '0 0 24 24', fill: 'none', 'aria-hidden': 'true' }, [
		E('path', {
			d: 'M12 3 3 7.5v4.2c0 4.6 3.6 8.4 9 9.3 5.4-.9 9-4.7 9-9.3V7.5L12 3Z',
			stroke: 'currentColor', 'stroke-width': '1.6', 'stroke-linejoin': 'round'
		}),
		E('path', {
			d: 'M8.6 12.1l2.3 2.3 4.5-4.6',
			stroke: 'currentColor', 'stroke-width': '1.6',
			'stroke-linecap': 'round', 'stroke-linejoin': 'round'
		})
	]);
}

return baseclass.extend({
	/* ------------------------------------------------------------ 开 / 关 */

	/* 菜单点一下就进这儿。重复点不该开出第二层来(第二层会把第一层的
	 * body.style.overflow 备份覆盖掉, 关掉之后页面就永久锁滚动了),
	 * 所以开着的时候再点只是把焦点拉回来。
	 *
	 * opts.standalone: 用户是直接导航到向导 URL 进来的(新标签页打开菜单项、
	 * 旧书签)。这种情况下背景是一张空白的内容区, 关窗之后不能把人留在那儿,
	 * 要送回概览页。从菜单正常点进来的那条路不带这个标记, 关窗就只是关窗。 */
	open: function (opts) {
		var self = this;

		if (this.rootEl) {
			this.focusFirst();
			return Promise.resolve(this.rootEl);
		}
		if (this.opening) return this.opening;

		this.standalone = !!(opts && opts.standalone);

		/* 先把遮罩和转圈挂上。load() 要等 status/modem/getTimezones 三个 rpc,
		 * 这台机器上 status 要跑一串 shell(读模组、读 LED、读启动分区),
		 * 实测一两秒 —— 中间什么都不显示的话, 用户会以为菜单点坏了, 然后连点。 */
		var scrim = E('div', { class: 'qsw-root qsw-loading', id: 'qsw-loading' }, [
			E('link', { rel: 'stylesheet', href: L.resource('mwrt-wizard.css') + '?v=4' }),
			E('div', { class: 'qsw-loadbox', role: 'status' }, [
				E('span', { class: 'qsw-spin' }),
				E('span', {}, _('正在读取设备状态…'))
			])
		]);
		document.body.appendChild(scrim);

		var dropScrim = function () {
			if (scrim.parentNode) scrim.parentNode.removeChild(scrim);
		};

		this.opening = this.load().then(function (data) {
			self.opening = null;
			var root = self.build(data);

			dropScrim();
			self.rootEl = root;
			document.body.appendChild(root);

			/* 背景不许跟着滚: 弹窗自己是 position:fixed, 底下那层再滚就会出现
			 * "两个滚动条互相抢"的廉价感。原值记下来, 关的时候逐字还原
			 * (页面可能本来就有 inline 的 overflow)。 */
			self.bodyOverflow = document.body.style.overflow;
			document.body.style.overflow = 'hidden';
			document.body.classList.add('qsw-open');

			self.renderStep();
			self.focusFirst();
			return root;
		}).catch(function (e) {
			self.opening = null;
			dropScrim();
			/* 这会儿弹窗没挂上, 没有自己的提示位置可用, 只能借 LuCI 的 */
			ui.addNotification(null, E('p', {}, _('向导打开失败：%s').format(e.message || e)), 'error');
			throw e;
		});

		return this.opening;
	},

	/* 关掉并把一切还原。应用成功后页面会被 ui.changes.apply(true) 接管,
	 * 那条路不走这儿 —— 它本来就要重载页面。 */
	close: function () {
		if (!this.rootEl) return Promise.resolve();

		var self = this;
		var root = this.rootEl;

		/* 试过灯但没应用 -> 把灯还回去。await 掉再拆 DOM, 否则 ledRestore
		 * 里那个 renderStep 会对着已经卸掉的节点写。 */
		return this.ledRestore().catch(function () {}).then(function () {
			self.teardown();
			if (root.parentNode) root.parentNode.removeChild(root);
			self.rootEl = null;

			/* 直接按 URL 进来的: 背景是空白内容区, 关窗得把人送回概览,
			 * 不然就是对着一张白页。从菜单点开的不走这条。 */
			if (self.standalone)
				window.location = L.url('admin', 'status', 'overview');
		});
	},

	/* 监听器/观察者全解开。忘一个就是一次泄漏: 向导可以反复开关,
	 * 每次都挂一个 pagehide 的话, 关了 N 次之后一次卸载发 N 个 beacon。 */
	teardown: function () {
		if (this.schemeObserver) {
			try { this.schemeObserver.disconnect(); } catch (e) {}
			this.schemeObserver = null;
		}
		if (this.onPageHide) {
			window.removeEventListener('pagehide', this.onPageHide);
			this.onPageHide = null;
		}
		if (this.onKeydown) {
			document.removeEventListener('keydown', this.onKeydown, true);
			this.onKeydown = null;
		}
		document.body.classList.remove('qsw-open');
		document.body.style.overflow = this.bodyOverflow || '';
		this.bodyOverflow = null;

		/* sync() 往 body 上镜像的配色属性要收回去 —— 向导是挂在 body 上的,
		 * 关窗后这个属性留在那儿就是对全站的污染。本包的 CSS 都带
		 * body.qsw-open 前缀所以不受影响, 但别的页面/主题万一也认
		 * data-qsw-scheme, 就会被一个已经关掉的向导钉住配色。 */
		document.body.removeAttribute('data-qsw-scheme');
	},

	focusFirst: function () {
		if (!this.rootEl) return;
		var el = this.rootEl.querySelector(
			'input:not([type="hidden"]):not(:disabled), select:not(:disabled), .qsw-btn.primary');
		if (el && el.focus) try { el.focus(); } catch (e) {}
	},

	/* 弹窗自己的消息条。
	 *
	 * 不能用 ui.addNotification: 它把节点插进 #maincontent
	 * (`const mc = document.querySelector('#maincontent') ?? document.body`),
	 * 而 argon 给 .main-right>#maincontent 的层级是 50 —— 压在 z-index 2000 的
	 * 弹窗底下, 用户一个字都看不见。「应用失败」「密码修改失败」「部分设置未能
	 * 保存」这些恰恰是必须看见的。所以提示挂在弹窗自己的壳里。
	 *
	 * 错误不自动消失(用户得有时间读完并决定怎么办), 成功/提示类 5 秒后淡出。 */
	notify: function (msg, kind) {
		if (!this.toasts) return;

		var cls = 'qsw-toast' + (kind ? ' ' + kind : '');
		var box = E('div', { class: cls, role: (kind == 'error') ? 'alert' : 'status' }, [
			E('span', { class: 'qsw-toast-msg' }, msg),
			E('button', {
				class: 'qsw-toast-x', 'aria-label': _('关闭'),
				click: function (ev) {
					var n = ev.target.parentNode;
					if (n && n.parentNode) n.parentNode.removeChild(n);
				}
			}, '×')
		]);

		this.toasts.appendChild(box);

		if (kind != 'error')
			window.setTimeout(function () {
				if (box.parentNode) box.parentNode.removeChild(box);
			}, 5000);

		return box;
	},

	load: function () {
		return Promise.all([
			callStatus().catch(function () { return {}; }),
			callModem().catch(function () { return { ok: false }; }),
			callTimezones().catch(function () { return {}; }),
			uci.load(['network', 'wireless', 'dhcp', 'system', 'luci']).catch(function () { return null; })
		]);
	},

	build: function (data) {
		var self = this;

		this.st = data[0] || {};
		this.modem = data[1] || { ok: false };
		this.zones = data[2] || {};

		this.led = jparse(this.st.led_raw, null);
		this.bootsw = jparse(this.st.bootsw_raw, null);

		/* 进来时的 LED 模式, 用于「没应用就离开」的恢复 */
		this.ledOrig = ledModeOf(this.st.led_cfg);
		this.ledPreviewed = false;
		/* 恢复只许发一次: 走了「跳过向导」那条, 紧接着的 pagehide 就不要再发;
		 * 第 9 步真的应用过之后, 更不能把用户刚存好的设置"恢复"掉。 */
		this.ledSettled = false;

		this.step = 0;
		this.err = {};

		/* 草稿。全部先攒在这里, 到第 9 步才落盘 —— 中途退出等于什么都没发生。 */
		var st = this.st;
		var radios = st.radios || [];
		var lcfg = st.led_cfg || {};

		this.d = {
			pw1: '', pw2: '',
			/* 下拉里的选项是 getTimezones 的键, 也就是 Olson 名 (Asia/Shanghai),
			 * 所以这里必须用 zonename 做种子。用 timezone (CST-8) 匹配不上任何
			 * option, 浏览器会默默选中第一个 (Africa/Abidjan) —— 用户没动过时区,
			 * 应用完时钟却跑到阿比让去了。 */
			tz: st.zonename || 'Asia/Shanghai',
			lang: langSeed(st.lang),
			hostname: st.hostname || '',

			wan_mode: (function () {
				var p = (st.wan || {}).proto;
				if (p == 'pppoe') return 'pppoe';
				if (p == 'static') return 'static';
				if (!p || p == 'none' || (st.wan || {}).disabled == '1') return 'cell';
				return 'dhcp';
			})(),
			pppoe_user: (st.wan || {}).username || '',
			pppoe_pass: '',
			wan_ip: (st.wan || {}).ipaddr || '',
			wan_mask: (st.wan || {}).netmask || '255.255.255.0',
			wan_gw: (st.wan || {}).gateway || '',
			wan_dns: (st.wan || {}).dns || '223.5.5.5 119.29.29.29',

			simsel: String((st.modem || {}).cur || ''),
			subsim: String((st.secondary || {}).cur || ''),
			apn: (st.modem || {}).apn || '',
			pdp_type: (st.modem || {}).pdp_type || '',
			enable_dial: (st.modem || {}).enable_dial || '1',

			radios: radios.map(function (r) {
				return {
					device: r.device, band: r.band, iface: r.iface,
					on: (r.radio_disabled != '1' && r.iface_disabled != '1'),
					ssid: r.ssid || '', key: r.key || '',
					originalEncryption: r.encryption || 'psk2',
					encryption: (r.encryption || 'psk2').split('+')[0]
				};
			}),

			led_mode: this.ledOrig,
			led_start: lcfg.starttime || '23:00',
			led_end: lcfg.endtime || '07:00',
			led_days: (function () {
				var w = lcfg.weekdays;
				if (!w) return [];
				return Array.isArray(w) ? w.map(String) : String(w).split(/[\s,]+/).filter(Boolean);
			})(),

			nros_on: ((st.nros || {}).enabled == '1'),
			nros_mac: '',

			rshowtime: (st.sysopt || {}).rshowtime || '1',
			showtime: (st.sysopt || {}).showtime || '10',
			usben: (st.sysopt || {}).USBEN || '0',

			fo_enabled: (st.failover || {}).enabled || '0',
			fo_primary: (st.failover || {}).primary_if || '',
			fo_secondary: (st.failover || {}).secondary_if || '',
			fo_target: (st.failover || {}).ping_target || '223.5.5.5',

			lan_ip: (st.lan || {}).ipaddr || '192.168.66.1',
			lan_mask: (st.lan || {}).netmask || '255.255.255.0',
			dhcp_on: ((st.lan || {}).ignore != '1'),
			dhcp_start: (st.lan || {}).start || '100',
			dhcp_limit: (st.lan || {}).limit || '150',
			dhcp_lease: (st.lan || {}).leasetime || '12h'
		};

		/* 没有 C2000 的三卡槽时别给用户选, simsel 默认只能是机身槽 6。
		 * 非授权模组切内置卡会把 SIM 封掉, 这道闸门后端也有一份。 */
		if (!st.c2000 && this.d.simsel != '6') this.d.simsel = '6';

		/* ?v= 必须跟着 css 一起涨: 这份 css 这次从"整屏页面"改成了"居中弹窗",
		 * 浏览器拿旧缓存会把新 DOM 按旧布局画出来(按钮条 position:fixed 贴在
		 * 屏幕底, 窗口没有 max-height), 看起来就是彻底坏掉。
		 *
		 * v3: 配色变量的宿主从单独的 .qsw-root 扩成选择器列表, 带上了
		 * body.qsw-open #modal_overlay —— 子对话框(跳过向导/关闭向导)的
		 * 主按钮之前拿不到变量, 成了白字+透明底+零边框, 整个按钮肉眼不可见。
		 * 拿旧缓存的话这个 bug 照旧, 所以这次必须涨。
		 *
		 * v4: 遮罩加了 backdrop-filter 毛玻璃, 两档 --qsw-scrim 跟着调淡。
		 * 拿旧表的话罩色还是重的那一版, 模糊也不会出现。 */
		this.css = E('link', {
			rel: 'stylesheet',
			href: L.resource('mwrt-wizard.css') + '?v=4'
		});

		this.body = E('div', { class: 'qsw-card', id: 'qsw-body' });
		this.bar = E('i', { style: 'width:0%' });
		this.stepLabel = E('span', {});
		this.stepCount = E('span', {});
		this.footer = E('div', { class: 'qsw-actions-in' });
		this.toasts = E('div', { class: 'qsw-toasts', 'aria-live': 'polite' });

		/* 右上角的关闭。跟「跳过向导」不是一回事, 这个只关窗口、不写
		 * wizard.done —— 用户是来看看的, 不是决定以后再也不用了。 */
		var closeBtn = E('button', {
			class: 'qsw-close', type: 'button', 'aria-label': _('关闭向导'), title: _('关闭向导'),
			click: ui.createHandlerFn(this, 'handleClose')
		}, [ E('svg', { viewBox: '0 0 24 24', fill: 'none', 'aria-hidden': 'true' }, [
			E('path', {
				d: 'M6 6l12 12M18 6L6 18', stroke: 'currentColor',
				'stroke-width': '1.8', 'stroke-linecap': 'round'
			})
		]) ]);

		/* 弹窗的壳: 外层 .qsw-root 是遮罩(铺满+变暗), 里层 .qsw-window 是那张
		 * 会飘起来的卡。点遮罩空白处不关 —— 向导是有状态的多步表单,
		 * 手一抖点在边上就把填了一半的东西丢掉, 这个代价太大。要关就点 × 。 */
		var win = E('div', {
			class: 'qsw-window', role: 'dialog', 'aria-modal': 'true',
			'aria-label': _('快速设置向导'), tabindex: '-1'
		}, [
			E('div', { class: 'qsw-head' }, [
				E('div', { class: 'qsw-brand' }, [ svgLogo(), E('span', {}, this.st.model || 'Mwrt') ]),
				closeBtn
			]),
			E('div', { class: 'qsw-progress' }, [
				E('div', { class: 'qsw-bar' }, [ this.bar ]),
				E('div', { class: 'qsw-steplabel' }, [ this.stepLabel, this.stepCount ])
			]),
			E('div', { class: 'qsw-scroll' }, [
				E('a', { class: 'qsw-skip', href: '#qsw-body' }, _('跳到设置内容')),
				this.body
			]),
			E('div', { class: 'qsw-actions' }, [ this.footer ]),
			this.toasts
		]);

		var root = E('div', { class: 'qsw-root', id: 'qsw-root' }, [ this.css, win ]);

		this.win = win;
		this.scroller = win.querySelector('.qsw-scroll');

		/* 深浅跟着整机皮肤走, 向导不自己做主 —— 不然系统深色时向导一片黑、
		 * 其它页面全是白的, 显得向导才是坏的那个。
		 *
		 * 这台固件上 <html data-darkmode> 是 mwrt-aurora-boot.js 写的
		 * (argon 自己的 header.htm 根本不输出这个属性, 它只靠 CSS 媒体查询):
		 *   FORCE_LIGHT=true  -> 恒写 "false", 全站锁浅色, 向导也跟着锁;
		 *   FORCE_LIGHT=false -> 写 matchMedia 的结果, 并挂 change 监听实时改。
		 * 所以不能只在 render 时读一次: 后一种模式下用户切换系统深浅时
		 * aurora 会改这个属性, 向导却停在挂载那一刻的配色上。用 observer 跟住。
		 * 属性不存在时一个都不钉, 让 CSS 的媒体查询自己决定。
		 *
		 * 为什么同一个属性要往 <body> 上也写一份: 向导自己的子对话框走
		 * ui.showModal, 节点落在 #modal_overlay —— 那是 body 的直接子节点,
		 * 跟 .qsw-root 是兄弟, 拿不到钉在 .qsw-root 元素上的属性, CSS 也没有
		 * "从兄弟身上取属性"的选择器。配色变量得靠 body 这个共同祖先传下去,
		 * 所以 wizard.css 里深色那几条写的是 body.qsw-open[data-qsw-scheme="dark"]。
		 * 不写这一份, 子对话框只能拿到浅色变量, 深色模式下对话框配色会错。 */
		var sync = function () {
			var dm = document.documentElement.getAttribute('data-darkmode');
			var mode = (dm === 'false') ? 'light' : (dm === 'true') ? 'dark' : null;
			if (mode) {
				root.setAttribute('data-qsw-scheme', mode);
				document.body.setAttribute('data-qsw-scheme', mode);
			}
			else {
				root.removeAttribute('data-qsw-scheme');
				document.body.removeAttribute('data-qsw-scheme');
			}
		};
		sync();
		try {
			this.schemeObserver = new MutationObserver(sync);
			this.schemeObserver.observe(document.documentElement,
				{ attributes: true, attributeFilter: [ 'data-darkmode' ] });
		} catch (e) { /* 老浏览器: 保持挂载时的配色, 不影响功能 */ }

		/* 试灯之后不走向导就离开, 在这儿把灯还回去。见 ledRestoreBeacon()。
		 * 没试灯时这个回调什么都不做, 所以挂着不花钱。
		 * 存成实例属性是为了 teardown 能原样摘掉 —— 弹窗可以反复开关,
		 * 匿名函数摘不掉, 关 N 次就会在下次卸载时发 N 个 beacon。 */
		this.onPageHide = function () { self.ledRestoreBeacon(); };
		window.addEventListener('pagehide', this.onPageHide);

		/* Esc 关窗。捕获阶段监听, 但向导自己的子对话框(切换启动系统 / 跳过向导 /
		 * 正在应用)开着时要让路: 那时 Esc 该先关掉子对话框, 由 ui.js 的
		 * cancelModal 处理, 不能一脚把整个向导踹掉。 */
		this.onKeydown = function (ev) {
			if (ev.key != 'Escape' && ev.keyCode != 27) return;
			if (document.body.classList.contains('modal-overlay-active')) return;
			if (self.applying) return;
			ev.preventDefault();
			ev.stopPropagation();
			self.handleClose();
		};
		document.addEventListener('keydown', this.onKeydown, true);

		return root;
	},

	/* × 和 Esc 都走这儿。填过东西就确认一次 —— 多步表单最气人的就是
	 * 点错一下全没了。什么都没改过的话直接关, 不要没事找事弹个框。 */
	handleClose: function () {
		var self = this;

		if (this.applying) return Promise.resolve();
		if (!this.dirty()) return this.close();

		ui.showModal(_('关闭向导'), [
			E('p', {}, _('已填写的内容还没有应用，关闭后会丢弃。设置本身不会有任何改动。')),
			E('div', { class: 'right' }, [
				this.btn(_('继续设置'), '', function () { ui.hideModal(); }),
				' ',
				this.btn(_('关闭并丢弃'), 'primary', function () {
					ui.hideModal();
					self.close();
				})
			])
		]);
		return Promise.resolve();
	},

	/* 用户到底动过没有。只要离开过第 0 步就算动过 —— 逐字段比对草稿和现值
	 * 要把十步里每个控件的"空值等价"(apn '' 对 'auto' 之类)全考虑一遍,
	 * 漏一个就是误判成"没改过"然后静默丢掉用户的输入。按步数判是保守的那一侧:
	 * 顶多多问一句, 不会丢东西。 */
	dirty: function () {
		return (this.step > 0) || this.ledPreviewed;
	},

	/* ---------------------------------------------------------------- 控件 */

	field: function (label, ctrl, hint, key) {
		var kids = [ E('label', { class: 'qsw-label' }, label), ctrl ];
		if (hint) kids.push(E('div', { class: 'qsw-hint' }, hint));
		if (key && this.err[key]) kids.push(E('div', { class: 'qsw-err' }, this.err[key]));
		return E('div', { class: 'qsw-field' }, kids);
	},

	input: function (key, opts) {
		var self = this;
		opts = opts || {};
		var el = E('input', {
			type: opts.type || 'text',
			value: this.d[key] != null ? this.d[key] : '',
			placeholder: opts.placeholder || '',
			autocomplete: opts.autocomplete || 'off',
			spellcheck: 'false',
			'aria-invalid': this.err[key] ? 'true' : null,
			'aria-label': opts.aria || null
		});
		el.addEventListener('input', function () {
			self.d[key] = this.value;
			if (self.err[key]) { delete self.err[key]; self.renderStep(); }
		});
		return el;
	},

	select: function (key, items, onchange) {
		var self = this;
		var el = E('select', {});
		items.forEach(function (it) {
			var v = String(it[0]);
			el.appendChild(E('option', {
				value: v, selected: (String(self.d[key]) === v) ? '' : null
			}, it[1]));
		});
		el.addEventListener('change', function () {
			self.d[key] = this.value;
			if (self.err[key]) delete self.err[key];
			if (onchange) onchange.call(self, this.value);
			else self.renderStep();
		});
		return el;
	},

	toggle: function (title, desc, get, set) {
		var self = this;
		var btn = E('button', {
			type: 'button', class: 'qsw-toggle', role: 'switch',
			'aria-checked': get() ? 'true' : 'false',
			'aria-label': title
		});
		btn.addEventListener('click', function () { set(!get()); self.renderStep(); });
		return E('div', { class: 'qsw-switch' }, [
			E('div', { class: 'qsw-switch-text' }, [
				E('b', {}, title), desc ? E('span', {}, desc) : ''
			]),
			btn
		]);
	},

	choice: function (key, items) {
		var self = this;
		var name = 'qsw-' + key + '-' + Math.random().toString(36).slice(2, 7);
		return E('div', { class: 'qsw-choices', role: 'radiogroup' }, items.map(function (it) {
			var on = (String(self.d[key]) === String(it[0]));
			var radio = E('input', {
				type: 'radio', name: name, value: String(it[0]), checked: on ? '' : null
			});
			var row = E('label', { class: 'qsw-choice', 'aria-checked': on ? 'true' : 'false' }, [
				radio,
				E('div', { class: 'qsw-choice-text' }, [
					E('b', {}, it[1]), it[2] ? E('span', {}, it[2]) : ''
				])
			]);
			radio.addEventListener('change', function () {
				self.d[key] = this.value;
				self.err = {};
				self.renderStep();
			});
			return row;
		}));
	},

	kv: function (rows) {
		return E('div', { class: 'qsw-kv' }, rows.filter(Boolean).map(function (r) {
			return E('div', {}, [
				E('span', { class: 'qsw-kv-k' }, r[0]),
				E('span', { class: 'qsw-kv-v' }, r[1])
			]);
		}));
	},

	note: function (text, kind) {
		return E('div', { class: 'qsw-note' + (kind ? ' ' + kind : '') }, text);
	},

	btn: function (label, cls, fn, dis) {
		var b = E('button', { type: 'button', class: 'qsw-btn' + (cls ? ' ' + cls : ''), disabled: dis ? '' : null }, label);
		if (!dis && fn) b.addEventListener('click', fn);
		return b;
	},

	/* -------------------------------------------------------------- 骨架 */

	renderStep: function () {
		var self = this;
		var s = STEPS[this.step];

		this.bar.style.width = (this.step / (STEPS.length - 1) * 100).toFixed(1) + '%';
		this.stepLabel.textContent = s.nav;
		this.stepCount.textContent = _('第 %d / %d 步').format(this.step + 1, STEPS.length);

		var kids = this['step_' + s.id].call(this);
		while (this.body.firstChild) this.body.removeChild(this.body.firstChild);
		(Array.isArray(kids) ? kids : [kids]).forEach(function (k) { if (k) self.body.appendChild(k); });

		while (this.footer.firstChild) this.footer.removeChild(this.footer.firstChild);

		var left = [];
		if (this.step > 0)
			left.push(this.btn(_('上一步'), 'ghost', function () { self.go(-1); }));
		else
			left.push(this.btn(_('跳过向导'), 'ghost', function () { self.skip(); }));

		left.forEach(function (b) { self.footer.appendChild(b); });
		this.footer.appendChild(E('div', { class: 'qsw-grow' }));

		if (this.step == STEPS.length - 1) {
			this.footer.appendChild(this.btn(_('确认并应用'), 'primary', function (ev) { self.finish(ev); }));
		} else {
			this.footer.appendChild(this.btn(
				this.step === 0 ? _('开始设置') : _('下一步'),
				'primary', function () { self.go(1); }
			));
		}

		/* 滚回顶部要滚**弹窗自己**那个容器, 不是 window ——
		 * 弹窗是 position:fixed, 背景页的滚动位置跟它毫无关系,
		 * window.scrollTo 的效果是换步之后新内容还停在上一步的位置。 */
		if (this.scroller) this.scroller.scrollTop = 0;
	},

	go: function (dir) {
		if (dir > 0 && !this.validate()) { this.renderStep(); return; }
		var n = this.step + dir;
		if (n < 0 || n >= STEPS.length) return;
		this.step = n;
		this.err = {};
		this.renderStep();
	},

	/* -------------------------------------------------------------- 步骤 */

	step_welcome: function () {
		var st = this.st;
		var m = st.modem || {};
		var done = (st.done_flag == '1');

		return [
			E('h1', { class: 'qsw-h1' }, _('欢迎使用')),
			E('p', { class: 'qsw-sub' },
				_('按步骤设置上网、无线与常用功能。中途退出不会改动设置。')),
			this.kv([
				[_('设备型号'), st.model || '-'],
				[_('蜂窝模组'), m.present ? (m.name || _('已识别')) : _('未检测到')],
				[_('管理地址'), (st.lan || {}).ipaddr || '-'],
				[_('当前系统'), st.system_name || '-']
			]),
			done ? this.note(_('这台设备已经完成过一次向导。继续操作会覆盖现有配置。'), 'warn') : '',
			this.note(_('全程不会向模组发送 AT 指令，蜂窝信息只读取后台缓存，不会影响正在进行的拨号。'))
		];
	},

	step_admin: function () {
		var self = this;
		var zones = Object.keys(this.zones).sort();
		if (!zones.length) zones = ['Asia/Shanghai', 'UTC'];

		return [
			E('h1', { class: 'qsw-h1' }, _('管理员与地区')),
			E('p', { class: 'qsw-sub' }, _('设置登录密码和所在时区。密码留空表示不修改。')),

			E('h2', { class: 'qsw-h2' }, _('登录密码')),
			this.field(_('新密码'), this.input('pw1', {
				type: 'password', autocomplete: 'new-password', placeholder: _('留空则不修改')
			}), _('至少 6 位。用户名固定为 root。'), 'pw1'),
			this.field(_('确认密码'), this.input('pw2', {
				type: 'password', autocomplete: 'new-password'
			}), null, 'pw2'),

			E('h2', { class: 'qsw-h2' }, _('地区与语言')),
			this.field(_('时区'), this.select('tz', zones.map(function (z) { return [z, z]; })),
				_('影响指示灯定时、日志时间戳和定时任务。'), 'tz'),
			this.field(_('界面语言'), this.select('lang', langOpts())),
			this.field(_('设备名称'), this.input('hostname', { placeholder: 'Mwrt' }),
				_('显示在局域网里的主机名。'), 'hostname')
		];
	},

	step_wan: function () {
		if (!(this.st.wan || {}).device) {
			return [E('h1',{class:'qsw-h1'},_('上网方式')),
				this.note(_('当前使用主、副 5G。扩展口用途与线路策略在蜂窝设置中管理。')),
				E('p',{},[E('a',{href:L.url('admin','modem','modem'),target:'_blank',rel:'noopener'},_('打开 PoE 副 5G 与线路设置'))])];
		}
		var self = this;
		var w = this.st.wan || {};
		var link = (w.carrier == '1');
		var extra = [];

		if (this.d.wan_mode == 'pppoe') {
			extra = [
				E('h2', { class: 'qsw-h2' }, _('宽带账号')),
				this.field(_('账号'), this.input('pppoe_user', { placeholder: _('宽带运营商提供') }), null, 'pppoe_user'),
				this.field(_('密码'), this.input('pppoe_pass', { type: 'password', autocomplete: 'new-password' }),
					_('留空表示沿用设备上已保存的密码。'), 'pppoe_pass')
			];
		} else if (this.d.wan_mode == 'static') {
			extra = [
				E('h2', { class: 'qsw-h2' }, _('静态地址')),
				E('div', { class: 'qsw-row' }, [
					this.field(_('IP 地址'), this.input('wan_ip', { placeholder: '192.168.1.2' }), null, 'wan_ip'),
					this.field(_('子网掩码'), this.input('wan_mask'), null, 'wan_mask')
				]),
				E('div', { class: 'qsw-row' }, [
					this.field(_('默认网关'), this.input('wan_gw', { placeholder: '192.168.1.1' }), null, 'wan_gw'),
					this.field(_('DNS 服务器'), this.input('wan_dns'), _('多个用空格分隔。'), 'wan_dns')
				])
			];
		} else if (this.d.wan_mode == 'cell') {
			extra = [ this.note(_('使用主、副蜂窝网络。有线 WAN 口会停用，下一步可选择 SIM 卡。')) ];
		}

		return [
			E('h1', { class: 'qsw-h1' }, _('上网方式')),
			E('p', { class: 'qsw-sub' }, [
				_('WAN 口当前'), ' ',
				E('span', { class: 'qsw-badge ' + (link ? 'ok' : 'warn') }, link ? _('已插网线') : _('未检测到网线')),
				'  ', w.device ? ('(' + w.device + ')') : ''
			]),
			this.choice('wan_mode', [
				['dhcp', _('自动获取 IP'), _('最常见。光猫或上级路由会自动分配地址。')],
				['pppoe', _('宽带账号拨号'), _('运营商给了账号和密码时选这个。')],
				['static', _('静态 IP'), _('专线或上级手工分配固定地址。')],
				['cell', _('仅用蜂窝网络'), _('不接网线，完全靠 SIM 卡上网。')]
			])
		].concat(extra);
	},

	step_cell: function () {
		var m=this.st.modem||{}, sub=this.st.secondary||{};
		var out=[E('h1',{class:'qsw-h1'},_('蜂窝设置')),
			this.note(_('主、副设备分别选择卡槽；只有改变选择并最终应用时才会切卡。'))];
		[[m,'simsel',_('主 5G')],[sub,'subsim',_('副 5G')]].forEach(function(item){
			var state=item[0];
			if (!(state.slots||[]).length) { out.push(this.note(item[2]+'：'+_('暂未取得卡槽信息，可继续设置其他项目。'))); return; }
			out.push(this.field(item[2]+' · '+(state.name||''),this.select(item[1],state.slots.map(function(slot){return [String(slot.value),slot.label];}))));
		},this);
		out.push(E('p',{},[E('a',{href:L.url('admin','modem','modem'),target:'_blank',rel:'noopener'},_('打开蜂窝详细设置：APN、网络模式、锁频及双 5G 线路'))]));
		return out;
	},

	step_wifi: function () {
		var self = this;
		var rs = this.d.radios;

		if (!rs.length) {
			return [
				E('h1', { class: 'qsw-h1' }, _('无线设置')),
				this.note(_('没有找到无线网卡，跳过这一步。'), 'warn')
			];
		}

		var out = [
			E('h1', { class: 'qsw-h1' }, _('无线设置')),
			E('p', { class: 'qsw-sub' }, _('两个频段可以各自命名，也可以用同一个名字让手机自动选择。'))
		];

		rs.forEach(function (r, i) {
			var label = (r.band == '2g') ? _('2.4G 频段') : (r.band == '5g' ? _('5G 频段') : r.device);
			out.push(E('h2', { class: 'qsw-h2' }, label));
			out.push(self.toggle(_('启用 %s').format(label), r.device,
				function () { return r.on; },
				function (v) { r.on = v; }));

			if (!r.on) return;

			var ssidEl = E('input', { type: 'text', value: r.ssid, spellcheck: 'false', maxlength: '32' });
			ssidEl.addEventListener('input', function () {
				r.ssid = this.value;
				if (self.err['ssid' + i]) { delete self.err['ssid' + i]; self.renderStep(); }
			});
			out.push(self.field(_('无线名称'), ssidEl, null, 'ssid' + i));

			var encEl = E('select', {});
			[['sae-mixed', 'WPA2 / WPA3 ' + _('混合（推荐）')],
			 ['sae', 'WPA3 ' + _('仅新设备')],
			 ['psk2', 'WPA2'],
			 ['psk-mixed', 'WPA / WPA2 ' + _('兼容老设备')],
			 ['none', _('不加密（开放）')]].forEach(function (it) {
				encEl.appendChild(E('option', {
					value: it[0], selected: (r.encryption == it[0]) ? '' : null
				}, it[1]));
			});
			encEl.addEventListener('change', function () { r.encryption = this.value; self.renderStep(); });
			out.push(self.field(_('加密方式'), encEl,
				(r.encryption == 'sae')
					? _('部分老手机和 IoT 设备连不上纯 WPA3，拿不准就选混合。')
					: null));

			if (r.encryption != 'none') {
				var keyEl = E('input', { type: 'text', value: r.key, spellcheck: 'false', autocomplete: 'off' });
				keyEl.addEventListener('input', function () {
					r.key = this.value;
					if (self.err['key' + i]) { delete self.err['key' + i]; self.renderStep(); }
				});
				out.push(self.field(_('无线密码'), keyEl, _('8 到 63 位。'), 'key' + i));
			} else {
				out.push(self.note(_('开放网络任何人都能连接，不建议长期使用。'), 'warn'));
			}
		});

		if (rs.length > 1) {
			out.push(E('div', { class: 'qsw-field' }, [
				this.btn(_('两个频段用同一组名称和密码'), '', function () {
					var a = rs[0];
					rs.forEach(function (r) { r.ssid = a.ssid; r.key = a.key; r.encryption = a.encryption; });
					self.err = {};
					self.renderStep();
				})
			]));
		}

		return out;
	},

	step_led: function () {
		var self = this;
		var leds = (this.led && this.led.leds) || [];
		var out = [
			E('h1', { class: 'qsw-h1' }, _('设备指示灯')),
			E('p', { class: 'qsw-sub' }, _('夜里嫌亮可以定时关灯，网络状态不受影响。')),
			this.choice('led_mode', [
				['always_on', _('一直亮'), _('默认。指示灯正常显示网络状态。')],
				['always_off', _('一直关'), _('全部熄灭，不影响任何功能。')],
				['schedule', _('定时关灯'), _('到点自动熄灭，第二天自动亮起。')]
			])
		];

		if (this.d.led_mode == 'schedule') {
			var mk = function (key, label) {
				var el = E('input', { type: 'time', value: self.d[key] });
				el.addEventListener('input', function () { self.d[key] = this.value; });
				return self.field(label, el, null, key);
			};
			out.push(E('h2', { class: 'qsw-h2' }, _('关灯时段')));
			out.push(E('div', { class: 'qsw-row' }, [ mk('led_start', _('关灯时间')), mk('led_end', _('亮灯时间')) ]));

			var cross = (this.d.led_start > this.d.led_end);
			out.push(this.note(cross
				? _('跨越午夜：当天 %s 熄灭，次日 %s 亮起。').format(this.d.led_start, this.d.led_end)
				: _('当天 %s 熄灭，%s 亮起。').format(this.d.led_start, this.d.led_end)));

			out.push(E('h2', { class: 'qsw-h2' }, _('生效日期')));
			out.push(E('div', { class: 'qsw-row' }, WEEKNAMES.map(function (n, i) {
				var on = self.d.led_days.indexOf(String(i)) >= 0;
				var b = E('button', {
					type: 'button', class: 'qsw-btn' + (on ? ' primary' : ''),
					'aria-pressed': on ? 'true' : 'false', style: 'flex:1 1 60px'
				}, n);
				b.addEventListener('click', function () {
					var k = String(i), ix = self.d.led_days.indexOf(k);
					if (ix >= 0) self.d.led_days.splice(ix, 1); else self.d.led_days.push(k);
					self.renderStep();
				});
				return b;
			})));
			out.push(E('div', { class: 'qsw-hint' }, _('一天都不选表示每天都生效。')));
		}

		if (leds.length) {
			out.push(E('h2', { class: 'qsw-h2' }, _('当前指示灯')));
			out.push(this.kv(leds.map(function (l) {
				return [l.name, l.brightness ? _('亮') : _('灭')];
			})));
		}

		/* 预览会真的落 manual 键, 单纯「不保存就走」回不去, 所以离开时按原模式回放一遍。 */
		out.push(E('div', { class: 'qsw-field' }, [
			this.btn(_('立即试一下'), '', function (ev) { self.ledPreview(ev); })
		]));
		out.push(this.note(this.ledPreviewed
			? _('已经试过一次。如果最后没有应用向导，离开时会自动恢复成原来的设置。')
			: _('点一下可以马上看到灯的变化，不影响最终是否应用。')));

		return out;
	},

	step_nros: function () {
		var self = this;
		var n = this.st.nros || {};
		var out = [
			E('h1', { class: 'qsw-h1' }, _('APP 远程管理')),
			E('p', { class: 'qsw-sub' }, _('开启后可以用手机 APP 在外网查看和管理这台设备。')),
			this.toggle(_('启用远程管理'), _('默认关闭。'),
				function () { return self.d.nros_on; },
				function (v) { self.d.nros_on = v; }),
			this.kv([
				[_('设备名称'), n.device_name || this.st.model || '-'],
				[_('设备标识'), n.label_mac_masked || '-'],
				[_('IMEI'), n.imei_masked || '-'],
				[_('服务状态'), n.agent_running ? _('运行中') : _('未运行')]
			])
		];

		if (this.d.nros_on) {
			out.push(E('h2', { class: 'qsw-h2' }, _('设备标识')));
			out.push(this.field(_('手动指定标识'),
				this.input('nros_mac', { placeholder: n.manual_mac_set ? n.manual_mac_masked : _('留空 = 用机身标签') }),
				_('只有 APP 提示设备标识不匹配时才需要填，格式和机身标签上的一致。'), 'nros_mac'));
			out.push(this.note(_('远程管理会把设备在线状态上报到厂商云端。不需要时随时可以在「远程管理」页关闭。')));
		}

		return out;
	},

	step_sysopt: function () {
		var self = this;
		var fo = this.st.failover || {};
		var ifs = this.st.ifaces || [];
		var out = [
			E('h1', { class: 'qsw-h1' }, _('屏保与系统')),
			E('p', { class: 'qsw-sub' }, _('管理页面的息屏保护，以及双线路自动切换。')),

			E('h2', { class: 'qsw-h2' }, _('管理页面屏保')),
			this.toggle(_('启用屏保'), _('管理页面长时间没有操作就显示遮罩。'),
				function () { return self.d.rshowtime == '1'; },
				function (v) { self.d.rshowtime = v ? '1' : '0'; })
		];

		if (this.d.rshowtime == '1') {
			out.push(this.field(_('多久后息屏'), this.select('showtime',
				SHOWTIMES.map(function (v) {
					/* 不能用 Math.round(v/60) —— SHOWTIMES 里 70/80/100 都不是整分钟,
					 * 四舍五入之后 60/70/80 全都显示"1 分钟"、90/100/120 全都显示
					 * "2 分钟", 下拉里出现三个一模一样的选项, 用户没法选。
					 * 整分钟说"N 分钟", 不满整分钟说"N 分 M 秒"。 */
					return [String(v),
						v < 60      ? _('%d 秒').format(v)
						: v % 60 == 0 ? _('%d 分钟').format(v / 60)
						              : _('%d 分 %d 秒').format(Math.floor(v / 60), v % 60)];
				})), null, 'showtime'));
		}
		/* 说清楚是网页遮罩, 不是面板 —— C2000 根本没有屏, 带屏的只有 EdgePi-E87N */
		out.push(this.note(_('这是浏览器里管理页面的遮罩，和设备上的物理屏幕无关。这台设备没有显示屏。')));

		out.push(E('h2', { class: 'qsw-h2' }, _('USB 设备检测')));
		out.push(this.toggle(_('开机检测 USB 存储'), _('不插 U 盘或硬盘时关掉可以略微加快开机。'),
			function () { return self.d.usben == '1'; },
			function (v) { self.d.usben = v ? '1' : '0'; }));

		out.push(E('h2', { class: 'qsw-h2' }, _('双线路自动切换')));
		out.push(this.toggle(_('启用自动切换'), _('主线路断了自动切到备用线路，恢复后自动切回。'),
			function () { return self.d.fo_enabled == '1'; },
			function (v) { self.d.fo_enabled = v ? '1' : '0'; }));

		if (this.d.fo_enabled == '1') {
			var opts = ifs.map(function (i) { return [i, i]; });
			if (!opts.length) opts = [['eth0', 'eth0']];
			if (!this.d.fo_primary) this.d.fo_primary = fo.primary_if || opts[0][0];
			out.push(E('div', { class: 'qsw-row' }, [
				this.field(_('主线路'), this.select('fo_primary', opts), null, 'fo_primary'),
				this.field(_('备用线路'), this.select('fo_secondary', opts), null, 'fo_secondary')
			]));
			out.push(this.field(_('探测地址'), this.input('fo_target'),
				_('用 ping 这个地址判断线路是否可用。'), 'fo_target'));
			out.push(this.note(_('这里填的是物理网卡名（例如 eth0、eth2），不是 wan、lan 这样的接口名。')));
		}

		if (this.bootsw && this.bootsw.ready) {
			out.push(E('h2', { class: 'qsw-h2' }, _('启动系统')));
			out.push(this.kv([
				[_('当前启动'), this.bootsw.system == 'sd' ? _('SD 卡系统') : _('官方系统')]
			]));
			out.push(E('div', { class: 'qsw-field' }, [
				this.btn(this.bootsw.system == 'sd' ? _('切换到官方系统') : _('切换到 SD 卡系统'), '',
					function (ev) { self.switchBoot(ev); })
			]));
			out.push(this.note(_('切换系统需要重启，并且立即生效，不随向导一起应用。'), 'warn'));
		}

		return out;
	},

	step_lan: function () {
		var self = this;
		var cur = (this.st.lan || {}).ipaddr || '';
		var changed = (this.d.lan_ip && this.d.lan_ip != cur);

		var out = [
			E('h1', { class: 'qsw-h1' }, _('局域网')),
			E('p', { class: 'qsw-sub' }, _('设备在局域网里的地址，以及分配给电脑和手机的地址范围。')),
			E('div', { class: 'qsw-row' }, [
				this.field(_('管理地址'), this.input('lan_ip'), null, 'lan_ip'),
				this.field(_('子网掩码'), this.input('lan_mask'), null, 'lan_mask')
			])
		];

		if (changed)
			out.push(this.note(_('应用后管理地址会变成 %s，浏览器需要用新地址重新打开。如果 90 秒内没有确认成功，设备会自动回到原来的地址。').format(this.d.lan_ip), 'warn'));

		var conflict = this.lanConflict();
		if (conflict)
			out.push(this.note(_('这个网段和上级网络（%s）重叠，会导致上不了网。请换一个，例如 192.168.66.1。').format(conflict), 'err'));

		out.push(E('h2', { class: 'qsw-h2' }, _('地址自动分配')));
		out.push(this.toggle(_('启用 DHCP 服务'), _('关掉之后局域网设备需要手工填地址。'),
			function () { return self.d.dhcp_on; },
			function (v) { self.d.dhcp_on = v; }));

		if (this.d.dhcp_on) {
			out.push(E('div', { class: 'qsw-row' }, [
				this.field(_('起始地址'), this.input('dhcp_start', { type: 'number' }),
					_('主机号，例如 100。'), 'dhcp_start'),
				this.field(_('可分配数量'), this.input('dhcp_limit', { type: 'number' }), null, 'dhcp_limit')
			]));
			out.push(this.field(_('租约时间'), this.select('dhcp_lease', [
				['2h', _('2 小时')], ['12h', _('12 小时')], ['24h', _('24 小时')], ['infinite', _('永久')]
			]), null, 'dhcp_lease'));
		}

		return out;
	},

	step_confirm: function () {
		var self = this;
		var d = this.d, st = this.st;
		var rows = [];

		if (d.pw1) rows.push([_('登录密码'), _('将被修改')]);
		rows.push([_('时区'), d.tz]);
		if (d.hostname) rows.push([_('设备名称'), d.hostname]);

		rows.push([_('上网方式'), ({
			dhcp: _('自动获取 IP'), pppoe: _('宽带账号拨号'),
			static: _('静态 IP'), cell: _('仅用蜂窝网络')
		})[d.wan_mode]]);
		if (d.wan_mode == 'pppoe') rows.push([_('宽带账号'), d.pppoe_user || '-']);
		if (d.wan_mode == 'static') rows.push([_('WAN 地址'), d.wan_ip + ' / ' + d.wan_mask]);

		[[st.modem,d.simsel,_('主 5G 卡槽')],[st.secondary,d.subsim,_('副 5G 卡槽')]].forEach(function(item){
			var slot=((item[0]||{}).slots||[]).filter(function(s){return String(s.value)===item[1];})[0];
			if(slot) rows.push([item[2],slot.label]);
		});

		d.radios.forEach(function (r) {
			var label = (r.band == '2g') ? '2.4G' : (r.band == '5g' ? '5G' : r.device);
			rows.push([_('无线 %s').format(label),
				r.on ? (r.ssid + '（' + (r.encryption == 'none' ? _('开放') : r.encryption) + '）') : _('已关闭')]);
		});

		rows.push([_('远程管理'), d.nros_on ? _('开启') : _('关闭')]);
		rows.push([_('管理地址'), d.lan_ip + ' / ' + d.lan_mask]);
		rows.push([_('DHCP'), d.dhcp_on ? (d.dhcp_start + ' 起 ' + d.dhcp_limit + ' 个') : _('已关闭')]);

		var lanChanged = (d.lan_ip != ((st.lan || {}).ipaddr || ''));

		return [
			E('h1', { class: 'qsw-h1' }, _('确认并应用')),
			E('p', { class: 'qsw-sub' }, _('核对一遍，没问题就点右下角应用。')),
			this.kv(rows),
			this.note(_('网络、无线和局域网的改动有 90 秒确认时间：如果改完连不上，设备会自动恢复到现在的配置。')),
			lanChanged ? this.note(_('管理地址会变成 %s。应用后请用新地址重新打开管理页面。').format(d.lan_ip), 'warn') : ''
		];
	},

	/* ------------------------------------------------------------ 校验 */

	lanConflict: function () {
		var bits = maskToBits(this.d.lan_mask);
		if (bits < 1) return null;
		var w = this.st.wan || {};
		var cands = [];
		if (w.ipaddr) cands.push(w.ipaddr);
		if (this.d.wan_mode == 'static' && this.d.wan_ip) cands.push(this.d.wan_ip);
		if (this.d.wan_mode == 'static' && this.d.wan_gw) cands.push(this.d.wan_gw);
		for (var i = 0; i < cands.length; i++)
			if (sameSubnet(this.d.lan_ip, cands[i], bits)) return cands[i];
		return null;
	},

	validate: function () {
		var d = this.d, e = {};
		var id = STEPS[this.step].id;

		if (id == 'admin') {
			if (d.pw1 || d.pw2) {
				if (d.pw1.length < 6) e.pw1 = _('密码至少 6 位。');
				else if (d.pw1 !== d.pw2) e.pw2 = _('两次输入的密码不一致。');
			}
			if (d.hostname && !/^[A-Za-z0-9][A-Za-z0-9-]{0,62}$/.test(d.hostname))
				e.hostname = _('只能用字母、数字和减号，且不能以减号开头。');
		}

		if (id == 'wan') {
			if (d.wan_mode == 'pppoe' && !d.pppoe_user)
				e.pppoe_user = _('请填写宽带账号。');
			if (d.wan_mode == 'static') {
				if (!isIP4(d.wan_ip)) e.wan_ip = _('请填写正确的 IP 地址。');
				if (maskToBits(d.wan_mask) < 1) e.wan_mask = _('请填写正确的子网掩码。');
				if (d.wan_gw && !isIP4(d.wan_gw)) e.wan_gw = _('请填写正确的网关地址。');
			}
		}

		if (id == 'cell') {
			/* APN 填错直接拨不上, 所以宁可严一点。留空 = 自动, 是推荐值。 */
			if (d.apn && !/^[A-Za-z0-9][A-Za-z0-9._-]{0,62}$/.test(d.apn))
				e.apn = _('APN 只能用字母、数字和 . _ - ，拿不准请留空。');
		}

		if (id == 'wifi') {
			d.radios.forEach(function (r, i) {
				if (!r.on) return;
				if (!r.ssid) e['ssid' + i] = _('请填写无线名称。');
				else if (r.ssid.length > 32) e['ssid' + i] = _('无线名称不能超过 32 个字符。');
				if (r.encryption != 'none') {
					if (!r.key || r.key.length < 8) e['key' + i] = _('无线密码至少 8 位。');
					else if (r.key.length > 63) e['key' + i] = _('无线密码不能超过 63 位。');
				}
			});
		}

		if (id == 'led' && d.led_mode == 'schedule') {
			if (!/^\d{1,2}:\d{2}$/.test(d.led_start)) e.led_start = _('请填写正确的时间。');
			if (!/^\d{1,2}:\d{2}$/.test(d.led_end)) e.led_end = _('请填写正确的时间。');
			if (d.led_start === d.led_end) e.led_end = _('关灯和亮灯时间不能相同。');
		}

		if (id == 'nros' && d.nros_on && d.nros_mac) {
			if (!/^([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}$/.test(d.nros_mac))
				e.nros_mac = _('格式应为 AA:BB:CC:DD:EE:FF。');
		}

		if (id == 'sysopt' && d.fo_enabled == '1') {
			if (!d.fo_primary) e.fo_primary = _('请选择主线路。');
			if (!d.fo_secondary) e.fo_secondary = _('请选择备用线路。');
			if (d.fo_primary && d.fo_primary === d.fo_secondary)
				e.fo_secondary = _('主线路和备用线路不能是同一个。');
			if (!isIP4(d.fo_target)) e.fo_target = _('请填写正确的探测地址。');
		}

		if (id == 'lan') {
			if (!isIP4(d.lan_ip)) e.lan_ip = _('请填写正确的 IP 地址。');
			if (maskToBits(d.lan_mask) < 1) e.lan_mask = _('请填写正确的子网掩码。');
			if (this.lanConflict()) e.lan_ip = _('和上级网络网段冲突，请换一个。');
			if (d.dhcp_on) {
				var s = +d.dhcp_start, l = +d.dhcp_limit;
				if (!(s >= 2 && s <= 254)) e.dhcp_start = _('起始地址应在 2 到 254 之间。');
				if (!(l >= 1 && l <= 253)) e.dhcp_limit = _('数量应在 1 到 253 之间。');
				else if (s + l > 255) e.dhcp_limit = _('起始地址加数量超出了网段范围。');
			}
		}

		this.err = e;
		var keys = Object.keys(e);
		if (keys.length) this.notify(e[keys[0]], 'warning');
		return keys.length === 0;
	},

	/* ------------------------------------------------------------ 动作 */

	ledPreview: function (ev) {
		var self = this, d = this.d;
		var b = ev.target;
		b.disabled = true;
		this.ledPreviewed = true;
		return callLed(d.led_mode, d.led_start, d.led_end, d.led_days.join(' '))
			.then(function (r) {
				self.led = jparse(r && r.led_raw, self.led);
				self.notify(_('已按当前选择点亮/熄灭一次。'), 'info');
			})
			.catch(function (e) {
				self.notify(_('试灯失败：%s').format(e.message || e), 'error');
			})
			.finally(function () { b.disabled = false; self.renderStep(); });
	},

	/* 预览过但最终没应用时, 把进来时的模式原样回放一遍。
	 * ledsched 的 on/off 会写 manual 键, 光靠「不保存」是回不去的。 */
	ledRestore: function () {
		if (!this.ledPreviewed || this.ledSettled) return Promise.resolve();
		this.ledSettled = true;
		var c = this.st.led_cfg || {};
		var days = c.weekdays
			? (Array.isArray(c.weekdays) ? c.weekdays.join(' ') : String(c.weekdays))
			: '';
		return callLed(this.ledOrig, c.starttime || '', c.endtime || '', days).catch(function () {});
	},

	/* 关标签页 / 点别的菜单 / 刷新 / 后退时的补偿。
	 *
	 * 「立即试一下」是整个向导里唯一立刻改硬件的动作, 第 5 步那行提示也明说了
	 * 「离开时会自动恢复成原来的设置」。可 ledRestore() 原先只挂在 skip() 上,
	 * 而「跳过向导」这个按钮只有第 0 步才有(renderStep 里 step>0 就换成「上一步」)
	 * —— 试完灯直接关页面, 灯就停在试灯后的样子, 配置里还多一个 manual=off,
	 * 而用户从头到尾没按过「确认并应用」。真机上测出来的, 不是推测。
	 *
	 * 这里不能用普通的 rpc: 导航一开始浏览器就把没完成的 XHR/fetch 砍掉, 请求
	 * 根本发不出去。sendBeacon 就是为这个场景设计的, 页面销毁后浏览器会接着把它
	 * 发完。不退回同步 XHR —— 那会卡住卸载流程, 为一盏灯不值得。
	 *
	 * 手搓一次 jsonrpc 包体: rpc.declare() 给的是 Promise, 这儿要的是裸请求体。
	 * 格式照 rpc.js 的 declare(), params[0] 是会话 id, 端点就是 admin/ubus。
	 *
	 * 只听 pagehide, 不听 visibilitychange: 后者在用户临时切个标签页查东西时也会
	 * 触发, 那会把他正在看的试灯效果直接撤掉。 */
	ledRestoreBeacon: function () {
		if (!this.ledPreviewed || this.ledSettled) return;
		if (typeof navigator.sendBeacon != 'function') return;
		this.ledSettled = true;

		var c = this.st.led_cfg || {};
		var days = c.weekdays
			? (Array.isArray(c.weekdays) ? c.weekdays.join(' ') : String(c.weekdays))
			: '';
		var body = JSON.stringify({
			jsonrpc: '2.0', id: 1, method: 'call',
			params: [ rpc.getSessionID(), 'mwrt-wizard', 'led', {
				mode: this.ledOrig,
				starttime: c.starttime || '',
				endtime: c.endtime || '',
				weekdays: days
			} ]
		});
		try {
			navigator.sendBeacon(rpc.getBaseURL(),
				new Blob([ body ], { type: 'application/json' }));
		} catch (e) { /* 发不出去就算了, 灯而已, 别把卸载搞崩 */ }
	},

	switchBoot: function (ev) {
		var self = this;
		var target = (this.bootsw.system == 'sd') ? 'to-official' : 'to-sd';
		var name = (target == 'to-sd') ? _('SD 卡系统') : _('官方系统');

		ui.showModal(_('切换启动系统'), [
			E('p', {}, _('下次重启会从 %s 启动。这一步立即生效，不随向导一起应用。').format(name)),
			E('p', {}, _('两套系统各有独立的配置，切换后当前这些设置不会跟过去。')),
			E('div', { class: 'right' }, [
				this.btn(_('取消'), '', function () { ui.hideModal(); }),
				' ',
				this.btn(_('确认切换'), 'primary', function () {
					ui.hideModal();
					callBootsw(target).then(function (r) {
						var raw = jparse(r && r.raw, null);
						if (r && r.ok && (!raw || raw.ok !== false)) {
							self.notify(_('已设置为从 %s 启动，重启后生效。').format(name), 'info');
							return callBootsw('status').then(function (s) {
								self.bootsw = jparse(s && s.raw, self.bootsw);
								self.renderStep();
							});
						}
						self.notify(_('切换失败：%s').format((raw && raw.error) || (r && r.error) || '-'), 'error');
					}).catch(function (e) {
						self.notify(_('切换失败：%s').format(e.message || e), 'error');
					});
				})
			])
		]);
	},

	skip: function () {
		var self = this;
		ui.showModal(_('跳过向导'), [
			E('p', {}, _('不会改动任何设置。以后随时可以从「系统」菜单里的「向导」重新打开。')),
			E('div', { class: 'right' }, [
				this.btn(_('继续设置'), '', function () { ui.hideModal(); }),
				' ',
				this.btn(_('跳过'), 'primary', function () {
					ui.hideModal();
					/* markdone 失败也照样关窗: 这个标记现在只用来记"用户见过了",
					 * 已经不控制任何弹出行为(首启不再自动弹), 失败没有后果。
					 * ledRestore 由 close() 统一负责, 这儿不要重复调 —— 它有
					 * ledSettled 一次性闸门, 先调会让 close() 里那次变成空操作,
					 * 顺序上没错但白绕一圈。 */
					callMarkdone('1').catch(function () {}).then(function () {
						return self.close();
					});
				})
			])
		]);
	},

	/* 落盘。先 rpcd(安全项), 再 uci + 90 秒回滚(可能断连的项)。 */
	finish: function (ev) {
		var self = this, d = this.d;
		var b = ev.target;
		b.disabled = true;

		/* 应用期间封掉 × 和 Esc: 这中间有一半配置已经下去了, 关窗只会让用户
		 * 以为"没应用成功", 而且后半段 ui.changes.apply(true) 的确认框还要用。 */
		this.applying = true;

		ui.showModal(_('正在应用'), [
			E('p', { class: 'spinning' }, _('正在保存设置，请不要关闭页面。'))
		]);

		var payload = {
            timezone: (this.zones[d.tz] || {}).tzstring || this.st.timezone || '',
            zonename: d.tz
        };
        if (d.nros_on !== ((this.st.nros || {}).enabled == '1')) payload.nros_enabled = d.nros_on ? '1' : '0';
        if (d.nros_mac) payload.nros_mac = d.nros_mac;
        if (d.simsel && d.simsel !== String((this.st.modem || {}).cur || '')) payload.main_sim=d.simsel;
        if (d.subsim && d.subsim !== String((this.st.secondary || {}).cur || '')) payload.secondary_sim=d.subsim;

		var pwPromise = d.pw1
			? callSetPassword('root', d.pw1).catch(function (e) {
				return { __err: e.message || String(e) };
			})
			: Promise.resolve(null);

		pwPromise.then(function (pw) {
			if (pw && pw.__err)
				self.notify(_('密码修改失败：%s').format(pw.__err), 'error');

			return callApply(JSON.stringify(payload));
		}).then(function (r) {
			/* apply 的 payload 里带着 led_mode, 后端已经把灯按用户的选择写死了。
			 * 这里必须立刻封掉「离开时恢复」那条路, 否则紧接着
			 * ui.changes.apply(true) 重载页面会触发 pagehide, 把用户刚刚存好的
			 * 指示灯设置又"恢复"成进来时的样子 —— 试过灯的人才会中这一下。
			 * 放在 callApply 成功之后: 失败时页面还开着, 恢复那条路要留着。 */
			self.ledSettled = true;

			/* 这两条警告之后紧跟着 ui.changes.apply(true), 那条路最终会重载页面,
			 * 所以提示本来就是短命的(原先用 ui.addNotification 也一样会被冲掉)。
			 * 留在弹窗里至少能让用户在 apply 的确认框弹出来之前读到。 */
			if (r && r.errors)
				self.notify(_('部分设置未能保存：%s').format(r.errors), 'warning');
			/* 守护进程必须恰好 1 个: 多了会有两个进程抢默认路由 */
			if (r && r.failover_procs != null && r.failover_procs > 1)
				self.notify(_('检测到 %d 个线路监测进程，建议重启设备。').format(r.failover_procs), 'warning');

			/* 切卡是异步的: 后端只负责把 mt5700m.sh 起起来, 整套动作(模组上电、
			 * 拨 cpe-sel0、发 AT^SCICHG/^HVSST/+CFUN、查 SIM 状态)约 30 秒,
			 * 期间蜂窝会断。这台机器上蜂窝往往是唯一上行, 所以必须说一声 ——
			 * 不然用户只看到"已完成"却上不了网, 会以为是向导把设置搞坏了。 */
			if (r && r.slot_switching)
				self.notify(_('正在切换 SIM 卡槽，约需 30 秒，期间蜂窝网络会短暂中断。'), 'warning');

			self.stageUci();
			return uci.save();
		}).then(function () {
			ui.hideModal();
			/* 必须走 ui.changes.apply(true), 不是 uci.apply()。
			 *
			 * uci.apply(timeout) 是"发了就自己确认"的那条路: 它在 1 秒后就开始
			 * 轮询 confirm, 默认 10 秒内确认掉 —— 等于 apply_unchecked, 根本没有
			 * 等用户重新连上的环节。改 LAN IP 时页面必然失联, 它会确认不成功然后
			 * reject, 配置却已经生效, 救不回来。
			 *
			 * ui.changes.apply(true) 才是带连通性回滚的那条: 先弹"Connectivity
			 * change"确认框, 然后 POST admin/uci/apply_rollback, 90 秒
			 * (luci.apply.rollback=90) 内没从新地址确认成功就自动回滚。
			 * 需要先 ui.changes.init() 让它知道有待提交的改动。 */
			ui.changes.init();
			return ui.changes.apply(true);
		}).catch(function (e) {
			ui.hideModal();
			b.disabled = false;
			/* 失败了就把窗口交还给用户: 他得能改一下再试, 或者关掉。 */
			self.applying = false;
			self.notify(_('应用失败：%s').format(e.message || e), 'error');
		});
	},

	/* network / wireless / dhcp 只 set, 由 uci.apply(true) 统一提交并带回滚 */
	stageUci: function () {
		var d = this.d;

		/* uci.set 自己会过 resolveSID, '@system[0]' 这种写法直接能用 */
		if (d.hostname) uci.set('system', '@system[0]', 'hostname', d.hostname);
		if (d.lang) uci.set('luci', 'main', 'lang', langSeed(d.lang));

		/* ---- WAN ---- */
		var wandev = (this.st.wan || {}).device;
		if (wandev) {
		if (d.wan_mode == 'dhcp') {
			uci.set('network', 'wan', 'proto', 'dhcp');
			['username', 'password', 'ipaddr', 'netmask', 'gateway', 'dns'].forEach(function (k) {
				uci.unset('network', 'wan', k);
			});
			uci.set('network', 'wan', 'device', wandev);
			uci.unset('network', 'wan', 'disabled');
		} else if (d.wan_mode == 'pppoe') {
			uci.set('network', 'wan', 'proto', 'pppoe');
			uci.set('network', 'wan', 'device', wandev);
			uci.set('network', 'wan', 'username', d.pppoe_user);
			if (d.pppoe_pass) uci.set('network', 'wan', 'password', d.pppoe_pass);
			['ipaddr', 'netmask', 'gateway'].forEach(function (k) { uci.unset('network', 'wan', k); });
			uci.unset('network', 'wan', 'disabled');
		} else if (d.wan_mode == 'static') {
			uci.set('network', 'wan', 'proto', 'static');
			uci.set('network', 'wan', 'device', wandev);
			uci.set('network', 'wan', 'ipaddr', d.wan_ip);
			uci.set('network', 'wan', 'netmask', d.wan_mask);
			if (d.wan_gw) uci.set('network', 'wan', 'gateway', d.wan_gw);
			uci.set('network', 'wan', 'dns', d.wan_dns.split(/\s+/).filter(Boolean));
			['username', 'password'].forEach(function (k) { uci.unset('network', 'wan', k); });
			uci.unset('network', 'wan', 'disabled');
		} else {
			/* 仅蜂窝: 停掉有线 WAN, 但不删这个 section —— 删了用户想回来就得重建 */
			uci.set('network', 'wan', 'disabled', '1');
		}

		}
		/* ---- 无线 ---- */
		d.radios.forEach(function (r) {
			if (!r.device) return;
			/* mtwifi_cfg 认 disabled 这个键(mtwifi_cfg:251,313,322,480), 跟 mac80211 一样 */
			uci.set('wireless', r.device, 'disabled', r.on ? '0' : '1');
			if (!r.iface) return;
			uci.set('wireless', r.iface, 'disabled', r.on ? '0' : '1');
			if (!r.on) return;
			uci.set('wireless', r.iface, 'ssid', r.ssid);
			uci.set('wireless', r.iface, 'encryption', r.originalEncryption.split('+')[0] == r.encryption ? r.originalEncryption : r.encryption);
			if (r.encryption == 'none') uci.unset('wireless', r.iface, 'key');
			else uci.set('wireless', r.iface, 'key', r.key);
		});

		/* ---- LAN / DHCP ---- */
		uci.set('network', 'lan', 'ipaddr', d.lan_ip);
		uci.set('network', 'lan', 'netmask', d.lan_mask);
		if (d.dhcp_on) {
			uci.unset('dhcp', 'lan', 'ignore');
			uci.set('dhcp', 'lan', 'start', String(d.dhcp_start));
			uci.set('dhcp', 'lan', 'limit', String(d.dhcp_limit));
			uci.set('dhcp', 'lan', 'leasetime', d.dhcp_lease);
		} else {
			uci.set('dhcp', 'lan', 'ignore', '1');
		}
	}
});
