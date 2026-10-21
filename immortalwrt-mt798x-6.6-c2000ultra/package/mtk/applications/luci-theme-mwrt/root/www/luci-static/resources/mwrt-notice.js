/* mwrt-notice.js —— 右上角全局消息中心
 *
 * 数据来自 rpcd luci.mwrt-notice list(只读文件和 sysfs, 不发 AT), 每 10 秒刷新一次。
 * 多条消息叠加显示, 按等级排序(critical > warn > info)。
 *  - closable 的消息右侧有 ×, 关掉后记进 localStorage, 同一 id 不再出现;
 *  - 不可关闭的消息(模组切换 USB3.0、SIM 初始化、首次开机建虚拟内存)没有 ×,
 *    条件消失后自动撤掉, 底部带一条流动进度线, 表示"进行中"; 后端给了
 *    progress(0-100) 就换成实心进度条, 标签后跟百分比, 原地更新不重建卡片。
 * 新固件提示也走这里(id 带版本号, 关掉的是这一版), 替代原来的 mwrt-ota-notify.js。
 * 由全局 footer.ut 注入, 所有主题都有。样式自带(半透明毛玻璃卡片), 按页面底色
 * 自动切深浅; 文字一律 textContent。 */
(function () {
	'use strict';

	if (window.__mwrtNotice) return;
	window.__mwrtNotice = true;

	var me = document.currentScript;
	var LINKS = { ota: (me && me.getAttribute('data-ota')) || '' };
	var KEY = 'mwrt-notice-dismissed';
	var EVERY = 10000;
	var RANK = { critical: 0, warn: 1, info: 2 };
	var LABEL = { critical: '重要提示', warn: '请稍候', info: '系统通知' };
	// 图标是写死的常量 SVG, 不含任何后端数据
	var SVG = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" ' +
		'stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">';
	var ICON = {
		critical: SVG + '<path d="M10.3 3.9 1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z"/>' +
			'<path d="M12 9v4"/><path d="M12 17h.01"/></svg>',
		warn: SVG + '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg>',
		info: SVG + '<path d="M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9"/><path d="M10.3 21a1.9 1.9 0 0 0 3.4 0"/></svg>'
	};
	var CLOSE = SVG.replace('width="18" height="18"', 'width="14" height="14"') +
		'<path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>';

	var CSS = [
		'#mwrt-notice{position:fixed;top:12px;right:12px;z-index:2147483000;display:flex;flex-direction:column;',
		'gap:10px;width:368px;max-width:calc(100vw - 24px);pointer-events:none;',
		'font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif}',

		'#mwrt-notice .mwrt-notice-item{--c:#3b82f6;--ct:#1d4ed8;--cs:rgba(59,130,246,.13);--cb:rgba(59,130,246,.24);--cf:rgba(59,130,246,.35);position:relative;pointer-events:auto;display:flex;',
		'align-items:flex-start;gap:12px;box-sizing:border-box;margin:0;padding:12px 12px 12px 16px;overflow:hidden;',
		'border-radius:14px;font-size:13px;line-height:1.55;letter-spacing:.1px;text-align:left;color:#1f2937;',
		'background:rgba(255,255,255,.72);border:1px solid rgba(255,255,255,.65);',
		'-webkit-backdrop-filter:blur(16px) saturate(170%);backdrop-filter:blur(16px) saturate(170%);',
		'box-shadow:0 12px 32px -10px rgba(15,23,42,.28),0 2px 8px rgba(15,23,42,.08),inset 0 1px 0 rgba(255,255,255,.7);',
		'animation:mwrt-notice-in .38s cubic-bezier(.2,.8,.2,1) both;transition:transform .2s ease,box-shadow .2s ease}',
		'#mwrt-notice .mwrt-notice-item:hover{transform:translateY(-1px);',
		'box-shadow:0 16px 36px -10px rgba(15,23,42,.34),0 2px 8px rgba(15,23,42,.1),inset 0 1px 0 rgba(255,255,255,.7)}',
		'#mwrt-notice .mwrt-notice-item[data-level="critical"]{--c:#e5484d;--ct:#c62828;--cs:rgba(229,72,77,.13);--cb:rgba(229,72,77,.24);--cf:rgba(229,72,77,.35)}',
		'#mwrt-notice .mwrt-notice-item[data-level="warn"]{--c:#f59e0b;--ct:#b45309;--cs:rgba(245,158,11,.15);--cb:rgba(245,158,11,.28);--cf:rgba(245,158,11,.4)}',
		'#mwrt-notice .mwrt-notice-item.mwrt-leave{animation:mwrt-notice-out .22s ease-in both}',

		// 左侧细色条
		'#mwrt-notice .mwrt-notice-item::before{content:"";position:absolute;left:0;top:10px;bottom:10px;width:3px;',
		'border-radius:0 3px 3px 0;background:linear-gradient(180deg,var(--c),var(--cf))}',

		'#mwrt-notice .mwrt-notice-icon{flex:none;display:flex;align-items:center;justify-content:center;width:32px;height:32px;',
		'border-radius:10px;color:var(--c);background:var(--cs);',
		'box-shadow:inset 0 0 0 1px var(--cb)}',
		'#mwrt-notice .mwrt-notice-body{flex:1;min-width:0;word-break:break-word}',
		'#mwrt-notice .mwrt-notice-label{display:block;margin:0 0 2px;font-size:12px;font-weight:600;line-height:1.4;color:var(--ct)}',
		'#mwrt-notice .mwrt-notice-text{display:block;color:inherit}',

		'#mwrt-notice a.mwrt-notice-link{display:inline-flex;align-items:center;gap:4px;margin:8px 0 0;padding:3px 10px;',
		'border-radius:999px;font-size:12px;font-weight:600;line-height:1.6;text-decoration:none;color:var(--ct);',
		'background:var(--cs);transition:background .15s ease}',
		'#mwrt-notice a.mwrt-notice-link:hover{background:var(--cb)}',

		'#mwrt-notice button.mwrt-notice-close{all:unset;flex:none;display:flex;align-items:center;justify-content:center;',
		'width:24px;height:24px;margin:-2px -2px 0 0;border-radius:8px;cursor:pointer;color:#6b7280;transition:background .15s,color .15s}',
		'#mwrt-notice button.mwrt-notice-close:hover{background:rgba(15,23,42,.07);color:#111827}',
		'#mwrt-notice button.mwrt-notice-close:focus-visible,#mwrt-notice a.mwrt-notice-link:focus-visible{',
		'outline:2px solid var(--c);outline-offset:2px}',

		// 进行中: 底部流动细线
		'#mwrt-notice .mwrt-notice-busy{position:absolute;left:0;right:0;bottom:0;height:2px;overflow:hidden;',
		'background:var(--cs)}',
		'#mwrt-notice .mwrt-notice-busy::after{content:"";position:absolute;top:0;bottom:0;width:40%;',
		'background:linear-gradient(90deg,transparent,var(--c),transparent);animation:mwrt-notice-busy 1.6s ease-in-out infinite}',
		'#mwrt-notice .mwrt-notice-busy.mwrt-det::after{left:0;width:var(--p,0%);animation:none;background:var(--c);',
		'transition:width .6s ease}',
		'#mwrt-notice .mwrt-notice-pct{margin-left:6px;font-weight:500;opacity:.8;font-variant-numeric:tabular-nums}',

		// 深色页面
		'#mwrt-notice.mwrt-dark .mwrt-notice-item{color:#e5e7eb;background:rgba(28,31,38,.72);border-color:rgba(255,255,255,.08);',
		'box-shadow:0 12px 32px -10px rgba(0,0,0,.6),0 2px 8px rgba(0,0,0,.3),inset 0 1px 0 rgba(255,255,255,.06)}',
		'#mwrt-notice.mwrt-dark .mwrt-notice-item{--ct:#93c5fd}',
		'#mwrt-notice.mwrt-dark .mwrt-notice-item[data-level="critical"]{--ct:#ff9b9b}',
		'#mwrt-notice.mwrt-dark .mwrt-notice-item[data-level="warn"]{--ct:#fcd34d}',
		'#mwrt-notice.mwrt-dark button.mwrt-notice-close{color:#9ca3af}',
		'#mwrt-notice.mwrt-dark button.mwrt-notice-close:hover{background:rgba(255,255,255,.08);color:#f3f4f6}',

		// 不支持毛玻璃的浏览器: 提高不透明度保证可读
		'@supports not ((backdrop-filter:blur(1px)) or (-webkit-backdrop-filter:blur(1px))){',
		'#mwrt-notice .mwrt-notice-item{background:rgba(255,255,255,.95)}',
		'#mwrt-notice.mwrt-dark .mwrt-notice-item{background:rgba(28,31,38,.95)}}',

		'@media (max-width:600px){#mwrt-notice{left:8px;right:8px;width:auto;max-width:none}}',

		'@keyframes mwrt-notice-in{from{opacity:0;transform:translateX(24px) scale(.98)}to{opacity:1;transform:none}}',
		'@keyframes mwrt-notice-out{to{opacity:0;transform:translateX(24px) scale(.98)}}',
		'@keyframes mwrt-notice-busy{from{left:-40%}to{left:100%}}',
		'@media (prefers-reduced-motion:reduce){#mwrt-notice .mwrt-notice-item,#mwrt-notice .mwrt-notice-item.mwrt-leave,',
		'#mwrt-notice .mwrt-notice-busy::after{animation:none}#mwrt-notice .mwrt-notice-item:hover{transform:none}}'
	].join('');

	var box = null, listFn = null, timer = null;

	function dismissed() {
		try { return JSON.parse(localStorage.getItem(KEY) || '[]') || []; } catch (e) { return []; }
	}

	function dismiss(id) {
		var d = dismissed();
		if (d.indexOf(id) < 0) d.push(id);
		// 只留最近 50 条, 免得越攒越多
		try { localStorage.setItem(KEY, JSON.stringify(d.slice(-50))); } catch (e) {}
	}

	// 按页面实际底色判深浅: argon 的深色可以是跟随系统也可以是强制, 只看 media 不准
	function pageIsDark() {
		var m = null;
		[document.body, document.documentElement].some(function (el) {
			var c = el && getComputedStyle(el).backgroundColor;
			var p = c && c.match(/rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)(?:,\s*([\d.]+))?/);
			if (p && (p[4] === undefined || +p[4] > 0.5)) { m = p; return true; }
			return false;
		});
		if (!m) return !!(window.matchMedia && matchMedia('(prefers-color-scheme: dark)').matches);
		return (0.299 * m[1] + 0.587 * m[2] + 0.114 * m[3]) < 110;
	}

	// 手机上 argon 顶部有一条 49px 的标题栏(汉堡菜单+标题), 消息排在它下面。
	// 标题栏是 relative, 跟着 .main-right 一起滚走, 所以按它的高度而不是当前位置算,
	// 滚动时消息不跟着上下跳。桌面版 header 高度是 0, 消息贴到 12px。
	function place() {
		if (!box) return;
		var top = 12;
		var hd = document.querySelector('header');
		if (hd) {
			var h = hd.getBoundingClientRect().height;
			if (h > 0 && h < 120) top = Math.round(h) + 8;
		}
		box.style.top = top + 'px';
		box.classList.toggle('mwrt-dark', pageIsDark());
	}

	function container() {
		if (box && document.body.contains(box)) return box;
		if (!document.getElementById('mwrt-notice-style')) {
			var st = document.createElement('style');
			st.id = 'mwrt-notice-style';
			st.textContent = CSS;
			(document.head || document.documentElement).appendChild(st);
		}
		box = document.createElement('div');
		box.id = 'mwrt-notice';
		box.setAttribute('role', 'region');
		box.setAttribute('aria-label', '系统消息');
		box.style.display = 'none';
		document.body.appendChild(box);
		place();
		window.addEventListener('resize', place);
		return box;
	}

	function leave(c) {
		if (c.classList.contains('mwrt-leave')) return;
		c.classList.add('mwrt-leave');
		window.setTimeout(function () {
			if (c.parentNode) c.parentNode.removeChild(c);
			if (box && !box.querySelector('.mwrt-notice-item:not(.mwrt-leave)')) box.style.display = 'none';
		}, 220);
	}

	// 百分比放在 aria-hidden 的 span 里: 卡片是 alert, 改它的文字读屏软件会反复播报
	function setProgress(c, p) {
		var bz = c.querySelector('.mwrt-notice-busy');
		var pc = c.querySelector('.mwrt-notice-pct');
		if (!bz) return;
		var ok = typeof p === 'number' && isFinite(p);
		if (ok) {
			p = Math.max(0, Math.min(100, Math.round(p)));
			bz.style.setProperty('--p', p + '%');
		}
		bz.classList.toggle('mwrt-det', ok);
		if (pc) pc.textContent = ok ? p + '%' : '';
	}

	function card(n) {
		var level = RANK[n.level] === undefined ? 'info' : n.level;
		var c = document.createElement('div');
		c.className = 'mwrt-notice-item';
		c.setAttribute('data-id', n.id);
		c.setAttribute('data-level', level);
		c.setAttribute('data-text', n.text || '');
		// critical/warn 用 alert 让读屏软件立即播报, info 用 status 不打断
		c.setAttribute('role', level === 'info' ? 'status' : 'alert');

		var ic = document.createElement('span');
		ic.className = 'mwrt-notice-icon';
		ic.innerHTML = ICON[level];

		var body = document.createElement('div');
		body.className = 'mwrt-notice-body';
		var lb = document.createElement('span');
		lb.className = 'mwrt-notice-label';
		lb.textContent = LABEL[level];
		var t = document.createElement('span');
		t.className = 'mwrt-notice-text';
		t.textContent = n.text || '';
		body.appendChild(lb);
		body.appendChild(t);

		var href = n.link && LINKS[n.link];
		if (href) {
			var a = document.createElement('a');
			a.className = 'mwrt-notice-link';
			a.href = href;
			a.textContent = '前往查看 →';
			body.appendChild(a);
		}

		c.appendChild(ic);
		c.appendChild(body);

		if (n.closable) {
			var x = document.createElement('button');
			x.type = 'button';
			x.className = 'mwrt-notice-close';
			x.innerHTML = CLOSE;
			x.setAttribute('aria-label', '标记已读并关闭');
			x.setAttribute('title', '已读');
			x.addEventListener('click', function () {
				dismiss(n.id);
				leave(c);
			});
			c.appendChild(x);
		} else {
			var pc = document.createElement('span');
			pc.className = 'mwrt-notice-pct';
			pc.setAttribute('aria-hidden', 'true');
			lb.appendChild(pc);
			var bz = document.createElement('span');
			bz.className = 'mwrt-notice-busy';
			bz.setAttribute('aria-hidden', 'true');
			c.appendChild(bz);
			setProgress(c, n.progress);
		}
		return c;
	}

	function render(list) {
		var d = dismissed();
		var want = (list || []).filter(function (n) {
			return n && n.id && !(n.closable && d.indexOf(n.id) >= 0);
		}).sort(function (a, b) {
			return (RANK[a.level] === undefined ? 9 : RANK[a.level]) - (RANK[b.level] === undefined ? 9 : RANK[b.level]);
		});

		var b = container();
		place();
		// 内容没变就不动 DOM, 免得读屏软件每 10 秒重播一次
		var sig = want.map(function (n) { return n.id + '|' + n.text + '|' + (n.progress == null ? '' : n.progress); }).join('\n');
		if (b.getAttribute('data-sig') === sig) return;
		b.setAttribute('data-sig', sig);

		// 按 id 增量更新: 没变的卡片原地保留(不重播动画/读屏), 新的滑入, 没了的滑出
		var keep = {};
		Array.prototype.forEach.call(b.querySelectorAll('.mwrt-notice-item:not(.mwrt-leave)'), function (c) {
			keep[c.getAttribute('data-id')] = c;
		});
		var live = want.map(function (n) {
			var c = keep[n.id];
			if (c && c.getAttribute('data-text') === (n.text || '') && c.getAttribute('data-level') === n.level) {
				delete keep[n.id];
				if (!n.closable) setProgress(c, n.progress);
				return c;
			}
			return card(n);
		});
		Object.keys(keep).forEach(function (id) { leave(keep[id]); });
		live.forEach(function (c, i) {
			var at = b.querySelectorAll('.mwrt-notice-item:not(.mwrt-leave)')[i] || null;
			if (at !== c) b.insertBefore(c, at);
		});
		if (want.length) b.style.display = 'flex';
	}

	function tick() {
		if (!listFn) return;
		listFn().then(function (r) {
			render(r && Array.isArray(r.notices) ? r.notices : []);
		}).catch(function () { /* 没权限或后端没装: 什么都不显示 */ });
	}

	function run() {
		if (!window.L || !L.require) return;
		L.require('rpc').then(function (rpc) {
			listFn = rpc.declare({ object: 'luci.mwrt-notice', method: 'list' });
			tick();
			timer = window.setInterval(function () {
				if (!document.hidden) tick();
			}, EVERY);
		}).catch(function () {});
	}

	if (document.readyState === 'loading')
		document.addEventListener('DOMContentLoaded', run);
	else
		run();
}());
