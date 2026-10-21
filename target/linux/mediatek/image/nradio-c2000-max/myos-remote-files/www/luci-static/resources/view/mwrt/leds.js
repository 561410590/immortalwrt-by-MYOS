'use strict';
'require view';
'require uci';
'require rpc';
'require form';
'require fs';
'require ui';
'require poll';

/* 定时关灯：接在原 LED 配置表格下面。
 *
 * 通过一个 zzz- 的 menu.d 条目只替换 admin/system/leds 的 action，故意不去覆盖
 * luci-mod-system 的 leds.js —— 那个文件属于别的包，升级/重装就会被冲掉。这里
 * 改成把原模块借过来，把本卡片接在它的表格下面：同一个地址、同一张表，多出一块面板。 */

var callLeds = rpc.declare({
	object: 'luci',
	method: 'getLEDs',
	expect: { '': {} }
});

var ENGINE = '/usr/bin/mwrt-ledsched';

var WEEKDAYS = [
	['1', _('周一')], ['2', _('周二')], ['3', _('周三')],
	['4', _('周四')], ['5', _('周五')], ['6', _('周六')],
	['0', _('周日')]
];

/* 取一个视图模块的“类”本身。
 *
 * 不能用 L.require()。它的收尾是
 *
 *     const instance = new _class(); ... classes[name] = instance;
 *
 * 也就是说 require 任何类都会顺手把它实例化 —— 而实例化一个 View 会跑完整的
 * View 生命周期，最后一步是 dom.content(#view, ...)。于是 require 原版 leds 视图
 * 会把它直接画进页面，盖掉本视图即将返回的内容，谁先谁后纯看 Promise 谁后完成。
 *
 * 所以这里照 luci.js 的做法自己编译一遍，只是不 new：取源码、解析开头的 require
 * 前导（原版 leds.js 依赖的 form/fs/uci/rpc 都是普通单例，正常 require 没问题），
 * 然后调用工厂函数。全程没有实例化，也就不会有人去动 DOM。 */
function borrowViewClass(name) {
	var url = L.env.base_url + '/' + name.replace(/\./g, '/') + '.js' +
	          (L.env.resource_version ? '?v=' + L.env.resource_version : '');

	return L.Request.get(url, { cache: true }).then(function (res) {
		if (!res.ok)
			throw new Error('HTTP %d for %s'.format(res.status, url));

		var src = res.text(),
		    deps = [], names = [];

		/* 只扫开头连续的字符串字面量，遇到既不是 'use strict' 也不是 require 的
		 * 就停 —— 和 luci.js 停的位置一样，这样下面代码里长得像 require 的字符串
		 * 不会被当成依赖。按 token 而不是按行扫：装到设备上的文件是压缩过的，
		 * 整个前导都在同一行上。 */
		for (var pos = 0; pos < src.length; ) {
			var rest = src.slice(pos),
			    skip = rest.match(/^(?:\s+|\/\/[^\n]*|\/\*[\s\S]*?\*\/)+/);

			if (skip) {
				pos += skip[0].length;
				rest = src.slice(pos);
			}

			var lit = rest.match(/^(['"])((?:[^'"\\]|\\.)*)\1[ \t]*;?/);
			if (!lit)
				break;

			var dep = lit[2].match(/^require[ \t]+(\S+?)(?:[ \t]+as[ \t]+([a-zA-Z_]\S*))?$/);
			if (dep) {
				deps.push(L.require(dep[1]));
				names.push(dep[2] || dep[1].replace(/[^a-zA-Z0-9_]/g, '_'));
			}
			else if (!lit[2].match(/^use[ \t]+strict$/)) {
				break;
			}

			pos += lit[0].length;
		}

		return Promise.all(deps).then(function (mods) {
			var factory = new Function(
				['window', 'document', 'L'].concat(names),
				src + '\n//# sourceURL=' + url + '\n');

			return factory.apply(null, [window, document, L].concat(mods));
		});
	});
}

/* 一排平铺的勾选框。
 *
 * form.MultiValue 只能是下拉框：它的 renderWidget 直接 new ui.Dropdown(multiple)，
 * 连自己的 widget 属性都不看。而 ui.Select 本身是支持 multiple + widget:'checkbox'
 * 的，所以这里只把 renderWidget 换掉，取值、依赖、保存语义全部沿用原类。 */
var CheckList = form.MultiValue.extend({
	__name__: 'CBI.MwrtCheckList',

	renderWidget: function (section_id, option_index, cfgvalue) {
		var widget = new ui.Select(
			L.toArray((cfgvalue != null) ? cfgvalue : this.default),
			this.transformChoices(), {
				id: this.cbid(section_id),
				sort: this.keylist,
				multiple: true,
				widget: 'checkbox',
				orientation: 'horizontal',
				disabled: (this.readonly != null) ? this.readonly : this.map.readonly
			});

		return widget.render();
	}
});

function engine(cmd) {
	return fs.exec(ENGINE, [cmd]);
}

function engineStatus() {
	return engine('status').then(function (res) {
		try {
			return JSON.parse((res.stdout || '').trim());
		}
		catch (e) {
			return null;
		}
	}).catch(function () {
		return null;
	});
}

/* “23:00”加上一个天数偏移 —— 引擎把偏移单独报出来，因为跨午夜的时间段结束那天
 * 和开始那天不是同一天。 */
function describeNext(st) {
	if (!st || !st.next_at)
		return null;

	var what = (st.next_state == 'off') ? _('熄灭') : _('恢复');
	var when = st.next_at;

	if (st.next_day == 1)
		when = _('明天 %s').format(st.next_at);
	else if (st.next_day > 1)
		when = _('%d 天后 %s').format(st.next_day, st.next_at);

	return _('下次%s：%s').format(what, when);
}

function badge(text, kind) {
	var bg = (kind == 'on')  ? '#16a34a'
	       : (kind == 'off') ? '#64748b'
	       :                   '#94a3b8';

	return E('span', {
		'style': 'display:inline-block;padding:.12em .6em;border-radius:999px;' +
		         'font-size:.82em;font-weight:600;color:#fff;white-space:nowrap;' +
		         'background:' + bg
	}, [text]);
}

function renderStatus(st) {
	if (!st)
		return E('em', {}, [_('LED 定时服务不可用。')]);

	if (!st.supported)
		return E('em', {}, [_('本机没有可控的指示灯。')]);

	var head;

	if (st.manual == 'off')
		head = [badge(_('手动关闭'), 'off'), ' ',
		        E('span', { 'style': 'opacity:.75' },
		          [_('（手动优先于定时，点“恢复定时”交回定时控制）')])];
	else if (st.manual == 'on')
		head = [badge(_('手动开启'), 'on'), ' ',
		        E('span', { 'style': 'opacity:.75' },
		          [_('（手动优先于定时，点“恢复定时”交回定时控制）')])];
	else if (st.dark)
		head = [badge(_('已熄灭'), 'off')];
	else
		head = [badge(_('点亮中'), 'on')];

	var next = describeNext(st);
	if (next && st.manual != 'off' && st.manual != 'on')
		head.push(E('span', { 'style': 'margin-left:.8em;opacity:.75' }, [next]));

	return E('div', {
		'style': 'display:flex;align-items:center;flex-wrap:wrap;gap:.3em'
	}, head);
}

return view.extend({
	load: function () {
		var self = this;

		/* 原模块只是借用、绝不实例化，所以它自己的视图生命周期不会跑，只有它的
		 * load()/render() 被复用 —— 原来的表格因此一字不改地照常工作。 */
		return Promise.all([
			uci.load('mwrt_led'),
			callLeds(),
			engineStatus(),
			borrowViewClass('view.system.leds').then(function (mod) {
				return Promise.resolve(mod.prototype.load.call(self)).then(function (d) {
					return { mod: mod, data: d };
				});
			}).catch(function (e) {
				/* 本卡片照样能用，只是上面那张原表格会缺失，原因留在控制台里。 */
				console.error('mwrt/leds: cannot borrow view.system.leds: ' +
					(e && (e.message || e)));
				return null;
			})
		]);
	},

	render: function (data) {
		var leds   = data[1] || {},
		    status = data[2],
		    stock  = data[3],
		    self   = this,
		    m, s, o;

		m = new form.Map('mwrt_led',
			_('定时关灯'),
			_('在设定的时间段内熄灭状态指示灯（比如夜里的卧室），到点后连触发器一起原样恢复。'));

		s = m.section(form.NamedSection, 'main', 'sched');
		s.anonymous = true;

		/* ---- 实时状态 ---------------------------------------------------- */

		o = s.option(form.DummyValue, '_status', _('当前状态'));
		o.rawhtml = false;
		o.cfgvalue = function () {
			return renderStatus(status);
		};

		o = s.option(form.DummyValue, '_actions');
		o.cfgvalue = function () {
			var refresh = function () {
				return engineStatus().then(function (st) {
					status = st;
					var box = document.querySelector('[data-ledsched-status]');
					if (box)
						box.parentNode.replaceChild(
							E('div', { 'data-ledsched-status': '' }, [renderStatus(st)]), box);
				});
			};

			var run = function (cmd, btn) {
				btn.classList.add('spinning');
				btn.disabled = true;
				return engine(cmd).then(refresh).catch(function (e) {
					ui.addNotification(null, E('p', {}, [
						_('LED 定时执行失败：%s').format(e.message || e)]), 'warning');
				}).finally(function () {
					btn.classList.remove('spinning');
					btn.disabled = false;
				});
			};

			return E('div', { 'style': 'display:flex;gap:.5em;flex-wrap:wrap' }, [
				E('button', {
					'class': 'cbi-button cbi-button-remove',
					'click': function (ev) { run('off', ev.currentTarget); }
				}, [_('立即关灯')]),
				E('button', {
					'class': 'cbi-button cbi-button-apply',
					'click': function (ev) { run('on', ev.currentTarget); }
				}, [_('立即开灯')]),
				E('button', {
					'class': 'cbi-button cbi-button-neutral',
					'click': function (ev) { run('auto', ev.currentTarget); }
				}, [_('恢复定时')])
			]);
		};

		/* ---- 定时设置 ---------------------------------------------------- */

		o = s.option(form.Flag, 'enabled', _('启用定时关灯'),
			_('关闭后所有指示灯完全交回系统自身控制。'));
		o.rmempty = false;

		o = s.option(form.ListValue, 'mode', _('模式'));
		o.value('schedule', _('按时间段关闭'));
		o.value('always_off', _('一直关闭'));
		o.default = 'schedule';
		o.depends('enabled', '1');

		/* LuCI 的校验库有 time（HH:MM:SS）和 timehhmmss，却没有纯 HH:MM 的类型。
		 * 所以在这里自己校验，而不是去写一个不存在的 datatype —— 未知 datatype 会
		 * 让控件在渲染时抛 SyntaxError: Unhandled token，整个页面一起白掉。 */
		var validHHMM = function (section_id, value) {
			if (value == null || value == '')
				return true;

			var m = String(value).match(/^([0-9]{1,2}):([0-9]{2})$/);
			if (!m || +m[1] > 23 || +m[2] > 59)
				return _('时间格式应为 HH:MM');

			return true;
		};

		o = s.option(form.Value, 'starttime', _('关灯时间'));
		o.placeholder = '23:00';
		o.validate = validHHMM;
		o.depends({ enabled: '1', mode: 'schedule' });

		o = s.option(form.Value, 'endtime', _('开灯时间'),
			_('早于关灯时间表示这个时间段跨过午夜。'));
		o.placeholder = '07:00';
		o.validate = validHHMM;
		o.depends({ enabled: '1', mode: 'schedule' });

		o = s.option(CheckList, 'weekdays', _('生效星期'),
			_('留空表示每天。跨午夜的时间段算在它开始的那一天。'));
		WEEKDAYS.forEach(function (d) { o.value(d[0], d[1]); });
		o.depends({ enabled: '1', mode: 'schedule' });

		o = s.option(CheckList, 'leds', _('要关闭的指示灯'),
			_('默认管理五个前面板灯，留空表示全部。关灯期间原信号灯守护暂时停用，未选灯保持当时状态；恢复后继续显示实时网络状态。'));
		Object.keys(leds).sort().forEach(function (n) { o.value(n, n); });
		o.depends('enabled', '1');

		/* ---- 渲染，接在原来的表格下面 ------------------------------------ */

		return m.render().then(function (ourNode) {
			/* 给状态格子做个标记，按钮才能就地刷新它 */
			var cell = ourNode.querySelector('.cbi-value-field');
			if (cell && cell.firstChild) {
				var wrap = E('div', { 'data-ledsched-status': '' }, [cell.firstChild]);
				cell.appendChild(wrap);
			}

			poll.add(function () {
				return engineStatus().then(function (st) {
					status = st;
					var box = ourNode.querySelector('[data-ledsched-status]');
					if (box)
						box.parentNode.replaceChild(
							E('div', { 'data-ledsched-status': '' }, [renderStatus(st)]), box);
				});
			}, 10);

			if (!stock)
				return ourNode;

			return Promise.resolve(stock.mod.prototype.render.call(self, stock.data))
				.then(function (stockNode) {
					return E('div', {}, [
						stockNode,
						E('hr', { 'style': 'margin:1.5em 0' }),
						ourNode
					]);
				})
				.catch(function () {
					return ourNode;
				});
		});
	},

	/* 两张 Map 都是 #maincontent 下普通的 .cbi-map 节点，luci.js 里 View 基类的
	 * handleSave/handleReset 都是遍历全部 .cbi-map 的，所以保存和复位不需要额外
	 * 处理；保存后的立即生效交给 /etc/init.d/mwrt-ledsched 的 config.change
	 * 触发器，不在这里追一次 apply —— LuCI 应用完会重载整个页面，那时候挂在
	 * handleSaveApply 后面的回调不一定还跑得到。 */
});

