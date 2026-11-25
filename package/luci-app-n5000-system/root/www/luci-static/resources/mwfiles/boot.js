/* Mwrt 文件管理入口(全局): 不占菜单栏。入口有三处: 首页「存储占用」卡片上的「文件管理」按钮、扁平首页的存储圆环、USB 存储设备接入通知里的「打开文件管理」;
   这些入口(以及任何带 data-mwfiles-open 的元素)点击后弹出文件管理浮窗, 浮窗脚本首次点击才加载。
   由 footer.ut 在装了文件管理(mwfiles 控制器存在)时下发; 没装时 window.MwrtFilesOpen 不存在, 各入口自行不显示。 */
(function() {
	'use strict';
	if (window.__mwrtFilesBoot) return;
	window.__mwrtFilesBoot = 1;

	var me = document.currentScript;
	var src = (me && me.getAttribute('data-src')) || '/luci-static/resources/mwfiles/main.js';
	var loading = false, queue = null;

	function open(opt) {
		if (window.MwrtFiles) { window.MwrtFiles.open(opt); return; }
		queue = opt || {};
		if (loading) return;
		loading = true;
		var s = document.createElement('script');
		s.src = src;
		s.onload = function() { loading = false; if (window.MwrtFiles) window.MwrtFiles.open(queue); queue = null; };
		s.onerror = function() { loading = false; queue = null; };
		document.head.appendChild(s);
	}
	window.MwrtFilesOpen = open;

	var RE = /\/admin\/mwfiles\/?(?:[?#].*)?$/;
	document.addEventListener('click', function(e) {
		if (e.defaultPrevented || e.button > 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
		if (e.target && e.target.id === 'mwfiles-host') return;	// 浮窗内部的点击不归入口脚本管(浮窗打开时 <html> 上也带 data-mwfiles-open 标记)
		var t = e.target && e.target.closest && e.target.closest('a[href],[data-mwfiles-open]');
		if (!t || t === document.documentElement) return;
		var hit = t.hasAttribute('data-mwfiles-open');
		if (!hit) {
			if (t.getAttribute('target') === '_blank') return;
			var h = t.getAttribute('href') || '';
			try { h = new URL(h, location.href); if (h.origin !== location.origin) return; h = h.pathname; } catch (x) { return; }
			hit = RE.test(h);
		}
		if (!hit) return;
		e.preventDefault();
		e.stopPropagation();
		open({ path: t.getAttribute('data-mwfiles-path') || '' });
	}, true);
})();
