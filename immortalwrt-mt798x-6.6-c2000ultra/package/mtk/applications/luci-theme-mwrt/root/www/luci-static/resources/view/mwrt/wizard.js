'use strict';
'require view';
'require ui';
'require mwrt.wizard as wizard';

/* admin/system/mwrt_wizard 这个地址的落点 —— 一层很薄的壳。
 *
 * 向导本体是 resources/mwrt/wizard.js, 一个 baseclass。为什么不能直接让菜单
 * 指向本体: luci.js 里的 View.__init__ 在**构造时**就 `DOM.content(#view, 转圈)`,
 * 而 L.require() 缓存的是实例 —— 只要本体是 view.extend, 光 require 就已经
 * 占了右侧内容区, 弹窗无从谈起。而 ui.instantiateView() 又对
 * `view instanceof View` 做硬校验, 所以菜单项也不能绕过 view 直接指 baseclass。
 *
 * 于是分成两层: 本体是 baseclass(能挂到 body 上当弹窗), 这里这层 view 只负责
 * 两件事 —— 把本体以 standalone 模式开起来, 并在底下留一句话。
 *
 * 正常从菜单点进来根本到不了这儿: mwrt-wizard-launch.js 会在捕获阶段拦掉那次
 * 点击, 直接在**当前页面**上开弹窗, 不发生导航。走到这儿的是另外几种人:
 *   - 中键 / Ctrl+点击 在新标签页打开(拦截器故意放过这些)
 *   - 老书签、手敲地址
 *   - 禁用了 footer 注入或者 JS 拦截没生效的场合(兜底)
 * 这几种情况下背景是一张空内容区, 所以用 standalone —— 关窗时送回概览页,
 * 而不是把人留在空白页上。
 */
return view.extend({
	/* 向导自己管所有的保存动作, 不要 LuCI 的标题栏和那条保存按钮 */
	handleSaveApply: null,
	handleSave: null,
	handleReset: null,

	render: function () {
		/* 弹窗是挂在 document.body 上的, 跟这个节点没有父子关系。
		 * 这里留一句话是为了"关了弹窗又按后退"之类的时候页面不是全白。 */
		var again = E('button', {
			class: 'cbi-button cbi-button-action',
			click: function () { wizard.open({ standalone: true }); }
		}, _('打开向导'));

		window.setTimeout(function () {
			wizard.open({ standalone: true });
		}, 0);

		return E('div', {}, [
			E('h2', {}, _('快速设置向导')),
			E('p', {}, _('向导会以窗口形式打开。如果没有自动弹出，请点击下面的按钮。')),
			again
		]);
	}
});
