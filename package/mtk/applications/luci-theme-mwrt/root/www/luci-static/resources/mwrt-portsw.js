/* mwrt-portsw.js —— 单网口机型(C2000 MAX)的网口 WAN/LAN 切换, 所有页面都有
 *
 * 两样东西:
 *  1. 切换浮窗: 这个网口当 LAN 还是 WAN, WAN 再选 DHCP / 宽带拨号 / 静态 IP,
 *     网关存活跟着开/关(后端一并写好)。首页网口卡片上的「切换 WAN/LAN」打开的
 *     就是它(mwrtPortSw.open())。后端 controller/modem.lua 的 port_get(只读, 不回传
 *     宽带密码) / port_set(POST + token)。写法、文案和默认值照设置向导。
 *  2. 接入检测提示: 网口当 LAN 时轮询 port_probe, 插上来的是光猫或上级路由器
 *     (对 DHCP / PPPoE 发现包有回应)就全屏问一句要不要改成 WAN。电脑这类终端
 *     不回应, 不弹。「切换为 WAN 口」打开上面那个浮窗并预选推荐的上网方式;
 *     「忽略」只管这一次插着: 记在路由器上(按网口的拔插计数), 换页面、换电脑都不再弹;
 *     网线拔了再插就重新检测, 还是路由器就照样弹。
 * 不是 MAX 的机型 port_probe 回 ok:false, 本脚本随即停下, 什么也不显示。
 *
 * 由全局 footer.ut 注入(install-mwrt-global.sh), 只给已登录页面; iframe 里不装。
 * 主题无关: 样式和图标(内联 SVG)都自带, 按页面底色切深浅, 扁平不加投影。
 * 后端给的数据(网关、拨号服务器名、报错)一律 textContent。 */
(function () {
	'use strict';

	if (window.mwrtPortSw) return;
	/* 5700 模组页等是 iframe 嵌进来的, 小框里弹全屏没有意义 */
	try { if (window.top !== window.self) return; } catch (e) { return; }

	var me = document.currentScript;
	function attr(k) { return (me && me.getAttribute(k)) || ''; }
	var BASE = attr('data-base');               // .../admin/modem
	if (!BASE) return;
	var URL_FO = attr('data-fo'), URL_LAN = attr('data-lan'), URL_HOME = attr('data-home');
	var GET = BASE + '/port_get', SET = BASE + '/port_set';
	var PROBE = BASE + '/port_probe', IGN = BASE + '/port_probe_ignore';

	var PD = { dhcp: '最常见。光猫或上级路由会自动分配地址。', pppoe: '运营商给了账号和密码时选这个。', static: '专线或上级手工分配固定地址。' };
	var PN = { dhcp: 'DHCP', pppoe: '宽带拨号', static: '静态 IP' };
	var MASK_DEF = '255.255.255.0', DNS_DEF = '223.5.5.5 119.29.29.29';

	/* 图标: 写死的常量 SVG, 不含任何后端数据 */
	var IC = {
		x: '<path d="M18 6 6 18"/><path d="m6 6 12 12"/>',
		ok: '<path d="M20 6 9 17l-5-5"/>',
		eth: '<path d="m15 20 3-3h2a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2H4a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h2l3 3z"/><path d="M6 8v1"/><path d="M10 8v1"/><path d="M14 8v1"/><path d="M18 8v1"/>',
		pc: '<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M2 20h20"/>',
		globe: '<circle cx="12" cy="12" r="10"/><path d="M12 2a14.5 14.5 0 0 0 0 20 14.5 14.5 0 0 0 0-20"/><path d="M2 12h20"/>',
		router: '<rect x="2" y="14" width="20" height="7" rx="2"/><path d="M6 17.5h.01"/><path d="M10 17.5h.01"/><path d="M15 10v4"/><path d="M17.84 7.17a4 4 0 0 0-5.66 0"/><path d="M20.66 4.34a8 8 0 0 0-11.31 0"/>',
		eye: '<path d="M2.06 12.35a1 1 0 0 1 0-.7 10.75 10.75 0 0 1 19.88 0 1 1 0 0 1 0 .7 10.75 10.75 0 0 1-19.88 0"/><circle cx="12" cy="12" r="3"/>',
		eyeoff: '<path d="M10.73 5.08A10.43 10.43 0 0 1 12 5c7 0 10 7 10 7a13.16 13.16 0 0 1-1.67 2.68"/><path d="M6.61 6.61A13.53 13.53 0 0 0 2 12s3 7 10 7a9.74 9.74 0 0 0 5.39-1.61"/><path d="M14.12 14.12a3 3 0 1 1-4.24-4.24"/><path d="m2 2 20 20"/>',
		pulse: '<path d="M22 12h-4l-3 9L9 3l-3 9H2"/>',
		arrow: '<path d="M5 12h14"/><path d="m12 5 7 7-7 7"/>',
		warn: '<path d="M10.3 3.9 1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z"/><path d="M12 9v4"/><path d="M12 17h.01"/>',
		plug: '<path d="M12 22v-5"/><path d="M9 8V2"/><path d="M15 8V2"/><path d="M18 8v5a4 4 0 0 1-4 4h-4a4 4 0 0 1-4-4V8Z"/>',
		info: '<circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/>',
		err: '<circle cx="12" cy="12" r="10"/><path d="M12 8v4"/><path d="M12 16h.01"/>',
		check: '<circle cx="12" cy="12" r="10"/><path d="m9 12 2 2 4-4"/>'
	};
	function ic(n, s) {
		s = s || 16;
		return '<svg viewBox="0 0 24 24" width="' + s + '" height="' + s + '" fill="none" stroke="currentColor" ' +
			'stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">' + IC[n] + '</svg>';
	}
	var SPIN = '<span class="mps-spin" aria-hidden="true"></span>';

	/* 所有选择器挂在 #mwrt-ps 下。主题(aurora)给 input/checkbox 的规则带了
	 * :not(#mwrt-nope) —— 一个 id 的权重, 输入框和勾选框这里也补一个 :not(#mwrt-nope) 压过去。 */
	var CSS = [
		'#mwrt-ps{--t1:#eef0f6;--t2:#8891a5;--t3:#3d4455;--ac:#7c5cfc;--gn:#34d399;--rd:#f87171;--bl:#60a5fa;--pk:#f472b6;--cy:#22d3ee;',
		'--wb:#121520;--ln:rgba(255,255,255,.08);--sf:rgba(136,145,165,.07);',
		'font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;font-size:13px;line-height:1.4;color:var(--t1)}',
		'#mwrt-ps.lt{--t1:#1a1d2e;--t2:#6b7185;--t3:#d4d7e0;--gn:#059669;--rd:#dc2626;--bl:#2563eb;--cy:#0891b2;--wb:#fff;--ln:rgba(20,24,40,.09);--sf:rgba(20,24,40,.025)}',
		'#mwrt-ps *{box-sizing:border-box;margin:0;padding:0;font-family:inherit;letter-spacing:normal;text-transform:none}',
		'#mwrt-ps [hidden]{display:none!important}',
		'#mwrt-ps svg{display:block;flex-shrink:0}',
		'#mwrt-ps button{-webkit-appearance:none;appearance:none;font:inherit;color:inherit;background:none;border:0;border-radius:0;',
		'box-shadow:none;text-shadow:none;min-width:0;min-height:0;height:auto;width:auto;line-height:1.3;cursor:pointer;gap:0}',
		'#mwrt-ps a{color:var(--ac);text-decoration:none;font-weight:600}',
		'#mwrt-ps a:hover{text-decoration:underline}',

		/* 罩: 盖住整个视口, 模糊背景。提示框的罩更浓一点 —— 它是主动弹出来的, 要把注意力收过来 */
		'#mwrt-ps .mps-ov{position:fixed;top:0;right:0;bottom:0;left:0;z-index:2147483100;display:none;align-items:center;justify-content:center;',
		'padding:20px;background:rgba(6,8,14,.55);-webkit-backdrop-filter:blur(16px) saturate(130%);backdrop-filter:blur(16px) saturate(130%)}',
		'#mwrt-ps .mps-ov.open{display:flex}',
		'#mwrt-ps.lt .mps-ov{background:rgba(226,230,240,.5)}',
		'#mwrt-ps #mps-pdm{background:rgba(6,8,14,.66);-webkit-backdrop-filter:blur(20px) saturate(130%);backdrop-filter:blur(20px) saturate(130%)}',
		'#mwrt-ps.lt #mps-pdm{background:rgba(214,219,232,.62)}',

		/* 窗口本身不透明, 不叠第二层毛玻璃(两层模糊文字会发虚); 扁平: 不加投影, 层次靠描边 + 顶部淡光晕 */
		'#mwrt-ps .mps-win{position:relative;width:100%;max-width:460px;max-height:100%;overflow-y:auto;overscroll-behavior:contain;',
		'border-radius:20px;border:1px solid var(--ln);padding:22px 22px 20px;text-align:left;',
		'background:radial-gradient(120% 60% at 0% 0%,rgba(124,92,252,.18),transparent 62%),radial-gradient(90% 50% at 100% 0%,rgba(34,211,238,.10),transparent 60%),var(--wb);',
		'animation:mps-in .24s ease-out}',
		'#mwrt-ps.lt .mps-win{background:radial-gradient(120% 60% at 0% 0%,rgba(124,92,252,.10),transparent 62%),radial-gradient(90% 50% at 100% 0%,rgba(34,211,238,.08),transparent 60%),#fff}',
		'@keyframes mps-in{from{opacity:0;transform:translateY(10px) scale(.98)}to{opacity:1;transform:none}}',
		'@keyframes mps-rot{to{transform:rotate(360deg)}}',
		'#mwrt-ps .mps-spin{display:inline-block;width:13px;height:13px;border-radius:50%;border:2px solid currentColor;border-right-color:transparent;animation:mps-rot .7s linear infinite;vertical-align:-2px}',

		'#mwrt-ps .mps-hd{display:flex;align-items:center;gap:13px;margin-bottom:20px;padding-right:34px}',
		'#mwrt-ps .mps-ico{width:42px;height:42px;border-radius:13px;display:flex;align-items:center;justify-content:center;color:#fff;background:linear-gradient(135deg,#7c5cfc,#22d3ee);flex-shrink:0}',
		'#mwrt-ps .mps-t{font-size:16px;font-weight:800;letter-spacing:.3px;line-height:1.25}',
		'#mwrt-ps .mps-s{font-size:11px;color:var(--t2);margin-top:4px;line-height:1.3;display:flex;align-items:center;gap:6px;flex-wrap:wrap}',
		'#mwrt-ps .mps-dot{width:6px;height:6px;border-radius:50%;background:var(--t3);flex-shrink:0}',
		'#mwrt-ps .mps-dot.on{background:var(--gn)}',
		'#mwrt-ps .mps-x{position:absolute;top:16px;right:16px;width:30px;height:30px;border-radius:10px;background:rgba(255,255,255,.06);color:var(--t2);',
		'display:flex;align-items:center;justify-content:center;padding:0;transition:background .2s,color .2s}',
		'#mwrt-ps .mps-x:hover{background:rgba(248,113,113,.16);color:#f87171}',
		'#mwrt-ps.lt .mps-x{background:rgba(0,0,0,.045)}',
		'#mwrt-ps.lt .mps-x:hover{background:rgba(220,38,38,.1);color:#dc2626}',
		'#mwrt-ps .mps-sec{display:flex;align-items:center;gap:10px;font-size:11px;font-weight:700;color:var(--t2);letter-spacing:1px;margin:0 0 10px}',
		'#mwrt-ps .mps-sec::after{content:"";flex:1;height:1px;background:linear-gradient(90deg,rgba(136,145,165,.25),transparent)}',

		/* 两张用途卡: LAN 绿、WAN 蓝(和首页网口格同色), 选中时描边 + 淡底 + 右上角对勾 */
		'#mwrt-ps .mps-modes{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:18px}',
		'#mwrt-ps .mps-card{--k:#34d399;--kb:rgba(52,211,153,.14);position:relative;display:grid;grid-template-columns:34px minmax(0,1fr);column-gap:11px;row-gap:3px;',
		'align-items:center;text-align:left;padding:13px 14px;border-radius:14px;border:1.5px solid rgba(136,145,165,.18);background:rgba(255,255,255,.025);',
		'color:var(--t1);transition:border-color .2s,background .2s}',
		'#mwrt-ps .mps-card[data-v="wan"]{--k:#60a5fa;--kb:rgba(96,165,250,.14)}',
		'#mwrt-ps.lt .mps-card{--k:#059669;--kb:rgba(5,150,105,.1);background:rgba(20,24,40,.02)}',
		'#mwrt-ps.lt .mps-card[data-v="wan"]{--k:#2563eb;--kb:rgba(37,99,235,.1)}',
		'#mwrt-ps .mps-card:hover{border-color:rgba(136,145,165,.36)}',
		'#mwrt-ps .mps-card.on{border-color:var(--k);background:var(--kb)}',
		'#mwrt-ps .mps-card:disabled:not(#mwrt-nope){cursor:default;opacity:.55}',
		'#mwrt-ps .pc-ic{grid-row:1/3;width:34px;height:34px;border-radius:10px;display:flex;align-items:center;justify-content:center;color:var(--k);background:var(--kb)}',
		'#mwrt-ps .mps-card.on .pc-ic{color:#fff;background:var(--k)}',
		'#mwrt-ps .pc-t{display:flex;align-items:center;gap:6px;font-size:14px;font-weight:800;letter-spacing:.3px;padding-right:22px}',
		'#mwrt-ps .pc-d{font-size:11px;color:var(--t2);line-height:1.45;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
		'#mwrt-ps .pc-cur{font-style:normal;font-size:10px;font-weight:700;padding:2px 6px;border-radius:6px;color:var(--k);background:var(--kb);white-space:nowrap}',
		'#mwrt-ps .pc-ck{position:absolute;top:12px;right:12px;width:18px;height:18px;border-radius:50%;display:flex;align-items:center;justify-content:center;',
		'color:#fff;background:var(--k);opacity:0;transform:scale(.6);transition:opacity .2s,transform .2s}',
		'#mwrt-ps .mps-card.on .pc-ck{opacity:1;transform:none}',

		/* 上网方式: 三段式, 每段两行(主名 + 一句话); 选中段描边 + 淡紫底 */
		'#mwrt-ps .mps-seg{display:grid;grid-template-columns:repeat(3,1fr);gap:4px;padding:4px;border-radius:13px;background:rgba(136,145,165,.1);margin-bottom:12px}',
		'#mwrt-ps .mps-seg button{display:flex;flex-direction:column;align-items:center;gap:2px;padding:8px 4px;border-radius:10px;border:1px solid transparent;',
		'color:var(--t2);transition:background .2s,color .2s,border-color .2s}',
		'#mwrt-ps .mps-seg b{font-size:13px;font-weight:700}',
		'#mwrt-ps .mps-seg small{font-size:10px;color:var(--t2);opacity:.85}',
		'#mwrt-ps .mps-seg button:hover{color:var(--t1)}',
		'#mwrt-ps .mps-seg button.on{color:var(--t1);background:rgba(124,92,252,.18);border-color:rgba(124,92,252,.55)}',
		'#mwrt-ps.lt .mps-seg button.on{background:#fff;border-color:rgba(124,92,252,.5)}',
		'#mwrt-ps .mps-pd{font-size:11px;color:var(--t2);line-height:1.5;margin:0 2px 14px}',
		'#mwrt-ps .mps-grid{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}',
		'#mwrt-ps .mps-f{display:flex;flex-direction:column;gap:5px;margin-bottom:12px}',
		'#mwrt-ps .mps-f>label{font-size:11px;font-weight:700;color:var(--t2);letter-spacing:.3px}',
		/* font-size 必须 ≥16px, 否则 iOS Safari 聚焦时会自动放大整页 */
		'#mwrt-ps .mps-f input:not(#mwrt-nope){display:block;width:100%;max-width:none;height:auto;min-height:0;margin:0;padding:10px 12px;border-radius:10px;',
		'border:1px solid rgba(136,145,165,.22);background:rgba(136,145,165,.08);color:var(--t1);font-size:16px;font-family:inherit;line-height:1.3;',
		'box-shadow:none;outline:0;transition:border-color .2s}',
		'#mwrt-ps .mps-pw{position:relative;display:block}',
		'#mwrt-ps .mps-pw input:not(#mwrt-nope){padding-right:40px}',
		'#mwrt-ps .mps-f input:not(#mwrt-nope):focus{border-color:var(--ac)}',
		'#mwrt-ps .mps-f input.bad:not(#mwrt-nope){border-color:var(--rd)}',
		'#mwrt-ps .mps-f input:not(#mwrt-nope)::placeholder{color:var(--t2);opacity:.7}',
		'#mwrt-ps .mps-pw button{position:absolute;top:0;right:0;height:100%;width:38px;padding:0;border-radius:0 10px 10px 0;display:flex;align-items:center;justify-content:center;color:var(--t2)}',
		'#mwrt-ps .mps-pw button:hover{color:var(--t1)}',

		/* 网关存活联动行: 左图标 + 说明, 右边一颗"现状 → 将变成"的胶囊 */
		'#mwrt-ps .mps-fo{display:flex;align-items:center;gap:12px;padding:12px 14px;border-radius:13px;border:1px solid rgba(136,145,165,.16);background:rgba(136,145,165,.05);margin:4px 0 12px}',
		'#mwrt-ps .mps-fo>i{width:30px;height:30px;border-radius:9px;display:flex;align-items:center;justify-content:center;color:var(--pk);background:rgba(244,114,182,.14);flex-shrink:0}',
		'#mwrt-ps .fo-m{flex:1;min-width:0}',
		'#mwrt-ps .fo-t{font-size:12px;font-weight:700;display:flex;align-items:center;gap:8px}',
		'#mwrt-ps .fo-t a{font-size:11px}',
		'#mwrt-ps .fo-d{font-size:11px;color:var(--t2);margin-top:3px;line-height:1.4}',
		'#mwrt-ps .fo-st{display:inline-flex;align-items:center;gap:6px;font-size:11px;font-weight:700;padding:5px 9px;border-radius:8px;white-space:nowrap;flex-shrink:0;',
		'color:var(--t2);background:rgba(136,145,165,.12)}',
		'#mwrt-ps .fo-st.on{color:var(--gn);background:rgba(52,211,153,.14)}',
		'#mwrt-ps.lt .fo-st.on{background:rgba(5,150,105,.1)}',
		'#mwrt-ps .fo-st svg{opacity:.7}',

		/* 提示条三种口气: warn 琥珀、bad 红、info 中性 */
		'#mwrt-ps .mps-note{display:flex;gap:10px;padding:12px 14px;border-radius:13px;font-size:12px;line-height:1.55;margin-bottom:12px;text-align:left}',
		'#mwrt-ps .mps-note>i{flex-shrink:0;margin-top:2px}',
		'#mwrt-ps .mps-note.warn{background:rgba(251,191,36,.1);color:#fcd34d;border:1px solid rgba(251,191,36,.22)}',
		'#mwrt-ps .mps-note.bad{background:rgba(248,113,113,.1);color:#fca5a5;border:1px solid rgba(248,113,113,.25)}',
		'#mwrt-ps .mps-note.info{background:rgba(96,165,250,.08);color:var(--t2);border:1px solid rgba(96,165,250,.18)}',
		'#mwrt-ps .mps-note.info>i{color:var(--bl)}',
		'#mwrt-ps.lt .mps-note.warn{background:rgba(217,119,6,.07);color:#92400e;border-color:rgba(217,119,6,.25)}',
		'#mwrt-ps.lt .mps-note.bad{background:rgba(220,38,38,.06);color:#b91c1c;border-color:rgba(220,38,38,.22)}',
		'#mwrt-ps.lt .mps-note.info{background:rgba(37,99,235,.05);border-color:rgba(37,99,235,.16)}',
		'#mwrt-ps .mps-note b{font-weight:800}',
		'#mwrt-ps .mps-note.bad a{color:inherit;text-decoration:underline}',
		'#mwrt-ps .mps-ack{display:flex;align-items:center;gap:8px;margin-top:9px;font-weight:700;cursor:pointer;-webkit-user-select:none;user-select:none}',
		'#mwrt-ps .mps-ack input:not(#mwrt-nope){position:absolute;width:1px;height:1px;margin:0;padding:0;opacity:0;pointer-events:none;-webkit-appearance:none;appearance:none}',
		'#mwrt-ps .mps-ack .bx{width:18px;height:18px;border-radius:6px;border:1.5px solid currentColor;display:flex;align-items:center;justify-content:center;flex-shrink:0;transition:background .2s}',
		'#mwrt-ps .mps-ack .bx svg{opacity:0;color:#1a1d2e}',
		'#mwrt-ps .mps-ack input:checked+.bx{background:currentColor}',
		'#mwrt-ps .mps-ack input:checked+.bx svg{opacity:1}',
		'#mwrt-ps.lt .mps-ack .bx svg{color:#fff}',
		'#mwrt-ps .mps-ack input:focus-visible+.bx{outline:2px solid currentColor;outline-offset:2px}',
		'#mwrt-ps .mps-err{display:none;align-items:center;gap:8px;font-size:12px;color:var(--rd);margin:0 2px 12px;line-height:1.45}',
		'#mwrt-ps .mps-err.show{display:flex}',

		'#mwrt-ps .mps-act{display:flex;gap:8px;margin-top:6px}',
		'#mwrt-ps .mps-act button{flex:1;display:inline-flex;align-items:center;justify-content:center;gap:7px;padding:12px;border-radius:12px;',
		'border:1px solid rgba(136,145,165,.2);background:rgba(136,145,165,.08);color:var(--t2);font-size:13px;font-weight:600;transition:background .2s,color .2s,opacity .2s,filter .2s}',
		'#mwrt-ps .mps-act button:hover{color:var(--t1)}',
		'#mwrt-ps .mps-act button.pri{border-color:transparent;background:linear-gradient(135deg,#7c5cfc,#a78bfa);color:#fff}',
		'#mwrt-ps .mps-act button.pri:hover{color:#fff;filter:brightness(1.08)}',
		'#mwrt-ps .mps-act button:disabled:not(#mwrt-nope){opacity:.5;cursor:default;filter:none}',
		'#mwrt-ps .mps-act button:focus-visible,#mwrt-ps .mps-card:focus-visible,#mwrt-ps .mps-seg button:focus-visible,#mwrt-ps .mps-x:focus-visible{outline:2px solid var(--ac);outline-offset:2px}',

		/* 应用之后窗口换成结果页: 大图标 + 一句结论 + 接下来会发生什么 */
		'#mwrt-ps .mps-done{display:none;flex-direction:column;align-items:center;text-align:center;padding:10px 6px 2px}',
		'#mwrt-ps .mps-win.done .mps-done{display:flex}',
		'#mwrt-ps .mps-win.done .mps-main{display:none}',
		'#mwrt-ps .dn-ic{width:62px;height:62px;border-radius:50%;display:flex;align-items:center;justify-content:center;color:#fff;',
		'background:linear-gradient(135deg,#34d399,#22d3ee);margin-bottom:16px;animation:mps-in .3s ease-out}',
		'#mwrt-ps .dn-t{font-size:17px;font-weight:800;margin-bottom:8px}',
		'#mwrt-ps .dn-d{font-size:12px;color:var(--t2);line-height:1.65;margin-bottom:18px}',
		'#mwrt-ps .mps-done .mps-act{width:100%}',

		/* === 接入检测提示: 居中排版, 顶部一颗带两圈扩散波纹的设备图标 === */
		'#mwrt-ps .pdm-win{max-width:440px;text-align:center;padding:30px 26px 22px;',
		'background:radial-gradient(90% 50% at 50% 0%,rgba(34,211,238,.16),transparent 66%),radial-gradient(120% 60% at 0% 0%,rgba(124,92,252,.14),transparent 62%),var(--wb)}',
		'#mwrt-ps.lt .pdm-win{background:radial-gradient(90% 50% at 50% 0%,rgba(34,211,238,.12),transparent 66%),radial-gradient(120% 60% at 0% 0%,rgba(124,92,252,.08),transparent 62%),#fff}',
		'#mwrt-ps .pdm-radar{position:relative;width:88px;height:88px;margin:0 auto 16px;display:flex;align-items:center;justify-content:center}',
		'#mwrt-ps .pdm-ring{position:absolute;top:0;left:0;right:0;bottom:0;border-radius:50%;border:1.5px solid var(--cy);opacity:0;animation:pdm-wave 2.4s ease-out infinite}',
		'#mwrt-ps .pdm-ring.r2{animation-delay:1.2s}',
		'@keyframes pdm-wave{0%{transform:scale(.6);opacity:.85}100%{transform:scale(1.28);opacity:0}}',
		'#mwrt-ps .pdm-core{position:relative;width:60px;height:60px;border-radius:19px;display:flex;align-items:center;justify-content:center;color:#fff;',
		'background:linear-gradient(135deg,#7c5cfc,#22d3ee)}',
		'#mwrt-ps .pdm-chip{display:inline-flex;align-items:center;gap:6px;padding:4px 11px;border-radius:999px;font-size:11px;font-weight:700;letter-spacing:.5px;',
		'color:var(--cy);background:rgba(34,211,238,.12);margin-bottom:12px}',
		'#mwrt-ps.lt .pdm-chip{background:rgba(8,145,178,.09)}',
		'#mwrt-ps .pdm-chip i{width:6px;height:6px;border-radius:50%;background:currentColor;animation:pdm-blink 1.6s ease-in-out infinite}',
		'@keyframes pdm-blink{50%{opacity:.3}}',
		'#mwrt-ps .pdm-t{font-size:18px;font-weight:800;line-height:1.5;letter-spacing:.2px;margin:0 4px 10px}',
		'#mwrt-ps .pdm-d{font-size:12.5px;color:var(--t2);line-height:1.7;margin:0 4px 18px}',
		'#mwrt-ps .pdm-kv{text-align:left;border:1px solid var(--ln);border-radius:14px;background:var(--sf);padding:2px 14px;margin-bottom:12px}',
		'#mwrt-ps .pdm-kv .kv{display:flex;align-items:center;justify-content:space-between;gap:14px;padding:10px 0;font-size:12px}',
		'#mwrt-ps .pdm-kv .kv+.kv{border-top:1px dashed var(--ln)}',
		'#mwrt-ps .pdm-kv .kv>span{color:var(--t2);flex-shrink:0}',
		'#mwrt-ps .pdm-kv .kv>b{font-weight:700;text-align:right;min-width:0;overflow-wrap:anywhere}',
		'#mwrt-ps .pdm-kv .kv>b.rec{display:inline-flex;align-items:center;gap:6px;color:var(--ac)}',
		'#mwrt-ps .pdm-win .mps-act{margin-top:16px}',
		'#mwrt-ps .pdm-ft{font-size:11px;color:var(--t2);line-height:1.65;margin-top:14px}',

		'@media(max-width:420px){#mwrt-ps .mps-ov{padding:12px}#mwrt-ps .mps-win{padding:18px 16px 16px}#mwrt-ps .mps-grid{grid-template-columns:1fr}',
		'#mwrt-ps .mps-card{grid-template-columns:1fr;row-gap:4px;align-items:start}#mwrt-ps .pc-ic{grid-row:auto;margin-bottom:4px}#mwrt-ps .pc-d{white-space:normal}',
		'#mwrt-ps .pdm-win{padding:24px 18px 18px}#mwrt-ps .pdm-t{font-size:16px}#mwrt-ps .pdm-act{flex-direction:column-reverse}#mwrt-ps .pdm-act button{flex:none}}'
	].join('');

	var DLG = [
		'<div class="mps-ov" id="mps-dlg" role="dialog" aria-modal="true" aria-labelledby="mps-t" aria-hidden="true">',
		'<div class="mps-win" id="mps-win">',
		'<button type="button" class="mps-x" data-act="close" aria-label="关闭">', ic('x', 15), '</button>',
		'<div class="mps-main">',
		'<div class="mps-hd"><span class="mps-ico">', ic('eth', 20), '</span><div><div class="mps-t" id="mps-t">网口用途</div><div class="mps-s" id="mps-sub"></div></div></div>',
		'<div class="mps-sec">接口用途</div>',
		'<div class="mps-modes">',
		'<button type="button" class="mps-card" data-act="mode" data-v="lan" disabled><span class="pc-ck">', ic('ok', 11), '</span><span class="pc-ic">', ic('pc', 17), '</span>',
		'<span class="pc-t">LAN<em class="pc-cur">当前</em></span><span class="pc-d">接电脑、交换机等设备</span></button>',
		'<button type="button" class="mps-card" data-act="mode" data-v="wan" disabled><span class="pc-ck">', ic('ok', 11), '</span><span class="pc-ic">', ic('globe', 17), '</span>',
		'<span class="pc-t">WAN<em class="pc-cur">当前</em></span><span class="pc-d">接光猫或上级路由上网</span></button>',
		'</div>',
		'<div id="mps-wan" hidden>',
		'<div class="mps-sec">上网方式</div>',
		'<div class="mps-seg" role="radiogroup" aria-label="上网方式">',
		'<button type="button" data-act="proto" data-v="dhcp" role="radio"><b>DHCP</b><small>自动获取</small></button>',
		'<button type="button" data-act="proto" data-v="pppoe" role="radio"><b>宽带拨号</b><small>账号密码</small></button>',
		'<button type="button" data-act="proto" data-v="static" role="radio"><b>静态 IP</b><small>手动填写</small></button>',
		'</div>',
		'<div class="mps-pd" id="mps-pd"></div>',
		'<div class="mps-note info" id="mps-pre" hidden><i>', ic('check', 14), '</i><span id="mps-pre-t"></span></div>',
		'<div id="mps-f-pppoe" hidden>',
		'<div class="mps-f"><label for="mps-user">宽带账号</label><input type="text" id="mps-user" maxlength="128" autocomplete="off" spellcheck="false" placeholder="运营商提供的宽带账号"></div>',
		'<div class="mps-f"><label for="mps-pass">宽带密码</label><span class="mps-pw"><input type="password" id="mps-pass" maxlength="128" autocomplete="new-password" spellcheck="false" placeholder="请输入宽带密码">',
		'<button type="button" id="mps-eye" data-act="eye" aria-label="显示或隐藏密码">', ic('eye', 15), '</button></span></div>',
		'</div>',
		'<div id="mps-f-static" class="mps-grid" hidden>',
		'<div class="mps-f"><label for="mps-ip">IP 地址</label><input type="text" id="mps-ip" inputmode="decimal" autocomplete="off" spellcheck="false" placeholder="例如 192.168.1.10"></div>',
		'<div class="mps-f"><label for="mps-mask">子网掩码</label><input type="text" id="mps-mask" inputmode="decimal" autocomplete="off" spellcheck="false" placeholder="255.255.255.0"></div>',
		'<div class="mps-f"><label for="mps-gw">网关</label><input type="text" id="mps-gw" inputmode="decimal" autocomplete="off" spellcheck="false" placeholder="例如 192.168.1.1"></div>',
		'<div class="mps-f"><label for="mps-dns">DNS 服务器</label><input type="text" id="mps-dns" autocomplete="off" spellcheck="false" placeholder="多个用空格隔开"></div>',
		'</div>',
		'</div>',
		'<div class="mps-fo" id="mps-fo" hidden><i>', ic('pulse', 14), '</i><div class="fo-m"><div class="fo-t">网关存活<a id="mps-fo-a" href="#">查看设置</a></div>',
		'<div class="fo-d" id="mps-fo-d"></div></div><span class="fo-st" id="mps-fo-st"></span></div>',
		'<div class="mps-note bad" id="mps-via" hidden><i>', ic('plug', 14), '</i><span>检测到你正通过这个网口访问本页面。切换成 WAN 后连接会立即断开，请先改用 WiFi 连接路由器再操作。</span></div>',
		'<div class="mps-note warn" id="mps-warn" hidden><i>', ic('warn', 14), '</i><div><div>选择 WAN 后该接口不能接电脑使用，只能接上级路由或宽带转 WiFi 使用，请知悉。</div>',
		'<label class="mps-ack" id="mps-ack-l"><input type="checkbox" id="mps-ack"><span class="bx">', ic('ok', 11), '</span>我已知悉</label></div></div>',
		'<div class="mps-note info" id="mps-lan" hidden><i>', ic('info', 14), '</i><span>改回 LAN 后，这个网口可以接电脑等局域网设备使用，有线上网随之停用，上网方式恢复为出厂的 DHCP（宽带账号会保留）。</span></div>',
		'<div class="mps-err" id="mps-err" role="alert">', ic('err', 14), '<span id="mps-err-t"></span></div>',
		'<div class="mps-act"><button type="button" data-act="close">取消</button><button type="button" class="pri" id="mps-ok" data-act="save" disabled></button></div>',
		'</div>',
		'<div class="mps-done" aria-live="polite"><span class="dn-ic">', ic('ok', 28), '</span><div class="dn-t" id="mps-dn-t"></div><div class="dn-d" id="mps-dn-d"></div>',
		'<div class="mps-act"><button type="button" class="pri" data-act="close">完成</button></div></div>',
		'</div></div>'
	].join('');

	/* 全屏提示。故意不给关闭键、点罩和 Esc 也不关: 只有「忽略」和「切换为 WAN 口」两条路 */
	var ASK = [
		'<div class="mps-ov" id="mps-pdm" role="alertdialog" aria-modal="true" aria-labelledby="mps-pt" aria-describedby="mps-pdd" aria-hidden="true">',
		'<div class="mps-win pdm-win">',
		'<div class="pdm-radar"><span class="pdm-ring"></span><span class="pdm-ring r2"></span><span class="pdm-core">', ic('router', 28), '</span></div>',
		'<div class="pdm-chip"><i></i>网口接入检测</div>',
		'<div class="pdm-t" id="mps-pt">检测到接入到路由器或网关设备，是否需要把网口切换为 WAN 口？</div>',
		'<div class="pdm-d" id="mps-pdd">这个网口现在是 LAN 口（接电脑用），插在上面的设备回应了上网请求，看起来是光猫或上级路由器。改成 WAN 口后，路由器才能通过它上网。</div>',
		'<div class="pdm-kv">',
		'<div class="kv"><span>识别依据</span><b id="mps-k-why"></b></div>',
		'<div class="kv" id="mps-k-gw"><span>上级网关</span><b id="mps-k-gwv"></b></div>',
		'<div class="kv" id="mps-k-ac"><span>拨号服务器</span><b id="mps-k-acv"></b></div>',
		'<div class="kv"><span>推荐上网方式</span><b class="rec" id="mps-k-rec"></b></div>',
		'</div>',
		'<div class="mps-note bad" id="mps-cf" hidden><i>', ic('warn', 14), '</i><span>对方分配的地址和本机局域网（<b id="mps-cf-ip"></b>）在同一网段，直接改成 WAN 会地址冲突、上不了网。',
		'请先<a id="mps-cf-a" href="#">修改局域网地址</a>，换到别的网段（例如 192.168.88.1）再来切换。</span></div>',
		'<div class="mps-act pdm-act"><button type="button" id="mps-ign" data-act="ign">忽略</button>',
		'<button type="button" class="pri" id="mps-go" data-act="go">', ic('arrow', 15), '切换为 WAN 口</button></div>',
		'<div class="pdm-ft">忽略只对这次接入有效，重新插拔网线后会再次检测；也可以随时在<a id="mps-home" href="#">首页</a>的网口卡片上点「切换 WAN/LAN」手动修改。</div>',
		'</div></div>'
	].join('');

	var host = null;
	var cur = null;                      /* port_get 的返回; null = 还没读到 */
	var sel = { mode: '', proto: 'dhcp' };  /* 浮窗里当前选中的 */
	var busy = false, seq = 0;           /* seq: 关掉再打开后, 丢弃上一次还没回来的读取 */
	var onclose = null;                  /* 从提示框进来的: 关浮窗时告诉它有没有真的切换 */
	var ask = null, ignBusy = false;     /* 提示框当前显示的那份检测结果 */
	var tm = null, stopped = false;

	function $(id) { return document.getElementById(id); }
	function show(id, on) { var e = $(id); if (e) e.hidden = !on; }
	function each(q, f) { var l = host.querySelectorAll(q); for (var i = 0; i < l.length; i++) f(l[i]); }
	function isOpen(e) { return !!(e && e.classList.contains('open')); }
	function href(id, url) { var a = $(id); if (!a) return; if (url) a.href = url; else a.removeAttribute('href'); }

	/* 按页面实际底色判深浅。首页(.P)有自己的深浅开关, 跟它走; 别的页面看 body 底色 */
	function dark() {
		var p = document.querySelector('.P');
		if (p && getComputedStyle(p).getPropertyValue('--t1').trim()) return !p.classList.contains('L');
		var m = null;
		[document.body, document.documentElement].some(function (el) {
			var c = el && getComputedStyle(el).backgroundColor;
			var x = c && c.match(/rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)(?:,\s*([\d.]+))?/);
			if (x && (x[4] === undefined || +x[4] > 0.5)) { m = x; return true; }
			return false;
		});
		if (!m) return !!(window.matchMedia && matchMedia('(prefers-color-scheme: dark)').matches);
		return (0.299 * m[1] + 0.587 * m[2] + 0.114 * m[3]) < 110;
	}

	function build() {
		if (host && document.body.contains(host)) return;
		if (!$('mps-css')) {
			var st = document.createElement('style');
			st.id = 'mps-css';
			st.textContent = CSS;
			/* 放在 body 末尾: 文档顺序排在主题样式表之后, 同权重时算我们的 */
			document.body.appendChild(st);
		}
		host = document.createElement('div');
		host.id = 'mwrt-ps';
		host.innerHTML = DLG + ASK;
		document.body.appendChild(host);
		href('mps-fo-a', URL_FO); href('mps-cf-a', URL_LAN); href('mps-home', URL_HOME);
		host.addEventListener('mousedown', function (e) { downT = e.target; });
		host.addEventListener('click', onClick);
		$('mps-ack').addEventListener('change', sync);
	}

	var downT = null;
	function onClick(e) {
		var t = e.target;
		/* 点罩空白处才关(在窗口里按下、拖到罩上松开的不算); 提示框的罩不响应 */
		if (t === $('mps-dlg')) { if (downT === t) close(); return; }
		for (var i = 0; t && t !== host && i < 6; i++, t = t.parentNode) {
			var a = t.getAttribute && t.getAttribute('data-act');
			if (!a) continue;
			if (t.disabled) return;
			var v = t.getAttribute('data-v');
			if (a === 'close') close();
			else if (a === 'mode') mode(v);
			else if (a === 'proto') proto(v);
			else if (a === 'eye') eye();
			else if (a === 'save') save();
			else if (a === 'ign') ignore();
			else if (a === 'go') goWan();
			return;
		}
	}

	var locks = 0;
	function ovOpen(m) {
		if (isOpen(m)) return;
		host.classList.toggle('lt', !dark());
		m.classList.add('open'); m.setAttribute('aria-hidden', 'false');
		if (locks++ === 0) document.documentElement.style.overflow = 'hidden';
	}
	function ovClose(m) {
		if (!isOpen(m)) return;
		m.classList.remove('open'); m.setAttribute('aria-hidden', 'true');
		if (--locks <= 0) { locks = 0; document.documentElement.style.overflow = ''; }
	}

	function token() {
		if (window.L && L.env && L.env.token) return L.env.token;
		var i = document.querySelector('input[name="token"]');
		return i ? i.value : '';
	}
	function req(method, url, body, cb, timeout) {
		var x = new XMLHttpRequest();
		x.open(method, method === 'GET' ? url + (url.indexOf('?') < 0 ? '?' : '&') + '_=' + Date.now() : url, true);
		x.timeout = timeout || 15000;
		if (body !== null) x.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
		x.onreadystatechange = function () {
			if (x.readyState !== 4) return;
			var d = null;
			try { d = JSON.parse(x.responseText); } catch (e) { d = null; }
			cb(d && typeof d === 'object' ? d : null, x.status);
		};
		x.send(body);
	}
	function form(o) {
		var b = [], k;
		for (k in o) b.push(encodeURIComponent(k) + '=' + encodeURIComponent(o[k]));
		return b.join('&');
	}

	/* ===================== 切换浮窗 ===================== */
	function err(msg, field) {
		$('mps-err-t').textContent = msg || '';
		$('mps-err').classList.toggle('show', !!msg);
		each('input.bad', function (e) { e.classList.remove('bad'); });
		if (field && $(field)) { $(field).classList.add('bad'); $(field).focus(); }
	}

	/* pre: 从提示框进来时预选 { mode:'wan', proto, note, onclose(done) } */
	function open(pre) {
		build();
		pre = pre && typeof pre === 'object' ? pre : {};
		var m = $('mps-dlg');
		busy = false; cur = null; sel = { mode: '', proto: 'dhcp' };
		onclose = typeof pre.onclose === 'function' ? pre.onclose : null;
		$('mps-win').classList.remove('done');
		$('mps-sub').textContent = '读取当前配置…';
		['mps-user', 'mps-pass', 'mps-ip', 'mps-mask', 'mps-gw', 'mps-dns'].forEach(function (id) { $(id).value = ''; });
		$('mps-pass').type = 'password'; $('mps-eye').innerHTML = ic('eye', 15);
		$('mps-ack').checked = false;
		$('mps-pre-t').textContent = pre.note || '';
		err('');
		each('.mps-card', function (c) { c.disabled = true; c.classList.remove('on'); });
		each('.pc-cur', function (c) { c.hidden = true; });
		['mps-wan', 'mps-fo', 'mps-via', 'mps-warn', 'mps-lan', 'mps-pre'].forEach(function (id) { show(id, false); });
		var ok = $('mps-ok'); ok.disabled = true; ok.innerHTML = ic('ok', 15) + '保存并应用';
		ovOpen(m);
		var my = ++seq;
		req('GET', GET, null, function (d, st) {
			if (my !== seq) return;
			if (!d || !d.ok) {
				$('mps-sub').textContent = (d && d.error) || (st === 403 ? '会话已过期，请刷新页面后重试' : '读取失败，请稍后重试');
				return;
			}
			cur = d;
			var now = d.mode === 'wan' ? 'WAN · ' + (PN[d.proto] || d.proto) : (d.mode === 'lan' ? 'LAN' : '未分配');
			var sub = $('mps-sub');
			sub.textContent = '';
			var dot = document.createElement('span');
			dot.className = 'mps-dot' + (d.carrier ? ' on' : '');
			sub.appendChild(dot);
			sub.appendChild(document.createTextNode(String(d.port) + ' · 当前为 ' + now + ' · ' + (d.carrier ? '已接网线' : '未接网线')));
			$('mps-user').value = d.username || '';
			$('mps-pass').placeholder = d.has_pass ? '已保存，留空表示不修改' : '请输入宽带密码';
			var stc = d.proto === 'static';
			$('mps-ip').value = stc ? (d.ipaddr || '') : '';
			$('mps-mask').value = (stc && d.netmask) || MASK_DEF;
			$('mps-gw').value = stc ? (d.gateway || '') : '';
			$('mps-dns').value = d.dns || DNS_DEF;
			each('.mps-card', function (c) {
				c.disabled = false;
				c.querySelector('.pc-cur').hidden = (c.getAttribute('data-v') !== d.mode);
			});
			/* 默认选中现状; 现在就是 WAN 的, 上网方式也带出现在用的那种。
			   从提示框进来的(现在还是 LAN)按检测结果预选 WAN + 推荐方式 */
			sel.mode = d.mode === 'wan' ? 'wan' : 'lan';
			sel.proto = (d.mode === 'wan' && PD[d.proto]) ? d.proto : 'dhcp';
			if (pre.mode === 'wan' && d.mode !== 'wan') {
				sel.mode = 'wan';
				if (PD[pre.proto]) sel.proto = pre.proto;
			}
			sync();
			if (sel.mode === 'wan' && sel.proto === 'pppoe' && !d.username) $('mps-user').focus();
		});
	}

	function close() {
		var m = $('mps-dlg');
		if (!m || busy || !isOpen(m)) return;
		var done = $('mps-win').classList.contains('done');
		seq++;
		ovClose(m);
		$('mps-pass').value = '';   /* 别把输入过的宽带密码留在 DOM 里 */
		var cb = onclose; onclose = null;
		if (cb) cb(done);
		later(3000);                /* 刚切过就早点再看一眼(改回 LAN 后要重新检测) */
	}

	function mode(v) { if (!cur || busy) return; sel.mode = v; err(''); sync(); }
	function proto(v) { if (!cur || busy) return; sel.proto = v; err(''); sync(); }

	/* 按当前选择刷新整个窗口: 哪块显示、联动说明、按钮能不能点 */
	function sync() {
		if (!cur) return;
		var wan = sel.mode === 'wan', fromLan = cur.mode !== 'wan';
		each('.mps-card', function (c) { var on = c.getAttribute('data-v') === sel.mode; c.classList.toggle('on', on); c.setAttribute('aria-pressed', on ? 'true' : 'false'); });
		show('mps-wan', wan);
		each('.mps-seg button', function (b) { var on = b.getAttribute('data-v') === sel.proto; b.classList.toggle('on', on); b.setAttribute('aria-checked', on ? 'true' : 'false'); });
		$('mps-pd').textContent = PD[sel.proto] || '';
		show('mps-pre', wan && fromLan && !!$('mps-pre-t').textContent);
		show('mps-f-pppoe', wan && sel.proto === 'pppoe');
		show('mps-f-static', wan && sel.proto === 'static');

		show('mps-fo', !!cur.fo_ok);
		if (cur.fo_ok) {
			var was = !!cur.failover;
			$('mps-fo-d').textContent = wan
				? '有线上网不通时自动切到 5G 蜂窝网络。选择 WAN 后自动开启。'
				: '网口改回 LAN 后没有有线线路需要守护，随之关闭。';
			var st = $('mps-fo-st');
			st.className = 'fo-st' + (wan ? ' on' : '');
			st.innerHTML = (was === wan) ? (wan ? '保持开启' : '保持关闭')
				: (was ? '已开启' : '已关闭') + ic('arrow', 11) + (wan ? '开启' : '关闭');
		}

		show('mps-via', wan && fromLan && !!cur.via);
		show('mps-warn', wan);
		show('mps-ack-l', wan && fromLan);
		show('mps-lan', !wan && cur.mode === 'wan');

		var same = !wan && cur.mode === 'lan';
		var ok = $('mps-ok');
		ok.disabled = busy || same || (wan && fromLan && !$('mps-ack').checked);
		if (!busy) ok.innerHTML = same ? '当前已是 LAN'
			: ic('ok', 15) + (wan ? (fromLan ? '切换为 WAN' : '保存并应用') : '切换为 LAN');
	}

	function eye() {
		var i = $('mps-pass'), on = i.type === 'password';
		i.type = on ? 'text' : 'password';
		$('mps-eye').innerHTML = ic(on ? 'eyeoff' : 'eye', 15);
	}

	/* 本地先挡一遍, 文案和后端一致; 真正的闸门在后端 */
	function ip4(s) {
		var m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(s), n = 0, i;
		if (!m) return null;
		for (i = 1; i <= 4; i++) { if (+m[i] > 255) return null; n = n * 256 + (+m[i]); }
		return n;
	}
	function maskBits(s) {
		var n = ip4(s), b; if (n === null) return null;
		for (b = 0; b <= 32; b++) if (n === Math.pow(2, 32) - Math.pow(2, 32 - b)) return b;
		return null;
	}
	function check(f) {
		if (f.mode !== 'wan') return null;
		if (f.proto === 'pppoe') {
			if (!f.username) return ['请填写宽带账号。', 'mps-user'];
			if (!f.password && !cur.has_pass) return ['请填写宽带密码。', 'mps-pass'];
		} else if (f.proto === 'static') {
			var n = ip4(f.ipaddr), o1 = n === null ? 0 : Math.floor(n / 16777216);
			if (n === null || o1 === 0 || o1 === 127 || o1 >= 224) return ['请填写正确的 IP 地址。', 'mps-ip'];
			var b = maskBits(f.netmask); if (!b) return ['请填写正确的子网掩码。', 'mps-mask'];
			if (f.gateway && ip4(f.gateway) === null) return ['请填写正确的网关地址。', 'mps-gw'];
			var d = f.dns.split(/[\s,，]+/).filter(Boolean), i;
			for (i = 0; i < d.length; i++) if (ip4(d[i]) === null) return ['DNS 服务器地址不正确：' + d[i], 'mps-dns'];
		}
		return null;
	}

	function save() {
		if (busy || !cur) return;
		var f = {
			mode: sel.mode, proto: sel.proto,
			username: $('mps-user').value.trim(), password: $('mps-pass').value,
			ipaddr: $('mps-ip').value.trim(), netmask: $('mps-mask').value.trim(),
			gateway: $('mps-gw').value.trim(), dns: $('mps-dns').value.trim()
		};
		var bad = check(f);
		if (bad) { err(bad[0], bad[1]); return; }
		err('');
		if (f.mode !== 'wan') f.proto = '';
		if (f.proto !== 'pppoe') { f.username = ''; f.password = ''; }
		if (f.proto !== 'static') { f.ipaddr = f.netmask = f.gateway = f.dns = ''; }
		f.token = token();
		var fromLan = cur.mode !== 'wan';

		busy = true;
		var ok = $('mps-ok'); ok.disabled = true; ok.innerHTML = SPIN + '正在应用…';
		each('.mps-card,.mps-seg button', function (b) { b.disabled = true; });
		req('POST', SET, form(f), function (rv, status) {
			busy = false;
			each('.mps-card,.mps-seg button', function (b) { b.disabled = false; });
			sync();
			if (status === 403) { err('会话已过期，请刷新页面后重试。'); return; }
			if (status === 0) { err('没有收到路由器的回应。如果你是通过这个网口连接的，属正常现象，请改用 WiFi 重新连接后刷新页面。'); return; }
			if (!rv || !rv.ok) { err((rv && rv.error) || ('保存失败（HTTP ' + status + '）')); return; }
			var t, dd;
			if (rv.changed === false) {
				t = '无需修改'; dd = '这个网口当前已是 LAN。';
			} else if (f.mode === 'wan') {
				t = fromLan ? '已切换为 WAN' : '上网设置已保存';
				dd = '网络正在重新加载，约 10 秒后生效，' +
					({ dhcp: '届时自动从光猫或上级路由获取地址。', pppoe: '届时用宽带账号拨号上网。', static: '届时使用填写的静态地址上网。' }[f.proto] || '') +
					(rv.failover ? '网关存活已开启，有线不通时会自动切到 5G 蜂窝网络。' : '') +
					(rv.via ? '你当前的有线连接马上会断开，请改用 WiFi 连接路由器。' : '');
			} else {
				t = '已切换为 LAN';
				dd = '网络正在重新加载，约 10 秒后生效，之后这个网口可以接电脑使用。' + (cur.fo_ok ? '网关存活已关闭。' : '');
			}
			$('mps-dn-t').textContent = t; $('mps-dn-d').textContent = dd;
			$('mps-win').classList.add('done');
			$('mps-pass').value = '';
		}, 20000);
	}

	/* ===================== 接入检测提示 ===================== */
	var WHY = {
		dhcp: '对方会自动分配上网地址（DHCP）',
		pppoe: '收到运营商拨号服务器回应（光猫桥接）',
		both: '会自动分配地址，也能收到运营商拨号回应'
	};
	var PRE = {
		dhcp: '已按检测结果选好「DHCP」：上级设备会自动分配地址，无需填写，确认后即可切换。',
		pppoe: '已按检测结果选好「宽带拨号」：光猫处于桥接模式，请填写运营商提供的宽带账号和密码。',
		both: '已按检测结果选好「DHCP」。如果想由本机用宽带账号拨号，也可以改选「宽带拨号」。'
	};
	function recProto(d) { return d.kind === 'pppoe' ? 'pppoe' : 'dhcp'; }

	function showAsk(d) {
		build();
		ask = d;
		var k = WHY[d.kind] ? d.kind : 'dhcp';
		$('mps-k-why').textContent = WHY[k];
		$('mps-k-gwv').textContent = d.gw || '';
		show('mps-k-gw', !!d.gw);
		$('mps-k-acv').textContent = d.ac || '';
		show('mps-k-ac', !!d.ac);
		$('mps-k-rec').textContent = recProto(d) === 'pppoe' ? '宽带拨号（需要账号密码）' : 'DHCP（自动获取）';
		$('mps-cf-ip').textContent = d.lan_ip || '';
		show('mps-cf', !!d.conflict);
		var ign = $('mps-ign'); ign.disabled = false; ign.textContent = '忽略';
		$('mps-go').disabled = false;
		var m = $('mps-pdm'), was = isOpen(m);
		ovOpen(m);
		if (!was) try { $('mps-go').focus({ preventScroll: true }); } catch (e) { /* 老浏览器不认参数 */ }
	}
	function hideAsk() { if (host) ovClose($('mps-pdm')); }

	function ignore() {
		if (ignBusy) return;
		ignBusy = true;
		var b = $('mps-ign'); b.disabled = true; b.innerHTML = SPIN + '忽略';
		$('mps-go').disabled = true;
		req('POST', IGN, form({ token: token() }), function (rv) {
			ignBusy = false;
			hideAsk(); ask = null;
			/* 后端没记上(会话过期之类): 至少这一页不再弹 */
			if (!rv || !rv.ok) stop();
		});
	}

	function goWan() {
		var d = ask;
		if (!d) return;
		hideAsk();
		var k = WHY[d.kind] ? d.kind : 'dhcp';
		open({
			mode: 'wan', proto: recProto(d), note: PRE[k],
			onclose: function (done) {
				/* 浮窗里点了取消: 回到提示框, 两条路还得选一条; 真切换了就收工 */
				if (done) { ask = null; return; }
				if (ask === d) showAsk(d);
			}
		});
	}

	/* 页面上别的弹窗(设置向导、免责说明、LuCI 自己的模态框、首页的 WiFi 浮窗)开着就先不打扰 */
	function others() {
		var b = document.body;
		return !!(b && (b.classList.contains('modal-overlay-active') ||
			document.querySelector('.qsw-root,#mwrt-dc-mask,.wfm.open')));
	}

	function stop() { stopped = true; clearTimeout(tm); tm = null; }
	function later(ms) {
		if (stopped) return;
		clearTimeout(tm);
		tm = setTimeout(poll, ms);
	}

	/* 轮询 port_probe。后端只在该测的时候拉起探测脚本, 这边按状态调快慢:
	   探测中 3 秒; 没插线 10 秒(等用户插上); 网口不是 LAN 60 秒; 其余 15 秒。
	   切到后台、切换浮窗开着、别的弹窗开着都跳过这一轮。 */
	function poll() {
		tm = null;
		if (stopped) return;
		var dlgOpen = host && isOpen($('mps-dlg'));
		var asking = host && isOpen($('mps-pdm'));
		if (document.hidden || dlgOpen || (!asking && others())) { later(5000); return; }
		req('GET', PROBE, null, function (d, st) {
			if (st === 403 || (d && d.ok === false)) { stop(); hideAsk(); return; }   /* 没登录 / 不是 MAX */
			if (!d) { later(15000); return; }
			var hit = d.mode === 'lan' && d.state === 'router' && !d.ignored;
			var dlg = host && isOpen($('mps-dlg'));
			if (hit && !dlg && !ignBusy) showAsk(d);
			else if (!hit && host && isOpen($('mps-pdm'))) { hideAsk(); ask = null; }   /* 拔线了 / 别处点了忽略 */
			later(hit ? 5000 : d.state === 'probing' ? 3000 : d.mode !== 'lan' ? 60000 : !d.carrier ? 10000 : 15000);
		}, 10000);
	}

	document.addEventListener('keydown', function (e) {
		if (e.key !== 'Escape' || !host) return;
		if (isOpen($('mps-dlg'))) close();
	});
	document.addEventListener('visibilitychange', function () {
		if (!document.hidden) later(800);   /* 切回来马上看一眼, 不干等这一轮 */
	});

	window.mwrtPortSw = { open: open, close: close };

	function boot() { later(1500); }
	if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
	else boot();
})();
