/* mwrt-topo-art.js —— 网络拓扑美术资产库（window.MwrtTopoArt）
 *
 * 一句话：给拓扑页提供全套矢量插画与链路动画，全部内联 SVG 字符串，无依赖、无外部图片（产品图除外）。
 *
 * 用法（B1）：
 *   1. 页面里注入一次 MwrtTopoArt.css（<style>），场景 <svg> 里放一次 MwrtTopoArt.defs()。
 *   2. 场景根元素加类 "ta-scope"（浅色皮肤再加 "ta-light"，或祖先带 ".P.L"）。
 *   3. 工厂返回 {svg,w,h,anchors}（对象带 toString，直接拼进字符串也行），用
 *      <g transform="translate(x,y) scale(k)">…</g> 放置。anchors 是工厂自身坐标系里的连接点。
 *   4. 页面隐藏时给场景根加类 "ta-paused"，动画整体暂停。
 *
 * 皮肤变量（定义在 .ta-scope / .ta-scope.ta-light 上，工厂内部全部带深色默认值兜底）：
 *   --ta-ink      主文字/图标描边        --ta-dim     次要文字
 *   --ta-line     链路底线/断开线        --ta-cable   网线缆身      --ta-cable-edge 网线外缘
 *   --ta-accent   主色(蓝)               --ta-accent2 副色(青)
 *   --ta-ok / --ta-warn / --ta-bad  良好/一般/较差 三态色
 *   --ta-glow     辉光色                 --ta-plat    地台光色     --ta-shadow 落地影色
 *   --ta-rim      深色物体的外轮廓光     --ta-tile1/--ta-tile2/--ta-tile-edge 客户端图块玻璃
 *   --ta-gi       客户端图标线条色       --ta-cloud-a/--ta-cloud-b/--ta-cloud-edge 云
 *   --ta-pill-bg / --ta-pill-ink  徽章底/字   --ta-font 字体栈
 *
 * 光影统一：主光源在左上，高光在上沿，阴影在右下；描边 1.5px 圆角；渐变填充 + 高光罩 + 微投影。
 * 性能：不使用 filter/blur；动画只用 transform / opacity / stroke-dashoffset。
 */
(function (root) {
	'use strict';

	var VERSION = 1;
	var U = 0;

	/* ------------------------------------------------------------------ 基础工具 */
	function n2(v) { return Math.round(v * 100) / 100; }
	function uid(p) { U++; return (p || 'ta') + U + 'x'; }
	function esc(s) {
		return String(s == null ? '' : s).replace(/&/g, '&amp;').replace(/</g, '&lt;')
			.replace(/>/g, '&gt;').replace(/"/g, '&quot;');
	}
	function V(k, fb) { return 'var(--ta-' + k + ',' + fb + ')'; }

	/* 元素构造：数字自动取两位小数；cls→class，st→style；值为 null/undefined/false 的属性忽略 */
	function el(tag, a, c) {
		var s = '<' + tag, k, v;
		for (k in a) {
			if (!Object.prototype.hasOwnProperty.call(a, k)) continue;
			v = a[k];
			if (v === null || v === undefined || v === false) continue;
			if (typeof v === 'number') v = n2(v); else v = esc(v);   /* r2: 属性值一律转义, 防止字符串里的引号截断属性 */
			s += ' ' + (k === 'cls' ? 'class' : (k === 'st' ? 'style' : k)) + '="' + v + '"';
		}
		return (c === undefined || c === null) ? s + '/>' : s + '>' + c + '</' + tag + '>';
	}
	function mix(a, b) { var o = {}, k; for (k in a) o[k] = a[k]; if (b) for (k in b) o[k] = b[k]; return o; }
	function rect(x, y, w, h, r, fill, ex) { return el('rect', mix({ x: x, y: y, width: w, height: h, rx: r || null, fill: fill || 'none' }, ex)); }
	function path(d, fill, ex) { return el('path', mix({ d: d, fill: fill || 'none' }, ex)); }
	function circ(x, y, r, fill, ex) { return el('circle', mix({ cx: x, cy: y, r: r, fill: fill || 'none' }, ex)); }
	function ell(x, y, rx, ry, fill, ex) { return el('ellipse', mix({ cx: x, cy: y, rx: rx, ry: ry, fill: fill || 'none' }, ex)); }
	function line(x1, y1, x2, y2, stroke, w, ex) { return el('line', mix({ x1: x1, y1: y1, x2: x2, y2: y2, stroke: stroke, 'stroke-width': w || 1.5, 'stroke-linecap': 'round' }, ex)); }
	function g(attrs, c) { return el('g', attrs || {}, c); }
	function tx(x, y, s, fs, fill, ex) {
		return el('text', mix({ x: x, y: y, 'font-size': fs, fill: fill || V('ink', '#e8f1ff'), cls: 'ta-t' }, ex), esc(s));
	}
	function textW(s, fs) {
		var w = 0, i, c;
		s = String(s == null ? '' : s);
		for (i = 0; i < s.length; i++) {
			c = s.charCodeAt(i);
			w += c > 255 ? fs : (c >= 65 && c <= 90 ? fs * 0.66 : (c === 32 ? fs * 0.3 : fs * 0.56));
		}
		return w;
	}
	function pack(svg, w, h, anchors, extra) {
		var o = { svg: svg, w: w, h: h, anchors: anchors || {} }, k;
		if (extra) for (k in extra) o[k] = extra[k];
		o.toString = function () { return this.svg; };
		return o;
	}
	function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }

	/* 描边：深蓝黑半透明，所有实体统一用它 */
	var OUT = 'rgba(12,26,46,.66)';
	var OUTL = 'rgba(12,26,46,.38)';
	function olStroke(w) { return { stroke: OUT, 'stroke-width': w || 1.5, 'stroke-linejoin': 'round', 'stroke-linecap': 'round' }; }
	/* 深色物体外轮廓光（浅色皮肤下透明）：先画这个再画物体 */
	function rimPath(d) { return path(d, 'none', { st: 'stroke:' + V('rim', 'rgba(150,205,255,.32)') + ';stroke-width:3.6;stroke-linejoin:round' }); }
	/* 带轮廓光 + 描边 + 高光罩的实体 */
	function solid(d, fill, o) {
		o = o || {};
		var s = '';
		if (o.rim !== false) s += rimPath(d);
		s += path(d, fill, mix(olStroke(o.w), o.attr));
		if (o.gloss !== false) s += path(d, 'url(#ta-gGloss)', { 'pointer-events': 'none', opacity: o.glossOp == null ? 1 : o.glossOp });
		return s;
	}
	/* 落地软影（用渐变椭圆，不用滤镜） */
	function gshadow(cx, cy, rx, ry, op) {
		return ell(cx, cy, rx, ry, 'url(#ta-gGround)', { opacity: op == null ? 1 : op, 'pointer-events': 'none' });
	}
	/* 交互包装：opts.hit 为真时外套 .ta-hit 组，带高亮环与透明命中区 */
	function wrapHit(name, w, h, inner, o) {
		if (!o || !o.hit) return inner;
		var pad = 4;
		return g({ cls: 'ta-hit' + (o.sel ? ' ta-sel' : ''), tabindex: 0, 'data-ta': name, role: 'button', 'aria-label': o.aria || null },
			rect(-pad, -pad, w + pad * 2, h + pad * 2, 16, 'none', { cls: 'ta-hl' }) + inner +
			rect(0, 0, w, h, 8, 'transparent', { 'pointer-events': 'all' }));
	}

	/* ------------------------------------------------------------------ 载波色 */
	var CARRIERS = {
		cmcc: { name: '中国移动', a: '#2fe0b6', b: '#0c8de0', dk: '#0a6c9c' },
		cu: { name: '中国联通', a: '#ff7a7a', b: '#d9202a', dk: '#9a1018' },
		ct: { name: '中国电信', a: '#6fb0ff', b: '#1e5fe0', dk: '#123f9a' },
		cbn: { name: '中国广电', a: '#ffc15e', b: '#ee7410', dk: '#a64e08' },
		other: { name: '运营商', a: '#aab9cc', b: '#6a7e96', dk: '#46566b' }
	};
	function carrierKey(x) {
		var s = String(x == null ? '' : x).toLowerCase();
		if (CARRIERS[s]) return s;
		if (/46000|46002|46004|46007|46008|46013|移动|cmcc|mobile/.test(s)) return 'cmcc';
		if (/46001|46006|46009|46010|联通|unicom|cucc/.test(s)) return 'cu';
		if (/46003|46005|46011|电信|telecom|ctcc/.test(s)) return 'ct';
		if (/46015|46016|广电|broadnet|cbn/.test(s)) return 'cbn';
		return 'other';
	}

	/* ------------------------------------------------------------------ 共享 defs */
	function stops(arr) {
		var s = '', i, a;
		for (i = 0; i < arr.length; i++) {
			a = arr[i];
			s += '<stop offset="' + a[0] + '" style="stop-color:' + a[1] + ';stop-opacity:' + (a[2] == null ? 1 : a[2]) + '"/>';
		}
		return s;
	}
	function lg(id, x1, y1, x2, y2, arr, units) {
		return '<linearGradient id="' + id + '" x1="' + x1 + '" y1="' + y1 + '" x2="' + x2 + '" y2="' + y2 + '"' +
			(units ? ' gradientUnits="' + units + '"' : '') + '>' + stops(arr) + '</linearGradient>';
	}
	function rg(id, cx, cy, r, arr, fx, fy) {
		return '<radialGradient id="' + id + '" cx="' + cx + '" cy="' + cy + '" r="' + r + '"' +
			(fx != null ? ' fx="' + fx + '" fy="' + fy + '"' : '') + '>' + stops(arr) + '</radialGradient>';
	}
	var DEFS_CACHE = null;
	function defs() {
		if (DEFS_CACHE) return DEFS_CACHE;
		var s = '<defs>', k, c;
		s += lg('ta-gSteel', 0, 0, 0, 1, [[0, '#fbfdff'], [0.45, '#d5deea'], [1, '#98a9be']]);
		s += lg('ta-gSteelH', 0, 0, 1, 0, [[0, '#a3b2c5'], [0.3, '#f4f8fc'], [0.58, '#cdd8e5'], [1, '#8394aa']]);
		s += lg('ta-gShield', 0, 0, 1, 1, [[0, '#f6f9fc'], [0.32, '#bac7d8'], [0.5, '#eef2f7'], [0.76, '#a1b0c4'], [1, '#d6dfeb']]);
		s += lg('ta-gDark', 0, 0, 0, 1, [[0, '#4d5a70'], [1, '#1a2231']]);
		s += lg('ta-gDarkH', 0, 0, 1, 0, [[0, '#262f40'], [0.45, '#4a5870'], [1, '#1f2837']]);
		s += lg('ta-gPcb', 0, 0, 0, 1, [[0, '#1f9079'], [1, '#0b463b']]);
		s += lg('ta-gPcbEdge', 0, 0, 0, 1, [[0, '#0b3d33'], [1, '#04201b']]);
		s += lg('ta-gGold', 0, 0, 0, 1, [[0, '#fff4bd'], [0.5, '#f2c34c'], [1, '#b17c20']]);
		s += lg('ta-gGoldH', 0, 0, 1, 0, [[0, '#b17c20'], [0.5, '#fff0a8'], [1, '#c8912a']]);
		s += lg('ta-gGloss', 0, 0, 0, 1, [[0, '#fff', 0.55], [0.42, '#fff', 0.08], [0.56, '#fff', 0], [1, '#000', 0.2]]);
		s += lg('ta-gWhite', 0, 0, 0, 1, [[0, '#ffffff'], [0.6, '#e8eef6'], [1, '#c3cedc']]);
		s += lg('ta-gWhiteH', 0, 0, 1, 0, [[0, '#ffffff'], [0.7, '#e6ecf4'], [1, '#bcc8d8']]);
		s += lg('ta-gSide', 0, 0, 1, 0, [[0, '#cdd7e4'], [1, '#97a7bc']]);
		s += lg('ta-gBlue', 0, 0, 0, 1, [[0, '#7cc0ff'], [1, '#1d62e8']]);
		s += lg('ta-gAcc', 0, 0, 1, 1, [[0, '#3fa8ff'], [1, '#1fe0cf']]);
		s += lg('ta-gGreen', 0, 0, 0, 1, [[0, '#8bf3cb'], [1, '#12a974']]);
		s += lg('ta-gAmber', 0, 0, 0, 1, [[0, '#ffe08a'], [1, '#ee9a10']]);
		s += lg('ta-gRed', 0, 0, 0, 1, [[0, '#ff9aa8'], [1, '#d82a47']]);
		s += lg('ta-gFiber', 0, 0, 1, 0, [[0, '#4fe38b'], [1, '#12a35a']]);
		s += rg('ta-gGround', 0.5, 0.5, 0.5, [[0, V('shadow', '#000'), 0.5], [0.6, V('shadow', '#000'), 0.18], [1, V('shadow', '#000'), 0]]);
		s += rg('ta-gPlat', 0.5, 0.5, 0.5, [[0, V('plat', '#4fb3ff'), 0.55], [0.55, V('plat', '#4fb3ff'), 0.2], [1, V('plat', '#4fb3ff'), 0]]);
		s += rg('ta-gPlatHot', 0.5, 0.5, 0.5, [[0, '#fff', 0.55], [1, V('plat', '#4fb3ff'), 0]]);
		s += lg('ta-gScreen', 0, 0, 1, 1, [[0, '#1b2f6b'], [0.5, '#5b3bb5'], [1, '#12b6c6']]);
		s += lg('ta-gScreenW', 0, 0, 1, 1, [[0, '#ffc474'], [0.5, '#ff6f91'], [1, '#6b5ce7']]);
		s += lg('ta-gScreenG', 0, 0, 1, 1, [[0, '#3b4658'], [1, '#1d2431']]);
		s += lg('ta-gTile', 0, 0, 0, 1, [[0, V('tile1', 'rgba(255,255,255,.14)')], [1, V('tile2', 'rgba(255,255,255,.04)')]]);
		s += lg('ta-gCloud', 0, 0, 0, 1, [[0, V('cloud-a', '#a9d3ff')], [1, V('cloud-b', '#3d72dc')]]);
		s += lg('ta-gCloudOff', 0, 0, 0, 1, [[0, '#c9d2de'], [1, '#7c8a9c']]);
		s += rg('ta-gGlobe', 0.5, 0.5, 0.62, [[0, '#a8e6ff'], [0.55, '#3b8fee'], [1, '#0b3a98']], 0.34, 0.28);
		s += lg('ta-gTower', 0, 0, 1, 0, [[0, '#8091a8'], [0.45, '#eef2f8'], [1, '#6f8098']]);
		s += lg('ta-gConcrete', 0, 0, 0, 1, [[0, '#eef1f6'], [1, '#aeb8c6']]);
		s += lg('ta-gConcreteD', 0, 0, 1, 0, [[0, '#a9b4c3'], [1, '#7f8ca0']]);
		s += lg('ta-gGrass', 0, 0, 0, 1, [[0, V('grass1', '#3b5f55'), 0.85], [1, V('grass2', '#1c3a3a'), 0]]);
		for (k in CARRIERS) {
			c = CARRIERS[k];
			s += lg('ta-gC-' + k, 0, 0, 0, 1, [[0, c.a], [1, c.b]]);
			s += lg('ta-gCH-' + k, 0, 0, 1, 0, [[0, c.b], [0.5, c.a], [1, c.b]]);
		}
		/* 箭头 marker，B1 的自绘线条可用 */
		s += '<marker id="ta-arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M1,1 L9,5 L1,9 Z" style="fill:' + V('accent', '#37a6ff') + '"/></marker>';
		s += '</defs>';
		DEFS_CACHE = s;
		return s;
	}

	/* ------------------------------------------------------------------ CSS */
	var FONT = '-apple-system,BlinkMacSystemFont,"PingFang SC","Microsoft YaHei","Segoe UI",Roboto,"Noto Sans SC","Noto Sans CJK SC","Helvetica Neue",Arial,sans-serif';
	var CSS = [
		'.ta-scope{--ta-ink:#e8f1ff;--ta-dim:#8fa5c2;--ta-line:#3a5173;--ta-cable:#5b7399;--ta-cable-edge:#0d1828;',
		'--ta-accent:#37a6ff;--ta-accent2:#1fe0cf;--ta-ok:#2fe3a0;--ta-warn:#ffb83d;--ta-bad:#ff5a73;--ta-glow:rgba(55,166,255,.5);',
		'--ta-plat:#4fb3ff;--ta-shadow:#000;--ta-rim:rgba(150,205,255,.34);',
		'--ta-tile1:rgba(255,255,255,.15);--ta-tile2:rgba(255,255,255,.04);--ta-tile-edge:rgba(180,215,255,.32);--ta-gi:#dcecff;',
		'--ta-cloud-a:#a9d3ff;--ta-cloud-b:#3d72dc;--ta-cloud-edge:rgba(196,226,255,.9);',
		'--ta-pill-bg:rgba(10,22,40,.72);--ta-pill-ink:#e8f1ff;--ta-grass1:#3b5f55;--ta-grass2:#1c3a3a;',
		'--ta-font:' + FONT + ';}',
		'.ta-scope.ta-light,.ta-light .ta-scope,.P.L .ta-scope,.P.L.ta-scope{--ta-ink:#15283f;--ta-dim:#5d7189;--ta-line:#9fb4cf;--ta-cable:#7b90ae;--ta-cable-edge:#30435d;',
		'--ta-accent:#1f7bff;--ta-accent2:#0fb9b1;--ta-ok:#12b886;--ta-warn:#f59f00;--ta-bad:#e8384f;--ta-glow:rgba(31,123,255,.35);',
		'--ta-plat:#5b9dff;--ta-shadow:#1d3b66;--ta-rim:rgba(255,255,255,0);',
		'--ta-tile1:#ffffff;--ta-tile2:#e9f0fa;--ta-tile-edge:rgba(70,110,170,.30);--ta-gi:#22406b;',
		'--ta-cloud-a:#ffffff;--ta-cloud-b:#a8cbf3;--ta-cloud-edge:#6b9bd8;',
		'--ta-pill-bg:rgba(255,255,255,.92);--ta-pill-ink:#15283f;--ta-grass1:#9fd0b6;--ta-grass2:#d8efe2;}',
		'.ta-scope svg,svg.ta-scope{overflow:visible}',
		'.ta-t{font-family:var(--ta-font);font-weight:600;letter-spacing:.2px;pointer-events:none;-webkit-user-select:none;user-select:none}',
		'.ta-tb{font-weight:700}',
		'.ta-fb{transform-box:fill-box;transform-origin:50% 50%}',
		/* 流动数据包：虚线周期固定 36，偏移动画 */
		'@keyframes ta-flow{to{stroke-dashoffset:-36}}',
		'.ta-fl{animation:ta-flow 1s linear infinite}',
		'.ta-fr{animation:ta-flow 1s linear infinite reverse}',
		'@keyframes ta-blink{0%,100%{opacity:1}50%{opacity:.22}}',
		'@keyframes ta-blink2{0%,100%{opacity:1}12%{opacity:.15}24%{opacity:1}36%{opacity:.25}48%{opacity:1}}',
		'@keyframes ta-breath{0%,100%{opacity:.4}50%{opacity:1}}',
		'@keyframes ta-ring{0%{transform:scale(.35);opacity:.85}100%{transform:scale(1.9);opacity:0}}',
		'@keyframes ta-wave{0%{transform:translate(0,0) scale(.5);opacity:0}14%{opacity:.95}100%{transform:translate(var(--d,100px),0) scale(1.4);opacity:0}}',
		'@keyframes ta-wf{0%,100%{opacity:.18}45%{opacity:1}}',
		'@keyframes ta-hsd{0%{stroke-dashoffset:0;opacity:0}6%{opacity:1}44%{stroke-dashoffset:calc(6px - var(--l,60px));opacity:1}52%,100%{stroke-dashoffset:calc(6px - var(--l,60px));opacity:0}}',
		'@keyframes ta-burst{0%,44%{transform:scale(.3);opacity:0}50%{opacity:.95}100%{transform:scale(2.4);opacity:0}}',
		'@keyframes ta-jit{0%,100%{transform:translate(0,0)}20%{transform:translate(0,-1.4px)}40%{transform:translate(.8px,1.1px)}60%{transform:translate(-.8px,-.7px)}80%{transform:translate(.6px,.9px)}}',
		'@keyframes ta-flick{0%,100%{opacity:.9}10%{opacity:.35}22%{opacity:.8}40%{opacity:.25}55%{opacity:.85}78%{opacity:.4}}',
		'@keyframes ta-beacon{0%,84%,100%{opacity:.12}88%,94%{opacity:1}}',
		'@keyframes ta-orbit{to{stroke-dashoffset:calc(var(--p,200px) * -1)}}',
		'@keyframes ta-float{0%,100%{transform:translateY(0)}50%{transform:translateY(-3px)}}',
		'@keyframes ta-grow{from{transform:scaleX(0)}to{transform:scaleX(1)}}',
		'@keyframes ta-pop{0%{transform:scale(.6);opacity:0}100%{transform:scale(1);opacity:1}}',
		'@keyframes ta-spin{to{transform:rotate(360deg)}}',
		'@keyframes ta-sweep{0%{transform:translateX(-120%)}100%{transform:translateX(220%)}}',
		'@keyframes ta-dash{to{stroke-dashoffset:-24}}',
		'@keyframes ta-shine{0%{transform:translateX(var(--x0,0px))}62%,100%{transform:translateX(var(--x1,200px))}}',
		'.ta-blink{animation:ta-blink 1.6s ease-in-out infinite}',
		'.ta-blink2{animation:ta-blink2 2.4s linear infinite}',
		'.ta-breath{animation:ta-breath 2.8s ease-in-out infinite}',
		'.ta-ring{animation:ta-ring 2.7s ease-out infinite;transform-box:fill-box;transform-origin:50% 50%}',
		'.ta-wave{animation:ta-wave 2.4s linear infinite}',
		'.ta-wf{animation:ta-wf 1.8s ease-in-out infinite}',
		'.ta-hsd{animation:ta-hsd 3.2s ease-in-out infinite}',
		'.ta-burst{animation:ta-burst 3.2s ease-out infinite;transform-box:fill-box;transform-origin:50% 50%}',
		'.ta-jit{animation:ta-jit .5s steps(1) infinite}',
		'.ta-flick{animation:ta-flick 1.7s linear infinite}',
		'.ta-beacon{animation:ta-beacon 2.2s linear infinite}',
		'.ta-orbit{animation:ta-orbit 6s linear infinite}',
		'.ta-float{animation:ta-float 4.5s ease-in-out infinite}',
		'.ta-grow{animation:ta-grow .7s cubic-bezier(.2,.8,.2,1) both;transform-box:fill-box;transform-origin:0 50%}',
		'.ta-pop{animation:ta-pop .45s cubic-bezier(.2,1.3,.4,1) both;transform-box:fill-box;transform-origin:50% 50%}',
		'.ta-spin{animation:ta-spin 14s linear infinite;transform-box:fill-box;transform-origin:50% 50%}',
		'.ta-dashmove{animation:ta-dash 1.2s linear infinite}',
		'.ta-shine{animation:ta-shine 6.5s cubic-bezier(.45,.05,.4,.95) infinite}',
		/* 交互高亮 */
		'.ta-hit{cursor:pointer;outline:none}',
		'.ta-hit .ta-hl{stroke:var(--ta-accent,#37a6ff);stroke-width:1.6;stroke-dasharray:5 4;opacity:0;transition:opacity .22s ease;fill:var(--ta-glow,rgba(55,166,255,.12));fill-opacity:.12}',
		'.ta-hit:hover .ta-hl,.ta-hit:focus .ta-hl,.ta-hit.ta-sel .ta-hl{opacity:1}',
		'.ta-hitline{pointer-events:stroke;cursor:pointer}',
		/* 整体暂停（页面隐藏/覆盖层不可见） */
		'.ta-paused *{animation-play-state:paused !important}'
	].join('\n');

	/* ------------------------------------------------------------------ 质量/分级工具 */
	var RANGES = {
		rsrp: { min: -140, max: -44, z: [[-110, 'bad'], [-100, 'warn'], [-85, 'ok'], [999, 'acc']], unit: 'dBm', name: 'RSRP' },
		rsrq: { min: -20, max: -3, z: [[-15, 'bad'], [-12, 'warn'], [-9, 'ok'], [999, 'acc']], unit: 'dB', name: 'RSRQ' },
		sinr: { min: -5, max: 35, z: [[0, 'bad'], [10, 'warn'], [20, 'ok'], [999, 'acc']], unit: 'dB', name: 'SINR' },
		rssi: { min: -100, max: -30, z: [[-80, 'bad'], [-70, 'warn'], [-60, 'ok'], [999, 'acc']], unit: 'dBm', name: 'RSSI' },
		snr: { min: 0, max: 60, z: [[15, 'bad'], [25, 'warn'], [40, 'ok'], [999, 'acc']], unit: 'dB', name: 'SNR' },
		noise: { min: -110, max: -70, z: [[-200, 'ok'], [-90, 'ok'], [-80, 'warn'], [999, 'bad']], unit: 'dBm', name: '噪声' }
	};
	var TONE = {
		bad: { c: '#ff5a73', v: V('bad', '#ff5a73'), t: '较差', q: 0.18 },
		warn: { c: '#ffb83d', v: V('warn', '#ffb83d'), t: '一般', q: 0.45 },
		ok: { c: '#2fe3a0', v: V('ok', '#2fe3a0'), t: '良好', q: 0.75 },
		acc: { c: '#37a6ff', v: V('accent', '#37a6ff'), t: '极佳', q: 1 }
	};
	/* quality('rsrp', -85) -> {q:0..1, tone:'ok', color:'var(..)', label:'良好', pct:0..1} */
	function quality(kind, val) {
		var r = RANGES[kind], i, tone = 'bad', pct;
		if (!r || val === null || val === undefined || isNaN(val)) return { q: 0, tone: 'dim', color: V('dim', '#8fa5c2'), label: '未知', pct: 0 };
		for (i = 0; i < r.z.length; i++) { if (val < r.z[i][0]) { tone = r.z[i][1]; break; } }
		pct = clamp((val - r.min) / (r.max - r.min), 0, 1);
		return { q: pct, tone: tone, color: TONE[tone].v, label: TONE[tone].t, pct: pct };
	}
	function toneOfQ(q) {
		if (q === null || q === undefined) return null;
		return q >= 0.75 ? 'ok' : (q >= 0.45 ? 'warn' : 'bad');
	}
	function toneColor(t) {
		if (TONE[t]) return TONE[t].v;
		if (t === 'info') return V('accent', '#37a6ff');
		if (t === 'dim' || t === 'off') return V('dim', '#8fa5c2');
		return t || V('accent', '#37a6ff');
	}

	/* ================================================================== 路由器（真实产品图） */
	/* 各机型产品图（797x618，max 为 600x400）的不透明包围盒与接口位置（图像像素坐标）。 */
	var RM = {
		'c5800-688': { w: 1278, h: 1230, b: [105, 58, 1234, 1176], wan: [1234, 950], lan: [105, 950], usb: [1234, 800], ant: [639, 350], led: [210, 910] },
		n5000: { w: 797, h: 618, b: [138, 143, 657, 474], wan: [510, 362], lan: [556, 336], usb: [601, 306], ant: [400, 205], led: [394, 330] },
		ultra: { w: 797, h: 618, b: [131, 11, 652, 588], wan: [470, 462], lan: [516, 462], usb: [425, 518], ant: [470, 105], led: [330, 480] },
		max: { w: 600, h: 400, b: [78, 68, 550, 346], wan: [158, 241], lan: [158, 241], usb: [215, 269], ant: [300, 150], led: [260, 200] },
		h5000m: { w: 797, h: 618, b: [145, 139, 652, 516], wan: [200, 325], lan: [272, 358], usb: [412, 418], ant: [390, 235], led: [330, 370] },
		e87n: { w: 797, h: 618, b: [69, 163, 727, 534], wan: [150, 410], lan: [330, 480], usb: [560, 470], ant: [390, 175], led: [220, 360] }
	};
	var IMGBASE = '/luci-static/router3d/';

	function ledColor(st) {
		return st === 'bad' ? ['#ff4d6a', V('bad', '#ff5a73')] : (st === 'warn' ? ['#ffb83d', V('warn', '#ffb83d')] : (st === 'off' ? ['#7d8aa0', '#7d8aa0'] : ['#2fe3a0', V('ok', '#2fe3a0')]));
	}

	function router(model, o) {
		o = o || {};
		var key = RM[model] ? model : 'n5000', m = RM[key], id = uid('rt');
		var W = 320, H = 262, base = 224;
		var bw = m.b[2] - m.b[0], bh = m.b[3] - m.b[1];
		var sc = Math.min(296 / bw, 216 / bh) * (o.scale || 1);
		var ox = 160 - (m.b[0] + bw / 2) * sc, oy = base - m.b[3] * sc;
		function pt(p) { return { x: n2(ox + p[0] * sc), y: n2(oy + p[1] * sc) }; }
		var rx = Math.min(150, bw * sc * 0.62), ry = rx * 0.2, cy = base - ry * 0.55;
		var iw = m.w * sc, ih = m.h * sc;
		var href = (o.imgBase || (root.MwrtTopoArt && root.MwrtTopoArt.imgBase) || IMGBASE) + key + '.png';
		var rh = Math.min(ih * 0.3, 54);
		var s = '';
		/* 1 地台：宽柔光 + 盘面 + 轨道扫光环 */
		s += ell(160, cy + 2, rx * 1.18, ry * 1.55, 'url(#ta-gPlat)', { cls: 'ta-breath', 'pointer-events': 'none' });
		s += ell(160, cy, rx * 1.02, ry * 1.02, 'url(#ta-gPlat)', { opacity: 0.8 });
		s += ell(160, cy, rx * 1.02, ry * 1.02, 'none', { st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-opacity:.5;stroke-width:1.3' });
		s += ell(160, cy, rx * 0.76, ry * 0.76, 'none', { st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-opacity:.28;stroke-width:1', 'stroke-dasharray': '3 5' });
		var per = Math.round(2 * Math.PI * Math.sqrt((rx * rx + ry * ry) / 2) * 1.02);
		s += ell(160, cy, rx * 1.02, ry * 1.02, 'none', {
			cls: 'ta-orbit', 'stroke-linecap': 'round', 'stroke-dasharray': '46 ' + (per - 46),
			st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:2;--p:' + per + 'px;animation-duration:7s'
		});
		/* 2 倒影（镜像 + 渐隐遮罩） */
		s += '<defs>' +
			lg(id + 'rg', 0, base, 0, base + rh, [[0, '#fff', 0.34], [1, '#fff', 0]], 'userSpaceOnUse') +
			'<mask id="' + id + 'rm" maskUnits="userSpaceOnUse" x="0" y="' + base + '" width="' + W + '" height="' + rh + '">' +
			rect(0, base, W, rh, 0, 'url(#' + id + 'rg)') + '</mask></defs>';
		s += g({ mask: 'url(#' + id + 'rm)', 'pointer-events': 'none' },
			el('image', { href: href, 'xlink:href': href, x: ox, y: oy, width: iw, height: ih, preserveAspectRatio: 'none', transform: 'matrix(1 0 0 -1 0 ' + (2 * base) + ')' }));
		/* 3 接触阴影 */
		s += gshadow(160, cy + ry * 0.3, rx * 0.95, ry * 0.95, 0.9);
		/* 4 矢量兜底（图片加载失败才显示）+ 产品图 */
		var fb = id + 'fb';
		s += g({ id: fb, st: 'display:none' },
			solid('M52,150 L108,120 L268,120 L268,168 L216,196 L52,196 Z', 'url(#ta-gWhite)') +
			rect(120, 168, 22, 14, 3, '#273245') + rect(150, 168, 22, 14, 3, '#273245'));
		s += el('image', {
			href: href, 'xlink:href': href, x: ox, y: oy, width: iw, height: ih, preserveAspectRatio: 'none',
			'data-fb': fb   /* r2: 不再用内联 onerror (CSP 友好); 由 bindFallback() 在插入 DOM 后挂 error 监听 */
		});
		/* 5 WiFi 辐射环（可选） */
		var ant = pt(m.ant);
		if (o.wifi) {
			s += g({ 'pointer-events': 'none' },
				circ(ant.x, ant.y, 22, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:1.6' }) +
				circ(ant.x, ant.y, 22, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:1.6;animation-delay:.9s' }) +
				circ(ant.x, ant.y, 22, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:1.6;animation-delay:1.8s' }));
		}
		/* 6 状态灯：地台正前方的胶囊灯 */
		var lc = ledColor(o.status || 'ok'), ly = cy + ry * 1.05 + 5;
		if (o.status !== 'none') {
			s += g({ 'pointer-events': 'none' },
				ell(160, ly, 26, 6, lc[0], { opacity: 0.2, cls: (o.status === 'warn' || o.status === 'bad') ? 'ta-blink' : 'ta-breath' }) +
				rect(150, ly - 1.6, 20, 3.2, 1.6, lc[0], { cls: (o.status === 'warn' || o.status === 'bad') ? 'ta-blink' : null }) +
				rect(152, ly - 1, 16, 1, 0.5, '#fff', { opacity: 0.55 }));
		}
		var an = {
			wan: pt(m.wan), lan: pt(m.lan), usb: pt(m.usb), ant: ant,
			top: { x: 160, y: n2(oy + m.b[1] * sc) }, bottom: { x: 160, y: n2(cy + ry * 1.1) },
			led: { x: 160, y: n2(ly) }, left: { x: n2(160 - rx), y: n2(cy) }, right: { x: n2(160 + rx), y: n2(cy) }, center: pt([(m.b[0] + m.b[2]) / 2, (m.b[1] + m.b[3]) / 2])
		};
		return pack(wrapHit('router', W, H, s, o), W, H, an, { model: key });
	}

	/* ================================================================== 5G 模组（M.2，3042 外形） */
	/* r3: 与真实产品照（MT5700M-CN）同款设计语言。
	 * 坐标：整图 W×H 内，照片区（500×291 的原图坐标）按 PS 缩放后放在 (PX,PY)；
	 * 矢量版画在同一 500×291 坐标系里（银色拉丝屏蔽罩 + 蓝色 PCB 边 + 左侧金手指缺口 + 右侧四个 U.FL 座 + 半圆固定孔），
	 * 所以照片版、矢量版、图片加载失败的回退三者几何一致，U.FL 座位置相同。
	 * anchors：usb（金手指一侧 USB 线接口）、rf（四根同轴线汇合的射频辐射中心）保持不变，另有 rf1-rf4 四个 U.FL 座。 */
	var MOD_PNG = 'mod-mt5700m.png';
	var MOD_UFL_Y = [16, 93, 195, 272], MOD_UFL_X = 481;     /* 原图坐标 */
	function modBodyVec(model, id) {
		var s = '', i, label = String(model || '').replace(/^\s+|\s+$/g, '') || '5G 模组';
		var fs = clamp(300 / Math.max(6, textW(label, 1)), 25, 38);
		s += '<defs>' +
			lg(id + 'pb', 0, 0, 0, 1, [[0, '#2f74d6'], [1, '#17458f']]) +
			lg(id + 'sh', 0, 0, 1, 1, [[0, '#f3f6fa'], [0.35, '#c4ceda'], [0.55, '#e9edf3'], [0.8, '#aeb9c8'], [1, '#d5dce6']]) +
			'</defs>';
		var board = 'M10,49 H38 V0 H500 V105 A36,36 0 0 0 500,177 V291 H38 V243 H10 Q0,243 0,233 V205 H20 V190 H0 V59 Q0,49 10,49 Z';
		s += path(board, 'url(#' + id + 'pb)', olStroke(2.4));
		s += path('M44,8 H494', 'none', { stroke: '#fff', 'stroke-opacity': 0.25, 'stroke-width': 3 });
		/* 金手指：两组，中间是 key 缺口 */
		for (i = 0; i < 14; i++) s += rect(3, 53 + i * 9.4, 19, 6, 1.4, 'url(#ta-gGoldH)', { stroke: 'rgba(80,50,0,.5)', 'stroke-width': 0.8 });
		for (i = 0; i < 3; i++) s += rect(3, 209 + i * 11, 19, 7, 1.4, 'url(#ta-gGoldH)', { stroke: 'rgba(80,50,0,.5)', 'stroke-width': 0.8 });
		/* 半圆固定孔金环 */
		s += path('M500,105 A36,36 0 0 0 500,177', 'none', { stroke: 'url(#ta-gGold)', 'stroke-width': 7, 'stroke-linecap': 'butt' });
		s += circ(481, 58, 6, 'url(#ta-gGold)', olStroke(1)) + circ(481, 232, 6, 'url(#ta-gGold)', olStroke(1));
		/* 屏蔽罩：银色拉丝 */
		s += rect(38, 0, 431, 291, 3, 'url(#' + id + 'sh)', olStroke(2.2));
		for (i = 0; i < 40; i++) s += line(44, 4 + i * 7.2, 465, 4 + i * 7.2, '#fff', 1.4, { opacity: i % 3 === 0 ? 0.34 : 0.16 });
		for (i = 0; i < 14; i++) s += line(44, 9 + i * 20.5, 465, 9 + i * 20.5, '#6a7788', 1.2, { opacity: 0.1 });
		s += rect(38, 0, 431, 291, 3, 'url(#ta-gGloss)', { 'pointer-events': 'none', opacity: 0.7 });
		/* 5G 标识 + 型号（中性文字，不出现厂牌） */
		s += circ(86, 54, 25, 'none', { stroke: 'rgba(55,66,84,.78)', 'stroke-width': 4.5 });
		s += tx(86, 64.5, '5G', 27, 'rgba(55,66,84,.88)', { 'text-anchor': 'middle', 'font-weight': 800 });
		s += tx(70, 128, '5G Module', 31, 'rgba(60,70,88,.9)', { 'font-weight': 700 });
		s += tx(70, 178, label, fs, 'rgba(40,50,68,.95)', { 'font-weight': 800, textLength: textW(label, fs) > 330 ? 330 : null, lengthAdjust: textW(label, fs) > 330 ? 'spacingAndGlyphs' : null });
		s += rect(70, 206, 270, 10, 5, 'rgba(70,82,100,.34)') + rect(70, 232, 216, 10, 5, 'rgba(70,82,100,.34)') + rect(70, 258, 150, 10, 5, 'rgba(70,82,100,.26)');
		/* 四个 U.FL 射频座 */
		for (i = 0; i < 4; i++) {
			var uy = MOD_UFL_Y[i];
			s += circ(MOD_UFL_X, uy, 14, 'url(#ta-gSteel)', olStroke(2));
			s += circ(MOD_UFL_X, uy, 8.5, '#141b27') + circ(MOD_UFL_X, uy, 3.6, 'url(#ta-gGold)');
			s += path('M' + (MOD_UFL_X - 12) + ',' + (uy - 5) + ' A13,13 0 0 1 ' + (MOD_UFL_X - 5) + ',' + (uy - 12), 'none', { stroke: '#fff', 'stroke-width': 2, 'stroke-opacity': 0.8, 'stroke-linecap': 'round' });
		}
		return s;
	}

	function module5g(o) {
		o = o || {};
		var W = 300, H = 166, id = uid('md'), s = '', i;
		var model = String(o.model == null ? '' : o.model);
		var active = o.active !== false;
		var photo = /MT5700/i.test(model) && o.photo !== false;
		var PX = 48, PY = 12, PS = 0.4, PW = 500 * PS, PH = 291 * PS;          /* 照片区 200×116.4 */
		var HUB = { x: 284, y: PY + PH / 2 };                                  /* 射频辐射中心 */
		function F(px, py) { return { x: n2(PX + px * PS), y: n2(PY + py * PS) }; }
		var imgBase = (root.MwrtTopoArt && root.MwrtTopoArt.imgBase) || IMGBASE;
		var href = imgBase + MOD_PNG + '?v=1';
		var sx0 = PX + 38 * PS, sx1 = PX + 469 * PS;                           /* 屏蔽罩范围（高光扫过范围） */
		var uflP = [], coax = ['#202838', '#4e5b72', '#e6ebf3', '#1f8fff'];

		/* 1 地台：宽柔光 + 盘面 + 扫光环（与产品图同一套地台语言） */
		var cx = PX + PW / 2, cyy = PY + PH + 11, rx = PW * 0.62, ry = 10;
		s += ell(cx, cyy + 1, rx * 1.2, ry * 1.5, 'url(#ta-gPlat)', { cls: 'ta-breath', 'pointer-events': 'none' });
		s += ell(cx, cyy, rx, ry, 'url(#ta-gPlat)', { opacity: 0.8 });
		s += ell(cx, cyy, rx, ry, 'none', { st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-opacity:.5;stroke-width:1.2' });
		s += ell(cx, cyy, rx * 0.74, ry * 0.74, 'none', { st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-opacity:.26;stroke-width:1', 'stroke-dasharray': '3 5' });
		var per = Math.round(2 * Math.PI * Math.sqrt((rx * rx + ry * ry) / 2));
		s += ell(cx, cyy, rx, ry, 'none', { cls: 'ta-orbit', 'stroke-linecap': 'round', 'stroke-dasharray': '40 ' + (per - 40), st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:1.8;--p:' + per + 'px;animation-duration:7s' });
		s += gshadow(cx, cyy - 2, rx * 0.9, ry * 0.8, 0.85);

		/* 2 倒影（照片版镜像照片；矢量版镜像矢量），渐隐遮罩 */
		var rh = 20, mid = id + 'rm', gid = id + 'rg', base = PY + PH + 1;
		s += '<defs>' + lg(gid, 0, base, 0, base + rh, [[0, '#fff', 0.3], [1, '#fff', 0]], 'userSpaceOnUse') +
			'<mask id="' + mid + '" maskUnits="userSpaceOnUse" x="0" y="' + base + '" width="' + W + '" height="' + rh + '">' + rect(0, base, W, rh, 0, 'url(#' + gid + ')') + '</mask></defs>';
		var refl = photo
			? el('image', { href: href, 'xlink:href': href, x: PX, y: PY, width: PW, height: PH, preserveAspectRatio: 'none', transform: 'matrix(1 0 0 -1 0 ' + (2 * base - 2) + ')' })
			: g({ transform: 'matrix(1 0 0 -1 0 ' + (2 * base - 2) + ') translate(' + PX + ',' + PY + ') scale(' + PS + ')' }, modBodyVec(model, id + 'r'));
		s += g({ mask: 'url(#' + mid + ')', 'pointer-events': 'none' }, refl);

		/* 3 模组本体：照片 +（图片加载失败时显示的）矢量回退；非 MT5700 型号直接用矢量版 */
		var vec = g({ transform: 'translate(' + PX + ',' + PY + ') scale(' + PS + ')' }, modBodyVec(model, id + 'v'));
		if (photo) {
			var fb = id + 'fb';
			s += g({ id: fb, st: 'display:none' }, vec);
			s += el('image', { href: href, 'xlink:href': href, x: PX, y: PY, width: PW, height: PH, preserveAspectRatio: 'none', 'data-fb': fb });
		} else {
			s += vec;
		}

		/* 4 工作中：屏蔽罩上缓慢扫过的高光（只在屏蔽罩范围内） */
		if (active) {
			var cid = id + 'cp';
			s += '<defs><clipPath id="' + cid + '"><rect x="' + n2(sx0 + 1) + '" y="' + PY + '" width="' + n2(sx1 - sx0 - 2) + '" height="' + n2(PH) + '"/></clipPath>' +
				lg(id + 'sw', 0, 0, 1, 0, [[0, '#fff', 0], [0.5, '#fff', 0.7], [1, '#fff', 0]]) + '</defs>';
			s += g({ 'clip-path': 'url(#' + cid + ')', 'pointer-events': 'none' },
				g({ cls: 'ta-shine', st: '--x0:' + n2(sx0 - 40) + 'px;--x1:' + n2(sx1 + 12) + 'px' },
					path('M0,' + n2(PY) + ' H30 L8,' + n2(PY + PH) + ' H-22 Z', 'url(#' + id + 'sw)', { opacity: 0.62 })));
			s += rect(n2(sx0), PY, n2(sx1 - sx0), n2(PH), 1, 'none', { cls: 'ta-breath', 'pointer-events': 'none', st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:1.2;animation-duration:3.6s' });
		}

		/* 5 金手指一侧：M.2 插座 + 转 USB 的小接口板（USB 线接这里），anchors.usb 在最左端 */
		var hx = 18, hy0 = PY + 17 * PS, hy1 = PY + 246 * PS;
		var hs = 'M' + hx + ',' + n2(hy0) + ' H' + n2(PX + 14 * PS) + ' Q' + n2(PX + 22 * PS) + ',' + n2(hy0) + ' ' + n2(PX + 22 * PS) + ',' + n2(hy0 + 6) + ' V' + n2(hy1 - 6) + ' Q' + n2(PX + 22 * PS) + ',' + n2(hy1) + ' ' + n2(PX + 14 * PS) + ',' + n2(hy1) + ' H' + hx + ' Q' + (hx - 5) + ',' + n2(hy1) + ' ' + (hx - 5) + ',' + n2(hy1 - 5) + ' V' + n2(hy0 + 5) + ' Q' + (hx - 5) + ',' + n2(hy0) + ' ' + hx + ',' + n2(hy0) + ' Z';
		s += g({ transform: 'translate(1.6,2.4)' }, path(hs, 'rgba(0,0,0,.28)'));
		s += rimPath(hs);
		s += path(hs, 'url(#ta-gDark)', olStroke(1.5));
		s += path(hs, 'url(#ta-gGloss)', { opacity: 0.8, 'pointer-events': 'none' });
		var hm = (hy0 + hy1) / 2;
		s += rect(hx - 3.5, hm - 7, 6, 14, 2.4, '#0e131d', olStroke(1)) + rect(hx - 2, hm - 4, 3.2, 8, 1.4, 'url(#ta-gGold)');
		s += circ(hx + 8, n2(hy0 + 9), 2.4, 'url(#ta-gSteel)', olStroke(0.8)) + circ(hx + 8, n2(hy1 - 9), 2.4, 'url(#ta-gSteel)', olStroke(0.8));
		s += circ(hx + 8, n2(hm - 17), 2.3, active ? '#2fe3a0' : '#516070', { cls: active ? 'ta-blink2' : null });
		s += circ(hx + 8, n2(hm - 17), 4.6, active ? 'rgba(47,227,160,.3)' : 'none', { cls: active ? 'ta-breath' : null });

		/* 6 四个 U.FL 座 → 细同轴线 → 射频辐射中心 */
		var ends = [];
		for (i = 0; i < 4; i++) {
			var p0 = F(MOD_UFL_X, MOD_UFL_Y[i]);
			uflP.push(p0);
			var dd = 'M' + p0.x + ',' + p0.y + ' C' + n2(p0.x + 26) + ',' + p0.y + ' ' + n2(HUB.x - 24) + ',' + n2(HUB.y + (i - 1.5) * 4.5) + ' ' + HUB.x + ',' + n2(HUB.y + (i - 1.5) * 3.2);
			ends.push(dd);
			s += path(dd, 'none', { stroke: OUT, 'stroke-width': 3.1, 'stroke-linecap': 'round' });
			s += path(dd, 'none', { stroke: coax[i], 'stroke-width': 1.7, 'stroke-linecap': 'round' });
			s += path(dd, 'none', { stroke: '#fff', 'stroke-width': 0.5, 'stroke-opacity': 0.4, transform: 'translate(-.4,-.6)', 'stroke-linecap': 'round' });
		}
		if (active) {
			for (i = 0; i < 4; i++) {
				s += path(ends[i], 'none', { cls: 'ta-fl', 'stroke-linecap': 'round', 'stroke-dasharray': '2.2 33.8', st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:1.9;animation-duration:1.5s;animation-delay:-' + n2(i * 0.37) + 's', opacity: 0.95 });
			}
		}
		/* 射频辐射中心：小天线振子 + 脉冲环 */
		if (active) {
			s += circ(HUB.x, HUB.y, 11, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:1.5' });
			s += circ(HUB.x, HUB.y, 11, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:1.4;animation-delay:1.3s' });
		}
		s += rect(HUB.x - 3.4, HUB.y - 11, 6.8, 22, 3.2, 'url(#ta-gDark)', olStroke(1.2));
		s += rect(HUB.x - 1.6, HUB.y - 9, 3.2, 18, 1.6, 'url(#ta-gSteelH)', { opacity: 0.9 });
		s += circ(HUB.x, HUB.y - 11, 2.6, 'url(#ta-gGold)', olStroke(0.8));
		s += circ(HUB.x, HUB.y, 2.2, active ? V('accent2', '#1fe0cf') : '#7d8aa0', { cls: active ? 'ta-breath' : null });

		var an = {
			usb: { x: hx - 5, y: n2(hm) }, rf: { x: n2(HUB.x + 4), y: n2(HUB.y) },
			rf1: uflP[0], rf2: uflP[1], rf3: uflP[2], rf4: uflP[3],
			top: { x: n2(cx), y: PY }, center: { x: n2(cx), y: n2(PY + PH / 2) }, bottom: { x: n2(cx), y: n2(cyy + ry) }, chip: { x: n2(cx), y: n2(PY + PH / 2) }
		};
		return pack(wrapHit('module', W, H, s, o), W, H, an, { photo: photo });
	}

	/* ================================================================== 通用小件：扇形弧（WiFi/扇区） */
	/* 以 (cx,cy) 为圆心，朝 angle 度方向发出的若干弧；radii 半径数组；span 半张角(度) */
	function arcFan(cx, cy, angle, radii, span, color, o) {
		o = o || {};
		var s = '', i, r, a = span * Math.PI / 180, x, y;
		for (i = 0; i < radii.length; i++) {
			r = radii[i]; x = r * Math.cos(a); y = r * Math.sin(a);
			s += path('M' + n2(x) + ',' + n2(-y) + ' A' + r + ',' + r + ' 0 0 1 ' + n2(x) + ',' + n2(y), 'none', {
				cls: o.still ? null : 'ta-wf', 'stroke-linecap': 'round',
				st: 'stroke:' + color + ';stroke-width:' + (o.w || 1.8) + (o.still ? ';opacity:' + (o.op == null ? 0.9 : o.op) : ';animation-delay:' + n2(i * (o.step || 0.22)) + 's;animation-duration:' + (o.dur || 1.8) + 's')
			});
		}
		return g({ transform: 'translate(' + n2(cx) + ',' + n2(cy) + ') rotate(' + angle + ')' }, s);
	}
	function pill(cx, y, text, tone, fs) {
		var b = badge(text, tone, { fs: fs || 11 });
		return g({ transform: 'translate(' + n2(cx - b.w / 2) + ',' + n2(y) + ')' }, b.svg);
	}

	/* ================================================================== 基站塔 */
	function tower(o) {
		o = o || {};
		var W = 224, H = 330, k = carrierKey(o.carrier), c = CARRIERS[k], s = '', i;
		var cx = 96, yb = 290, yt = 76, active = o.active !== false;
		function hw(y) { return 5 + (y - yt) / (yb - yt) * 29; }
		function xl(y) { return cx - hw(y); }
		function xr(y) { return cx + hw(y); }
		/* 地面 */
		s += gshadow(108, 300, 112, 13, 0.9);
		s += ell(106, 298, 108, 12, 'url(#ta-gGrass)', { opacity: 0.9 });
		/* 混凝土基座 */
		s += ell(cx, 293, 48, 8, 'url(#ta-gConcrete)', olStroke(1.3));
		s += ell(cx, 291, 40, 5.5, 'rgba(255,255,255,.4)');
		/* 格构：交叉斜撑 + 横杆 */
		var ys = [290, 264, 240, 217, 196, 177, 160, 145, 131, 118, 106, 95, 85, 76], lat = '';
		for (i = 0; i < ys.length - 1; i++) {
			var a1 = ys[i], a2 = ys[i + 1];
			lat += 'M' + n2(xl(a1)) + ',' + a1 + ' L' + n2(xr(a2)) + ',' + a2 + ' M' + n2(xr(a1)) + ',' + a1 + ' L' + n2(xl(a2)) + ',' + a2;
		}
		for (i = 0; i < ys.length; i++) lat += 'M' + n2(xl(ys[i])) + ',' + ys[i] + ' H' + n2(xr(ys[i]));
		s += path(lat, 'none', { stroke: 'rgba(8,20,40,.55)', 'stroke-width': 3, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' });
		s += path(lat, 'none', { stroke: '#d6e0ee', 'stroke-width': 1.3, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' });
		/* 主腿：深描边 + 亮钢 + 高光 */
		var legs = 'M' + n2(xl(yb)) + ',' + yb + ' L' + n2(xl(yt)) + ',' + yt + ' M' + n2(xr(yb)) + ',' + yb + ' L' + n2(xr(yt)) + ',' + yt;
		s += path(legs, 'none', { stroke: OUT, 'stroke-width': 5, 'stroke-linecap': 'round' });
		s += path(legs, 'none', { stroke: '#e9eff7', 'stroke-width': 3, 'stroke-linecap': 'round' });
		s += path('M' + n2(xl(yb) - 0.7) + ',' + yb + ' L' + n2(xl(yt) - 0.5) + ',' + yt, 'none', { stroke: '#fff', 'stroke-width': 1, 'stroke-linecap': 'round', opacity: 0.9 });
		s += path('M' + n2(xr(yb) + 0.9) + ',' + yb + ' L' + n2(xr(yt) + 0.6) + ',' + yt, 'none', { stroke: '#8a9bb2', 'stroke-width': 1, 'stroke-linecap': 'round', opacity: 0.8 });
		s += path('M' + cx + ',' + yb + ' V' + yt, 'none', { stroke: 'rgba(20,34,56,.35)', 'stroke-width': 1.4, 'stroke-dasharray': '2 3' });
		/* 微波天线（右侧） */
		var dy0 = 150, dxr = xr(dy0);
		s += line(dxr, dy0, dxr + 16, dy0, OUT, 4);
		s += line(dxr, dy0, dxr + 16, dy0, '#dfe7f2', 2.2);
		s += ell(dxr + 21, dy0, 7.5, 17, 'url(#ta-gWhiteH)', olStroke(1.4));
		s += ell(dxr + 23, dy0, 4.4, 12.5, 'rgba(120,140,170,.35)');
		s += line(dxr + 14, dy0, dxr + 31, dy0, OUT, 2);
		s += circ(dxr + 31, dy0, 2.3, '#cfd9e6', olStroke(1));
		/* 顶部平台与横担 */
		s += ell(cx, 79, 31, 7, 'rgba(10,22,40,.55)', olStroke(1.2));
		s += ell(cx, 77, 31, 7, 'none', { stroke: '#e6edf7', 'stroke-width': 1.4 });
		s += line(70, 68, 122, 68, OUT, 4) + line(70, 68, 122, 68, '#cdd8e6', 2);
		s += line(70, 99, 122, 99, OUT, 4) + line(70, 99, 122, 99, '#cdd8e6', 2);
		/* 三扇区天线：左右偏转面板 + 正面面板 */
		var pL = 'M65,57 L77,60 L77,106 L65,109 Z', pR = 'M127,57 L115,60 L115,106 L127,109 Z';
		s += path(pL, 'url(#ta-gSide)', olStroke(1.4)) + path(pR, 'url(#ta-gSide)', olStroke(1.4));
		s += path('M65,57 L77,60 L77,66 L65,63 Z', 'url(#ta-gC-' + k + ')', olStroke(1)) + path('M127,57 L115,60 L115,66 L127,63 Z', 'url(#ta-gC-' + k + ')', olStroke(1));
		s += rect(87, 52, 22, 58, 4, 'url(#ta-gWhiteH)', olStroke(1.5));
		s += rect(87, 52, 22, 58, 4, 'url(#ta-gGloss)', { 'pointer-events': 'none' });
		s += rect(87, 52, 22, 8, 3.5, 'url(#ta-gCH-' + k + ')', olStroke(1));
		s += rect(95.5, 62, 5, 40, 2.5, 'rgba(70,90,120,.18)');
		s += line(92, 64, 92, 100, '#fff', 1.2, { opacity: 0.85 });
		s += rect(91, 106, 5, 6, 1, '#3a4658', olStroke(0.8)) + rect(100, 106, 5, 6, 1, '#3a4658', olStroke(0.8));
		s += rect(91.4, 111, 4.2, 2.4, 0.6, 'url(#ta-gGold)') + rect(100.4, 111, 4.2, 2.4, 0.6, 'url(#ta-gGold)');
		/* 避雷针 + 航空障碍灯 */
		s += line(cx, 52, cx, 26, OUT, 3.2) + line(cx, 52, cx, 26, '#dfe7f2', 1.6);
		s += circ(cx, 23, 8, 'rgba(255,70,90,.28)', { cls: 'ta-beacon' });
		s += circ(cx, 23, 3.6, '#ff4a60', mix(olStroke(1), { cls: 'ta-beacon' }));
		s += circ(cx - 1, 22, 1, '#fff', { opacity: 0.8 });
		/* 扇区电波 */
		if (active) {
			s += arcFan(60, 82, 180, [10, 17, 24], 34, c.a, { w: 2, step: 0.28, dur: 1.9 });
			s += arcFan(132, 82, 0, [10, 17, 24], 34, c.a, { w: 2, step: 0.28, dur: 1.9 });
		}
		/* 机房 */
		var bx = 148;
		s += gshadow(bx + 28, 296, 38, 6, 0.9);
		s += path('M' + bx + ',262 H' + (bx + 46) + ' V294 H' + bx + ' Z', 'url(#ta-gConcrete)', olStroke(1.4));
		s += path('M' + (bx + 46) + ',262 L' + (bx + 58) + ',256 V288 L' + (bx + 46) + ',294 Z', 'url(#ta-gConcreteD)', olStroke(1.4));
		s += path('M' + bx + ',262 L' + (bx + 12) + ',254 H' + (bx + 58) + ' L' + (bx + 46) + ',262 Z', 'url(#ta-gWhite)', olStroke(1.4));
		s += rect(bx, 262, 46, 6, 0, 'url(#ta-gC-' + k + ')', { opacity: 0.95 });
		s += rect(bx + 6, 273, 14, 21, 1.5, '#55657c', olStroke(1.1));
		s += rect(bx + 8, 275, 10, 17, 1, 'rgba(255,255,255,.14)');
		s += circ(bx + 17, 284, 1.2, '#f1c04a');
		s += rect(bx + 26, 273, 15, 12, 1.5, '#dbe3ee', olStroke(1));
		s += circ(bx + 33.5, 279, 4.2, '#7f8fa5', olStroke(0.9)) + line(bx + 29.5, 279, bx + 37.5, 279, 'rgba(0,0,0,.5)', 0.8) + line(bx + 33.5, 275, bx + 33.5, 283, 'rgba(0,0,0,.5)', 0.8);
		/* 馈线：塔基 → 机房 */
		var fd = 'M' + n2(xr(yb) - 2) + ',284 C' + (bx - 24) + ',284 ' + (bx - 16) + ',268 ' + (bx + 8) + ',270';
		s += path(fd, 'none', { stroke: OUT, 'stroke-width': 3.6, 'stroke-linecap': 'round' });
		s += path(fd, 'none', { stroke: '#6b7b92', 'stroke-width': 2, 'stroke-linecap': 'round' });
		/* 回传光纤 */
		var bd = 'M' + (bx + 58) + ',284 H214';
		s += path(bd, 'none', { stroke: OUT, 'stroke-width': 4, 'stroke-linecap': 'round' });
		s += path(bd, 'none', { stroke: 'url(#ta-gFiber)', 'stroke-width': 2.4, 'stroke-linecap': 'round' });
		s += circ(216, 284, 5, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:1.5' });
		s += circ(216, 284, 3.2, V('accent2', '#1fe0cf'), olStroke(1));
		var an = { air: { x: 62, y: 82 }, back: { x: 218, y: 284 }, top: { x: cx, y: 20 }, base: { x: cx, y: 292 }, center: { x: cx, y: 84 }, right: { x: 130, y: 82 } };
		if (o.label) s += pill(cx + 8, 308, o.label === true ? c.name : o.label, 'carrier:' + k, 11);
		return pack(wrapHit('tower', W, H, s, o), W, H, an, { carrier: k, color: c.b });
	}

	/* ================================================================== 互联网 */
	function cloud(o) {
		o = o || {};
		var W = 250, H = 178, id = uid('cl'), s = '', i, down = o.state === 'down';
		var gx = 125, gy = 86, gr = 36;
		var cd = 'M62,148 C32,148 14,128 20,108 C25,92 42,86 56,90 C56,66 80,48 104,54 C116,30 154,26 172,50 C196,44 224,60 222,90 C238,96 246,126 228,140 C222,146 212,148 202,148 Z';
		s += gshadow(126, 158, 108, 11, 0.9);
		/* 云体 */
		s += path(cd, 'none', { st: 'stroke:' + V('rim', 'rgba(150,205,255,.32)') + ';stroke-width:4;stroke-linejoin:round' });
		s += path(cd, down ? 'url(#ta-gCloudOff)' : 'url(#ta-gCloud)', { st: 'stroke:' + (down ? '#90a0b5' : V('cloud-edge', 'rgba(196,226,255,.9)')) + ';stroke-width:1.8;stroke-linejoin:round', 'stroke-dasharray': down ? '5 4' : null });
		s += path(cd, 'url(#ta-gGloss)', { opacity: 0.8, 'pointer-events': 'none' });
		s += path('M34,100 C42,90 52,88 58,92 M108,62 C118,48 140,44 154,52 M176,58 C192,54 206,60 214,72', 'none', { stroke: '#fff', 'stroke-opacity': down ? 0.25 : 0.55, 'stroke-width': 2, 'stroke-linecap': 'round' });
		/* 轨道后半 */
		var orb = 'translate(' + gx + ',' + gy + ') rotate(-16)';
		s += g({ transform: orb }, path('M-62,0 A62,17 0 0 1 62,0', 'none', { cls: down ? null : 'ta-orbit', 'stroke-linecap': 'round', st: 'stroke:' + (down ? 'rgba(130,150,175,.5)' : V('accent', '#37a6ff')) + ';stroke-width:1.5;--p:90px;animation-duration:5s', 'stroke-dasharray': '3 6', opacity: 0.85 }));
		/* 地球 */
		s += '<defs><clipPath id="' + id + 'c"><circle cx="' + gx + '" cy="' + gy + '" r="' + gr + '"/></clipPath></defs>';
		s += circ(gx, gy, gr + 4, 'rgba(60,150,255,.18)', { cls: down ? null : 'ta-breath' });
		s += circ(gx, gy, gr, down ? 'url(#ta-gCloudOff)' : 'url(#ta-gGlobe)');
		var gg = '';
		if (!down) {
			gg += path('M-30,-8 C-26,-20 -12,-26 -6,-20 C0,-14 -8,-8 -4,-2 C-2,4 -12,8 -16,2 C-20,-2 -28,2 -30,-8 Z', '#8cf0c8', { opacity: 0.5 });
			gg += path('M8,-26 C18,-30 28,-22 24,-12 C30,-6 26,4 18,2 C12,6 6,-2 10,-8 C4,-12 4,-22 8,-26 Z', '#8cf0c8', { opacity: 0.5 });
			gg += path('M-6,12 C0,8 8,12 8,20 C8,28 0,32 -6,28 C-12,24 -12,16 -6,12 Z', '#8cf0c8', { opacity: 0.45 });
		}
		var gl = '', mr = [0.35, 0.72];
		for (i = 0; i < mr.length; i++) gl += ell(0, 0, gr * mr[i], gr, 'none');
		gl += line(0, -gr, 0, gr, '#fff', 0.8) + line(-gr, 0, gr, 0, '#fff', 0.8);
		gl += path('M-31,-18 Q0,-12 31,-18 M-31,18 Q0,24 31,18', 'none');
		gg += g({ st: 'stroke:#fff;stroke-opacity:' + (down ? 0.3 : 0.38) + ';stroke-width:.9;fill:none' }, gl);
		s += g({ 'clip-path': 'url(#' + id + 'c)', transform: 'translate(' + gx + ',' + gy + ')' }, gg + ell(-12, -16, 20, 12, 'rgba(255,255,255,.28)', { transform: 'rotate(-28)' }) + circ(14, 20, 38, 'none', { stroke: 'rgba(0,10,50,.35)', 'stroke-width': 14 }));
		s += circ(gx, gy, gr, 'none', { st: 'stroke:' + (down ? '#8e9db1' : 'rgba(210,235,255,.9)') + ';stroke-width:1.4' });
		/* 节点与链路 */
		if (!down) {
			var nd = [[-16, -12], [10, -17], [20, 8], [-6, 16], [0, -1]];
			var lk = 'M-16,-12 Q-6,-20 10,-17 M10,-17 Q22,-6 20,8 M20,8 Q10,18 -6,16 M-6,16 Q-16,6 -16,-12 M0,-1 L-16,-12 M0,-1 L10,-17 M0,-1 L20,8 M0,-1 L-6,16';
			s += g({ transform: 'translate(' + gx + ',' + gy + ')' }, path(lk, 'none', { cls: 'ta-dashmove', 'stroke-dasharray': '2 4', st: 'stroke:#fff;stroke-opacity:.8;stroke-width:1' }));
			for (i = 0; i < nd.length; i++) {
				s += circ(gx + nd[i][0], gy + nd[i][1], 5, 'none', { cls: 'ta-ring', st: 'stroke:#fff;stroke-width:1;animation-delay:' + n2(i * 0.5) + 's' });
				s += circ(gx + nd[i][0], gy + nd[i][1], 2.3, '#fff');
			}
		} else {
			s += g({ cls: 'ta-blink' }, line(gx - 9, gy - 9, gx + 9, gy + 9, V('bad', '#ff5a73'), 3.4) + line(gx + 9, gy - 9, gx - 9, gy + 9, V('bad', '#ff5a73'), 3.4));
		}
		/* 轨道前半 */
		s += g({ transform: orb }, path('M-62,0 A62,17 0 0 0 62,0', 'none', { cls: down ? null : 'ta-orbit', 'stroke-linecap': 'round', st: 'stroke:' + (down ? 'rgba(130,150,175,.7)' : V('accent2', '#1fe0cf')) + ';stroke-width:1.8;--p:90px;animation-duration:5s', 'stroke-dasharray': '3 6' }));
		/* 云外小节点 */
		if (!down) {
			var ex = [[20, 46, 0], [232, 50, 0.6], [236, 128, 1.1], [14, 128, 1.6]];
			for (i = 0; i < ex.length; i++) {
				s += path('M' + ex[i][0] + ',' + ex[i][1] + ' L' + (ex[i][0] < 125 ? ex[i][0] + 14 : ex[i][0] - 14) + ',' + (ex[i][1] < 90 ? ex[i][1] + 10 : ex[i][1] - 6), 'none', { st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-opacity:.55;stroke-width:1', 'stroke-dasharray': '2 3' });
				s += circ(ex[i][0], ex[i][1], 3.6, V('accent2', '#1fe0cf'), { cls: 'ta-breath', st: 'animation-delay:' + ex[i][2] + 's' });
				s += circ(ex[i][0], ex[i][1], 3.6, 'none', olStroke(0.8));
			}
		}
		if (o.label) s += pill(126, 162, o.label === true ? '互联网' : o.label, down ? 'bad' : 'info', 11);
		var an = { 'in': { x: 126, y: 150 }, l: { x: 18, y: 112 }, r: { x: 236, y: 112 }, top: { x: 126, y: 30 }, center: { x: gx, y: gy }, bottom: { x: 126, y: 150 } };
		return pack(wrapHit('cloud', W, H, s, o), W, H, an);
	}

	/* ================================================================== 上游 WiFi 路由器 / AP */
	function upRouter(o) {
		o = o || {};
		var W = 224, H = 176, s = '', i, st = o.state || 'up', lc = ledColor(st === 'down' ? 'bad' : (st === 'connecting' ? 'warn' : 'ok'));
		s += gshadow(112, 156, 104, 11, 0.9);
		var top = 'M52,100 H172 Q178,100 181,106 L196,128 H28 L43,106 Q46,100 52,100 Z';
		var front = 'M28,128 H196 V142 Q196,150 188,150 H36 Q28,150 28,142 Z';
		/* 天线（先画，机身盖住根部） */
		var bx = [60, 90, 134, 164], ag = [-19, -7, 7, 19];
		for (i = 0; i < 4; i++) {
			s += g({ transform: 'rotate(' + ag[i] + ',' + bx[i] + ',102)' },
				rect(bx[i] - 3.6, 36, 7.2, 68, 3.6, 'url(#ta-gDarkH)', { stroke: OUT, 'stroke-width': 1.4 }) +
				rect(bx[i] - 2.2, 40, 1.5, 54, 0.75, '#fff', { opacity: 0.4 }) +
				rect(bx[i] - 3.6, 36, 7.2, 8, 3.6, 'rgba(255,255,255,.12)'));
		}
		s += rimPath(top) + rimPath(front);
		s += path(front, 'url(#ta-gWhite)', olStroke(1.5));
		s += path(front, 'url(#ta-gGloss)', { opacity: 0.7 });
		s += path(top, 'url(#ta-gWhiteH)', olStroke(1.5));
		s += path(top, 'url(#ta-gGloss)', { opacity: 0.55 });
		/* 顶面：WiFi 发光标 + 通风缝 */
		var gc = st === 'down' ? '#8d9bb0' : V('accent', '#37a6ff');
		s += g({ transform: 'translate(112,121)' }, arcFan(0, 0, -90, [5, 10, 15], 42, gc, { still: true, w: 2.2, op: 1 }) + circ(0, 0, 2.2, gc));
		for (i = 0; i < 5; i++) s += line(142 + i * 4.4, 110, 141 + i * 5, 121, 'rgba(60,80,110,.28)', 1.4);
		for (i = 0; i < 5; i++) s += line(82 - i * 4.4, 110, 83 - i * 5, 121, 'rgba(60,80,110,.28)', 1.4);
		/* 前沿状态灯 */
		var lx = [56, 72, 88, 104], on;
		for (i = 0; i < 4; i++) {
			on = (st !== 'down') || i === 0;
			s += circ(lx[i], 139, 2.1, on ? lc[0] : '#aab5c4', { cls: (on && i === 2 && st === 'up') ? 'ta-blink2' : (st === 'connecting' && i === 1 ? 'ta-blink' : null) });
			if (on) s += circ(lx[i], 139, 4.6, lc[0], { opacity: 0.2, cls: st === 'connecting' ? 'ta-blink' : null });
		}
		s += rect(168, 134, 16, 8, 4, 'rgba(60,80,110,.18)', { stroke: 'rgba(60,80,110,.4)', 'stroke-width': 0.8 });
		s += rect(32, 150, 14, 4, 1.5, '#3a4658') + rect(178, 150, 14, 4, 1.5, '#3a4658');
		for (i = 0; i < 4; i++) s += circ(bx[i], 102, 5.4, 'url(#ta-gSteel)', olStroke(1.2));
		if (o.pulse) {
			s += g({ 'pointer-events': 'none' },
				circ(112, 50, 20, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:1.6' }) +
				circ(112, 50, 20, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:1.6;animation-delay:.9s' }) +
				circ(112, 50, 20, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent2', '#1fe0cf') + ';stroke-width:1.6;animation-delay:1.8s' }));
		}
		var an = { ant: { x: 112, y: 40 }, lan: { x: 196, y: 138 }, wan: { x: 28, y: 138 }, top: { x: 112, y: 30 }, bottom: { x: 112, y: 152 }, center: { x: 112, y: 100 } };
		return pack(wrapHit('uprouter', W, H, s, o), W, H, an);
	}

	/* ================================================================== 光猫 / 运营商网关 */
	function onu(o) {
		o = o || {};
		var W = 172, H = 208, s = '', i, lab = ['PON', 'LOS', 'LAN', 'WLAN', 'TEL'];
		var up = o.state !== 'down';
		var front = 'M44,38 H106 Q114,38 114,46 V170 Q114,178 106,178 H44 Q36,178 36,170 V46 Q36,38 44,38 Z';
		var side = 'M114,44 L134,31 V158 L114,172 Z';
		var topf = 'M40,40 L58,26 H128 Q134,26 134,31 L114,44 H44 Z';
		s += gshadow(86, 186, 70, 9, 0.9);
		s += path(topf, 'none', { st: 'stroke:' + V('rim', 'rgba(150,205,255,.32)') + ';stroke-width:3.6;stroke-linejoin:round' });
		s += rimPath(front);
		s += path(side, 'url(#ta-gSide)', olStroke(1.5));
		s += path(side, 'url(#ta-gGloss)', { opacity: 0.35 });
		s += path(front, 'url(#ta-gWhiteH)', olStroke(1.5));
		s += path(front, 'url(#ta-gGloss)', { opacity: 0.6 });
		s += path(topf, 'url(#ta-gWhite)', olStroke(1.5));
		for (i = 0; i < 4; i++) s += line(66 + i * 12, 33, 60 + i * 12, 38, 'rgba(50,70,100,.35)', 2);
		/* 散热缝 */
		for (i = 0; i < 5; i++) s += line(48, 52 + i * 5.4, 102, 52 + i * 5.4, 'rgba(40,60,90,.32)', 2.4);
		for (i = 0; i < 5; i++) s += line(48, 51 + i * 5.4, 102, 51 + i * 5.4, '#fff', 0.8, { opacity: 0.6 });
		/* 光纤徽标（原创：同心弧 + 光点） */
		s += circ(75, 92, 9, 'rgba(18,163,90,.14)', { stroke: 'rgba(18,140,80,.7)', 'stroke-width': 1.3 });
		s += arcFan(71, 92, 0, [4, 8], 55, '#12a35a', { still: true, w: 1.6, op: 0.9 });
		s += circ(70, 92, 1.7, '#12a35a');
		/* 指示灯组 */
		var stt = [up ? 'ok' : 'off', up ? 'off' : 'bad', 'ok', 'ok', 'off'], y, lcn;
		for (i = 0; i < 5; i++) {
			y = 112 + i * 10.4; lcn = ledColor(stt[i]);
			s += circ(50, y, 2.4, lcn[0], { cls: i === 2 && up ? 'ta-blink2' : (i === 1 && !up ? 'ta-blink' : null) });
			if (stt[i] !== 'off') s += circ(50, y, 5, lcn[0], { opacity: 0.2, cls: i === 2 && up ? 'ta-blink2' : null });
			s += tx(58, y + 2.4, lab[i], 6.4, 'rgba(40,60,90,.78)', { 'font-weight': 700 });
		}
		/* 网口 */
		s += rect(78, 108, 28, 58, 4, 'rgba(40,60,90,.07)');
		for (i = 0; i < 3; i++) {
			s += rect(82, 112 + i * 17, 20, 13, 2, '#2a3445', olStroke(1));
			s += rect(84, 114 + i * 17, 16, 7, 1, '#10151f');
			s += rect(86, 120 + i * 17, 12, 2, 0.6, 'url(#ta-gGold)');
			s += rect(88, 111 + i * 17, 8, 2, 1, '#fff', { opacity: 0.6 });
		}
		s += rect(40, 178, 70, 5, 2.5, '#394558', olStroke(1.2));
		/* 光纤尾纤 */
		var fd = 'M52,181 C50,196 28,190 14,202';
		s += path(fd, 'none', { stroke: OUT, 'stroke-width': 4.6, 'stroke-linecap': 'round' });
		s += path(fd, 'none', { stroke: 'url(#ta-gFiber)', 'stroke-width': 3, 'stroke-linecap': 'round' });
		s += path(fd, 'none', { stroke: '#fff', 'stroke-width': 0.8, 'stroke-opacity': 0.55, transform: 'translate(-.4,-.8)', 'stroke-linecap': 'round' });
		if (up) s += path(fd, 'none', { cls: 'ta-fl', 'stroke-dasharray': '3 33', 'stroke-linecap': 'round', st: 'stroke:#eafff2;stroke-width:1.8;animation-duration:1.1s' });
		s += rect(7, 197, 8, 6, 1.5, '#2fbf7a', olStroke(1));
		s += rect(3, 198, 5, 4, 1, '#d7efe0', olStroke(0.8));
		s += rect(121, 142, 10, 8, 1.5, '#2a3445', olStroke(0.9));
		var an = { lan: { x: 134, y: 146 }, fiber: { x: 4, y: 200 }, top: { x: 84, y: 24 }, center: { x: 84, y: 100 }, bottom: { x: 84, y: 184 }, right: { x: 134, y: 100 } };
		return pack(wrapHit('onu', W, H, s, o), W, H, an);
	}

	/* ================================================================== USB 网卡 */
	function usbNic(o) {
		o = o || {};
		var W = 236, H = 112, s = '', i, sp = o.speed == null ? '1000M' : String(o.speed), active = o.active !== false;
		var body = 'M74,30 H162 Q178,30 178,46 V66 Q178,82 162,82 H74 Q58,82 58,66 V46 Q58,30 74,30 Z';
		s += gshadow(120, 96, 112, 9, 0.9);
		/* USB-A 插头 */
		s += rect(2, 44, 46, 24, 2.5, 'url(#ta-gSteelH)', olStroke(1.5));
		s += rect(6, 48, 40, 16, 1.5, '#10151f');
		s += rect(8, 52, 36, 8, 1, '#2f7fff', { stroke: 'rgba(0,0,0,.4)', 'stroke-width': 0.6 });
		s += rect(8, 52, 36, 3, 1, '#fff', { opacity: 0.35 });
		for (i = 0; i < 5; i++) s += rect(12 + i * 6, 57, 3.6, 2.6, 0.6, 'url(#ta-gGold)');
		s += rect(2, 44, 46, 24, 2.5, 'url(#ta-gGloss)', { opacity: 0.5, 'pointer-events': 'none' });
		s += circ(25, 46.4, 1.1, 'rgba(0,0,0,.45)') + circ(25, 65.6, 1.1, 'rgba(0,0,0,.45)');
		s += rect(46, 41, 14, 30, 4, 'url(#ta-gDark)', olStroke(1.4));
		s += rect(48, 43, 3, 26, 1.5, '#fff', { opacity: 0.18 });
		/* 机身 */
		s += rimPath(body);
		s += path(body, 'url(#ta-gDark)', olStroke(1.6));
		s += path(body, 'url(#ta-gGloss)', { opacity: 0.85 });
		s += path('M72,35 H164', 'none', { stroke: '#fff', 'stroke-opacity': 0.4, 'stroke-width': 1.2, 'stroke-linecap': 'round' });
		for (i = 0; i < 6; i++) s += line(72 + i * 4.4, 44, 70 + i * 4.4, 52, 'rgba(0,0,0,.4)', 1.6);
		s += tx(104, 64, sp, 12.5, '#d9e9ff', { 'font-weight': 800 });
		s += tx(104, 74.5, 'Gigabit', 6.4, 'rgba(200,225,255,.6)', { 'font-weight': 600 });
		for (i = 0; i < 3; i++) s += rect(72 + i * 8, 61, 6, 4, 0.8, 'none', { stroke: '#8fc2ff', 'stroke-width': 1 }) + line(75 + i * 8, 65, 75 + i * 8, 69, '#8fc2ff', 1);
		s += line(75, 69, 91, 69, '#8fc2ff', 1);
		s += rect(70, 78, 98, 2, 1, 'url(#ta-gAcc)', { opacity: 0.85 });
		s += circ(130, 41, 2.6, active ? '#2fe3a0' : '#516070', { cls: active ? 'ta-blink2' : null });
		if (active) s += circ(130, 41, 5.5, '#2fe3a0', { opacity: 0.2, cls: 'ta-blink2' });
		s += circ(142, 41, 2.6, active ? '#ffb83d' : '#516070', { cls: active ? 'ta-blink' : null });
		/* RJ45 口 */
		s += rect(176, 26, 50, 60, 5, 'url(#ta-gSteelH)', olStroke(1.6));
		s += rect(176, 26, 50, 60, 5, 'url(#ta-gGloss)', { opacity: 0.5, 'pointer-events': 'none' });
		s += rect(184, 36, 36, 42, 3, '#0d121b', olStroke(1.1));
		s += rect(188, 38, 28, 22, 2, '#1d2635');
		for (i = 0; i < 8; i++) s += rect(190 + i * 3.2, 40, 1.5, 15, 0.6, 'url(#ta-gGold)');
		s += rect(194, 70, 16, 8, 1.5, '#0d121b') + rect(197, 72, 10, 5, 1, '#1b2434');
		s += circ(184, 31, 2, '#2fe3a0', { cls: active ? 'ta-blink2' : null }) + circ(218, 31, 2, '#ffb83d', { cls: active ? 'ta-blink' : null });
		var an = { usb: { x: 0, y: 56 }, lan: { x: 226, y: 56 }, rj45: { x: 226, y: 56 }, top: { x: 118, y: 26 }, bottom: { x: 118, y: 88 }, center: { x: 118, y: 56 } };
		return pack(wrapHit('usbnic', W, H, s, o), W, H, an);
	}

	/* ================================================================== 手机 */
	function phone(kind, o) {
		o = o || {};
		kind = kind === 'iphone' || kind === 'android' ? kind : 'unknown';
		var W = 112, H = 208, id = uid('ph'), s = '', i, ip = kind === 'iphone';
		var R = ip ? 22 : 17;
		var body = 'M' + (6 + R) + ',4 H' + (106 - R) + ' Q106,4 106,' + (4 + R) + ' V' + (200 - R) + ' Q106,200 ' + (106 - R) + ',200 H' + (6 + R) + ' Q6,200 6,' + (200 - R) + ' V' + (4 + R) + ' Q6,4 ' + (6 + R) + ',4 Z';
		var sr = R - 4, scr = 'M' + (10 + sr) + ',8 H' + (102 - sr) + ' Q102,8 102,' + (8 + sr) + ' V' + (196 - sr) + ' Q102,196 ' + (102 - sr) + ',196 H' + (10 + sr) + ' Q10,196 10,' + (196 - sr) + ' V' + (8 + sr) + ' Q10,8 ' + (10 + sr) + ',8 Z';
		s += gshadow(56, 204, 52, 6, 0.9);
		var btn = function (x, y, h) { return rect(x, y, 2.6, h, 1.2, 'url(#ta-gSteelH)', olStroke(0.9)); };
		if (ip) s += btn(3.4, 52, 9) + btn(3.4, 70, 16) + btn(3.4, 92, 16) + btn(106, 78, 28);
		else s += btn(106, 62, 22) + btn(106, 96, 16) + btn(106, 118, 16);
		s += rimPath(body);
		s += path(body, ip ? 'url(#ta-gSteelH)' : (kind === 'android' ? 'url(#ta-gDarkH)' : 'url(#ta-gSide)'), olStroke(1.6));
		s += path(body, 'none', { stroke: '#fff', 'stroke-opacity': ip ? 0.7 : 0.35, 'stroke-width': 0.9, transform: 'translate(0,.4)' });
		s += path(scr, '#05070c', olStroke(1));
		s += '<defs><clipPath id="' + id + 'c"><path d="' + scr + '"/></clipPath></defs>';
		var wp = kind === 'unknown' ? 'url(#ta-gScreenG)' : (ip ? 'url(#ta-gScreenW)' : 'url(#ta-gScreen)');
		var sc = rect(10, 8, 92, 188, 0, wp);
		sc += circ(90, 40, 52, 'rgba(255,255,255,.12)') + circ(24, 170, 62, 'rgba(0,0,0,.18)') + circ(100, 160, 36, 'rgba(255,255,255,.10)');
		sc += path('M10,126 C34,112 60,140 102,118 V196 H10 Z', 'rgba(255,255,255,.08)');
		/* 状态栏 */
		sc += tx(19, 22, '9:41', 7.6, '#fff', { 'font-weight': 700 });
		for (i = 0; i < 4; i++) sc += rect(78 + i * 3.4, 20 - i * 1.5, 2.2, 3 + i * 1.5, 0.6, '#fff', { opacity: i < 3 ? 1 : 0.45 });
		sc += rect(91, 15.5, 8, 5, 1.4, 'none', { stroke: '#fff', 'stroke-width': 0.8, opacity: 0.9 }) + rect(92.2, 16.7, 5, 2.6, 0.6, '#fff');
		if (ip) sc += rect(38, 12, 36, 10.5, 5.25, '#02040a');
		else if (kind === 'android') sc += circ(56, 16, 3, '#02040a') + circ(55.2, 15.2, 0.9, '#3b4a66');
		if (kind === 'unknown') {
			sc += circ(56, 100, 22, 'none', { stroke: 'rgba(255,255,255,.55)', 'stroke-width': 2.4, 'stroke-dasharray': '5 4' });
			sc += tx(56, 109, '?', 28, 'rgba(255,255,255,.85)', { 'text-anchor': 'middle', 'font-weight': 800 });
		} else {
			sc += tx(56, 52, '09:41', 21, '#fff', { 'text-anchor': 'middle', 'font-weight': 700, opacity: 0.95 });
			sc += tx(56, 64, '10月30日 周五', 7.4, '#fff', { 'text-anchor': 'middle', opacity: 0.8 });
			sc += rect(17, 74, 78, 58, 11, 'rgba(255,255,255,.2)', { stroke: 'rgba(255,255,255,.4)', 'stroke-width': 0.8 });
			sc += circ(31, 90, 9.5, 'url(#ta-gAcc)', { stroke: 'rgba(255,255,255,.7)', 'stroke-width': 1 });
			if (ip) {
				sc += rect(25.5, 87, 8, 5, 2.5, 'none', { stroke: '#fff', 'stroke-width': 1.5 }) + rect(29, 88.6, 8, 5, 2.5, 'none', { stroke: '#fff', 'stroke-width': 1.5 });
			} else {
				sc += path('M31,84.5 V96 M31,89.5 L26.6,87 M31,92 L35.4,89.6', 'none', { stroke: '#fff', 'stroke-width': 1.3, 'stroke-linecap': 'round' });
				sc += circ(26.6, 86.6, 1.3, '#fff') + rect(34.2, 88.2, 2.6, 2.6, 0.5, '#fff') + circ(31, 97, 1.8, 'none', { stroke: '#fff', 'stroke-width': 1 }) + path('M31,82.6 l-2,2.8 h4 z', '#fff');
			}
			sc += tx(45, 89, ip ? '个人热点' : 'USB 共享', 8.4, '#fff', { 'font-weight': 700 });
			sc += tx(45, 99.5, ip ? '1 个连接' : '网络共享中', 6.8, '#fff', { opacity: 0.82 });
			sc += line(24, 108, 88, 108, 'rgba(255,255,255,.3)', 0.8);
			sc += tx(24, 124, '已开启', 7.6, '#fff', { 'font-weight': 600 });
			sc += rect(66, 114, 22, 12, 6, '#34e0a1') + circ(82, 120, 4.4, '#fff');
			sc += circ(31, 90, 12.5, 'none', { cls: 'ta-ring', st: 'stroke:#fff;stroke-width:1' });
		}
		var dc = ['#ff6b6b', '#ffc24b', '#4cd9a0', '#5aa9ff'];
		for (i = 0; i < 4; i++) sc += rect(17 + i * 20, 160, 16, 16, 4.5, dc[i], { opacity: kind === 'unknown' ? 0.35 : 0.95 }) + rect(17 + i * 20, 160, 16, 8, 4.5, '#fff', { opacity: 0.2 });
		sc += rect(40, 186, 32, 2.8, 1.4, '#fff', { opacity: 0.85 });
		s += g({ 'clip-path': 'url(#' + id + 'c)' }, sc + path('M10,8 H62 L10,86 Z', 'rgba(255,255,255,.1)'));
		s += rect(47, 197, 18, 3.4, 1.7, '#06080d', { stroke: 'rgba(255,255,255,.3)', 'stroke-width': 0.6 });
		if (o.pulse) {
			s += circ(56, 4, 14, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:1.6' }) +
				circ(56, 4, 14, 'none', { cls: 'ta-ring', st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:1.6;animation-delay:.9s' });
		}
		var an = { usb: { x: 56, y: 201 }, ant: { x: 56, y: 2 }, top: { x: 56, y: 2 }, bottom: { x: 56, y: 204 }, center: { x: 56, y: 102 } };
		return pack(wrapHit('phone', W, H, s, o), W, H, an, { kind: kind });
	}

	/* ================================================================== 徽章 / 信号格 / 仪表条 */
	function badge(text, tone, o) {
		o = o || {};
		text = String(text == null ? '' : text);
		var fs = o.fs || 11, h = Math.round(fs + 9), padX = Math.round(fs * 0.85), dot = o.dot !== false;
		var col, c2 = null, ck;
		if (typeof tone === 'string' && tone.indexOf('carrier:') === 0) { ck = tone.slice(8); col = CARRIERS[ck] ? CARRIERS[ck].b : '#6a7e96'; c2 = CARRIERS[ck] ? CARRIERS[ck].a : col; }
		else col = toneColor(tone || 'info');
		var tw = textW(text, fs), w = Math.round(tw + padX * 2 + (dot ? fs * 0.9 : 0)), s = '';
		if (o.solid) {
			s += rect(0, 0, w, h, h / 2, c2 ? 'url(#ta-gC-' + ck + ')' : col, { stroke: 'rgba(0,0,0,.25)', 'stroke-width': 1 });
			s += rect(1, 1, w - 2, h / 2 - 1, h / 4, '#fff', { opacity: 0.2 });
			s += tx(w / 2, h / 2 + fs * 0.36, text, fs, '#fff', { 'text-anchor': 'middle', 'font-weight': 700 });
		} else {
			s += rect(0.6, 0.6, w - 1.2, h - 1.2, (h - 1.2) / 2, V('pill-bg', 'rgba(10,22,40,.72)'), { st: 'stroke:' + col + ';stroke-opacity:.8;stroke-width:1.2' });
			if (dot) {
				s += circ(padX * 0.85 + 2, h / 2, fs * 0.26, col);
				s += circ(padX * 0.85 + 2, h / 2, fs * 0.26 + 2, col, { opacity: 0.22 });
			}
			s += tx(padX + (dot ? fs * 0.9 : 0), h / 2 + fs * 0.36, text, fs, V('pill-ink', '#e8f1ff'), { 'font-weight': 700 });
		}
		return pack(s, w, h, { center: { x: w / 2, y: h / 2 }, left: { x: 0, y: h / 2 }, right: { x: w, y: h / 2 } });
	}

	/* 信号格：q 0..1（4 格）；o.wifi=true 画 WiFi 弧；o.s 缩放 */
	function signalBars(q, o) {
		o = o || {};
		var sc = o.s || 1, s = '', i, lit, col, t = toneOfQ(q);
		q = q == null || isNaN(q) ? 0 : clamp(q, 0, 1);
		col = o.color || (t ? TONE[t].v : V('dim', '#8fa5c2'));
		if (o.wifi) {
			lit = q <= 0 ? 0 : (q < 0.34 ? 1 : (q < 0.67 ? 2 : 3));
			var rr = [5, 9.5, 14];
			s += circ(9, 15, 1.9, lit >= 1 ? col : V('dim', '#8fa5c2'), { opacity: lit >= 1 ? 1 : 0.3 });
			for (i = 0; i < 3; i++) s += path('M' + n2(9 - rr[i] * 0.72) + ',' + n2(15 - rr[i] * 0.72) + ' A' + rr[i] + ',' + rr[i] + ' 0 0 1 ' + n2(9 + rr[i] * 0.72) + ',' + n2(15 - rr[i] * 0.72), 'none', {
				'stroke-linecap': 'round', st: 'stroke:' + (i < lit ? col : V('dim', '#8fa5c2')) + ';stroke-width:2;opacity:' + (i < lit ? 1 : 0.3)
			});
			return pack(g({ transform: 'scale(' + sc + ')' }, s), 18 * sc, 16 * sc, { center: { x: 9 * sc, y: 8 * sc } });
		}
		lit = q <= 0 ? 0 : Math.max(1, Math.ceil(q * 4 - 0.001));
		for (i = 0; i < 4; i++) {
			s += rect(i * 5.2, 14 - (4 + i * 3.3), 3.6, 4 + i * 3.3, 1.2, i < lit ? col : V('dim', '#8fa5c2'), { opacity: i < lit ? 1 : 0.28 });
		}
		return pack(g({ transform: 'scale(' + sc + ')' }, s), 19 * sc, 14 * sc, { center: { x: 9.5 * sc, y: 7 * sc } });
	}

	/* 仪表条：value 当前值；o.kind 取 RANGES 预设（rsrp/rsrq/sinr/rssi/snr/noise），也可自带 zones；
	 * o.w 宽度；o.label 名称；o.unit 单位。高 44。 */
	function meter(value, min, max, o) {
		o = o || {};
		var R = o.kind && RANGES[o.kind] ? RANGES[o.kind] : null;
		if (min == null && R) min = R.min;
		if (max == null && R) max = R.max;
		var w = o.w || 180, h = 44, s = '', i, ok = !(value === null || value === undefined || isNaN(value));
		var pct = ok ? clamp((value - min) / (max - min), 0, 1) : 0;
		var unit = o.unit != null ? o.unit : (R ? R.unit : ''), label = o.label != null ? o.label : (R ? R.name : '');
		var q = R && ok ? quality(o.kind, value) : null;
		var col = o.color || (q ? q.color : V('accent', '#37a6ff'));
		var ty = 20, th = 8;
		/* 底轨：分区淡色段 */
		var seg = [], prev = min, zi;
		if (R) {
			for (zi = 0; zi < R.z.length; zi++) {
				var end = Math.min(R.z[zi][0], max);
				if (end > prev) { seg.push([prev, end, R.z[zi][1]]); prev = end; }
			}
			if (prev < max) seg.push([prev, max, 'acc']);
		}
		s += rect(0, ty, w, th, th / 2, V('dim', '#8fa5c2'), { opacity: 0.16 });
		for (i = 0; i < seg.length; i++) {
			var xa = (seg[i][0] - min) / (max - min) * w, xb = (seg[i][1] - min) / (max - min) * w;
			s += rect(xa + (i ? 0.8 : 0), ty, Math.max(0, xb - xa - (i < seg.length - 1 ? 0.8 : 0)), th, i === 0 || i === seg.length - 1 ? th / 2 : 0, TONE[seg[i][2]].v, { opacity: 0.2 });
		}
		/* 已填充段 */
		if (ok) {
			s += rect(0, ty, Math.max(th, pct * w), th, th / 2, col, { cls: 'ta-grow', opacity: 0.95 });
			s += rect(1.5, ty + 1, Math.max(th, pct * w) - 3, 2, 1, '#fff', { cls: 'ta-grow', opacity: 0.35 });
			var kx = Math.max(th / 2, pct * w);
			s += circ(kx, ty + th / 2, 9, col, { opacity: 0.2 });
			s += circ(kx, ty + th / 2, 5.2, '#fff', { st: 'stroke:' + col + ';stroke-width:2.2' });
		}
		/* 刻度 */
		for (i = 0; i <= 4; i++) s += line(i * w / 4, ty + th + 2, i * w / 4, ty + th + 5, V('dim', '#8fa5c2'), 1, { opacity: 0.5 });
		s += tx(0, h - 2, min + (unit ? '' : ''), 8, V('dim', '#8fa5c2'), { opacity: 0.8 });
		s += tx(w, h - 2, String(max), 8, V('dim', '#8fa5c2'), { 'text-anchor': 'end', opacity: 0.8 });
		/* 顶行：名称 + 数值 */
		if (label) s += tx(0, 12, label, 10.5, V('dim', '#8fa5c2'), { 'font-weight': 600 });
		var vt = ok ? String(value) : '—';
		s += tx(w, 12.5, vt + (ok && unit ? ' ' + unit : ''), 12, ok ? col : V('dim', '#8fa5c2'), { 'text-anchor': 'end', 'font-weight': 800 });
		if (q && o.tag !== false) s += tx(w - textW(vt + ' ' + unit, 12) - 8, 12, q.label, 9, col, { 'text-anchor': 'end', opacity: 0.9, 'font-weight': 700 });
		return pack(s, w, h, { center: { x: w / 2, y: ty + th / 2 } }, { pct: pct, tone: q ? q.tone : null });
	}

	/* ================================================================== 客户端图标（56x56） */
	function guessKind(name, vendor) {
		var s = (String(name || '') + ' ' + String(vendor || '')).toLowerCase();
		if (/ipad|tablet|tab[-_ ]?[a-z0-9]|matepad|mipad|pad\b/.test(s)) return 'tablet';
		if (/iphone|android|phone|redmi|xiaomi[-_ ]?\d|honor|oppo|vivo|galaxy|pixel|oneplus|realme|nova|mate\d|poco|手机/.test(s)) return 'phone';
		if (/macbook|laptop|notebook|thinkpad|surface|ideapad|latitude|xps|zenbook|book|笔记本/.test(s)) return 'laptop';
		if (/xbox|playstation|ps[345]\b|nintendo|switch|steamdeck|steam|游戏机/.test(s)) return 'console';
		if (/printer|laserjet|officejet|epson|canon|brother|lexmark|打印/.test(s)) return 'printer';
		if (/camera|ipc|hikvision|dahua|ezviz|reolink|imou|tapo|yi-|摄像/.test(s)) return 'camera';
		if (/synology|qnap|diskstation|unraid|truenas|nas\b|群晖|威联通/.test(s)) return 'nas';
		if (/speaker|echo|homepod|sonos|xiaoai|tmall|alexa|小爱|天猫精灵|音箱/.test(s)) return 'speaker';
		if (/tv|bravia|roku|appletv|chromecast|hisense|konka|skyworth|mibox|电视|盒子/.test(s)) return 'tv';
		if (/esp[-_ ]?\d|iot|plug|bulb|light|lamp|sensor|tuya|yeelight|lumi|aqara|mijia|智能|插座|灯/.test(s)) return 'iot';
		if (/desktop|pc\b|imac|workstation|win|台式|main/.test(s)) return 'pc';
		return 'unknown';
	}
	var KINDS = ['pc', 'laptop', 'phone', 'tablet', 'tv', 'iot', 'printer', 'console', 'camera', 'nas', 'speaker', 'unknown'];

	function glyph(kind) {
		var GI = 'stroke:' + V('gi', '#dcecff') + ';stroke-width:2;fill:none;stroke-linecap:round;stroke-linejoin:round';
		var ACC = 'url(#ta-gAcc)';
		function st(extra) { return { st: GI + (extra ? ';' + extra : '') }; }
		var s = '', i, j;
		switch (kind) {
		case 'pc':
			s += rect(8, 11, 30, 21, 3.5, 'none', st()) + rect(11, 14, 24, 15, 1.8, ACC, { opacity: 0.92 });
			s += path('M23,32 V38 M16.5,40 H29.5', 'none', st());
			s += rect(41, 15, 8, 27, 2.5, 'none', st()) + line(43.5, 20, 46.5, 20, V('gi', '#dcecff'), 1.6) + circ(45, 37, 1.3, V('accent2', '#1fe0cf'));
			break;
		case 'laptop':
			s += rect(14, 12, 28, 20, 3, 'none', st()) + rect(17, 15, 22, 14, 1.5, ACC, { opacity: 0.92 });
			s += path('M8,36 H48 L45.4,42 Q44.8,43.6 43,43.6 H13 Q11.2,43.6 10.6,42 Z', 'none', st());
			s += path('M23.5,38.8 H32.5', 'none', st('stroke-width:1.5'));
			break;
		case 'phone':
			s += rect(18.5, 8, 19, 36, 5, 'none', st()) + rect(21.5, 13, 13, 25, 2, ACC, { opacity: 0.92 });
			s += path('M25.5,10.6 H30.5', 'none', st('stroke-width:1.5')) + circ(28, 41, 1.2, V('gi', '#dcecff'));
			break;
		case 'tablet':
			s += rect(8, 13, 40, 30, 5, 'none', st()) + rect(12, 17, 32, 22, 2, ACC, { opacity: 0.92 });
			s += circ(10, 28, 0.9, V('gi', '#dcecff'));
			s += path('M24,41 H32', 'none', st('stroke-width:1.2;opacity:.8'));
			break;
		case 'tv':
			s += rect(7, 12, 42, 27, 3.5, 'none', st()) + rect(10, 15, 36, 21, 2, ACC, { opacity: 0.92 });
			s += path('M25,20.5 L33,25.5 L25,30.5 Z', '#fff', { opacity: 0.95 });
			s += path('M19,39 L16,44 M37,39 L40,44', 'none', st());
			break;
		case 'iot':
			s += path('M28,7 C20,7 15.5,13 15.5,19.5 C15.5,24.5 18.5,26.5 20.5,30.5 V33.5 H35.5 V30.5 C37.5,26.5 40.5,24.5 40.5,19.5 C40.5,13 36,7 28,7 Z', ACC, { opacity: 0.92, st: GI });
			s += path('M21,38.5 H35 M23.5,43 H32.5', 'none', st());
			s += path('M24.4,27 L26.4,20.5 L28,25 L29.6,20.5 L31.6,27', 'none', { stroke: '#fff', 'stroke-width': 1.5, 'stroke-linecap': 'round', 'stroke-linejoin': 'round', opacity: 0.9 });
			s += path('M7,16 H10.5 M45.5,16 H49 M9,8.5 L11.6,10.6 M47,8.5 L44.4,10.6', 'none', st('stroke-width:1.5;opacity:.55'));
			break;
		case 'printer':
			s += rect(16, 7, 24, 12, 1.8, 'none', st());
			s += rect(7, 18, 42, 19, 5, ACC, { opacity: 0.4, st: GI });
			s += rect(14, 30, 28, 15, 1.8, V('tile1', '#fff'), { st: GI });
			s += path('M18.5,35.5 H37.5 M18.5,40 H32', 'none', st('stroke-width:1.5'));
			s += circ(41, 24, 1.7, V('accent2', '#1fe0cf'));
			break;
		case 'console':
			s += path('M15,16 H41 C46.5,16 49.5,21 50.5,28 L52,36 C52.6,41.5 46.6,43.5 43.4,39.4 L39.4,34 H16.6 L12.6,39.4 C9.4,43.5 3.4,41.5 4,36 L5.5,28 C6.5,21 9.5,16 15,16 Z', ACC, { opacity: 0.4, st: GI });
			s += path('M16,22 V31 M11.5,26.5 H20.5', 'none', st('stroke-width:2.2'));
			s += circ(38, 23.5, 2.1, V('accent2', '#1fe0cf')) + circ(43, 28, 2.1, '#fff', { opacity: 0.9 });
			break;
		case 'camera':
			s += rect(7, 17, 30, 17, 5, ACC, { opacity: 0.4, st: GI });
			s += circ(41, 25.5, 7, V('tile2', '#0b1c33'), { st: GI }) + circ(41, 25.5, 2.8, ACC);
			s += path('M7,15.5 H37', 'none', st('stroke-width:2.6'));
			s += path('M22,34 V40 H14 M12,35 V45', 'none', st());
			s += circ(13.5, 23, 1.7, '#ff5a5a', { cls: 'ta-blink' });
			break;
		case 'nas':
			s += rect(10, 8, 36, 40, 5.5, 'none', st());
			for (i = 0; i < 2; i++) {
				s += rect(14.5, 13 + i * 15, 27, 11, 2.6, ACC, { opacity: 0.55, st: GI + ';stroke-width:1.6' });
				s += path('M18,' + (18.5 + i * 15) + ' H28', 'none', st('stroke-width:1.4;opacity:.8'));
				s += circ(37, 18.5 + i * 15, 1.5, i ? V('accent2', '#1fe0cf') : V('ok', '#2fe3a0'), { cls: i ? 'ta-blink2' : null });
			}
			s += path('M19,43 H29', 'none', st('stroke-width:1.5;opacity:.8'));
			break;
		case 'speaker':
			s += rect(15, 7, 26, 42, 11, 'none', st());
			s += ell(28, 14, 9.5, 3.2, ACC, { opacity: 0.9, st: GI + ';stroke-width:1.5' });
			for (i = 0; i < 4; i++) for (j = 0; j < 3; j++) s += circ(21.5 + j * 6.5, 24 + i * 6.2, 1.15, V('gi', '#dcecff'), { opacity: 0.75 });
			s += circ(28, 14, 1.4, V('accent2', '#1fe0cf'), { cls: 'ta-blink' });
			break;
		default:
			s += rect(11, 11, 34, 34, 10, 'none', { st: GI + ';stroke-dasharray:4 4' });
			s += tx(28, 37.5, '?', 25, V('gi', '#dcecff'), { 'text-anchor': 'middle', 'font-weight': 800 });
		}
		return s;
	}

	/* client(kind, {link:'wired'|'wifi', q:0..1, online:bool, sel:bool, hit:bool}) —— 统一 56x56 */
	function client(kind, o) {
		o = o || {};
		kind = KINDS.indexOf(kind) >= 0 ? kind : 'unknown';
		var W = 56, H = 56, s = '', i;
		s += gshadow(28, 55, 24, 4, 0.7);
		s += rect(0.8, 0.8, 54.4, 54.4, 15, 'url(#ta-gTile)', { st: 'stroke:' + V('tile-edge', 'rgba(180,215,255,.32)') + ';stroke-width:1.2' });
		s += path('M12,1.8 H44', 'none', { stroke: '#fff', 'stroke-opacity': 0.5, 'stroke-width': 1.2, 'stroke-linecap': 'round' });
		s += glyph(kind);
		if (o.link === 'wired') {
			s += circ(45.5, 45.5, 9.2, 'url(#ta-gBlue)', { stroke: '#fff', 'stroke-width': 1.3, 'stroke-opacity': 0.85 });
			s += rect(41.2, 41.8, 8.6, 6.8, 1.2, 'none', { stroke: '#fff', 'stroke-width': 1.3 });
			s += path('M43.6,48.6 V50.4 M47.4,48.6 V50.4 M43.6,41.8 V44 M45.5,41.8 V44 M47.4,41.8 V44', 'none', { stroke: '#fff', 'stroke-width': 1, 'stroke-linecap': 'round' });
		} else if (o.link === 'wifi') {
			var qc = o.q == null ? null : TONE[toneOfQ(o.q)].v;
			s += circ(45.5, 45.5, 9.2, qc ? qc : 'url(#ta-gGreen)', { stroke: '#fff', 'stroke-width': 1.3, 'stroke-opacity': 0.85 });
			s += circ(45.5, 49, 1.3, '#fff');
			s += path('M41.6,46 A5.6,5.6 0 0 1 49.4,46 M39.4,43.6 A8.4,8.4 0 0 1 51.6,43.6', 'none', { stroke: '#fff', 'stroke-width': 1.4, 'stroke-linecap': 'round', transform: 'translate(0,-.5)' });
		}
		if (o.online === true) {
			s += circ(48.5, 7.5, 4.6, V('ok', '#2fe3a0'), { opacity: 0.28, cls: 'ta-breath' });
			s += circ(48.5, 7.5, 2.8, V('ok', '#2fe3a0'), { stroke: '#fff', 'stroke-width': 0.9, 'stroke-opacity': 0.8 });
		} else if (o.online === false) {
			s += circ(48.5, 7.5, 2.8, V('dim', '#8fa5c2'), { opacity: 0.8 });
		}
		if (o.sel) s += rect(-2.5, -2.5, 61, 61, 17.5, 'none', { st: 'stroke:' + V('accent', '#37a6ff') + ';stroke-width:2' });
		var an = { top: { x: 28, y: 0 }, bottom: { x: 28, y: 56 }, left: { x: 0, y: 28 }, right: { x: 56, y: 28 }, center: { x: 28, y: 28 }, badge: { x: 45.5, y: 45.5 } };
		return pack(wrapHit('client', W, H, s, o), W, H, an, { kind: kind });
	}

	/* ================================================================== 链路 */
	function plug(kind, x, y, ang, edge) {
		if (!kind || kind === 'none') return '';
		var s = '', cb = V('cable', '#5b7399');
		if (kind === 'rj45') {
			s += rect(-2, -5, 11, 10, 1.5, '#dde7f3', olStroke(1.1));
			for (var i = 0; i < 4; i++) s += rect(0 + i * 0.01, -3.6 + i * 2.4, 3.6, 1.2, 0.4, 'url(#ta-gGold)');
			s += rect(9, -5.6, 8, 11.2, 1.8, '#c4d2e3', olStroke(1.1));
			s += path('M17,-4.6 L31,-3 V3 L17,4.6 Z', cb, olStroke(1.1));
		} else if (kind === 'a') {
			s += rect(-2, -6.5, 17, 13, 1.6, 'url(#ta-gSteelH)', olStroke(1.2));
			s += rect(0, -4.5, 12, 9, 1, '#10151f') + rect(1.4, -2.4, 10.5, 4.8, 0.8, '#2f7fff');
			s += rect(15, -7.6, 10, 15.2, 3, 'url(#ta-gDark)', olStroke(1.2));
			s += path('M25,-5.4 L38,-3.2 V3.2 L25,5.4 Z', '#3a465c', olStroke(1.1));
		} else if (kind === 'c') {
			s += rect(-2, -3.8, 14, 7.6, 3.8, 'url(#ta-gSteelH)', olStroke(1.2));
			s += rect(1, -1.8, 8, 3.6, 1.8, '#10151f');
			s += rect(12, -5.6, 10, 11.2, 3, 'url(#ta-gDark)', olStroke(1.2));
			s += path('M22,-4.2 L34,-2.8 V2.8 L22,4.2 Z', '#3a465c', olStroke(1.1));
		}
		return g({ transform: 'translate(' + n2(x) + ',' + n2(y) + ') rotate(' + n2(ang) + ')' }, s);
	}

	function flowLayers(d, cols, dir, dur, w, dash, op) {
		var gap = 36 - dash, s = '';
		function one(cls, col, delay, ww, o2) {
			return path(d, 'none', { cls: cls, 'stroke-linecap': 'round', 'stroke-dasharray': (dash <= 0.2 ? 0.1 : dash) + ' ' + (dash <= 0.2 ? 35.9 : gap),
				st: 'stroke:' + col + ';stroke-width:' + ww + ';stroke-opacity:' + o2 + ';animation-duration:' + n2(dur) + 's' + (delay ? ';animation-delay:' + n2(delay) + 's' : '') });
		}
		if (dir === 'fwd' || dir === 'both') s += one('ta-fl', cols[0], 0, w + 4.5, 0.22 * op) + one('ta-fl', cols[0], 0, w, op);
		if (dir === 'rev' || dir === 'both') {
			var c = dir === 'both' ? cols[1] : cols[0];
			s += one('ta-fr', c, dir === 'both' ? -dur * 0.5 : 0, w + 4.5, 0.22 * op) + one('ta-fr', c, dir === 'both' ? -dur * 0.5 : 0, w, op);
		}
		return s;
	}

	function link(type, from, to, o) {
		o = o || {};
		type = type || 'cable';
		var id = o.id || uid('lk'), x1 = +from.x, y1 = +from.y, x2 = +to.x, y2 = +to.y, dx = x2 - x1, dy = y2 - y1;
		var L0 = Math.sqrt(dx * dx + dy * dy) || 1, c1, c2, i, s = '';
		var speed = (o.speed == null ? 1 : o.speed) > 0 ? (o.speed == null ? 1 : o.speed) : 1, dur = 36 / (52 * speed);
		var dir = o.dir || (type === 'wifi' ? 'both' : 'fwd');
		var accent = o.color || V('accent', '#37a6ff'), accent2 = o.color2 || (o.color ? o.color : V('accent2', '#1fe0cf'));
		var q = o.q == null ? null : clamp(o.q, 0, 1), qt = q == null ? null : toneOfQ(q);
		if (type === 'rf' || o.curve === 'straight') {
			c1 = [x1 + dx / 3, y1 + dy / 3]; c2 = [x1 + dx * 2 / 3, y1 + dy * 2 / 3];
		} else if (type === 'wifi') {
			var bow = o.bow != null ? o.bow : Math.min(34, L0 * 0.16), nx = dy / L0, ny = -dx / L0;
			if (ny > 0.0001 && Math.abs(ny) > Math.abs(nx)) { nx = -nx; ny = -ny; }
			var qx = (x1 + x2) / 2 + nx * bow * 2, qy = (y1 + y2) / 2 + ny * bow * 2;
			c1 = [x1 + 2 / 3 * (qx - x1), y1 + 2 / 3 * (qy - y1)]; c2 = [x2 + 2 / 3 * (qx - x2), y2 + 2 / 3 * (qy - y2)];
		} else if (Math.abs(dx) >= Math.abs(dy)) {
			c1 = [x1 + dx * 0.5, y1]; c2 = [x2 - dx * 0.5, y2];
		} else {
			c1 = [x1, y1 + dy * 0.5]; c2 = [x2, y2 - dy * 0.5];
		}
		var d = 'M' + n2(x1) + ',' + n2(y1) + ' C' + n2(c1[0]) + ',' + n2(c1[1]) + ' ' + n2(c2[0]) + ',' + n2(c2[1]) + ' ' + n2(x2) + ',' + n2(y2);
		function B(t) {
			var u = 1 - t;
			return [u * u * u * x1 + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t * t * t * x2, u * u * u * y1 + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t * t * t * y2];
		}
		var len = 0, pp = B(0), pn, N = 40;
		for (i = 1; i <= N; i++) { pn = B(i / N); len += Math.sqrt((pn[0] - pp[0]) * (pn[0] - pp[0]) + (pn[1] - pp[1]) * (pn[1] - pp[1])); pp = pn; }
		var mid = B(0.5);
		var t0 = [c1[0] - x1, c1[1] - y1]; if (Math.abs(t0[0]) + Math.abs(t0[1]) < 0.01) t0 = [c2[0] - x1, c2[1] - y1];
		var t1 = [x2 - c2[0], y2 - c2[1]]; if (Math.abs(t1[0]) + Math.abs(t1[1]) < 0.01) t1 = [x2 - c1[0], y2 - c1[1]];
		var a0 = Math.atan2(t0[1], t0[0]) * 180 / Math.PI, a1 = Math.atan2(-t1[1], -t1[0]) * 180 / Math.PI;
		var cls = 'ta-link ta-link-' + type;

		if (type === 'dashed' || o.state === 'down') {
			var dc = o.color || V('line', '#3a5173');
			s += path(d, 'none', { st: 'stroke:' + dc + ';stroke-width:2.2;stroke-dasharray:5 6;stroke-linecap:round;opacity:.85' });
			if (o.mark !== false) {
				var bad = V('bad', '#ff5a73');
				s += circ(mid[0], mid[1], 10, 'none', { cls: 'ta-ring', st: 'stroke:' + bad + ';stroke-width:1.5' });
				s += circ(mid[0], mid[1], 9, V('pill-bg', 'rgba(10,22,40,.72)'), { st: 'stroke:' + bad + ';stroke-width:1.4' });
				s += line(mid[0] - 3.6, mid[1] - 3.6, mid[0] + 3.6, mid[1] + 3.6, bad, 1.8) + line(mid[0] + 3.6, mid[1] - 3.6, mid[0] - 3.6, mid[1] + 3.6, bad, 1.8);
			}
		} else if (type === 'cable') {
			/* 细线 + 端点小圆: 去掉粗管与 RJ45 插头, 线只表达连通与流向 */
			s += path(d, 'none', { st: 'stroke:' + V('cable', '#5b7399') + ';stroke-width:2;stroke-opacity:.6;stroke-linecap:round' });
			if (o.active !== false) s += flowLayers(d, [accent, accent2], dir, dur, 1.8, 5, 0.95);
			s += circ(x1, y1, 3.4, V('pill-bg', 'rgba(10,22,40,.72)'), { st: 'stroke:' + V('cable', '#5b7399') + ';stroke-width:1.4' }) +
				circ(x2, y2, 3.4, V('pill-bg', 'rgba(10,22,40,.72)'), { st: 'stroke:' + V('cable', '#5b7399') + ';stroke-width:1.4' });
		} else if (type === 'usb') {
			var ue = o.ends || ['a', 'c'];
			s += path(d, 'none', { stroke: o.cableColor || '#4d5d79', 'stroke-width': 2, 'stroke-opacity': 0.7, 'stroke-linecap': 'round' });
			if (o.active !== false) s += flowLayers(d, [o.color || '#4cc3ff', accent2], dir, dur * 0.8, 1.8, 4, 0.95);
			s += circ(x1, y1, 3.2, V('pill-bg', 'rgba(10,22,40,.72)'), { st: 'stroke:' + (o.cableColor || '#4d5d79') + ';stroke-width:1.4' }) +
				circ(x2, y2, 3.2, V('pill-bg', 'rgba(10,22,40,.72)'), { st: 'stroke:' + (o.cableColor || '#4d5d79') + ';stroke-width:1.4' });
		} else if (type === 'rf') {
			var col = o.color || (qt ? TONE[qt].v : V('accent2', '#1fe0cf'));
			var ang = Math.atan2(dy, dx) * 180 / Math.PI, spread = Math.min(46, L0 * 0.22), nxr = -dy / L0, nyr = dx / L0;
			s += '<defs>' + lg(id + 'bg', x1, y1, x2, y2, [[0, col, 0.26], [1, col, 0.02]], 'userSpaceOnUse') + '</defs>';
			s += path('M' + n2(x1) + ',' + n2(y1) + ' L' + n2(x2 + nxr * spread) + ',' + n2(y2 + nyr * spread) + ' L' + n2(x2 - nxr * spread) + ',' + n2(y2 - nyr * spread) + ' Z', 'url(#' + id + 'bg)', { 'pointer-events': 'none' });
			s += path(d, 'none', { st: 'stroke:' + col + ';stroke-width:1.2;stroke-opacity:.5;stroke-dasharray:2 4' });
			if (o.active !== false) s += flowLayers(d, [col, col], dir, dur * 1.2, 2.2, 2, 0.9);
			var sets = dir === 'both' ? [[x1, y1, ang], [x2, y2, ang + 180]] : (dir === 'rev' ? [[x2, y2, ang + 180]] : [[x1, y1, ang]]);
			if (o.active !== false) {
				for (var si = 0; si < sets.length; si++) {
					for (i = 0; i < 4; i++) {
						s += g({ transform: 'translate(' + n2(sets[si][0]) + ',' + n2(sets[si][1]) + ') rotate(' + n2(sets[si][2]) + ')' },
							g({ cls: 'ta-wave', st: '--d:' + n2(Math.max(10, L0 - 16)) + 'px;animation-delay:' + n2(i * 0.6 + si * 0.3) + 's;animation-duration:2.4s' },
								path('M2,-13 A15,15 0 0 1 2,13', 'none', { st: 'stroke:' + col + ';stroke-width:2;stroke-linecap:round' }) +
								path('M1,-8.4 A10,10 0 0 1 1,8.4', 'none', { st: 'stroke:' + col + ';stroke-width:1.6;stroke-linecap:round;stroke-opacity:.7' })));
					}
				}
			}
			s += circ(x1, y1, 4, col, { cls: 'ta-breath' }) + circ(x1, y1, 7, 'none', { cls: 'ta-ring', st: 'stroke:' + col + ';stroke-width:1.3' });
			s += circ(x2, y2, 3.4, col, { opacity: 0.8 });
		} else if (type === 'wifi') {
			var wq = q == null ? 0.85 : q, wc = o.color || (q == null ? V('accent', '#37a6ff') : TONE[toneOfQ(wq)].v);
			var connecting = o.state === 'connecting';
			if (connecting) wc = V('warn', '#ffb83d');
			var lw = 1.3 + wq * 1.3, lop = 0.3 + wq * 0.5;
			var pat = wq >= 0.75 ? null : (wq >= 0.45 ? '9 4' : '4 7');
			var baseL = path(d, 'none', { 'stroke-linecap': 'round', 'stroke-dasharray': pat, st: 'stroke:' + wc + ';stroke-width:' + n2(lw) + ';stroke-opacity:' + n2(lop) });
			s += g({ cls: wq < 0.4 ? 'ta-jit' : null }, baseL + (wq < 0.4 ? path(d, 'none', { cls: 'ta-flick', st: 'stroke:' + wc + ';stroke-width:1;stroke-dasharray:2 9;stroke-linecap:round' }) : ''));
			/* 两端 WiFi 弧 */
			var lit = connecting ? 3 : (wq < 0.34 ? 1 : (wq < 0.67 ? 2 : 3)), rr = [9, 15.5, 22], ends2 = [[x1, y1, a0], [x2, y2, a1]], ei, ri, a, ax, ay, fs2;
			for (ei = 0; ei < 2; ei++) {
				fs2 = '';
				for (ri = 0; ri < 3; ri++) {
					a = 38 * Math.PI / 180; ax = rr[ri] * Math.cos(a); ay = rr[ri] * Math.sin(a);
					fs2 += path('M' + n2(ax) + ',' + n2(-ay) + ' A' + rr[ri] + ',' + rr[ri] + ' 0 0 1 ' + n2(ax) + ',' + n2(ay), 'none', {
						cls: (ri < lit) ? 'ta-wf' : null, 'stroke-linecap': 'round',
						st: 'stroke:' + wc + ';stroke-width:2;' + (ri < lit ? 'animation-delay:' + n2(ri * 0.24 + ei * 0.9) + 's;animation-duration:' + (connecting ? 0.9 : 1.8) + 's' : 'opacity:.15')
					});
				}
				s += g({ transform: 'translate(' + n2(ends2[ei][0]) + ',' + n2(ends2[ei][1]) + ') rotate(' + n2(ends2[ei][2]) + ')' }, fs2);
			}
			/* 握手：两端光点向中点汇合 + 爆发环 */
			var P01 = [(x1 + c1[0]) / 2, (y1 + c1[1]) / 2], P12 = [(c1[0] + c2[0]) / 2, (c1[1] + c2[1]) / 2], P23 = [(c2[0] + x2) / 2, (c2[1] + y2) / 2];
			var P012 = [(P01[0] + P12[0]) / 2, (P01[1] + P12[1]) / 2], P123 = [(P12[0] + P23[0]) / 2, (P12[1] + P23[1]) / 2];
			var dA = 'M' + n2(x1) + ',' + n2(y1) + ' C' + n2(P01[0]) + ',' + n2(P01[1]) + ' ' + n2(P012[0]) + ',' + n2(P012[1]) + ' ' + n2(mid[0]) + ',' + n2(mid[1]);
			var dB = 'M' + n2(x2) + ',' + n2(y2) + ' C' + n2(P23[0]) + ',' + n2(P23[1]) + ' ' + n2(P123[0]) + ',' + n2(P123[1]) + ' ' + n2(mid[0]) + ',' + n2(mid[1]);
			var hl = n2(len / 2), hdur = connecting ? 1.6 : 3.4, hs = '';
			var hp = ['', ''];
			[dA, dB].forEach(function (dd, k2) {
				hp[k2] = path(dd, 'none', { cls: 'ta-hsd', 'stroke-linecap': 'round', 'stroke-dasharray': '7 99999', st: 'stroke:' + wc + ';stroke-width:8;stroke-opacity:.25;--l:' + hl + 'px;animation-duration:' + hdur + 's' }) +
					path(dd, 'none', { cls: 'ta-hsd', 'stroke-linecap': 'round', 'stroke-dasharray': '3.6 99999', st: 'stroke:#fff;stroke-width:3.6;--l:' + hl + 'px;animation-duration:' + hdur + 's' });
			});
			s += hp[0] + hp[1];
			s += circ(mid[0], mid[1], 8, 'none', { cls: 'ta-burst', st: 'stroke:' + wc + ';stroke-width:1.6;animation-duration:' + hdur + 's' });
			s += circ(mid[0], mid[1], 2.6, wc, { cls: 'ta-breath' });
			/* 数据点 */
			if (!connecting && o.active !== false) s += g({ st: 'opacity:' + n2(0.35 + 0.65 * wq) }, flowLayers(d, [wc, wc], dir, dur * 1.1, 2.2, 3, 1));
		}
		if (o.label) {
			var lb = badge(o.label, o.labelTone || (qt || 'info'), { fs: o.labelFs || 10.5 });
			var ly = type === 'wifi' ? mid[1] - 20 : mid[1] - lb.h / 2;
			s += g({ transform: 'translate(' + n2(mid[0] - lb.w / 2) + ',' + n2(ly) + ')' }, lb.svg);
		}
		var hit = o.hit ? path(d, 'none', { cls: 'ta-hitline', stroke: 'transparent', 'stroke-width': 20, 'stroke-linecap': 'round', 'data-ta': 'link', 'data-id': id, tabindex: 0 }) : '';
		var minx = Math.min(x1, x2), miny = Math.min(y1, y2);
		return pack(g({ cls: cls, id: id, 'data-ta-link': id }, s + hit), Math.abs(dx), Math.abs(dy), { start: { x: x1, y: y1 }, end: { x: x2, y: y2 }, mid: { x: n2(mid[0]), y: n2(mid[1]) } }, { mid: { x: n2(mid[0]), y: n2(mid[1]) }, len: n2(len), d: d, id: id, type: type });
	}


	/*__EXPORT__*/
	var API = {
		version: VERSION, css: CSS, defs: defs, imgBase: IMGBASE,
		router: router, module5g: module5g, tower: tower, cloud: cloud, upRouter: upRouter, onu: onu, usbNic: usbNic, phone: phone, arcFan: arcFan, badge: badge, signalBars: signalBars, meter: meter, client: client, link: link, guessKind: guessKind, kinds: KINDS,
		carriers: CARRIERS, carrierKey: carrierKey, quality: quality, ranges: RANGES, models: RM,
		vars: 'ink dim line cable cable-edge accent accent2 ok warn bad glow plat shadow rim tile1 tile2 tile-edge gi cloud-a cloud-b cloud-edge pill-bg pill-ink font'.split(' '),
		/* 给 root 内带 data-fb 的 <image> 挂载入失败回退: 显示同 id 的矢量兜底, 隐藏图片 */
		bindFallback: function (rt) {
			if (!rt || !rt.querySelectorAll) return;
			var im = rt.querySelectorAll('image[data-fb]'), i;
			for (i = 0; i < im.length; i++) {
				if (im[i]._fbBound) continue;
				im[i]._fbBound = 1;
				im[i].addEventListener('error', function () {
					var fid = this.getAttribute('data-fb'), d = root.document, f = null;
					if (fid && d) { f = d.getElementById(fid); if (f) f.style.display = ''; }
					this.style.display = 'none';
				});
			}
		},
		inject: function (doc) {
			doc = doc || root.document;
			if (!doc || doc.getElementById('ta-css')) return;
			var st = doc.createElement('style'); st.id = 'ta-css'; st.appendChild(doc.createTextNode(CSS));
			(doc.head || doc.documentElement).appendChild(st);
		}
	};
	/* imgBase 可在运行时改写（gallery 用）：router() 每次读取 API.imgBase */
	root.MwrtTopoArt = API;
}(window));
