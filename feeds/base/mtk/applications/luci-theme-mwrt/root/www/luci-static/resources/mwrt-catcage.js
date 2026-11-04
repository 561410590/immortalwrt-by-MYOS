/* mwrt-catcage.js —— 侧栏「笼中猫」互动游戏的控制层(s90, 线程 C2)
 *
 * 位置: argon/aurora 侧栏 .main-left 里, 菜单 ul.nav 之下、.mwrt-sb-ver(固件版本块)之上的空闲区。
 * 本文件负责: 空闲区测量与自适应、笼子位移物理(弹簧-阻尼)、待机冲撞、悬停/触摸呼救、
 *            6 次点击破笼、猫跳出撒娇消失、调用 MwrtUnlock.open()、取消后复位砸笼、
 *            已解锁态的布偶猫蹦跳、键盘与无障碍、生命周期。
 * 画面全部交给 window.MwrtCatArt(mwrt-cat-art.js, 线程 C1); 美术库没加载或抛异常时用本文件里的
 * 极简占位绘制兜底(物理与时间线不依赖美术)。
 *
 * 状态(G.st):
 *   wait     等 unlock_state 判定, 区域不可见(避免刷新后先闪笼子再变玩偶)
 *   off      不适用(非 argon / 判定失败且无提示位): 整块 display:none
 *   idle     锁定态待机: 踱步 / 打哈欠 / 不定时冲撞
 *   plead    悬停或轻触: 停止冲撞, 泪眼呼救, 光标=锤子
 *   freed    第 6 次敲击后: 笼子爆散 → 猫跳出 → 放大撒娇 → 化作星光
 *   dialog   已调用 MwrtUnlock.open(), 等认证结果
 *   recage   认证框被关掉: 新笼子"哐"一声砸下来罩住猫
 *   success  认证成功(页面马上刷新)
 *   plush    已解锁态: 布偶猫蹦跳; s90 第四轮起可以用鼠标/手指抓起来拖着走(被抓摆动、松手抛出落地, 物理在 mwrt-cat-art.js 的 MwrtDrag)
 *
 * 约束(自检用几何断言验证): 空闲区高度 < 56px 整块隐藏并停掉动画循环; 笼子位移(含旋转后的包围盒)
 * 始终在空闲区矩形内; 空闲区本身 pointer-events:none, 只有笼子/玩偶的包围盒可点;
 * 空闲区靠 flex:1 1 auto + height:0 + min-height:0 取"剩余空间", 自身不撑高、不引起侧栏滚动条。
 *
 * ES5, 无依赖; 任何异常都 try/catch 吞掉, 不影响页面。 */
(function () {
	'use strict';

	if (window.__mwrtCatCage) return;
	window.__mwrtCatCage = true;
	try { if (window.top !== window.self) return; } catch (e) { return; }

	var ML = document.querySelector('.main .main-left');
	if (!ML) return;                       /* 不是 argon 主题: 整块 no-op */

	var SELF = document.currentScript;
	var STATE_URL = (SELF && SELF.getAttribute('data-state')) || '';

	/* ───────────────────────── 常量 ───────────────────────── */
	var CW0 = 160, CH0 = 150;              /* 笼子逻辑尺寸(逻辑单位 1 = 笼宽 160 时的 1px) */
	var MIN_ZH = 56, SHOW_ZH = 60;         /* 隐藏阈值 / 重新显示阈值(滞回, 避免菜单动画途中来回闪); 56-120 用紧凑尺寸 */
	var MIN_ZW = 120, MAX_CH = 156;
	var GAP = 4, MARGIN = 3;               /* 笼底离空闲区底边 / 笼子离区域边界的最小边距(px) */
	var MARGIN_T = 8;                      /* s90r3: 笼顶离空闲区上沿(即离菜单末项「退出登录」)的最小间距(px), 要求 >= 6 */
	var NHITS = 6, HIT_GAP = 0.12;
	var KC = 0.98;                         /* 笼内猫相对笼子的缩放(猫 v2 身高 96 逻辑单位, 头比旧猫大 20%; 笼内高 103) */
	var GEO = { l: 19, r: 141, floor: 129, ceil: 26 };   /* 笼内活动范围(逻辑单位, 笼左上为原点) */
	var CAT_HW = 34;                       /* 笼内猫半宽(逻辑单位, 已乘 KC 后的视觉半宽) */
	var PLUSH_W = 62, PLUSH_H = 62;        /* 玩偶 v2: 站立身高 60、含尾巴宽约 62(逻辑单位, s=1) */
	var PLUSH_SMAX = 1.1;                  /* 玩偶缩放上限: 猫最高约 66px("小一点") */
	var PEEK_POSES = { idle: 1, sit: 1, yawn: 1, plead: 1, cute: 1, clap: 1, scared: 1, wave: 1 };   /* 这些姿态在前景栏杆之后再补画一次脸, 让脸从栏杆缝里露出来 */
	var K_HITS = 'mwrt-cat-hits', K_CELE = 'mwrt-cat-celebrate', K_HINT = 'mwrt-cat-unlocked';
	var UI_FONT = '"PingFang SC","Microsoft YaHei","Hiragino Sans GB",sans-serif';

	/* ───────────────────────── 小工具 ───────────────────────── */
	function rnd(a, b) { return a + Math.random() * (b - a); }
	function rint(a, b) { return a + Math.floor(Math.random() * (b - a + 1)); }
	function pick(a) { return a[Math.floor(Math.random() * a.length)]; }
	function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
	function lerp(a, b, t) { return a + (b - a) * t; }
	function eOut(t) { t = clamp(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }
	function eIn(t) { t = clamp(t, 0, 1); return t * t * t; }
	function eIO(t) { t = clamp(t, 0, 1); return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2; }
	function eBack(t) { t = clamp(t, 0, 1); var c1 = 1.70158, c3 = c1 + 1; return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2); }
	function sgn(v) { return v < 0 ? -1 : 1; }
	function ssGet(k) { try { return sessionStorage.getItem(k); } catch (e) { return null; } }
	function ssSet(k, v) { try { sessionStorage.setItem(k, v); } catch (e) {} }
	function ssDel(k) { try { sessionStorage.removeItem(k); } catch (e) {} }
	function lsGet(k) { try { return localStorage.getItem(k); } catch (e) { return null; } }
	function lsSet(k, v) { try { localStorage.setItem(k, v); } catch (e) {} }

	function art() { return (!G.artBroken && window.MwrtCatArt) || null; }

	/* ───────────────────────── 全局状态 ───────────────────────── */
	var G = {
		st: 'wait', clock: 0, freeze: 0, fastUntil: 0,
		hits: 0, lastHit: -9, hover: false, hoverLeave: 0, touchArmed: false, kbFocus: false, refocus: false,
		ptr: null, shake: 0, flash: 0, flashMax: 0.3,
		visible: false, running: false, manual: false, artBroken: false,
		unlocked: null, errs: 0, dead: false, lastTouch: 0
	};
	var L = { zw: 0, zh: 0, dpr: 1, cw: 0, ch: 0, cs: 1, restL: 0, ground: 0, dark: true, ok: false,
		pS: 1, pw: 0, ph: 0 };
	/* 笼子物理: dx/dy 为相对静止位置的位移(dy<=0 表示离地), rot 为撞击摆动, tilt 为破坏后的永久歪斜(弧度) */
	var C = { dx: 0, dy: 0, vx: 0, vy: 0, rot: 0, vrot: 0, tilt: 0, tiltT: 0, shake: 0, sq: 0, vsq: 0,
		exploded: false, dropping: false };
	var cg = null;                         /* 当前笼子破坏状态(符合 drawCage 的 c 字段约定) */
	/* 笼内猫 */
	var K = { x: 80, face: 1, mode: 'idle', mt: 1, pt: 0, pose: 'idle', sub: '', tx: 80, offy: 0, sx: 1, sy: 1, rot: 0,
		vis: true, nextCharge: 3.5, blink: 0, bt: 2, cd: null, hp: 0, look: { x: 0, y: 0.15 } };
	/* 跳出笼子后的猫(世界坐标, 脚底锚点) */
	var W = { vis: false, x: 0, y: 0, s: 1, k: 1, pose: 'scared', sx: 1, sy: 1, rot: 0, face: 1, ph: 'wait', t: 0,
		x0: 0, y0: 0, x1: 0, y1: 0, H: 0, T: 0.8, kF: 1.7, kFly: 1, hpT: 0 };
	/* 布偶猫 */
	var P = { x: 0, y: 0, vy: 0, face: 1, mode: 'sit', mt: 1, pose: 'sit', ph: 0, sx: 1, sy: 1, rot: 0, tx: 0, x0: 0, T: 1,
		H: 0, t: 0, react: 0, flip: false, hpT: 0, vis: false };
	var hammers = [];                      /* 正在挥动的锤子 */
	var sched = [];                        /* 按游戏时钟触发的定时回调 */
	var FXA = [], FXB = [];                /* 粒子: 美术库的池 / 本文件兜底池 */
	var DR = null, drAtt = null, drS = 0, drZW = -1, drZH = -1, drG = -1, drFail = false;   /* 玩偶拖拽: 控制器 / 输入层 / 已按哪个缩放建的 / 已同步的空闲区尺寸 / 建失败就不再试 */
	var DRO = { t: 0, dark: true };        /* 传给 DR.render 的复用对象(不在帧里新建) */
	var pending = null;                    /* 布局/皮肤脏标记 */

	/* ───────────────────────── DOM 与样式 ───────────────────────── */
	var zone, cv, cx, hit, hamIcon, live;
	var CSS = [
		'.main .main-left .mwrt-cat-zone{order:50!important;flex:1 1 auto!important;height:0;min-height:0!important;max-height:none;position:relative!important;',
		'pointer-events:none!important;overflow:hidden;margin:0!important;padding:0!important;border:0!important;background:none!important;',
		'-webkit-tap-highlight-color:transparent;box-sizing:border-box;display:block}',
		'.main .main-left .mwrt-cat-zone.mc-off{visibility:hidden}',
		'.main .main-left .mwrt-cat-zone.mc-none{display:none!important}',
		'.mwrt-cat-zone canvas{position:absolute;left:0;top:0;display:block;pointer-events:none!important;margin:0;padding:0}',
		'.mwrt-cat-zone .mwrt-cat-hit{position:absolute;left:0;top:0;width:10px;height:10px;pointer-events:auto!important;outline:none;border-radius:14px;',
		'-webkit-tap-highlight-color:transparent;touch-action:manipulation;-webkit-user-select:none;user-select:none;background:transparent;display:none}',
		'.mwrt-cat-zone .mwrt-cat-hit.mc-on{display:block}',
		'.mwrt-cat-zone .mwrt-cat-hit.mc-kb{box-shadow:0 0 0 2px #20b7d5 inset,0 0 10px rgba(32,183,213,.55)}',
		'.mwrt-cat-zone .mwrt-cat-ham{position:absolute;left:0;top:0;width:34px;height:34px;pointer-events:none!important;display:none;',
		'filter:drop-shadow(0 2px 3px rgba(0,0,0,.45));animation:mwrt-cat-bob .7s ease-in-out infinite}',
		'.mwrt-cat-zone .mwrt-cat-ham.mc-on{display:block}',
		'.mwrt-cat-zone .mwrt-cat-ham svg{display:block;width:100%;height:100%}',
		'@keyframes mwrt-cat-bob{0%,100%{margin-top:0;margin-left:0}50%{margin-top:-4px;margin-left:2px}}',
		'.mwrt-cat-live{position:absolute!important;width:1px;height:1px;overflow:hidden;clip:rect(0 0 0 0);white-space:nowrap}'
	].join('');

	function buildDom() {
		var st = document.createElement('style');
		st.id = 'mwrt-cat-style';
		st.appendChild(document.createTextNode(CSS));
		document.head.appendChild(st);

		zone = document.createElement('div');
		zone.className = 'mwrt-cat-zone mc-off';
		zone.setAttribute('data-mwrt-cat', 'wait');
		cv = document.createElement('canvas');
		cv.setAttribute('aria-hidden', 'true');
		cx = cv.getContext('2d');
		hit = document.createElement('div');
		hit.className = 'mwrt-cat-hit';
		hit.setAttribute('role', 'button');
		hit.setAttribute('tabindex', '0');
		hit.setAttribute('aria-label', '被困的小猫，点击帮它破笼，按 Enter 连击');
		hamIcon = document.createElement('div');
		hamIcon.className = 'mwrt-cat-ham';
		hamIcon.setAttribute('aria-hidden', 'true');
		live = document.createElement('div');
		live.className = 'mwrt-cat-live';
		live.setAttribute('role', 'status');
		live.setAttribute('aria-live', 'polite');
		zone.appendChild(cv);
		zone.appendChild(hit);
		zone.appendChild(hamIcon);
		zone.appendChild(live);
		ML.appendChild(zone);
	}

	function say(msg) { try { if (live) live.textContent = msg; } catch (e) {} }

	/* ───────────────────────── 美术适配: 占位绘制 + 兜底粒子 ───────────────────────── */
	/* 约定(见 NOTES.md): 传给美术库的坐标都是 canvas 像素, 猫/玩偶锚点=脚底中心, 笼子 c.x/c.y 为左上角。 */
	function phCat(c, o) {
		c.save();
		c.translate(o.x, o.y);
		c.rotate(o.rot || 0);
		c.scale((o.flip ? -1 : 1) * (o.sx || 1) * o.s, (o.sy || 1) * o.s);
		var pose = o.pose;
		c.lineWidth = 2.2; c.strokeStyle = '#5a3a24'; c.lineJoin = 'round';
		/* 尾巴 */
		c.beginPath(); c.moveTo(-18, -16); c.quadraticCurveTo(-42, -22 + Math.sin(o.t * 5) * 6, -34, -46);
		c.lineWidth = 6; c.strokeStyle = '#e8924a'; c.lineCap = 'round'; c.stroke();
		c.lineWidth = 2.2; c.strokeStyle = '#5a3a24';
		/* 身体 */
		c.fillStyle = '#f6e6cf';
		c.beginPath(); c.ellipse(0, -20, 24, 19, 0, 0, 6.283); c.fill(); c.stroke();
		c.fillStyle = '#e8924a';
		c.beginPath(); c.ellipse(-9, -30, 7, 4, 0.5, 0, 6.283); c.fill();
		/* 头 */
		c.fillStyle = '#f6e6cf';
		c.beginPath(); c.moveTo(-20, -56); c.lineTo(-16, -78); c.lineTo(-4, -66); c.closePath(); c.fill(); c.stroke();
		c.beginPath(); c.moveTo(20, -56); c.lineTo(16, -78); c.lineTo(4, -66); c.closePath(); c.fill(); c.stroke();
		c.beginPath(); c.ellipse(0, -52, 24, 21, 0, 0, 6.283); c.fill(); c.stroke();
		c.fillStyle = '#e8924a';
		c.beginPath(); c.ellipse(0, -66, 9, 4, 0, 0, 6.283); c.fill();
		/* 眼睛 */
		var lx = ((o.look && o.look.x) || 0) * 2.5, ly = ((o.look && o.look.y) || 0) * 2;
		c.fillStyle = '#2a1a10'; c.strokeStyle = '#2a1a10'; c.lineWidth = 2.4;
		if (pose === 'bonk' || pose === 'scared') {          /* > < */
			c.beginPath(); c.moveTo(-14, -58); c.lineTo(-7, -54); c.lineTo(-14, -50); c.moveTo(14, -58); c.lineTo(7, -54); c.lineTo(14, -50); c.stroke();
		} else if (pose === 'dizzy') {
			c.beginPath(); c.moveTo(-14, -58); c.lineTo(-7, -50); c.moveTo(-7, -58); c.lineTo(-14, -50);
			c.moveTo(14, -58); c.lineTo(7, -50); c.moveTo(7, -58); c.lineTo(14, -50); c.stroke();
		} else if (pose === 'cute' || o.blink > 0 || pose === 'yawn') {
			c.beginPath(); c.arc(-10, -54, 5, 3.4, 6.0); c.arc(10, -54, 5, 3.4, 6.0); c.stroke();
		} else {
			c.beginPath(); c.arc(-10 + lx, -54 + ly, pose === 'plead' ? 6.5 : 4.6, 0, 6.283); c.arc(10 + lx, -54 + ly, pose === 'plead' ? 6.5 : 4.6, 0, 6.283); c.fill();
			c.fillStyle = '#fff'; c.beginPath(); c.arc(-11 + lx, -56 + ly, 1.8, 0, 6.283); c.arc(9 + lx, -56 + ly, 1.8, 0, 6.283); c.fill();
		}
		/* 鼻口 */
		c.fillStyle = '#f08aa0'; c.beginPath(); c.arc(0, -47, 2.4, 0, 6.283); c.fill();
		c.strokeStyle = '#5a3a24'; c.lineWidth = 1.6; c.beginPath();
		if (pose === 'yawn') c.ellipse(0, -41, 5, 6, 0, 0, 6.283);
		else { c.moveTo(0, -45); c.lineTo(0, -42); c.moveTo(-5, -40); c.quadraticCurveTo(-2.5, -37, 0, -42); c.quadraticCurveTo(2.5, -37, 5, -40); }
		c.stroke();
		/* 前爪(鼓掌/呼救/撒娇抬起) */
		c.fillStyle = '#fff'; c.strokeStyle = '#5a3a24'; c.lineWidth = 2;
		var up = (pose === 'clap' || pose === 'plead' || pose === 'cute' || pose === 'wave') ? 1 : 0;
		c.beginPath(); c.arc(-16, up ? -34 - Math.abs(Math.sin(o.t * 12)) * 4 * (pose === 'clap' ? 1 : 0) : -4, 6, 0, 6.283); c.fill(); c.stroke();
		c.beginPath(); c.arc(16, up ? -34 - Math.abs(Math.sin(o.t * 12)) * 4 * (pose === 'clap' ? 1 : 0) : -4, 6, 0, 6.283); c.fill(); c.stroke();
		c.restore();
	}

	var PH_BAR_COL = ['#7f95ad', '#8ea3ba'];
	function phCageDraw(c, o, front) {
		c.save();
		var w = o.w, h = o.h, x = o.x, y = o.y, i, n = o.bars ? o.bars.length : 7;
		var bx0 = x + w * 0.12, bx1 = x + w * 0.88, top = y + h * 0.1, bot = y + h * 0.86;
		if (!front) {
			c.fillStyle = 'rgba(40,52,72,.35)';
			c.fillRect(bx0, top, bx1 - bx0, bot - top);
			c.restore();
			return;
		}
		c.lineCap = 'round';
		/* 栏杆 */
		for (i = 0; i < n; i++) {
			var b = o.bars[i];
			if (b.gone) continue;
			var px = bx0 + (bx1 - bx0) * (i + 0.5) / n;
			c.strokeStyle = '#2f3b4d'; c.lineWidth = w * 0.05 + 2;
			c.beginPath(); c.moveTo(px, top); c.quadraticCurveTo(px + (b.bend || 0) * w * 0.12, (top + bot) / 2, px, bot); c.stroke();
			c.strokeStyle = PH_BAR_COL[i & 1]; c.lineWidth = w * 0.05;
			c.beginPath(); c.moveTo(px, top); c.quadraticCurveTo(px + (b.bend || 0) * w * 0.12, (top + bot) / 2, px, bot); c.stroke();
		}
		/* 顶与底 */
		c.fillStyle = '#4b5d77'; c.strokeStyle = '#2f3b4d'; c.lineWidth = 2;
		if (o.topDamage < 1) { c.fillRect(x + w * 0.06, y + h * 0.04, w * 0.88, h * 0.08); c.strokeRect(x + w * 0.06, y + h * 0.04, w * 0.88, h * 0.08); }
		c.fillRect(x, y + h * 0.84, w, h * 0.16); c.strokeRect(x, y + h * 0.84, w, h * 0.16);
		/* 挂锁 */
		if (o.lock < 3) {
			c.fillStyle = '#f2c14e';
			c.fillRect(x + w * 0.44, y + h * 0.62, w * (o.lock === 2 ? 0.06 : 0.12), h * 0.12);
		}
		if (o.flash > 0) { c.fillStyle = 'rgba(255,255,255,' + Math.min(1, o.flash) + ')'; c.fillRect(x, y, w, h); }
		c.restore();
	}

	function phPlush(c, o) {
		c.save();
		c.translate(o.x, o.y);
		c.rotate(o.rot || 0);
		c.scale((o.flip ? -1 : 1) * (o.sx || 1) * o.s, (o.sy || 1) * o.s);
		c.lineWidth = 2.4; c.strokeStyle = '#6b4a6a'; c.lineJoin = 'round';
		c.fillStyle = '#f2b9c9';
		c.beginPath(); c.ellipse(0, -26, 28, 24, 0, 0, 6.283); c.fill(); c.stroke();
		c.beginPath(); c.moveTo(-26, -66); c.lineTo(-22, -92); c.lineTo(-6, -76); c.closePath(); c.fill(); c.stroke();
		c.beginPath(); c.moveTo(26, -66); c.lineTo(22, -92); c.lineTo(6, -76); c.closePath(); c.fill(); c.stroke();
		c.beginPath(); c.ellipse(0, -60, 30, 26, 0, 0, 6.283); c.fill(); c.stroke();
		c.fillStyle = '#3b2a3a'; c.beginPath(); c.arc(-11, -62, 4.5, 0, 6.283); c.arc(11, -62, 4.5, 0, 6.283); c.fill();
		c.strokeStyle = '#6b4a6a'; c.lineWidth = 1.6; c.setLineDash([3, 3]); c.beginPath(); c.moveTo(0, -36); c.lineTo(0, -12); c.stroke(); c.setLineDash([]);
		c.restore();
	}

	/* 兜底粒子 */
	var FB = (function () {
		var pool = [], MAXP = 360;
		function d(v, def) { return v === undefined || v === null ? def : v; }
		function spawn(list, kind, x, y, o) {
			o = o || {};
			if (list.length >= MAXP) return null;
			var p = pool.length ? pool.pop() : {};
			p.k = kind; p.x = x; p.y = y; p.life = 0; p.txt = o.text || ''; p.shape = o.shape || ''; p.ground = o.ground || 0;
			p.c = o.color || null; p.rot = d(o.rot, 0); p.vr = d(o.vr, 0); p.ay = 0; p.drag = 0; p.bn = 0;
			p.vx = d(o.vx, 0); p.vy = d(o.vy, 0); p.sz = d(o.size, 8); p.max = d(o.life, 1);
			switch (kind) {
				case 'heart': p.vy = d(o.vy, -34 - rnd(0, 16)); p.vx = d(o.vx, rnd(-12, 12)); p.max = d(o.life, rnd(0.9, 1.3)); p.sz = d(o.size, rnd(6, 9)); break;
				case 'star': p.max = d(o.life, 0.8); p.ay = 40; p.vr = d(o.vr, rnd(-6, 6)); p.sz = d(o.size, rnd(4, 7)); break;
				case 'spark': p.max = d(o.life, rnd(0.25, 0.4)); p.ay = 600; p.drag = 2; p.sz = d(o.size, rnd(1.4, 2.4)); break;
				case 'dust': p.max = d(o.life, rnd(0.5, 0.8)); p.drag = 3; p.sz = d(o.size, rnd(4, 8)); break;
				case 'debris': p.max = d(o.life, 1.6); p.ay = 1100; p.bn = 2; p.sz = d(o.size, 14); break;
				case 'ring': p.max = d(o.life, 0.38); p.sz = d(o.size, 24); break;
				case 'text': p.max = d(o.life, 0.95); p.vy = d(o.vy, -26); p.sz = d(o.size, 18); break;
				case 'confetti': p.max = d(o.life, rnd(1.8, 2.6)); p.ay = 120; p.drag = 0.8; p.vr = d(o.vr, rnd(-9, 9)); p.sz = d(o.size, rnd(4, 7)); break;
				case 'poof': p.max = d(o.life, 0.7); p.drag = 2.5; p.sz = d(o.size, 14); break;
				case 'sweat': p.max = d(o.life, 0.7); p.ay = 200; p.sz = d(o.size, 4); break;
				case 'note': p.vy = d(o.vy, -28); p.max = d(o.life, 1.0); p.sz = d(o.size, 12); break;
				default: break;
			}
			list.push(p);
			return p;
		}
		function step(list, dt) {
			var i = list.length, p;
			while (i--) {
				p = list[i];
				p.life += dt;
				if (p.life >= p.max) { list[i] = list[list.length - 1]; list.pop(); pool.push(p); continue; }
				if (p.drag) { var f = Math.max(0, 1 - p.drag * dt); p.vx *= f; p.vy *= f; }
				p.vy += p.ay * dt;
				p.x += p.vx * dt; p.y += p.vy * dt; p.rot += p.vr * dt;
				if (p.k === 'debris' && p.ground && p.y > p.ground && p.vy > 0 && p.bn > 0) {
					p.y = p.ground; p.vy = -p.vy * 0.35; p.vx *= 0.6; p.vr *= 0.5; p.bn--;
				}
			}
		}
		function heart(c, s) {
			c.beginPath(); c.moveTo(0, s * 0.4);
			c.bezierCurveTo(-s * 1.2, -s * 0.3, -s * 0.6, -s * 1.1, 0, -s * 0.45);
			c.bezierCurveTo(s * 0.6, -s * 1.1, s * 1.2, -s * 0.3, 0, s * 0.4); c.closePath();
		}
		function star(c, s) {
			c.beginPath();
			for (var i = 0; i < 10; i++) { var r = i & 1 ? s * 0.45 : s, a = -1.5708 + i * 0.6283; c.lineTo(Math.cos(a) * r, Math.sin(a) * r); }
			c.closePath();
		}
		function draw(c, list) {
			var i, p, u, a;
			for (i = 0; i < list.length; i++) {
				p = list[i];
				u = p.life / p.max;
				a = u < 0.65 ? 1 : 1 - (u - 0.65) / 0.35;
				c.globalAlpha = a < 0 ? 0 : a;
				switch (p.k) {
					case 'heart': c.save(); c.translate(p.x, p.y); c.scale(1 + 0.15 * Math.sin(p.life * 9), 1 + 0.15 * Math.sin(p.life * 9)); heart(c, p.sz); c.fillStyle = p.c || '#ff5c8a'; c.fill(); c.lineWidth = 1.4; c.strokeStyle = '#a3264f'; c.stroke(); c.restore(); break;
					case 'star': c.save(); c.translate(p.x, p.y); c.rotate(p.rot); star(c, p.sz); c.fillStyle = p.c || '#ffd84a'; c.fill(); c.lineWidth = 1.2; c.strokeStyle = '#a8761a'; c.stroke(); c.restore(); break;
					case 'spark': c.strokeStyle = p.c || '#ffe28a'; c.lineWidth = p.sz; c.lineCap = 'round'; c.beginPath(); c.moveTo(p.x, p.y); c.lineTo(p.x - p.vx * 0.035, p.y - p.vy * 0.035); c.stroke(); break;
					case 'dust': c.fillStyle = p.c || 'rgba(190,190,200,.8)'; c.beginPath(); c.arc(p.x, p.y, p.sz * (0.6 + u), 0, 6.283); c.fill(); break;
					case 'ring': c.strokeStyle = p.c || '#fff'; c.lineWidth = 3 * (1 - u) + 0.5; c.beginPath(); c.arc(p.x, p.y, p.sz * (0.25 + eOut(u)), 0, 6.283); c.stroke(); break;
					case 'poof': c.fillStyle = p.c || '#fff6e8'; c.beginPath(); c.arc(p.x, p.y, p.sz * (0.5 + eOut(u) * 1.2), 0, 6.283); c.fill(); break;
					case 'sweat': c.fillStyle = '#7cc8ff'; c.beginPath(); c.ellipse(p.x, p.y, p.sz * 0.7, p.sz, 0, 0, 6.283); c.fill(); break;
					case 'note': c.fillStyle = p.c || '#ff9bd0'; c.font = 'bold ' + Math.round(p.sz) + 'px ' + UI_FONT; c.fillText('♪', p.x, p.y); break;
					case 'confetti': c.save(); c.translate(p.x, p.y); c.rotate(p.rot); c.scale(1, Math.cos(p.life * 8 + p.x)); c.fillStyle = p.c || pick(['#ff6b8a', '#ffd84a', '#5ad1ff', '#8cf07a', '#c28bff']); c.fillRect(-p.sz, -p.sz * 0.5, p.sz * 2, p.sz); c.restore(); break;
					case 'debris':
						c.save(); c.translate(p.x, p.y); c.rotate(p.rot);
						c.fillStyle = p.shape === 'lock' ? '#f2c14e' : (p.shape === 'base' ? '#3d4a5e' : '#7f95ad'); c.strokeStyle = '#2f3b4d'; c.lineWidth = 1.6;
						if (p.shape === 'bar') { c.fillRect(-2.5, -p.sz, 5, p.sz * 2); c.strokeRect(-2.5, -p.sz, 5, p.sz * 2); }
						else { c.fillRect(-p.sz * 0.5, -p.sz * 0.35, p.sz, p.sz * 0.7); c.strokeRect(-p.sz * 0.5, -p.sz * 0.35, p.sz, p.sz * 0.7); }
						c.restore(); break;
					case 'text':
						c.save(); c.translate(p.x, p.y);
						var sc = 1 + 0.5 * (1 - eOut(p.life / 0.16)); c.scale(sc, sc); c.rotate(p.rot);
						c.font = '900 ' + Math.round(p.sz) + 'px ' + UI_FONT; c.textAlign = 'center'; c.textBaseline = 'middle';
						c.lineJoin = 'round'; c.lineWidth = Math.max(3, p.sz * 0.22); c.strokeStyle = '#5a2e10';
						c.strokeText(p.txt, 0, 0); c.fillStyle = p.c || '#ffe36b'; c.fillText(p.txt, 0, 0);
						c.restore(); break;
					default: break;
				}
			}
			c.globalAlpha = 1;
		}
		return { spawn: spawn, step: step, draw: draw };
	}());

	/* 本文件的选项名 → 美术库粒子字段名 */
	function artOpt(kind, o) {
		var r = {}, k;
		if (!o) return r;
		for (k in o) { if (o.hasOwnProperty(k)) r[k] = o[k]; }
		if (r.color !== undefined) { r.col = r.color; delete r.color; }
		if (r.ground !== undefined) { r.floor = r.ground; delete r.ground; }
		if (r.shape === 'top') r.shape = 'roof';
		if (kind === 'debris') { r.wl = MARGIN + 6; r.wr = Math.max(MARGIN + 7, L.zw - MARGIN - 6); }   /* s90r3: 碎片碰到侧栏边缘反弹+略淡, 不被画布硬裁 */
		if (kind === 'ring' && r.size !== undefined) { r.r1 = r.size; r.r0 = 4; delete r.size; }
		return r;
	}
	function fxSpawn(kind, x, y, o) {
		var A = art();
		if (A && A.fx && typeof A.fx.spawn === 'function') {
			try { A.fx.spawn(FXA, kind, x, y, artOpt(kind, o)); return; } catch (e) { G.artBroken = true; }
		}
		FB.spawn(FXB, kind, x, y, o);
	}
	function fxStep(dt) {
		var A = art();
		if (A && A.fx && typeof A.fx.step === 'function') {
			try { A.fx.step(FXA, dt); } catch (e) { G.artBroken = true; FXA.length = 0; }
		}
		FB.step(FXB, dt);
	}
	function fxDraw() {
		var A = art();
		if (A && A.fx && typeof A.fx.draw === 'function') {
			try { A.fx.draw(cx, FXA, L.dark); } catch (e) { G.artBroken = true; FXA.length = 0; }
		}
		FB.draw(cx, FXB);
	}
	function fxCount() { return FXA.length + FXB.length; }
	function fxClear() { FXA.length = 0; FXB.length = 0; }

	/* 绘制适配 */
	function aCat(o) {
		var A = art();
		if (A && A.drawCat) { try { return A.drawCat(cx, o); } catch (e) { G.artBroken = true; } }
		phCat(cx, o);
	}
	function aCage(o, front) {
		var A = art();
		var fn = A && (front ? A.drawCageFront : A.drawCageBack);
		if (fn) { try { fn.call(A, cx, o); return; } catch (e) { G.artBroken = true; } }
		phCageDraw(cx, o, front);
	}
	function aPlush(o) {
		var A = art();
		if (A && A.drawPlush) { try { return A.drawPlush(cx, o); } catch (e) { G.artBroken = true; } }
		phPlush(cx, o);
	}
	function nBars() { var A = art(); return (A && A.CAGE && A.CAGE.bars) || 7; }

	/* 笼子 6 个破坏阶段(0=完好)。优先用美术库的 cageStage(n), 没有就自己拼。统一深拷贝成本地对象。 */
	function cloneStage(s) {
		var bars = [], i;
		for (i = 0; i < s.bars.length; i++) bars.push({ hp: s.bars[i].hp === undefined ? 1 : s.bars[i].hp, bend: s.bars[i].bend || 0, gone: !!s.bars[i].gone });
		return { bars: bars, lock: s.lock || 0, topDamage: s.topDamage || 0, baseDamage: s.baseDamage || 0, tilt: s.tilt || 0, flash: 0 };
	}
	function fbStage(n) {
		var N = nBars(), bars = [], i, mid = N >> 1;
		for (i = 0; i < N; i++) bars.push({ hp: 1, bend: 0, gone: false });
		var c = { bars: bars, lock: 0, topDamage: 0, baseDamage: 0, tilt: 0, flash: 0 };
		if (n >= 1) { bars[mid].bend = 0.8; bars[Math.min(N - 1, mid + 1)].bend = 0.5; bars[Math.max(0, mid - 1)].bend = -0.4; }
		if (n >= 2) { bars[mid].gone = true; }
		if (n >= 3) { c.lock = 2; }
		if (n >= 4) { c.topDamage = 1; c.tilt = 0.09; c.baseDamage = 0.3; }
		if (n >= 5) { bars[0].gone = true; bars[N - 2].gone = true; bars[1].bend = 0.8; c.baseDamage = 0.8; }
		if (n >= 6) { for (i = 0; i < N; i++) bars[i].gone = true; c.lock = 3; c.topDamage = 2; c.baseDamage = 1; }
		return c;
	}
	function stageState(n) {
		var A = art(), s = null;
		if (A && typeof A.cageStage === 'function') { try { s = A.cageStage(n); } catch (e) { G.artBroken = true; s = null; } }
		if (!s || !s.bars) s = fbStage(n);
		return cloneStage(s);
	}
	function barXl(i) {                       /* 栏杆在笼内的逻辑 x */
		var A = art();
		if (A && typeof A.barX === 'function') { try { var v = A.barX(i); if (typeof v === 'number' && v > -80 && v < 80) return v + 80; } catch (e) {} }
		var N = nBars();
		return GEO.l + (GEO.r - GEO.l) * (i + 0.5) / N;
	}

	/* ───────────────────────── 布局测量 ───────────────────────── */
	function parseRGB(s) {
		var m = /rgba?\(\s*([\d.]+)[ ,]+([\d.]+)[ ,]+([\d.]+)(?:[ ,/]+([\d.]+))?/.exec(s || '');
		return m ? [+m[1], +m[2], +m[3], m[4] === undefined ? 1 : +m[4]] : null;
	}
	function detectDark() {
		try {
			var cs = getComputedStyle(ML), c = parseRGB(cs.backgroundColor);
			if (!c || c[3] < 0.2) c = parseRGB(cs.backgroundImage);
			if (!c) return true;
			return (0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]) < 140;
		} catch (e) { return true; }
	}

	function measure() {
		pending = null;
		if (!zone) return;
		var r = zone.getBoundingClientRect();
		var zw = Math.floor(r.width), zh = Math.floor(r.height);
		var show;
		if (G.st === 'off') show = false;
		else if (L.ok) show = !(zh < MIN_ZH || zw < MIN_ZW);
		else show = !(zh < SHOW_ZH || zw < MIN_ZW);
		/* 滞回: 已显示时 >=56 保持, 隐藏时 >=60 才重新显示(上面两个分支已体现) */
		var wasOk = L.ok;
		L.ok = show;
		if (!show) { applyVisible(false); return; }
		var dpr = Math.min(window.devicePixelRatio || 1, 2);
		var changed = (zw !== L.zw || zh !== L.zh || dpr !== L.dpr);
		L.zw = zw; L.zh = zh; L.dpr = dpr;
		if (changed || !wasOk) {
			var cw = Math.floor(zw * 0.58);   /* s91: 笼子占空闲区宽度 0.8→0.58, 高度上限 220→156, 解锁前的笼子缩小约三成 */
			var ch = Math.min(MAX_CH, zh - MARGIN_T - GAP, cw * CH0 / CW0);
			ch = Math.max(40, Math.floor(ch));
			cw = Math.floor(ch * CW0 / CH0);
			L.cw = cw; L.ch = ch; L.cs = cw / CW0;
			L.restL = Math.floor((zw - cw) / 2);
			L.ground = zh - GAP;
			/* 玩偶尺寸 */
			var pS = Math.min(zh * 0.55 / PLUSH_H, PLUSH_SMAX, zw * 0.46 / PLUSH_W);
			L.pS = pS; L.pw = PLUSH_W * pS; L.ph = PLUSH_H * pS;
			L.pTopFlip = plushTop(pS, 'flip'); L.pTopHop = plushTop(pS, 'hop');
			cv.width = Math.round(zw * dpr); cv.height = Math.round(zh * dpr);
			cv.style.width = zw + 'px'; cv.style.height = zh + 'px';
			if (G.st !== 'wait') { fitCage(); fitPlush(); }
		}
		L.dark = detectDark();
		applyVisible(true);
	}

	function applyVisible(v) {
		var was = G.visible;
		G.visible = v && (G.st !== 'wait') && (G.st !== 'off');
		/* 先比较再赋值: 同值写 class 也会产生 MutationObserver 记录, 触发 measure 死循环 */
		var zcn = 'mwrt-cat-zone' + (G.visible ? '' : (G.st === 'off' ? ' mc-none' : ' mc-off'));
		if (zone.className !== zcn) zone.className = zcn;
		if (G.visible && !was) wake();
		if (!G.visible && was) { dragAbort(); stopLoop(); hit.className = 'mwrt-cat-hit'; hitRect.on = false; hamIcon.className = 'mwrt-cat-ham'; }
	}

	function queueMeasure() {
		if (pending) return;
		pending = window.requestAnimationFrame ? requestAnimationFrame(measure) : setTimeout(measure, 16);
	}

	/* ───────────────────────── 定时器(游戏时钟) ───────────────────────── */
	function after(sec, fn) { sched.push({ t: G.clock + sec, fn: fn }); }
	function runSched() {
		if (!sched.length) return;
		var i, due = [], keep = [];
		for (i = 0; i < sched.length; i++) { if (sched[i].t <= G.clock) due.push(sched[i]); else keep.push(sched[i]); }
		if (!due.length) return;
		sched = keep;
		due.sort(function (a, b) { return a.t - b.t; });
		for (i = 0; i < due.length; i++) { try { due[i].fn(); } catch (e) { G.errs++; } }
	}
	function fast(sec) { G.fastUntil = Math.max(G.fastUntil, G.clock + sec); }

	/* ───────────────────────── 几何 ───────────────────────── */
	function cageDims() { return { w: L.cw * (1 + C.sq * 0.6), h: L.ch * (1 - C.sq) }; }
	function cageAABB() {
		var d = cageDims(), a = C.rot, s = Math.sin(a), c = Math.cos(a), tl = C.tilt;
		var px = L.restL + L.cw / 2 + C.dx, py = L.ground + C.dy;
		var xs = [-d.w / 2, d.w / 2, -d.w / 2, d.w / 2], ys = [0, 0, -d.h, -d.h], i, X, Y, xk;
		var l = 1e9, r = -1e9, t = 1e9, b = -1e9;
		for (i = 0; i < 4; i++) {
			xk = xs[i] - tl * ys[i];                   /* 与美术库一致的横向斜切 x' = x - tilt*y */
			X = px + xk * c - ys[i] * s; Y = py + xk * s + ys[i] * c;
			if (X < l) l = X; if (X > r) r = X; if (Y < t) t = Y; if (Y > b) b = Y;
		}
		return { l: l, r: r, t: t, b: b, px: px, py: py, w: d.w, h: d.h, a: a };
	}
	function fitCage() {
		if (C.exploded) return;
		var b = cageAABB();
		if (b.l < MARGIN) { C.dx += MARGIN - b.l; if (C.vx < 0) C.vx = -C.vx * 0.35; }
		else if (b.r > L.zw - MARGIN) { C.dx -= b.r - (L.zw - MARGIN); if (C.vx > 0) C.vx = -C.vx * 0.35; }
		if (!C.dropping) {
			if (b.t < MARGIN_T) { C.dy += MARGIN_T - b.t; if (C.vy < 0) C.vy = 0; }
			var over = b.b - (L.zh - 1);
			if (over > 0) C.dy -= over;
		}
	}
	function cageToWorld(xl, yl) {
		var d = cageDims(), b = cageAABB(), cs = L.cs;
		var lx = xl - CW0 / 2, ly = yl - CH0;
		var X = (lx - C.tilt * ly) * cs * (d.w / L.cw), Y = ly * cs * (d.h / L.ch);
		var s = Math.sin(b.a), c = Math.cos(b.a);
		return { x: b.px + X * c - Y * s, y: b.py + X * s + Y * c };
	}
	function fitPlush() {
		var lo = L.pw / 2 + MARGIN, hi = L.zw - L.pw / 2 - MARGIN;
		if (P.x < lo) P.x = lo; else if (P.x > hi) P.x = hi;
	}
	/* 玩偶某跳姿在整个周期里脚底以上的最高像素(含翻转、耳尖、尾、星星), 来自美术库 plushBounds 的 worst 盒; 取不到就按身高估算 */
	function plushTop(sc, pose) {
		var A = art(), b, v = 0;
		if (A && typeof A.plushBounds === 'function') {
			try { b = A.plushBounds({ s: sc, pose: pose, noLift: true, worst: true }); v = -b.t; } catch (e) { v = 0; }
		}
		if (!(v > 0)) v = PLUSH_H * sc * (pose === 'flip' ? 1.45 : 1.15);
		return v;
	}
	function plushMaxH(flip) { return Math.max(0, L.ground - (flip ? L.pTopFlip : L.pTopHop) - MARGIN_T); }   /* 抛物线最大高度: 画布顶部给 worst 盒留足, 翻跟头不裁头 */

	/* ───────────────────────── 笼子物理 ───────────────────────── */
	function cageReset(stage) {
		C.dx = 0; C.dy = 0; C.vx = 0; C.vy = 0; C.rot = 0; C.vrot = 0; C.sq = 0; C.vsq = 0; C.shake = 0;
		C.exploded = false; C.dropping = false;
		cg = stageState(stage || 0);
		C.tiltT = cg.tilt; C.tilt = cg.tilt;
	}
	function cageStep(dt) {
		var k = 70, c = 7.0, g = 1800 * L.cs;
		if (!C.dropping) {
			C.vx += (-k * C.dx - c * C.vx) * dt; C.dx += C.vx * dt;
			if (C.dy < 0 || C.vy < 0) {
				C.vy += g * dt; C.dy += C.vy * dt;
				if (C.dy >= 0) {
					C.dy = 0;
					if (C.vy > 120) { C.sq = Math.max(C.sq, clamp(C.vy / 2200, 0.03, 0.12)); onCageThud(C.vy); }
					C.vy = -C.vy * 0.2; if (Math.abs(C.vy) < 40) C.vy = 0;
				}
			}
		} else {
			C.vy += g * 1.35 * dt; C.dy += C.vy * dt;
			if (C.dy >= 0) { C.dy = 0; C.dropping = false; var v = C.vy; C.vy = 0; onCageDropLanded(v); }
		}
		C.vrot += (-60 * C.rot - 5.5 * C.vrot) * dt; C.rot += C.vrot * dt;
		C.rot = clamp(C.rot, -0.14, 0.14);
		C.tilt += (C.tiltT - C.tilt) * Math.min(1, dt * 6);
		C.vsq += (-420 * C.sq - 22 * C.vsq) * dt; C.sq += C.vsq * dt;
		C.shake = Math.max(0, C.shake - dt * 2.2);
		fitCage();
	}
	function onCageThud(v) {
		var b = cageAABB();
		if (v > 400) { fxSpawn('dust', b.l + 10, L.ground, { vx: -40, vy: -20, size: 5 }); fxSpawn('dust', b.r - 10, L.ground, { vx: 40, vy: -20, size: 5 }); }
	}

	/* ───────────────────────── 笼内猫 AI ───────────────────────── */
	function catRange() { return [GEO.l + CAT_HW + 2, GEO.r - CAT_HW - 2]; }
	function catWorld(headUp) {                /* 笼内猫在 canvas 里的位置(脚底)/头部 */
		return cageToWorld(K.x, GEO.floor - K.offy - (headUp ? 44 : 0));
	}
	function setMode(m, t, sub) { K.mode = m; K.mt = t || 0; K.pt = 0; K.sub = sub || ''; }

	function pickIdle() {
		var r = Math.random(), rg = catRange();
		if (r < 0.5) {
			var tx = rnd(rg[0], rg[1]), tries = 0;
			while (Math.abs(tx - K.x) < 16 && tries++ < 5) tx = rnd(rg[0], rg[1]);
			K.tx = tx; setMode('walk', 0);
		} else if (r < 0.68) setMode('yawn', 1.9);
		else if (r < 0.86) setMode('sit', 2.4);
		else { K.look = { x: rnd(-1, 1), y: rnd(-0.3, 0.4) }; setMode('idle', rnd(0.9, 1.6)); }
	}

	function startCharge() {
		var r = Math.random(), side = r < 0.42 ? -1 : (r < 0.84 ? 1 : 0);
		var rg = catRange();
		K.cd = { side: side, x0: K.x, x1: side === 0 ? K.x : (side < 0 ? GEO.l + CAT_HW : GEO.r - CAT_HW) };
		if (side !== 0) K.face = side;
		setMode(side === 0 ? 'upwind' : 'windup', side === 0 ? 0.32 : 0.38);
		fast(2.6);
		K.nextCharge = rnd(2.5, 5);
		void rg;
	}

	function catBonk(side) {
		var cs = L.cs, xl = side < 0 ? GEO.l : (side > 0 ? GEO.r : K.x);
		var yl = side === 0 ? GEO.ceil : GEO.floor - 40 * KC - 8;
		var w = cageToWorld(xl, yl);
		G.freeze = Math.max(G.freeze, 0.045);
		C.shake = 1;
		var room = Math.max(2, (L.zw - L.cw) / 2 - MARGIN);
		var peak = rnd(0.55, 1) * Math.min(0.16 * L.cw, room);
		if (side !== 0) {
			C.vx += side * peak * 8.37 * 1.3;
			C.vrot += side * rnd(0.45, 0.85);
			C.vy -= rnd(60, 120) * cs;
			C.sq = Math.max(C.sq, 0.06);               /* 撞击帧竖向压扁, 约 3 帧回弹 */
		} else {
			C.vy -= rnd(150, 230) * cs;
			C.sq = Math.max(C.sq, 0.07); C.vsq = 0;
			C.vrot += rnd(-0.3, 0.3);
		}
		fxSpawn('ring', w.x, w.y, { size: 24 * cs + 6, color: '#fff7d6' });
		var i, n = rint(6, 9), dirx = side === 0 ? 0 : -side;
		for (i = 0; i < n; i++) fxSpawn('spark', w.x, w.y, { vx: dirx * rnd(80, 260) + rnd(-90, 90), vy: rnd(-230, 40) - (side === 0 ? 40 : 0), size: rnd(1.4, 2.6) });
		for (i = 0; i < 3; i++) fxSpawn('dust', w.x, L.ground - 2, { vx: rnd(-60, 60), vy: rnd(-40, -5), size: rnd(4, 7) });
		fxSpawn('text', w.x + dirx * 14, w.y - 18 * cs, { text: pick(['咚!', '砰!', '咣!', '哐!', '咚咚!']), size: 17 * Math.max(0.8, cs), rot: rnd(-0.25, 0.25) });
		if (Math.random() < 0.3) {
			var b = cageToWorld(rnd(40, 120), GEO.ceil + 4);
			for (i = 0; i < 5; i++) fxSpawn('dust', b.x + rnd(-10, 10), b.y, { vx: rnd(-14, 14), vy: rnd(10, 50), size: rnd(2, 3.5), life: 0.9 });
		}
	}

	function catUpdate(dt) {
		var k = K, rg = catRange(), u, i;
		k.pt += dt;
		k.bt -= dt;
		if (k.bt <= 0) { k.blink = 0.14; k.bt = rnd(2, 5); }
		if (k.blink > 0) k.blink -= dt;
		var rec = Math.min(1, dt * 11);
		k.sx += (1 - k.sx) * rec; k.sy += (1 - k.sy) * rec; k.rot += (0 - k.rot) * rec;
		var idleish = k.mode === 'idle' || k.mode === 'walk' || k.mode === 'yawn' || k.mode === 'sit';
		if (idleish && G.st === 'idle') {
			k.nextCharge -= dt;
			if (k.nextCharge <= 0) { startCharge(); return; }
		}
		switch (k.mode) {
			case 'idle': k.pose = 'idle'; k.mt -= dt; k.look.y += (0.1 - k.look.y) * dt; if (k.mt <= 0) pickIdle(); break;
			case 'walk':
				k.pose = 'walk';
				var d = k.tx - k.x, st = 34 * dt;
				if (Math.abs(d) <= st) { k.x = k.tx; setMode('idle', rnd(0.6, 1.6)); }
				else { k.x += sgn(d) * st; k.face = sgn(d); }
				k.offy = Math.abs(Math.sin(k.pt * 9)) * 1.6;
				break;
			case 'yawn': k.pose = 'yawn'; k.mt -= dt; if (k.mt <= 0) setMode('idle', rnd(0.8, 1.5)); break;
			case 'sit': k.pose = 'sit'; k.mt -= dt; if (k.mt <= 0) setMode('idle', rnd(0.8, 1.5)); break;
			case 'windup':
				k.pose = 'windup'; u = clamp(k.pt / 0.38, 0, 1);
				k.x = k.cd.x0 - k.cd.side * 10 * eOut(u);
				k.sx = 1.1; k.sy = 0.88 - 0.05 * u;
				if (u >= 1) { k.cd.x0 = k.x; setMode('run', 0.24); }
				break;
			case 'run':
				k.pose = 'charge'; u = clamp(k.pt / 0.24, 0, 1);
				k.x = lerp(k.cd.x0, k.cd.x1, eIn(u));
				k.sx = 1.15; k.sy = 0.92;
				if (((k.pt * 16) | 0) !== (((k.pt - dt) * 16) | 0)) { var f = cageToWorld(k.x - k.cd.side * 12, GEO.floor); fxSpawn('dust', f.x, f.y, { vx: -k.cd.side * 30, vy: -10, size: 3.5, life: 0.4 }); }
				if (u >= 1) { k.x = k.cd.x1; catBonk(k.cd.side); k.sx = 1.14; k.sy = 0.88; setMode('bonk', 0.18); }
				break;
			case 'bonk': k.pose = 'bonk'; k.sx = 1.14; k.sy = 0.89; if (k.pt >= 0.18) setMode('dizzy', 1.15); break;
			case 'dizzy':
				k.pose = 'dizzy'; u = clamp(k.pt / 1.15, 0, 1);
				if (k.cd && k.cd.side) k.x -= k.cd.side * 12 * dt;
				k.x = clamp(k.x, rg[0], rg[1]);
				if (((k.pt * 3.2) | 0) !== (((k.pt - dt) * 3.2) | 0)) { var h = catWorld(true); fxSpawn('star', h.x + rnd(-14, 14) * L.cs, h.y - 14 * L.cs, { vx: rnd(-30, 30), vy: rnd(-40, -10), size: 5 * Math.max(0.8, L.cs), life: 0.7 }); }
				if (u >= 1) { k.cd = null; setMode('idle', rnd(0.8, 1.6)); }
				break;
			case 'upwind':
				k.pose = 'windup'; u = clamp(k.pt / 0.32, 0, 1); k.sx = 1.12; k.sy = 0.84 + 0.0 * u;
				if (u >= 1) setMode('upjump', 0.52);
				break;
			case 'upjump':
				k.pose = 'jump'; u = clamp(k.pt / 0.52, 0, 1);
				var H = Math.max(6, GEO.floor - GEO.ceil - 96 * KC - 2);   /* 头顶刚好碰到顶栏 */
				k.offy = Math.max(0, 4 * H * u * (1 - u)) * 1.0;
				k.sy = 1.06 - 0.06 * Math.abs(u - 0.5) * 2; k.sx = 0.96;
				if (u >= 0.5 && !k.cd.hit) { k.cd.hit = true; catBonk(0); k.sy = 0.85; }
				if (u >= 1) { k.offy = 0; k.sx = 1.12; k.sy = 0.9; setMode('dizzy', 1.15); }
				break;
			case 'plead':
				k.pose = 'plead'; k.offy += (0 - k.offy) * Math.min(1, dt * 10);
				if (G.ptr && L.ok) {
					var wx = (G.ptr.x - (L.restL + L.cw / 2 + C.dx)) / (L.cs) + CW0 / 2;
					var tgt = clamp(wx, rg[0], rg[1]);
					var dd = tgt - k.x;
					if (Math.abs(dd) > 1) { var s2 = Math.min(Math.abs(dd), 70 * dt); k.x += sgn(dd) * s2; k.face = sgn(dd); }
				}
				k.hp -= dt;
				if (k.hp <= 0) {
					k.hp = rnd(0.42, 0.62);
					var hh = catWorld(true);
					fxSpawn('heart', hh.x + rnd(-10, 10) * L.cs, hh.y - 8 * L.cs, { size: rnd(6, 9) * Math.max(0.8, L.cs) });
					if (Math.random() < 0.3) fxSpawn('sweat', hh.x + 18 * L.cs, hh.y + 4 * L.cs, {});
				}
				break;
			case 'scared':
				k.pose = 'scared'; u = clamp(k.pt / 0.45, 0, 1);
				k.sx = 0.96; k.sy = 1.05; k.x += Math.sin(k.pt * 70) * 0.35;
				if (u >= 1) setMode('clap', 1.1);
				break;
			case 'clap':
				k.pose = 'clap'; k.mt -= dt;
				if (k.mt <= 0) setMode(G.st === 'plead' ? 'plead' : 'idle', 0.6);
				break;
			case 'recagewait':
				k.pose = 'idle'; k.look = { x: 0, y: -0.9 };
				if (C.dropping && (C.dy > -L.ch * 0.8)) k.pose = 'scared';
				break;
			default: break;
		}
		i = 0;
	}

	/* ───────────────────────── 呼救(悬停/轻触) ───────────────────────── */
	function enterPlead(byTouch) {
		if (G.st !== 'idle') return;
		G.st = 'plead';
		K.cd = null; K.offy = 0;
		setMode('plead', 0); K.hp = 0.05;
		fast(3);
		var hh = catWorld(true);
		for (var i = 0; i < 3; i++) fxSpawn('heart', hh.x + (i - 1) * 12 * L.cs, hh.y - 6 * L.cs, { size: 7 * Math.max(0.8, L.cs), vy: -40 - i * 8, vx: (i - 1) * 14 });
		K.sy = 1.06; K.sx = 0.96;
		setHammerIcon(byTouch);
		setHammerCursor(!byTouch);
		say('小猫在向你呼救，点击笼子帮它破笼');
	}
	function leavePlead() {
		if (G.st !== 'plead') return;
		G.st = 'idle'; G.touchArmed = false;
		setMode('idle', 0.8);
		K.nextCharge = rnd(2.8, 4.5);
		setHammerIcon(false);
		setHammerCursor(false);
	}
	var curSet = false;
	function setHammerCursor(on) {
		if (on === curSet) return;
		curSet = on;
		var v = '';
		if (on) {
			var A = art();
			try { v = (A && A.hammerCursor) ? A.hammerCursor(32) : ''; } catch (e) { v = ''; }
			if (!v) v = 'pointer';
		}
		hit.style.cursor = v;
	}
	function setHammerIcon(on) {
		if (!on) { hamIcon.className = 'mwrt-cat-ham'; return; }
		if (!hamIcon.firstChild) {
			var A = art(), svg = '';
			try { svg = (A && A.hammerSVG) ? A.hammerSVG(34) : ''; } catch (e) { svg = ''; }
			if (!svg) svg = '<svg viewBox="0 0 34 34" xmlns="http://www.w3.org/2000/svg"><rect x="15" y="12" width="5" height="20" rx="2" transform="rotate(-35 17 22)" fill="#b87a3d" stroke="#5a3a24" stroke-width="1.6"/><rect x="4" y="5" width="18" height="10" rx="3" transform="rotate(-35 13 10)" fill="#9aa8b8" stroke="#2f3b4d" stroke-width="1.8"/></svg>';
			hamIcon.innerHTML = svg;
		}
		hamIcon.className = 'mwrt-cat-ham mc-on';
	}

	/* ───────────────────────── 敲笼子 ───────────────────────── */
	var HAM_SWING = 0.14;
	function doClick(px, py) {
		if (G.st !== 'idle' && G.st !== 'plead') return false;
		if (G.hits >= NHITS) return false;
		if (G.hits > 0 && G.clock - G.lastHit < HIT_GAP) return false;
		if (G.st === 'idle') enterPlead(false);
		G.lastHit = G.clock;
		G.hits++;
		var n = G.hits;
		if (n < NHITS) ssSet(K_HITS, String(n)); else ssDel(K_HITS);
		var b = cageAABB();
		px = clamp(px, b.l + 8, b.r - 8); py = clamp(py, b.t + 8, b.b - 8);
		hammers.push({ t0: G.clock, x: px, y: py, side: px > L.zw * 0.55 ? -1 : 1, n: n, done: false });
		fast(HAM_SWING + 2.5);
		after(HAM_SWING, function () { impactHit(n, px, py); });
		return true;
	}

	function debrisBar(i, strike) {
		var w = cageToWorld(barXl(i), 74);
		fxSpawn('debris', w.x, w.y, { shape: 'bar', vx: (w.x - strike.x) * 3 + rnd(-90, 90), vy: rnd(-420, -230), vr: rnd(-14, 14), size: rnd(18, 26) * L.cs, ground: L.ground, life: 1.7 });
	}
	function sparksAt(x, y, n, spread) {
		for (var i = 0; i < n; i++) fxSpawn('spark', x, y, { vx: rnd(-1, 1) * spread, vy: rnd(-1, 0.2) * spread, size: rnd(1.4, 2.6), color: pick(['#ffe28a', '#fff4c0', '#ffb347']) });
	}

	function impactHit(n, px, py) {
		if (G.st !== 'idle' && G.st !== 'plead') return;
		var cs = L.cs, prev = cg, next = stageState(n), i, w, N = next.bars.length;
		G.freeze = Math.max(G.freeze, n === NHITS ? 0.075 : 0.045);
		cg = next;
		C.tiltT = next.tilt;
		C.shake = 1; C.sq = Math.max(C.sq, 0.05 + n * 0.008);
		var dir = sgn(L.restL + L.cw / 2 + C.dx - px);
		C.vx += dir * (60 + n * 22) * cs;
		C.vrot += dir * (0.4 + n * 0.08);
		C.vy -= (60 + n * 30) * cs;
		fxSpawn('ring', px, py, { size: (22 + n * 5) * Math.max(0.8, cs), color: '#fff7d6' });
		G.shake = Math.max(G.shake, 0.05 + 0.03 * n);
		var txts = ['锵!', '咔嚓!', '哐当!', '轰隆!', '砰砰砰!', '轰!!'];
		/* 拟声词放在笼顶(猫头上方的空白带), 不压住猫脸; 字号随击数变大, 宽度不超出空闲区 */
		var tsz = Math.min((24 + n * 2.5) * Math.max(0.9, cs), (L.zw - 8) / txts[n - 1].length);
		var tb = cageAABB();
		var thw = tsz * txts[n - 1].length * 0.5;
		var tx = clamp(px, thw + 3, L.zw - thw - 3);
		fxSpawn('text', tx, clamp(tb.t + tsz * 0.62 + 2, tsz * 0.62 + 1, L.zh - tsz * 0.5), { text: txts[n - 1], size: tsz, rot: rnd(-0.2, 0.2) });
		/* 猫的受惊 */
		if (n < NHITS) { setMode('scared', 0.45); K.cd = null; K.offy = 0; }
		if (n === 1) {
			sparksAt(px, py, 9, 260);
		} else if (n === 2) {
			sparksAt(px, py, 7, 240);
			for (i = 0; i < N; i++) if (next.bars[i].gone && !prev.bars[i].gone) debrisBar(i, { x: px, y: py });
		} else if (n === 3) {
			sparksAt(px, py, 8, 260);
			w = cageToWorld(130, 116);
			for (i = 0; i < 3; i++) fxSpawn('debris', w.x, w.y, { shape: 'lock', vx: rnd(-160, 160), vy: rnd(-380, -200), vr: rnd(-16, 16), size: rnd(6, 10) * cs, ground: L.ground, life: 1.6 });
		} else if (n === 4) {
			w = cageToWorld(80, 16);
			fxSpawn('debris', w.x, w.y, { shape: 'top', vx: rnd(-60, 60), vy: rnd(-120, -60), vr: rnd(-6, 6), size: 34 * cs, ground: L.ground, life: 1.8 });
			for (i = 0; i < 12; i++) fxSpawn('dust', w.x + rnd(-40, 40) * cs, w.y + rnd(0, 20), { vx: rnd(-50, 50), vy: rnd(-20, 40), size: rnd(4, 8) * cs, life: 0.9 });
			after(0.5, function () { fxSpawn('ring', L.restL + L.cw / 2, L.ground - 2, { size: 40 * cs, color: '#d8d8e0' }); for (var j = 0; j < 8; j++) fxSpawn('dust', L.restL + L.cw / 2 + rnd(-50, 50) * cs, L.ground - 4, { vx: rnd(-80, 80), vy: rnd(-40, -5), size: rnd(4, 8) }); });
		} else if (n === 5) {
			sparksAt(px, py, 14, 300);
			for (i = 0; i < N; i++) if (next.bars[i].gone && !prev.bars[i].gone) debrisBar(i, { x: px, y: py });
			for (i = 0; i < 2; i++) { w = cageToWorld(rnd(30, 130), 132); fxSpawn('debris', w.x, w.y, { shape: 'base', vx: rnd(-140, 140), vy: rnd(-300, -160), vr: rnd(-10, 10), size: rnd(10, 16) * cs, ground: L.ground, life: 1.6 }); }
			fxSpawn('ring', px, py, { size: 40 * cs, color: '#ffd37a', life: 0.5 });
		}
		if (n < NHITS) {
			say('笼子被敲坏了一部分（' + n + '/' + NHITS + '）');
			after(1.6, function () { if (G.st === 'plead' && K.mode === 'plead') K.sub = ''; });
			return;
		}
		/* ── 第 6 次: 整个笼子爆散, 猫飞出 ── */
		startFree(px, py, prev);
	}

	/* ───────────────────────── 破笼 → 跳出 → 撒娇 → 消失 → 认证框 ───────────────────────── */
	function startFree(px, py, prev) {
		var cs = L.cs, i, w, b = cageAABB();
		G.st = 'freed';
		G.refocus = !!G.kbFocus;              /* 键盘用户: 取消认证复位后把焦点还给猫 */
		setHammerIcon(false);
		setHammerCursor(false);
		hit.className = 'mwrt-cat-hit';
		fast(30);
		/* 猫转成世界坐标 */
		var f = cageToWorld(K.x, GEO.floor - K.offy);
		W.vis = true; W.x = f.x; W.y = f.y; W.s = cs * KC; W.k = 1; W.pose = 'scared'; W.sx = 1; W.sy = 1; W.rot = 0; W.face = K.face; W.ph = 'wait'; W.t = 0; W.hpT = 0;
		K.vis = false;
		/* 目标: 空闲区中央; 放大倍数受高度限制 */
		var catH = 96 * W.s;
		W.kF = clamp(Math.min(1.5, (L.zh * 0.86) / catH), 1.12, 1.5);   /* s90r3: 撒娇放大上限 1.9 -> 1.5, 与玩偶同一只, 不要太大 */
		W.x0 = f.x; W.y0 = f.y; W.x1 = L.zw / 2; W.y1 = L.ground - 2;
		W.H = Math.min(L.zh * 0.32, 70 * Math.max(1, cs)) + 16;
		W.T = 0.8;
		C.exploded = true;
		/* 全部碎片 */
		var N = cg.bars.length;
		for (i = 0; i < N; i++) if (!prev.bars[i].gone) debrisBar(i, { x: px, y: py });
		for (i = 0; i < 3; i++) { w = cageToWorld(130, 116); fxSpawn('debris', w.x, w.y, { shape: 'lock', vx: rnd(-180, 180), vy: rnd(-420, -220), vr: rnd(-16, 16), size: rnd(6, 10) * cs, ground: L.ground, life: 1.6 }); }
		for (i = 0; i < 4; i++) { w = cageToWorld(rnd(30, 130), 132); fxSpawn('debris', w.x, w.y, { shape: 'base', vx: rnd(-200, 200), vy: rnd(-380, -200), vr: rnd(-12, 12), size: rnd(10, 16) * cs, ground: L.ground, life: 1.7 }); }
		for (i = 0; i < 3; i++) { w = cageToWorld(rnd(30, 130), 20); fxSpawn('debris', w.x, w.y, { shape: 'top', vx: rnd(-200, 200), vy: rnd(-420, -250), vr: rnd(-12, 12), size: rnd(14, 22) * cs, ground: L.ground, life: 1.7 }); }
		sparksAt(f.x, f.y - 30 * cs, 22, 420);
		for (i = 0; i < 12; i++) fxSpawn('dust', f.x + rnd(-60, 60) * cs, f.y + rnd(-60, 0) * cs, { vx: rnd(-120, 120), vy: rnd(-100, 40), size: rnd(5, 10) * cs, life: 0.9 });
		fxSpawn('ring', f.x, f.y - 40 * cs, { size: 70 * cs, color: '#ffffff', life: 0.55 });
		fxSpawn('ring', f.x, f.y - 40 * cs, { size: 46 * cs, color: '#ffd37a', life: 0.42 });
		G.flash = 0.3; G.flashMax = 0.3; G.shake = 0.38;
		void b;
		say('笼子碎了，小猫获救');
		after(0.15, function () { W.ph = 'fly'; W.t = 0; W.pose = 'jump'; });
	}

	function freedUpdate(dt) {
		if (!W.vis) return;
		W.t += dt;
		var u, cs = L.cs;
		switch (W.ph) {
			case 'wait': W.pose = 'scared'; W.sy = 1.08; break;
			case 'fly':
				u = clamp(W.t / W.T, 0, 1);
				W.x = lerp(W.x0, W.x1, eIO(u) * 0.35 + u * 0.65);
				W.y = lerp(W.y0, W.y1, u) - 4 * W.H * u * (1 - u);
				W.k = lerp(1, 1.15, u); W.pose = 'jump'; W.sx = 1 - 0.06 * Math.sin(u * 3.1416); W.sy = 1 + 0.12 * Math.sin(u * 3.1416);
				W.rot = Math.sin(u * 6.283) * 0.12;
				W.face = sgn(W.x1 - W.x0) || 1;
				if (u >= 1) { W.ph = 'land'; W.t = 0; W.rot = 0; fxSpawn('dust', W.x - 22 * cs, L.ground - 2, { vx: -60, vy: -10, size: 6 }); fxSpawn('dust', W.x + 22 * cs, L.ground - 2, { vx: 60, vy: -10, size: 6 }); }
				break;
			case 'land':
				u = clamp(W.t / 0.32, 0, 1);
				W.k = lerp(1.15, W.kF, eBack(u)); W.pose = 'cute';
				W.sy = u < 0.25 ? lerp(1, 0.78, u / 0.25) : lerp(0.78, 1, (u - 0.25) / 0.75); W.sx = 2 - W.sy;
				W.face = 1;
				if (u >= 1) { W.ph = 'cute'; W.t = 0; W.sx = 1; W.sy = 1; after(0.25, function () { if (W.vis) say('小猫在撒娇'); }); }
				break;
			case 'cute':
				u = W.t / 1.85;
				W.pose = 'cute'; W.rot = Math.sin(W.t * 5.2) * 0.09; W.sy = 1 + 0.03 * Math.sin(W.t * 9); W.sx = 2 - W.sy;
				W.hpT -= dt;
				if (W.hpT <= 0) {
					W.hpT = 0.22;
					var hy = W.y - 80 * W.s * W.k;
					fxSpawn('heart', W.x + rnd(-34, 34) * W.s * W.k, hy + rnd(-8, 16), { size: rnd(6, 11), vy: rnd(-46, -30) });
				}
				if (!W.m1 && W.t > 0.3) { W.m1 = true; fxSpawn('text', W.x + 30 * W.s * W.k, W.y - 96 * W.s * W.k, { text: '喵~', size: 22, rot: 0.15, life: 1.1, color: '#ffe36b' }); }
				if (!W.m2 && W.t > 1.1) { W.m2 = true; fxSpawn('text', W.x - 30 * W.s * W.k, W.y - 90 * W.s * W.k, { text: '喵~', size: 20, rot: -0.15, life: 1.0, color: '#ffe36b' }); }
				if (u >= 1) { W.ph = 'poof'; W.t = 0; poofCat(); }
				break;
			default: break;
		}
	}

	function poofCat() {
		var cx0 = W.x, cy0 = W.y - 46 * W.s * W.k, i;
		for (i = 0; i < 10; i++) fxSpawn('poof', cx0 + rnd(-30, 30) * W.s * W.k, cy0 + rnd(-34, 34) * W.s * W.k, { vx: rnd(-50, 50), vy: rnd(-60, 20), size: rnd(10, 18), life: 0.7 });
		for (i = 0; i < 12; i++) fxSpawn('star', cx0, cy0, { vx: rnd(-1, 1) * 180, vy: rnd(-1, 0.4) * 180, size: rnd(4, 8), life: 0.9, color: pick(['#ffd84a', '#fff', '#ffb3d1']) });
		fxSpawn('ring', cx0, cy0, { size: 60, color: '#ffffff', life: 0.5 });
		W.vis = false;
		after(0.45, openDialog);
	}

	var dialogTimer = 0;
	function openDialog() {
		if (G.st !== 'freed') return;
		G.st = 'dialog';
		var U = window.MwrtUnlock, ok = false;
		try { if (U && typeof U.open === 'function') { U.open(); ok = true; } } catch (e) { ok = false; }
		if (!ok) {
			/* 认证框还没就绪(脚本顺序/加载失败): 等一等再试, 实在不行就当取消处理, 让猫复位 */
			var tries = 0;
			(function retry() {
				if (G.st !== 'dialog') return;
				var U2 = window.MwrtUnlock;
				if (U2 && U2.open) { try { U2.open(); return; } catch (e) {} }
				if (++tries > 6) { startRecage(); return; }
				setTimeout(retry, 300);
			}());
		}
	}
	function onUnlockClose() {
		if (G.st === 'dialog') startRecage();
	}
	function onUnlockOk() {
		/* 页面马上刷新。无论猫此刻在哪个状态(含键入 mwrtmwrt 直接认证时的 idle/plead), 统一转入 success:
		 * 笼子与猫化作星光、礼花先放一次, 与刷新后的玩偶入场衔接; 刷新后由 unlock_state 决定画面。 */
		if (G.st !== 'plush' && G.st !== 'off' && G.st !== 'wait' && G.st !== 'success') {
			var cxs = L.restL + L.cw / 2 + C.dx, cys = L.ground - L.ch * 0.45, i;
			var wasCage = !C.exploded && L.ok;
			G.st = 'success';
			G.hover = false; G.touchArmed = false; G.hits = 0; ssDel(K_HITS);
			setHammerIcon(false); setHammerCursor(false);
			hit.className = 'mwrt-cat-hit'; hitRect.on = false;
			hammers.length = 0; sched.length = 0;
			K.vis = false; W.vis = false; C.exploded = true;
			if (L.ok) {
				fast(4);
				if (wasCage) {
					for (i = 0; i < 5; i++) fxSpawn('poof', cxs + rnd(-34, 34) * L.cs, cys + rnd(-18, 22) * L.cs, { vx: rnd(-70, 70), vy: rnd(-30, 30), size: rnd(7, 12), life: 0.42 });   /* s90r3: 烟团小而短, 不再盖住蹦出来的玩偶 */
				}
				for (i = 0; i < 40; i++) fxSpawn('confetti', rnd(8, L.zw - 8), rnd(-30, -4), { vx: rnd(-50, 50), vy: rnd(40, 130) });
				successPlush(cxs);   /* 同一只猫在星光里蹦出来: 原地翻个跟头, 落地举爱心, 不再空场到刷新 */
				wake();
			}
			say('认证成功，页面即将刷新');
		}
		lsSet(K_HINT, '1');
	}

	/* ───────────────────────── 复位: 新笼子砸下罩住猫 ───────────────────────── */
	function startRecage() {
		G.st = 'recage';
		G.hits = 0; ssDel(K_HITS);
		G.hover = false; G.touchArmed = false;
		fast(30);
		cageReset(0);
		C.dropping = true; C.dy = -(L.ground - L.ch * 0.4); C.vy = 0;   /* 起点: 笼体下 40% 已在区内, 缩短空白 */
		K.vis = true; K.x = 80; K.offy = 0; K.face = 1; K.sx = 1; K.sy = 1; K.cd = null; K.nextCharge = 4;
		setMode('recagewait', 99);
		W.vis = false;
		hammers.length = 0;
		say('一个新的笼子落了下来，小猫又被关住了');
	}
	function onCageDropLanded(v) {
		var cs = L.cs, b = cageAABB(), i;
		G.freeze = Math.max(G.freeze, 0.06);
		C.shake = 1; C.sq = 0.16; C.vsq = 0; C.vrot += rnd(-0.5, 0.5);
		fxSpawn('ring', b.px, L.ground - 2, { size: 60 * cs, color: '#e8e8f0', life: 0.45 });
		for (i = 0; i < 10; i++) fxSpawn('dust', b.px + rnd(-70, 70) * cs, L.ground - 3, { vx: rnd(-100, 100), vy: rnd(-50, -5), size: rnd(4, 9), life: 0.7 });
		fxSpawn('text', b.px, L.ground - L.ch * 0.6, { text: '哐!', size: 22 * Math.max(0.85, cs), rot: rnd(-0.2, 0.2) });
		for (i = 0; i < 4; i++) fxSpawn('spark', b.px + rnd(-60, 60) * cs, L.ground - 6, { vx: rnd(-200, 200), vy: rnd(-200, -60) });
		G.shake = 0.22;
		setMode('dizzy', 1.4); K.sx = 1.12; K.sy = 0.89; K.cd = null;
		if (G.st === 'recage') after(1.4, function () { if (G.st === 'recage') { G.st = 'idle'; setMode('idle', 0.8); K.nextCharge = rnd(3, 5); say('小猫又在笼子里了'); } });
	}

	/* ───────────────────────── 玩偶猫 v2(已解锁态; 与笼中猫是同一只) ─────────────────────────
	 * 约定(美术库 plush-v2): 脚底锚点; hop/flip 一个周期 P.T 秒, 相位 P.ph=0..1, 起跳段由本文件按抛物线改 y(noLift:true),
	 * 抛物线形状取美术库的 plushHopLift(ph, pose); 落地挤压/起跳蓄力由美术自带, 不传 sx/sy。 */
	function hopLift(ph, pose) {
		var A = art();
		if (A && typeof A.plushHopLift === 'function') { try { return A.plushHopLift(ph, pose); } catch (e) {} }
		var a = pose === 'flip' ? 0.16 : 0.2, e2 = pose === 'flip' ? 0.86 : 0.82;
		if (ph <= a || ph >= e2) return 0;
		var w = (ph - a) / (e2 - a); return 4 * w * (1 - w);
	}
	function plushRange() { return [L.pw / 2 + MARGIN, L.zw - L.pw / 2 - MARGIN]; }
	function plushStart(celebrate) {
		P.vis = true; P.x = L.zw / 2; P.y = 0; P.vy = 0; P.face = 1; P.sx = 1; P.sy = 1; P.rot = 0; P.flip = false; P.react = 0; P.hpT = 0;
		P.fx = P.x; P.fy = L.ground - L.ph * 0.6; P.T = 0.62; P.ph = 0;
		G.st = 'plush';
		if (celebrate) {
			P.mode = 'drop'; P.y = -(L.ground + L.ph * 1.3); P.vy = 0; P.pose = 'hop'; P.ph = 0.5;
			fast(40);
			var i;
			for (i = 0; i < 48; i++) fxSpawn('confetti', rnd(8, L.zw - 8), rnd(-30, -4), { vx: rnd(-50, 50), vy: rnd(40, 130) });
			after(0.5, function () { for (var j = 0; j < 30; j++) fxSpawn('confetti', (j % 2 ? L.zw - 10 : 10), rnd(0, 20), { vx: (j % 2 ? -1 : 1) * rnd(40, 160), vy: rnd(10, 90) }); });
			say('认证成功，一只猫玩偶蹦了出来');
		} else {
			plushIdle(1.0);
		}
		hit.setAttribute('aria-label', '卡通猫玩偶，点击逗它，按住可以拖着走');
		hit.setAttribute('tabindex', '0');
		zone.setAttribute('data-mwrt-cat', 'plush');
		if (DR) { try { if (drAtt) drAtt.cancel(); DR.reset(P.x); } catch (e) {} }
		drEnsure();
	}
	function plushRest(mode, pose, t) { P.mode = mode; P.pose = pose; P.mt = t; P.t = 0; P.flip = false; P.ph = 0; P.y = 0; }
	function plushIdle(t) { plushRest('rest', pick(['idle', 'sit']), t); }
	function plushChoose() {
		var r = Math.random(), rg = plushRange(), span = rg[1] - rg[0];
		if (span < 8) { plushIdle(rnd(1, 2)); return; }
		var canFlip = plushMaxH(true) > L.ph * 0.3, canHop = plushMaxH(false) > L.ph * 0.12;
		if (r < 0.40 && canHop) plushHop(false);
		else if (r < 0.52 && canFlip) plushHop(true);
		else if (r < 0.66) plushRest('rest', 'sit', rnd(1.6, 2.6));
		else if (r < 0.76) plushRest('rest', 'idle', rnd(1.4, 2.4));
		else if (r < 0.85) plushRest('rest', 'wave', 1.5);
		else if (r < 0.93) plushRest('rest', 'yawn', 2.0);
		else plushRest('rest', 'chase-tail', 2.4);
	}
	function plushHop(flip, inPlace) {
		var rg = plushRange(), span = rg[1] - rg[0], tx;
		if (inPlace) tx = P.x;
		else {
			var tries = 0; tx = rnd(rg[0], rg[1]);
			while (Math.abs(tx - P.x) < span * 0.15 && tries++ < 6) tx = rnd(rg[0], rg[1]);
		}
		P.x0 = P.x; P.tx = tx; P.face = tx > P.x + 1 ? 1 : (tx < P.x - 1 ? -1 : P.face);
		var Hm = plushMaxH(flip), hh = Math.min(Hm, rnd(0.2, 0.36) * L.zh);
		if (flip) hh = Math.min(Hm, Math.max(hh, L.ph * 0.55));
		P.H = Math.max(0, hh);
		P.flip = !!flip && P.H > L.ph * 0.2;
		P.T = P.flip ? rnd(0.86, 1.0) : rnd(0.62, 0.78);
		P.mode = 'hop'; P.t = 0; P.pose = P.flip ? 'flip' : 'hop'; P.ph = 0;
		fast(P.T + 1);
	}
	function plushHearts(n) {
		for (var i = 0; i < n; i++) fxSpawn('heart', P.fx + rnd(-12, 12) * L.pS, P.fy - rnd(8, 18) * L.pS, { size: rnd(6, 9) });
	}
	function plushUpdate(dt) {
		if (!P.vis) return;
		if (G.st === 'plush' && drUpdate(dt)) return;      /* 被抓 / 下落 / 弹跳 / 落稳由拖拽物理接管 */
		var u, cs = L.pS, i;
		P.t += dt;
		switch (P.mode) {
			case 'drop':
				P.vy += 1900 * L.pS * dt; P.y += P.vy * dt; P.pose = 'hop'; P.ph = 0.5;
				if (P.y >= 0) {
					P.y = 0; P.vy = 0; P.mode = 'land'; P.pose = 'land'; P.t = 0; P.ph = 0;
					fxSpawn('ring', P.x, L.ground, { size: 40 * cs * 1.4, color: '#fff' });
					for (i = 0; i < 8; i++) fxSpawn('dust', P.x + rnd(-24, 24), L.ground - 3, { vx: rnd(-70, 70), vy: rnd(-40, -5), size: rnd(4, 8) });
					fxSpawn('text', P.x, L.ground - L.ph - 10, { text: '喵~', size: 20, color: '#ffe36b', life: 1.1 });
				}
				break;
			case 'land':
				u = clamp(P.t / 0.5, 0, 1); P.pose = 'land'; P.ph = u; P.y = 0;
				if (u >= 1) { if (P.react > 0) plushRest('rest', 'love', 1.2); else plushIdle(rnd(0.5, 1.0)); }
				break;
			case 'rest':
				P.mt -= dt;
				if (P.mt <= 0) { P.react > 0 ? plushRest('rest', 'love', Math.max(0.8, P.react)) : plushChoose(); }
				break;
			case 'hop':
				u = clamp(P.t / P.T, 0, 1); P.ph = u;
				P.y = -P.H * hopLift(u, P.pose);
				P.x = lerp(P.x0, P.tx, clamp((u - (P.flip ? 0.16 : 0.2)) / (P.flip ? 0.7 : 0.62), 0, 1));
				if (u >= 1) {
					P.y = 0; P.x = P.tx; P.flip = false;
					if (P.H > 6) { fxSpawn('dust', P.x - P.face * 8 * cs, L.ground - 2, { vx: P.face * -30, vy: -10, size: 4, life: 0.4 }); fxSpawn('dust', P.x + P.face * 8 * cs, L.ground - 2, { vx: P.face * 30, vy: -10, size: 4, life: 0.4 }); }
					if (P.react > 0) plushRest('rest', 'love', Math.max(0.8, P.react));
					else if (Math.random() < 0.3) plushHop(Math.random() < 0.25);
					else plushIdle(rnd(0.5, 1.4));
				}
				break;
			case 'celeb':        /* 认证成功到刷新之间的“开心跳”: 原地翻个跟头, 落地后爱心 */
				break;
			default: break;
		}
		if (P.react > 0) {
			P.react -= dt;
			if (P.mode !== 'hop') { P.hpT -= dt; if (P.hpT <= 0) { P.hpT = 0.28; plushHearts(1); } }
		}
		fitPlush();
	}
	function plushPet() {
		if (!P.vis) return;
		var wasReact = P.react > 0;
		P.react = 1.3; P.hpT = 0;
		fast(3);
		fxSpawn('text', P.fx, P.fy - 24 * L.pS - 6, { text: '喵~', size: 19, color: '#ffe36b', life: 1.0, rot: rnd(-0.15, 0.15) });
		if (P.mode === 'rest' && P.pose !== 'love') plushRest('rest', 'love', 1.4);
		return wasReact;
	}
	function plushClick() {
		var again = plushPet();
		if (again && P.mode === 'rest') plushHop(Math.random() < 0.4, true);
	}
	/* ───────────────────────── 玩偶拖拽(s90 第四轮) ─────────────────────────
	 * 物理与绘制全在 mwrt-cat-art.js 的 MwrtDrag(位置 Verlet 关节链, 固定 1/240 s 子步, 帧率无关)。本文件只做四件事:
	 *   1) 建控制器与输入层(Pointer Events + 指针捕获; touch-action:none 只加在 hit 元素上, 空闲区其余部分仍是 pointer-events:none);
	 *   2) 每帧同步空闲区矩形、待机站位(含半空高度, 蹦跳中被抓不瞬移);
	 *   3) mode≠0(被抓/下落/弹跳/落稳)时接管更新与绘制, 落稳后把站位交还给原来的 hop/sit 待机行为;
	 *   4) 小位移(<5px 且很短)仍是原来的「点它冒爱心、喵~」: 由输入层的 onClick 触发, 原 click 监听在有拖拽时让路。
	 * 命中元素在下落期间隐藏(不接新的抓取, 也不挡菜单), 被抓期间保持不动(指针已捕获)。 */
	function drLocal(e, o) { var r = zone.getBoundingClientRect(); o.x = e.clientX - r.left; o.y = e.clientY - r.top; }
	function drClick() { if (G.st === 'plush') { plushClick(); wake(); } }
	/* 抓取瞬间把悬停/点击留下的「喵~」字与爱心在 0.15 秒内收掉: 这些粒子在世界坐标里, 不会跟着被提起的猫走, 会盖在猫身上或悬在半空 */
	function drFadeFx() {
		var i, p;
		for (i = 0; i < FXA.length; i++) { p = FXA[i]; if ((p.kind === 'text' || p.kind === 'heart') && p.life > p.age + 0.15) p.life = p.age + 0.15; }
		for (i = 0; i < FXB.length; i++) { p = FXB[i]; if ((p.k === 'text' || p.k === 'heart') && p.max > p.life + 0.15) p.max = p.life + 0.15; }
	}
	function drGrab() {
		P.mode = 'drag'; P.react = 0; P.flip = false; drFadeFx();
		fast(1); wake();
	}
	function drDrop() { if (hitRect.on) { hit.className = 'mwrt-cat-hit'; hitRect.on = false; } fast(3); wake(); }     /* 松手当帧立刻隐藏命中元素, 不等下一帧 */
	function drEnsure() {
		var M = window.MwrtDrag;
		if (drFail || !M || !P.vis) return false;
		if (DR && (drS === L.pS || DR.mode() !== 0)) return true;      /* 抓取/下落中不重建, 落稳后再换缩放 */
		try {
			if (drAtt) { drAtt.detach(); drAtt = null; }
			DR = M.create({ s: L.pS, dark: L.dark }); drS = L.pS; drZW = -1;
			drAtt = M.attach(hit, { ctl: DR, toLocal: drLocal, onClick: drClick, onGrab: drGrab, onDrop: drDrop });
		} catch (e) { drFail = true; DR = null; drAtt = null; return false; }
		return true;
	}
	/* 返回 true = 本帧由拖拽物理处理完(调用者直接返回) */
	function drUpdate(dt) {
		if (!drEnsure()) return false;
		if (drZW !== L.zw || drZH !== L.zh || drG !== L.ground) {
			drZW = L.zw; drZH = L.zh; drG = L.ground;
			DR.setZone({ l: 3, t: 2, r: L.zw - 3, b: L.zh }, L.ground);
		}
		DR.setDark(L.dark);
		if (DR.mode() === 0) {
			if (P.mode === 'drag') plushIdle(0.5);               /* 保险: 控制器已空闲却还停在拖拽态 */
			DR.setHome(P.x, P.y);                                /* 待机站位跟随蹦跳(P.y<=0 为离地高度), 半空被抓从当前高度提起 */
			if (!DR.active()) return false;
		}
		DR.step(dt);
		if (DR.mode() !== 0) {
			if (P.mode !== 'drag') { P.mode = 'drag'; P.react = 0; P.flip = false; }
			return true;
		}
		if (DR.consumeDone()) {                                  /* 落稳: 站位交还, 回到原来的 hop/sit 待机 */
			P.x = DR.snap().root.x; fitPlush(); P.fx = P.x;
			plushIdle(rnd(0.5, 1.0));
			DR.setHome(P.x, 0);
			return true;
		}
		return false;
	}
	/* 区域被隐藏/状态切换/销毁时: 松手并把猫就地放回地面, 不留在抓取态 */
	function dragAbort() {
		if (!DR) return;
		try {
			if (drAtt) drAtt.cancel();
			if (DR.mode() !== 0 || DR.active()) {
				var x = DR.mode() !== 0 ? DR.snap().feet.x : P.x;
				DR.reset(x); P.x = x; fitPlush();
				if (P.mode === 'drag') plushIdle(0.8);
			}
		} catch (e) {}
	}

	/* 认证成功 → 刷新(约 1.2 秒)之间: 同一只猫星光里蹦出来, 原地翻个跟头, 落地后举爱心, 不再空场 */
	function successPlush(x0) {
		P.vis = true; P.x = x0 == null ? L.zw / 2 : x0; fitPlush(); P.y = 0; P.vy = 0; P.face = 1; P.sx = 1; P.sy = 1; P.rot = 0; P.react = 1.6; P.hpT = 0;
		P.fx = P.x; P.fy = L.ground - L.ph * 0.6; P.T = 0.7; P.ph = 0;
		var i, cy0 = L.ground - L.ph * 0.5;
		for (i = 0; i < 10; i++) fxSpawn('star', P.x, cy0, { vx: rnd(-1, 1) * 150, vy: rnd(-1, 0.2) * 150, size: rnd(4, 7), life: 0.8, color: pick(['#ffd84a', '#fff', '#ffb3d1']) });
		fxSpawn('ring', P.x, cy0, { size: 46 * L.pS + 10, color: '#fff7d6', life: 0.45 });
		plushHop(plushMaxH(true) > L.ph * 0.3, true);
		P.T = P.flip ? 0.86 : 0.7;
		zone.setAttribute('data-mwrt-cat', 'plush');
		hit.className = 'mwrt-cat-hit'; hitRect.on = false;
	}

	/* ───────────────────────── 锤子挥动(画布上的矢量锤) ───────────────────────── */
	function drawHammers() {
		var i, h, u, ang, Lh, uS = clamp(L.cs, 0.6, 1.4), a0 = -1.05, a1 = -2.75;
		for (i = hammers.length - 1; i >= 0; i--) {
			h = hammers[i];
			var t = G.clock - h.t0;
			if (t > HAM_SWING + 0.28) { hammers.splice(i, 1); continue; }
			Lh = 44 * uS;
			if (t < HAM_SWING) { u = eIn(t / HAM_SWING); ang = lerp(a0, a1, u); }
			else { var r = (t - HAM_SWING) / 0.28; ang = a1 + 0.14 * Math.sin(Math.min(1, r * 2) * 3.1416) * (r < 0.5 ? 1 : 0.4); }
			var al = t < HAM_SWING ? 1 : clamp(1 - (t - HAM_SWING) / 0.11, 0, 1);
			var m = h.side;                       /* m=-1: 从左边抡, 镜像 */
			var dx = Math.cos(a1) * m, dy = Math.sin(a1);
			var pvx = h.x - dx * Lh, pvy = h.y - dy * Lh;
			cx.save();
			cx.globalAlpha = al;
			cx.translate(pvx, pvy);
			if (m < 0) cx.scale(-1, 1);
			cx.rotate(ang);
			/* 手柄朝 +x 方向画, 锤头在末端 */
			cx.lineCap = 'round'; cx.lineJoin = 'round';
			cx.strokeStyle = '#4a2c16'; cx.lineWidth = 7 * uS; cx.beginPath(); cx.moveTo(0, 0); cx.lineTo(Lh, 0); cx.stroke();
			cx.strokeStyle = '#c98b4a'; cx.lineWidth = 4.2 * uS; cx.beginPath(); cx.moveTo(0, 0); cx.lineTo(Lh, 0); cx.stroke();
			cx.fillStyle = '#9fb0c4'; cx.strokeStyle = '#2b3646'; cx.lineWidth = 2 * uS;
			cx.beginPath(); cx.rect(Lh - 6 * uS, -11 * uS, 14 * uS, 22 * uS); cx.fill(); cx.stroke();
			cx.fillStyle = 'rgba(255,255,255,.55)'; cx.fillRect(Lh - 3 * uS, -9 * uS, 3 * uS, 18 * uS);
			cx.restore();
		}
		cx.globalAlpha = 1;
	}

	/* ───────────────────────── 逐帧: 更新 ───────────────────────── */
	function update(real) {
		var dt = real;
		if (G.freeze > 0) { G.freeze -= real; dt = 0; }
		G.clock += dt;
		runSched();
		if (dt > 0) {
			if (G.shake > 0) G.shake = Math.max(0, G.shake - dt);
			if (G.flash > 0) G.flash = Math.max(0, G.flash - dt);
			var s = G.st;
			if (s === 'idle' || s === 'plead' || s === 'freed' || s === 'dialog' || s === 'recage') {
				cageStep(dt);
				if (s === 'idle' || s === 'plead' || s === 'recage') catUpdate(dt);
				if (s === 'freed') freedUpdate(dt);
				if (s === 'plead' && !G.hover && !G.kbFocus && G.clock > G.hoverLeave) leavePlead();
			} else if (s === 'plush' || s === 'success') {
				plushUpdate(dt);
			}
			fxStep(dt);
		}
		if (G.st === 'plead' && G.touchArmed) positionHammerIcon();
		syncHit();
	}

	function positionHammerIcon() {
		var b = cageAABB();
		var x = clamp(b.r - 12, 0, L.zw - 34), y = clamp(b.t - 14, 0, L.zh - 34);
		hamIcon.style.left = Math.round(x) + 'px'; hamIcon.style.top = Math.round(y) + 'px';
	}

	var hitRect = { x: -1, y: -1, w: -1, h: -1, on: false };
	function syncHit() {
		var on = false, x = 0, y = 0, w = 0, h = 0;
		if (G.visible && (G.st === 'idle' || G.st === 'plead') && !C.exploded) {
			var b = cageAABB(); on = true; x = b.l; y = b.t; w = b.r - b.l; h = b.b - b.t;
		} else if (G.visible && G.st === 'plush' && P.vis) {
			var dm = DR ? DR.mode() : 0;
			if (dm === 1) return;                                 /* 被抓中: 命中元素原地不动(指针已捕获), 不重写 */
			if (dm === 0 && P.mode !== 'drop') { on = true; w = L.pw * 0.92; h = L.ph * 1.05; x = P.x - w / 2; y = L.ground + P.y - h; }   /* 下落/弹跳/落稳/入场空降期间隐藏: 不接新的抓取, 也不挡菜单 */
		}
		if (on) {
			x = Math.round(x); y = Math.round(y); w = Math.round(w); h = Math.round(h);
			if (!hitRect.on) {
				hit.className = 'mwrt-cat-hit mc-on' + (G.kbFocus ? ' mc-kb' : '');
				if (G.refocus && G.st === 'idle') { G.refocus = false; try { hit.focus(); } catch (e) {} }
			}
			if (x !== hitRect.x || y !== hitRect.y) hit.style.transform = hit.style.webkitTransform = 'translate(' + x + 'px,' + y + 'px)';
			if (w !== hitRect.w) hit.style.width = w + 'px';
			if (h !== hitRect.h) hit.style.height = h + 'px';
			hitRect.x = x; hitRect.y = y; hitRect.w = w; hitRect.h = h; hitRect.on = true;
		} else if (hitRect.on) {
			hit.className = 'mwrt-cat-hit'; hitRect.on = false;
		}
	}

	/* ───────────────────────── 逐帧: 绘制 ───────────────────────── */
	function lookVec(hx, hy) {
		if (G.ptr && (G.st === 'plead')) {
			var dx = G.ptr.x - hx, dy = G.ptr.y - hy, d = Math.sqrt(dx * dx + dy * dy) || 1, m = Math.min(1, d / 60);
			return { x: dx / d * m, y: dy / d * m };
		}
		return K.look;
	}

	function draw() {
		var d = L.dpr, ox = 0, oy = 0;
		cx.setTransform(d, 0, 0, d, 0, 0);
		cx.clearRect(0, 0, L.zw, L.zh);
		if (G.shake > 0) { var sk = G.shake * 7; ox = Math.sin(G.clock * 97) * sk; oy = Math.cos(G.clock * 83) * sk * 0.7; }
		cx.save();
		cx.translate(ox, oy);
		var st = G.st, cs = L.cs;
		if (st === 'plush' || st === 'success') drawPlushScene();
		else if (st !== 'wait' && st !== 'off' && st !== 'success') drawLockedScene();
		fxDraw();
		drawHammers();
		cx.restore();
		if (G.flash > 0) {
			/* 以笼体中心为圆心的径向渐变, 边缘透明(不再是整块硬边矩形) */
			var fa = 0.5 * G.flash / G.flashMax, fx0 = L.restL + L.cw / 2, fy0 = L.ground - L.ch / 2, fr = Math.max(40, 0.9 * L.zw);
			var fg = cx.createRadialGradient(fx0, fy0, 0, fx0, fy0, fr);
			fg.addColorStop(0, 'rgba(255,255,255,' + fa.toFixed(3) + ')');
			fg.addColorStop(1, 'rgba(255,255,255,0)');
			cx.fillStyle = fg; cx.fillRect(0, 0, L.zw, L.zh);
		}
		void cs;
	}

	function shadow(cxp, wpx, k) {
		cx.save();
		cx.fillStyle = L.dark ? 'rgba(0,0,0,' + (0.34 * k).toFixed(3) + ')' : 'rgba(30,40,60,' + (0.22 * k).toFixed(3) + ')';
		cx.beginPath(); cx.ellipse(cxp, L.ground + 2, wpx * 0.5, Math.max(3, wpx * 0.06), 0, 0, 6.283); cx.fill();
		cx.restore();
	}

	function drawLockedScene() {
		var cs = L.cs, b = cageAABB(), i;
		/* 地面阴影: 随笼子离地高度变淡变小; 掉落中随距离渐显 */
		var lift = clamp(-C.dy / (L.ch * 0.5), 0, 1);
		var kk = C.dropping ? clamp(1 + C.dy / (L.ground + L.ch), 0, 1) * 0.9 : (1 - lift * 0.6);
		if (!C.exploded) shadow(b.px, L.cw * (1.0 - lift * 0.2) * (C.dropping ? (0.6 + 0.4 * kk) : 1), kk);
		else shadow(L.zw / 2, L.cw * 0.55, 0.6);
		if (!C.exploded) {
			var d = cageDims();
			cx.save();
			cx.translate(b.px, b.py);
			cx.scale(d.w / L.cw, d.h / L.ch);          /* 挤压: 绕底边中点 */
			cx.rotate(C.rot);
			/* 笼子以底边中点为锚点, 旋转/斜切/抖动由美术库自己处理(rot=0 因为外层已转) */
			var cobj = { x: 0, y: 0, w: L.cw, h: L.ch, rot: 0, t: G.clock, shake: C.shake, bars: cg.bars, lock: cg.lock,
				topDamage: cg.topDamage, baseDamage: cg.baseDamage, tilt: C.tilt, flash: cg.flash, gone: false, dark: L.dark };
			aCage(cobj, false);
			if (K.vis) {
				var kly = GEO.floor - K.offy - CH0;       /* 逻辑纵坐标(向上为负) */
				var kx = (K.x - CW0 / 2 - C.tilt * kly) * cs;
				var ky = kly * cs;
				var hx = b.px + kx, hy = b.py + ky - 40 * cs;
				var lk = lookVec(hx, hy);
				var kobj = { x: kx, y: ky, s: cs * KC, t: G.clock, pose: K.pose, look: lk, sx: K.sx, sy: K.sy, rot: K.rot, flip: K.face < 0, dark: L.dark };
				aCat(kobj);
			}
			aCage(cobj, true);
			if (K.vis && PEEK_POSES[K.pose] && kobj) { kobj.peek = true; aCat(kobj); }   /* 脸从栏杆缝里露出来 */
			cx.restore();
		}
		if (W.vis) {
			var sc = W.s * W.k;
			if (W.ph === 'fly') { cx.save(); cx.globalAlpha = 0.25; cx.fillStyle = '#000'; cx.beginPath(); cx.ellipse(W.x, L.ground + 2, 22 * sc * 0.8, 3, 0, 0, 6.283); cx.fill(); cx.restore(); }
			else shadow(W.x, 70 * sc, 0.8);
			aCat({ x: W.x, y: W.y, s: sc, t: G.clock, pose: W.pose, look: { x: 0, y: -0.1 }, sx: W.sx, sy: W.sy, rot: W.rot, flip: W.face < 0, dark: L.dark });
		}
		void b; void i;
	}

	function drawPlushScene() {
		if (!P.vis) return;
		if (DR && DR.mode() !== 0) { DRO.t = G.clock; DRO.dark = L.dark; DR.render(cx, DRO); return; }   /* 被抓/下落: 影子、猫、汗滴气泡灰尘都由拖拽层画 */
		var sc = L.pS, lift = clamp(-P.y / (L.zh * 0.6), 0, 0.5), r;
		/* 地面阴影由本文件画(noLift: 抛物线由调用方驱动, 美术库自带阴影会跟着抬高) */
		shadow(P.x, L.pw * 0.9 * (1 - lift), 1 - lift * 1.4);
		P.look = null;
		if (G.hover && G.ptr && G.st === 'plush') P.look = { x: clamp((G.ptr.x - P.x) / 60, -1, 1), y: clamp((G.ptr.y - (L.ground + P.y - L.ph * 0.6)) / 60, -1, 1) };
		r = aPlush({ x: P.x, y: L.ground + P.y, s: sc, t: G.clock, pose: P.pose, phase: P.ph, dur: P.T, noLift: true, shadow: false, dark: L.dark, flip: P.face < 0, look: P.look });
		if (r && r.face) { P.fx = r.face.x; P.fy = r.face.y; }
		else { P.fx = P.x; P.fy = L.ground + P.y - L.ph * 0.6; }
	}

	/* ───────────────────────── 循环 / 生命周期 ───────────────────────── */
	var raf = 0, lastT = 0, lowAcc = 0;
	function canRun() { return G.visible && !document.hidden && !G.dead && !G.manual && G.st !== 'wait' && G.st !== 'off'; }
	function wake() { if (!canRun()) return; if (G.running) return; G.running = true; lastT = 0; lowAcc = 0; raf = requestAnimationFrame(frame); }
	function stopLoop() { G.running = false; if (raf) { cancelAnimationFrame(raf); raf = 0; } }

	function tick(real) {
		update(real);
		draw();
	}

	function frame(ts) {
		raf = 0;
		if (!G.running) return;
		if (!canRun()) { G.running = false; return; }
		var now = ts / 1000, dt = lastT ? Math.min(0.05, now - lastT) : 1 / 60;
		lastT = now;
		var low = G.clock > G.fastUntil && fxCount() === 0 && hammers.length === 0 && G.freeze <= 0 && !(DR && (DR.mode() !== 0 || DR.active()));
		if (low) {
			lowAcc += dt;
			if (lowAcc < 0.048) { raf = requestAnimationFrame(frame); return; }
			dt = Math.min(lowAcc, 0.05); lowAcc = 0;
		}
		try { tick(dt); G.errs = 0; } catch (e) {
			if (++G.errs > 5) { G.dead = true; killAll(); return; }
			G.artBroken = true;
		}
		raf = requestAnimationFrame(frame);
	}

	function killAll() {
		dragAbort();
		stopLoop();
		try { zone.className = 'mwrt-cat-zone mc-none'; } catch (e) {}
	}

	/* ───────────────────────── 输入 ───────────────────────── */
	function localPt(e) {
		var r = zone.getBoundingClientRect();
		var cxp = e.clientX, cyp = e.clientY;
		if ((cxp === undefined || cxp === null) && e.touches && e.touches[0]) { cxp = e.touches[0].clientX; cyp = e.touches[0].clientY; }
		return { x: cxp - r.left, y: cyp - r.top };
	}
	function hoverOn(by) {
		G.hover = true; G.hoverLeave = G.clock + 99;
		if (G.st === 'idle') enterPlead(by === 'touch');
		else if (G.st === 'plush') plushPet();
		wake();
	}
	function hoverOff(delay) {
		G.hover = false;
		G.hoverLeave = G.clock + (delay === undefined ? 2.5 : delay);
	}

	function bindInput() {
		hit.addEventListener('mouseenter', function (e) {
			if (Date.now() - G.lastTouch < 800) return;
			G.ptr = localPt(e);
			hoverOn('mouse');
		});
		hit.addEventListener('mousemove', function (e) { G.ptr = localPt(e); });
		hit.addEventListener('mouseleave', function () {
			if (Date.now() - G.lastTouch < 800) return;
			hoverOff(2.5);
		});
		hit.addEventListener('mousedown', function (e) { e.preventDefault(); });   /* 鼠标用户不显示键盘焦点环 */
		hit.addEventListener('touchstart', function (e) {
			G.lastTouch = Date.now();
			G.ptr = localPt(e);
		}, { passive: true });
		hit.addEventListener('click', function (e) {
			var fromTouch = Date.now() - G.lastTouch < 900;
			var kb = !fromTouch && e.detail === 0;
			if (G.st === 'plush') { if (!drAtt) plushClick(); return; }      /* 有拖拽层时点击由它的 onClick 触发(避免拖完又冒一次、点一下冒两次) */
			if (G.st !== 'idle' && G.st !== 'plead') return;
			var pt = localPt(e);
			if (kb) { var b = cageAABB(); pt = { x: (b.l + b.r) / 2, y: (b.t + b.b) / 2 }; }
			G.ptr = pt;
			if (fromTouch) {
				if (G.st === 'plead' && G.touchArmed) {
					G.hoverLeave = G.clock + 4;
					doClick(pt.x, pt.y);
				} else {
					G.touchArmed = true; G.hover = false; G.hoverLeave = G.clock + 4;
					enterPlead(true);
					setHammerIcon(true);
				}
				return;
			}
			doClick(pt.x, pt.y);
			wake();
		});
		hit.addEventListener('keydown', function (e) {
			var k = e.key || e.keyCode;
			if (k === 'Enter' || k === ' ' || k === 'Spacebar' || k === 13 || k === 32) {
				e.preventDefault();
				if (G.st === 'plush') { plushClick(); return; }
				var b = cageAABB();
				if (G.st === 'idle') enterPlead(false);
				doClick((b.l + b.r) / 2 + rnd(-10, 10), (b.t + b.b) / 2 + rnd(-10, 10));
				wake();
			}
		});
		hit.addEventListener('focus', function () {
			G.kbFocus = true; hit.className = hit.className.replace(' mc-kb', '') + (hitRect.on ? ' mc-kb' : '');
			if (G.st === 'idle') enterPlead(false);
			wake();
		});
		hit.addEventListener('blur', function () {
			G.kbFocus = false; hit.className = hit.className.replace(' mc-kb', '');
			G.hoverLeave = G.clock + 2.5;
		});
		window.addEventListener('mwrt-unlock-close', onUnlockClose);
		window.addEventListener('mwrt-unlock-ok', onUnlockOk);
		document.addEventListener('visibilitychange', function () {
			if (document.hidden) stopLoop(); else { queueMeasure(); wake(); }
		});
		window.addEventListener('focus', function () { wake(); });
		window.addEventListener('pagehide', function () { stopLoop(); });
		window.addEventListener('resize', queueMeasure);
		window.addEventListener('orientationchange', queueMeasure);
	}

	function bindObservers() {
		try {
			if (window.ResizeObserver) {
				var ro = new ResizeObserver(function () { queueMeasure(); });
				ro.observe(zone);
				ro.observe(ML);
			}
		} catch (e) {}
		try {
			if (window.MutationObserver) {
				var mo = new MutationObserver(function (recs) {
						/* 猫区自己的 style/class 写入(动画每帧都在写)不触发重新测量 */
						for (var i = 0; i < recs.length; i++) { if (!zone.contains(recs[i].target)) { queueMeasure(); return; } }
					});
				mo.observe(ML, { childList: true, subtree: true, attributes: true, attributeFilter: ['class', 'style', 'hidden'] });
				var mo2 = new MutationObserver(function () { queueMeasure(); });
				mo2.observe(document.documentElement, { attributes: true, attributeFilter: ['class', 'data-darkmode', 'style'] });
			}
		} catch (e) {}
		/* 没有 ResizeObserver 的旧浏览器: 低频兜底测量 */
		if (!window.ResizeObserver) setInterval(function () { if (!document.hidden) queueMeasure(); }, 700);
	}

	/* ───────────────────────── 解锁判定 ───────────────────────── */
	function fetchState(cb) {
		var url = STATE_URL;
		if (!url) {
			try { if (window.L && window.L.url) url = window.L.url('admin', 'modem', 'unlock_state'); } catch (e) {}
		}
		if (!url) url = '/cgi-bin/luci/admin/modem/unlock_state';
		var done = false;
		function fin(ok, v) { if (done) return; done = true; cb(ok, v); }
		try {
			var x = new XMLHttpRequest();
			x.open('GET', url, true);
			x.timeout = 8000;
			x.onload = function () {
				var r = null;
				try { r = JSON.parse(x.responseText); } catch (e) {}
				if (x.status === 200 && r && r.ok && typeof r.unlocked === 'boolean') fin(true, r.unlocked);
				else fin(false, false);
			};
			x.onerror = x.ontimeout = function () { fin(false, false); };
			x.send(null);
		} catch (e) { fin(false, false); }
	}

	function enterLocked() {
		cageReset(0);
		var h = parseInt(ssGet(K_HITS), 10);
		if (h >= 1 && h <= NHITS - 1) { G.hits = h; cg = stageState(h); C.tiltT = cg.tilt; C.tilt = cg.tilt; }
		G.st = 'idle';
		K.vis = true; K.x = 80; K.nextCharge = rnd(2.4, 4);
		setMode('idle', 0.8);
		zone.setAttribute('data-mwrt-cat', 'locked');
		hit.setAttribute('aria-label', '被困的小猫，点击帮它破笼，按 Enter 连击');
	}

	var celeSaved = false, retried = false;
	function begin(ok, unlocked) {
		var cele = ssGet(K_CELE) === '1' || celeSaved;
		if (cele) { ssDel(K_CELE); celeSaved = true; }
		if (!ok) {
			var hint = lsGet(K_HINT);
			if (hint === '1') unlocked = true;
			else if (hint === '0') unlocked = false;
			else {
				G.st = 'off'; applyVisible(false);       /* 不自己猜: 判定失败又没有提示位, 暂不显示 */
				if (!retried) {                          /* 偶发 5xx/超时: 15 秒后只再试一次 */
					retried = true;
					setTimeout(function () { fetchState(function (ok2, v2) { try { if (ok2 && G.st === 'off') begin(ok2, v2); } catch (e) { G.dead = true; killAll(); } }); }, 15000);
				}
				return;
			}
		} else {
			lsSet(K_HINT, unlocked ? '1' : '0');
		}
		G.unlocked = unlocked;
		if (unlocked) { G.st = 'plush'; }
		else { G.st = 'idle'; }
		measure();
		if (!L.ok) {
			/* 空闲区当前太小: 先把状态摆好, 区域变大时 measure 会唤醒循环 */
			if (unlocked) { G.st = 'wait'; deferStart(function () { plushStart(cele); }); }
			else { G.st = 'wait'; deferStart(enterLocked); }
			return;
		}
		if (unlocked) plushStart(cele); else enterLocked();
		applyVisible(true);
		wake();
	}
	var deferred = null;
	function deferStart(fn) {
		deferred = fn;
		/* 区域变大(measure 里 L.ok 变 true)时启动; 用轮询式的一次性钩子, 不额外开定时器 */
		var chk = function () {
			if (!deferred) return;
			if (L.ok) { var f = deferred; deferred = null; f(); applyVisible(true); wake(); }
		};
		deferChk = chk;
	}
	var deferChk = null;

	/* ───────────────────────── 启动 ───────────────────────── */
	function init() {
		try {
			buildDom();
			bindInput();
			bindObservers();
			cageReset(0);
			measure();
			fetchState(function (ok, v) {
				try { begin(ok, v); } catch (e) { G.dead = true; killAll(); }
			});
		} catch (e) { G.dead = true; try { if (zone) killAll(); } catch (e2) {} }
	}

	/* measure 之后检查推迟启动 */
	var _measure = measure;
	measure = function () { _measure(); if (deferChk) { try { deferChk(); } catch (e) {} } };

	/* ───────────────────────── 对外接口与测试钩子 ───────────────────────── */
	window.MwrtCatCage = {
		version: 1,
		state: function () { return G.st; },
		hits: function () { return G.hits; },
		isVisible: function () { return !!G.visible; },
		/* 笼子当前包围盒(相对视口), 自检用 */
		cageRectViewport: function () {
			if (!L.ok || C.exploded) return null;
			var b = cageAABB(), r = zone.getBoundingClientRect();
			return { l: r.left + b.l, t: r.top + b.t, r: r.left + b.r, b: r.top + b.b };
		},
		zoneRect: function () { var r = zone.getBoundingClientRect(); return { l: r.left, t: r.top, r: r.right, b: r.bottom, w: r.width, h: r.height }; },
		_test: window.__mwrtCatTest !== true ? undefined : {     /* 仅自检探针在加载前置 window.__mwrtCatTest=true 时暴露 */
			G: G, C: C, K: K, W: W, P: P, L: L,
			/* 手动推进游戏时钟(暂停 rAF 循环, 定步长, 不依赖真实时间) */
			advance: function (sec, step) {
				step = step || 1 / 60;
				G.manual = true; stopLoop();
				var n = Math.round(sec / step), i;
				for (i = 0; i < n; i++) tick(step);
				return G.st;
			},
			resume: function () { G.manual = false; wake(); },
			click: function (px, py) { var b = cageAABB(); return doClick(px === undefined ? (b.l + b.r) / 2 : px, py === undefined ? (b.t + b.b) / 2 : py); },
			measure: function () { measure(); },
			fxCount: fxCount,
			sched: function () { return sched.length; },
			parts: function () { return FXA.length + FXB.length; },
			hammers: function () { return hammers.length; },
			bbox: function () { return cageAABB(); },
			plushHop: plushHop,
			plushBox: function () { return { x: P.x, y: L.ground + P.y, w: L.pw, h: L.ph }; },
			forceCharge: function () { if (G.st === 'idle') { K.nextCharge = 0; } },
			setHits: function (n) { G.hits = n; cg = stageState(n); C.tiltT = cg.tilt; C.tilt = cg.tilt; },
			start: function (unlocked, cele) { begin(true, !!unlocked); if (unlocked && cele) { } },
			plushCelebrate: function () { fxClear(); plushStart(true); },
			plushMaxH: plushMaxH,
			drag: function () { return DR; },
			dragAtt: function () { return drAtt; },
			plushPet: plushPet,
			successPlush: function () { fxClear(); onUnlockOk(); },
			plushTop: function () {    /* 当前帧玩偶实际包围盒顶(相对画布); 取不到返回 null */
				var A = art(); if (!A || !A.plushBounds || !P.vis) return null;
				try { var b = A.plushBounds({ x: P.x, y: L.ground + P.y, s: L.pS, t: G.clock, pose: P.pose, phase: P.ph, dur: P.T, noLift: true }); var y0 = L.ground + P.y; return { t: y0 + b.t, l: P.x + b.l, r: P.x + b.r, b: y0 + b.b }; } catch (e) { return null; }
			}
		}
	};

	if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
	else init();
}());
