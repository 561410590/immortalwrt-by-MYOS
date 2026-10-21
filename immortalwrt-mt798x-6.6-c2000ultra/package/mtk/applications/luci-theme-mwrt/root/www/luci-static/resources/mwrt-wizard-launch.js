/* mwrt-wizard-launch.js —— 菜单里那个「向导」的点击拦截器
 *
 * 这个文件取代了原来的 mwrt-wizard-boot.js。老版本干的是**首次登录自动跳转**
 * (window.location.replace 到向导页), 现在不要那个行为了: 向导只在用户主动点
 * 菜单时出现, 首启不弹。所以这里一行跳转都没有 —— 换成"把那次点击接住, 就地
 * 在当前页面上开弹窗"。
 *
 * 为什么必须在这儿拦, 而不是在菜单项上挂 click:
 *   argon 的 menu-argon.js 渲染叶子菜单时, 只有顶层(l==1)才挂 handleMenuExpand,
 *   叶子就是一个光秃秃的 <a href=L.url(...)>:
 *       E('a', { 'href': L.url(url, children[i].name),
 *                'click': (l==1) ? ui.createHandlerFn(...) : null, ... })
 *   菜单由主题渲染, 应用包改不到。所以用事件委托, 在 document 上捕获。
 *
 * 为什么用捕获阶段(第三个参数 true):
 *   要在主题/LuCI 自己的导航处理之前拿到这次点击。冒泡阶段可能已经有人
 *   preventDefault 或者开始跳页了。
 *
 * 故意**不**拦的几种点击: Ctrl/Cmd/Shift/中键/有 target 属性 —— 用户明确想在
 * 新标签页里打开, 那就让它正常导航到 admin/system/mwrt_wizard, 由那边的
 * view 壳子以 standalone 模式开窗。拦下来的话"新标签页打开"会变成死链, 很怪。
 */
(function () {
	'use strict';

	/* 菜单项的地址。判断只看这一段, 不拼完整 URL —— scriptname 可能是
	 * /cgi-bin/luci 也可能是 /, 还可能带语言前缀, 拼出来的不一定对得上。 */
	var MARK = 'mwrt_wizard';

	/* 本体的 require 名。resources/mwrt/wizard.js -> 'mwrt.wizard' */
	var CLASS = 'mwrt.wizard';

	function ready() {
		return !!(window.L && typeof window.L.require === 'function');
	}

	/* 点开的那一下。L.require 第一次会去拉 wizard.js(约 54KB), 所以可能有
	 * 一小段延迟; 本体自己会先挂一个转圈遮罩, 这里不用再管。
	 * 重复点击由本体的 open() 挡掉(rootEl/opening 两道), 不会开出两层。 */
	function launch() {
		if (!ready()) return false;
		try {
			window.L.require(CLASS).then(function (w) {
				w.open();
			}).catch(function (e) {
				/* 本体加载失败就退回"正常导航", 让 view 壳子去试 ——
				 * 至少用户不会点了没反应。 */
				try {
					window.location = window.L.url('admin', 'system', 'mwrt_wizard');
				} catch (e2) { /* 连 L.url 都没了, 只能放弃 */ }
			});
			return true;
		} catch (e) {
			return false;
		}
	}

	function onClick(ev) {
		/* 用户想在新标签页/新窗口打开 —— 放过 */
		if (ev.defaultPrevented) return;
		if (ev.button != null && ev.button !== 0) return;
		if (ev.ctrlKey || ev.metaKey || ev.shiftKey || ev.altKey) return;

		/* closest 在这台固件的 chrome 上有, 但 IE/老 webview 没有,
		 * 所以手工往上爬几层 —— 菜单结构是 li > a, 爬 4 层足够。 */
		var el = ev.target, a = null;
		for (var i = 0; el && i < 4; i++, el = el.parentNode) {
			if (el.tagName === 'A') { a = el; break; }
		}
		if (!a) return;
		if (a.target && a.target !== '' && a.target !== '_self') return;

		/* getAttribute 而不是 a.href: 后者会被浏览器补成绝对地址,
		 * 判断串匹配时无所谓, 但 getAttribute 拿到的就是 L.url() 生成的原样。 */
		var href = a.getAttribute('href') || '';
		if (href.indexOf(MARK) < 0) return;

		if (launch()) {
			ev.preventDefault();
			ev.stopPropagation();
		}
		/* launch() 失败(L 还没就绪)就不拦, 让它正常跳过去 —— 兜底那条路 */
	}

	try {
		/* iframe 里不装: 5700 模组页是 iframe 嵌进来的, 在小框里开一个
		 * 全屏弹窗没有意义, 而且外层不知道发生了什么。 */
		if (window.top !== window.self) return;

		var p = String(window.location.pathname || '');

		/* 登录页不装: 还没有会话, rpc 全是 403 */
		if (p.indexOf('/admin/') < 0) return;

		document.addEventListener('click', onClick, true);
	} catch (e) {
		/* 向导入口坏掉绝不能连带把管理界面弄瘫, 静默放过 */
	}
})();
