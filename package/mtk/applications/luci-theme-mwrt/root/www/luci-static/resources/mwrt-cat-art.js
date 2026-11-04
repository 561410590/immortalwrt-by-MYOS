/* mwrt-cat-art.js —— 侧栏「笼中猫」互动的美术库（窗口全局 MwrtCatArt）
 *
 * 全部是“给我一个 ctx 和状态，我画一帧”的纯绘制函数，不持有时间状态（时间由调用者传 t 秒）。
 * ES5，无依赖，不碰 DOM（除 hammerCursor/hammerSVG 只拼字符串）。
 *
 * 坐标约定（逻辑单位：笼子宽 160 时的 1px）：
 *   drawCat / drawPlush：(x,y) = 脚底中点（猫站着的地面点），s = 缩放，猫身高约 96 逻辑单位（含耳朵）。
 *   drawCage*：(x,y) = 笼子底边中点（origin 默认 'bottom'；也可给 c.origin='center'|'topleft'），
 *              c.w/c.h = 笼子在画布上的像素宽高（逻辑宽 160，缩放 k=c.w/160）。
 *   猫脸朝向 +x（flip=true 镜像）。旋转 rot 绕 (0,pivotY) 转，pivotY 默认 0（脚底），翻跟头建议 -34。
 *
 * s90 第三轮：drawCat / catAnchors / CAT_POSES 指向 MwrtCatV2（笼中猫 v2），drawPlush / PLUSH_POSES 指向 MwrtPlushV2（玩偶 v2，同一只猫）；
 *   旧版保留为 drawCatV1 / catAnchorsV1 / drawPlushV1 / PLUSH_POSES_V1 保底（V2 抛异常时自动回退）。
 *   新增 catBounds / plushBounds / plushHopLift / PLUSH_SIZE。两个 V2 IIFE 内联在本文件前部，也各自导出 window.MwrtCatV2 / window.MwrtPlushV2。
 * s90 第四轮：玩偶 v2 新增 'drag'（被抓住）姿态与参数 DG（造型不变，参数全为 0 时与原姿态逐像素一致）；紧随其后内联拖拽物理，导出 window.MwrtDrag。
 * 导出：
 *   drawCat(ctx,o) / catAnchors(o) / CAT_POSES
 *   drawCage(ctx,c) / drawCageBack / drawCageFront / cageStage(n) / cageRect(c) / cageFloor(c) / barX(i) / barPoint(c,i,u) / cagePoint(c,lx,ly) / CAGE
 *   drawPlush(ctx,o) / PLUSH_POSES
 *   fx.spawn / fx.burst / fx.step / fx.draw / fx.clear / fx.stats
 *   hammerCursor(size) / hammerSVG(size) / palette(isDark) / setDark(b)
 */
/* cat-v2.js —— 笼中猫 v2 终版（基于草稿 a，嫁接草稿 b 的弹簧二级运动/尾巴波/时间条）：与玩偶 v2 严格同一只猫（参考图：日系 Q 版沙橙豹纹小猫）
 *
 * 头 / 眼 / 耳 / 斑纹 / 身体 / 腿 / 尾 / 铃铛 的造型、配色表与路径全部拷贝自 plush-v2.js（未改一个控制点），
 * 在其上新增“笼内表演”所需的：手臂（抓栏杆、鼓掌、捂脸、托腮）、表情（泪汪汪、瞪大、螺旋眼、爱心眼、咬牙、吐舌、O 嘴、苦脸）、
 * 眉毛、泪痕、汗滴、炸毛、速度线，以及 14 个 pose 的解析动作与二级运动（耳/尾/铃铛/头的阻尼弹簧滞后）。
 *
 * 纯 ES5，无依赖，不碰 DOM。导出：window.MwrtCatV2 = { draw(ctx,o), anchors(o,ctx), bounds(o), poses, HEIGHT, WIDTH, K, palette(dark) }
 *
 * draw(ctx,o) 与旧 MwrtCatArt.drawCat 同签名：
 *   o = { x, y,        // (x,y) = 脚底中点，向上为 -y，脸朝 +x
 *         s,           // 缩放，默认 1；s=1 时脚底到耳尖 96 逻辑单位（与旧猫同高；头比旧猫大约 20%）
 *         t,           // 秒，驱动呼吸/眨眼/摆尾/二级运动，请传连续时钟
 *         pose,        // idle walk windup charge bonk dizzy plead clap scared jump cute wave sit yawn
 *         look:{x,y},  // -1..1，瞳孔朝向（同时让头略转向）
 *         sx, sy,      // 调用方追加的整体挤压拉伸（以脚底为原点）
 *         rot, pivotY, // 调用方追加的旋转（弧度）与旋转中心（逻辑单位，默认 0=脚底）
 *         flip,        // true=水平镜像（脸朝 -x）
 *         blink,       // 0..1，给了就覆盖自动眨眼
 *         mouth, eye,  // 可选覆盖：数字口型/眼型（见下方 MOUTH_*、EYE_*）
 *         extra:{tailAmp,blush}, dark, palette, shadow,
 *         peek,        // true：只重画头部椭圆（脸），在前景栏杆之后再调一次，让脸从栏杆间鼓出来
         pt }         // 可选：本 pose 已持续的秒数（bonk 的余震用）。不给则 draw 内部按 ctx+pose 自动计时
 *   返回 anchors（同 anchors(o)）：{feet, body, head, top, mouth, handL, handR, eyeL, eyeR, crown, stars, tail, bell, bounds}
 *   全是画布坐标。 head=头中心，top=耳尖，crown=头顶上方一点（爱心/星星出生点），stars=转星星中心，
 *   handL/handR=前爪（抓栏杆/鼓掌时是举起的爪心；站立时是前脚掌）。
 */
(function (root) {
	'use strict';

	var PI = Math.PI, TAU = PI * 2, sin = Math.sin, cos = Math.cos, abs = Math.abs, min = Math.min, max = Math.max, sqrt = Math.sqrt, atan2 = Math.atan2, exp = Math.exp;
	var K = 0.8;                          /* 设计单位 → 逻辑单位。整只猫 120 设计单位 = 96 逻辑单位（旧猫 96） */
	var DESIGN_H = 120;
	var HEIGHT = DESIGN_H * K;
	var WIDTH = 124 * K;
	var POSES = ['idle', 'walk', 'windup', 'charge', 'bonk', 'dizzy', 'plead', 'clap', 'scared', 'jump', 'cute', 'wave', 'sit', 'yawn'];

	function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
	function lerp(a, b, t) { return a + (b - a) * t; }
	function sstep(a, b, x) { var t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
	function pulse(t, period, offset, width) {
		var p = (((t + offset) % period) + period) % period;
		if (p >= width) return 0;
		return sin(p / width * PI);
	}
	function ell(c, x, y, rx, ry) {
		c.moveTo(x + rx, y);
		c.save(); c.translate(x, y); c.scale(rx, ry); c.arc(0, 0, 1, 0, TAU); c.restore();
	}
	function heartPath(c, x, y, s) {
		c.moveTo(x, y + s * 0.92);
		c.bezierCurveTo(x - s * 1.35, y + s * 0.05, x - s * 0.95, y - s * 0.95, x, y - s * 0.38);
		c.bezierCurveTo(x + s * 0.95, y - s * 0.95, x + s * 1.35, y + s * 0.05, x, y + s * 0.92);
		c.closePath();
	}

	/* ---------------- 调色板（与 plush-v2 的 PL/PD 完全相同） ---------------- */
	function mk(dark) {
		return {
			dark: dark,
			ink: '#6a3b24', inkA: 'rgba(106,59,36,0.55)', inkB: 'rgba(106,59,36,0.32)',
			fur: '#f4cb95', furSh: 'rgba(188,106,46,0.30)', lash: '#43251a',
			spot: '#a8693a', spotDk: '#86532a',
			white: '#fffaf2', whiteSh: 'rgba(104,126,186,0.22)',
			pink: '#f8b4ba', pinkDeep: '#ec8190',
			nose: '#f2829a', mouthIn: '#8b3245', tongue: '#f6919f',
			blush: 'rgba(255,112,130,0.52)',
			rim: dark ? 'rgba(255,238,214,0.20)' : 'rgba(255,255,255,0)',
			shadow: dark ? 'rgba(0,0,0,0.42)' : 'rgba(86,110,168,0.38)',
			star: '#f8a458'
		};
	}
	var PL = mk(false), PD = mk(true);
	function isDark(o) {
		if (o.dark != null) return !!o.dark;
		var A = root.MwrtCatArt;
		return !!(A && A.isDark);
	}

	/* ---------------- 线宽（每次 draw 重算，保证最细约 1.1 设备像素） ---------------- */
	var MINW = 0.5;
	function lw(w) { return w > MINW ? w : MINW; }
	function fillInk(c, fill, ink, w) {
		if (fill) { c.fillStyle = fill; c.fill(); }
		if (ink) { c.strokeStyle = ink; c.lineWidth = lw(w); c.stroke(); }
	}
	function rimInk(c, P, w) {
		if (P.dark) { c.strokeStyle = P.rim; c.lineWidth = lw(w + 1.7); c.stroke(); }
		c.strokeStyle = P.ink; c.lineWidth = lw(w); c.stroke();
	}

	/* ---------------- 姿态参数（单例复用，不在每帧分配） ---------------- */
	var Q = {};
	function resetQ(q) {
		/* 与 plush-v2 同名字段 */
		q.bsx = 1; q.bsy = 1; q.by = 0; q.rot = 0; q.bx = 0; q.hx = 0; q.hy = 0; q.hrot = 0.09; q.hsx = 1; q.hsy = 1;
		q.earL = 0; q.earR = 0; q.eye = 0; q.mouth = 0; q.mo = 0; q.blush = 0; q.sit = 0; q.tuck = 0; q.vy = 0; q.pawR = 0;
		q.tailFront = 0; q.tsw = 0; q.tph = 0; q.t0x = 20; q.t0y = -12; q.twAmp = 2; q.twPh = 0;
		q.bellAng = 0; q.bellDy = 0; q.tsx = 1; q.yaw = 0.3; q.lookx = 0; q.looky = 0; q.op = 1; q.stars = 0; q.turn = 1;
		/* v2 新增：表情 */
		q.brow = 0; q.tear = 0; q.sweat = 0; q.ir = 1; q.pp = 1; q.es = 1; q.puff = 0; q.tw = 1; q.lines = 0; q.lid = 0;
		/* 腿：前左/前右的摆角、长度比例；后脚抬起与横移 */
		q.flA = 0; q.frA = 0; q.flS = 1; q.frS = 1; q.hlL = 0; q.hrL = 0; q.hlX = 0; q.hrX = 0;
		/* 手臂：on、爪心目标(身体坐标)、弯曲、抓握(0 掌心/1 握拳/2 手背)、绘制层(0 头后/1 全在头前/2 臂在头后、爪在头前) */
		q.alOn = 0; q.alx = 0; q.aly = 0; q.alB = 0; q.alG = 0;
		q.arOn = 0; q.arx = 0; q.ary = 0; q.arB = 0; q.arG = 0;
		q.armMode = 0; q.armMax = 52;
		setTail(q, 0, 0, 0, 0, 0, 0);
	}
	Q.tp = [0, 0, 0, 0, 0, 0];
	resetQ(Q);
	function setTail(q, a, b, c2, d, e, f) { var p = q.tp; p[0] = a; p[1] = b; p[2] = c2; p[3] = d; p[4] = e; p[5] = f; }

	/* ---------------- 二级运动：无状态弹簧（回看 0.8 秒的一级驱动信号）----------------
	 * 与 plush-v2 同一套：头、耳、尾、铃铛各是一个阻尼弹簧追着“身体顶部的垂直位移 yb”走，差值 d 就是滞后与过冲。
	 * 笼内猫的 yb 由各 pose 的周期运动解析给出（走路弹跳、鼓掌弹跳、冲刺颠簸、受惊颤抖……）。 */
	/* ---------------- 根运动惯性（有状态，存在 ctx 上）----------------
	 * 调用方移动猫（o.x 逐帧变化，比如冲刺）时，耳、尾、铃铛、头由一个阻尼弹簧拖在后面；停下时有过冲回弹。
	 * LG.v 是弹簧当前位移（设计单位）。draw 每帧更新并写回 ctx；anchors(o, ctx) 读 ctx 里最近一次 draw 的值，所以两者一致。 */
	var LG = { v: 0 };
	function lagStep(G, o, s, fl, t) {
		var x = o.x || 0, dt, vx, tgt, n, i, h, w = 15, z = 0.32;
		if (G.lt == null || t < G.lt - 0.001 || t - G.lt > 0.3) { G.lp = 0; G.lv = 0; G.lx = x; G.lt = t; }
		dt = t - G.lt;
		if (dt > 0.0005) {
			vx = (x - G.lx) / dt * fl / (K * s);
			tgt = clamp(vx * 0.03, -6, 6);
			n = Math.ceil(dt / 0.008); h = dt / n;
			for (i = 0; i < n; i++) { G.lv += (w * w * (tgt - G.lp) - 2 * z * w * G.lv) * h; G.lp += G.lv * h; }
			G.lp = clamp(G.lp, -9, 9);
			G.lx = x; G.lt = t;
		}
		LG.v = G.lp;
		return G.lp;
	}

	var SPR_W = [22, 28, 12, 16], SPR_Z = [0.45, 0.28, 0.28, 0.2];
	var SD = [0, 0, 0, 0];
	var WIN = 0.8, NS = 32;
	/* 换 pose 的“一脚冲量”：驱动信号在 pose 起点处跳一个台阶，弹簧追不上就产生滞后与过冲（衰减振铃）。
	 * 单位同下面的周期驱动信号（头 hy 约 0.18 倍、耳 0.05 弧度每单位、尾 0.35~0.7 倍、铃 0.04 弧度每单位）。 */
	var KICK = { windup: -3, charge: 9, bonk: 7, dizzy: 5, plead: -5, clap: 3, scared: 10, jump: 8, cute: 3, wave: 3, walk: 2, sit: -2, yawn: 0, idle: 0 };
	function periodic(pose, tm) {
		switch (pose) {
		case 'walk': return abs(sin(tm * 8.6)) * 2.4;
		case 'charge': return abs(cos(tm * 17)) * 2.2;
		case 'clap': return abs(sin(tm * 7)) * 3.2;
		case 'cute': return abs(sin(tm * 3.1)) * 1.6;
		case 'scared': return sin(tm * 49) * 0.5;
		case 'dizzy': return sin(tm * 3.4) * 2;
		case 'windup': return sin(tm * 58) * 0.5;
		case 'wave': return abs(sin(tm * 7.5)) * 0.6;
		case 'plead': return sin(tm * 31) * 0.3;
		case 'jump': return sin(tm * 9) * 1.2;
		}
		return 0;
	}
	/* tm=绝对时间，tp=该时刻对应的“pose 已持续秒数”（<0 表示 pose 还没开始） */
	function drive(pose, tm, tp) {
		if (tp < 0) return 0;
		return periodic(pose, tm) * sstep(0, 0.12, tp) + (KICK[pose] || 0) * sstep(0, 0.045, tp);
	}
	function springs(pose, t, pt) {
		var i, dt = WIN / NS, y, y0, x0, x1, x2, x3, v0 = 0, v1 = 0, v2 = 0, v3 = 0, a, w, tm;
		SD[0] = SD[1] = SD[2] = SD[3] = 0;
		if (pose === 'idle' || pose === 'sit' || pose === 'yawn') return;
		y0 = drive(pose, t - WIN, pt - WIN);
		x0 = x1 = x2 = x3 = y0;
		for (i = 1; i <= NS; i++) {
			tm = t - WIN + i * dt;
			y = drive(pose, tm, pt - WIN + i * dt);
			w = SPR_W[0]; a = w * w * (y - x0) - 2 * SPR_Z[0] * w * v0; v0 += a * dt; x0 += v0 * dt;
			w = SPR_W[1]; a = w * w * (y - x1) - 2 * SPR_Z[1] * w * v1; v1 += a * dt; x1 += v1 * dt;
			w = SPR_W[2]; a = w * w * (y - x2) - 2 * SPR_Z[2] * w * v2; v2 += a * dt; x2 += v2 * dt;
			w = SPR_W[3]; a = w * w * (y - x3) - 2 * SPR_Z[3] * w * v3; v3 += a * dt; x3 += v3 * dt;
		}
		SD[0] = x0 - y; SD[1] = x1 - y; SD[2] = x2 - y; SD[3] = x3 - y;
	}

	/* ---------------- 各 pose 的解析动作 ----------------
	 * 方向约定：脸朝 +x。ear 角度：正=向外摊平（后压/飞机耳），负=向内竖起。
	 * 手臂目标 (alx,aly) 是爪心，坐标在身体空间（脚底原点，向上为负）。 */
	function solve(name, t, o, pt) {
		var q = Q, k, w, h, sq, open, ph, c, s1, s2, u, e, osc;
		resetQ(q);
		var br = sin(t * 2.1), sw = sin(t * 1.7), sl = sin(t * 1.7 - 0.9);
		q.twPh = t * 3.3;
		setTail(q, 50 + 2 * sw, -2, 62 + 3 * sw, -40 + 1.5 * sw, 38 + 6 * sl, -66 + 2 * sl);
		switch (name) {
		case 'walk':
			ph = t * 8.6; s1 = sin(ph); s2 = abs(s1);
			/* 重心摆动：整个身体随步伐左右移（bx）并侧倾，头反向稳住；每步落地时躯干压缩一下 */
			q.by = -s2 * 1.5; q.rot = s1 * 0.05; q.bx = s1 * 2.8; q.hrot = 0.07 - s1 * 0.05; q.hx = -s1 * 2.2; q.hy = -s2 * 0.7 + 0.6;
			q.bsy = 1 - 0.035 * (1 - s2) * (1 - s2) + 0.012 * cos(ph * 2); q.bsx = 1 + 0.03 * (1 - s2) * (1 - s2) - 0.008 * cos(ph * 2);
			q.flA = s1 * 0.34; q.frA = -s1 * 0.34; q.flS = 1 - 0.3 * max(0, s1); q.frS = 1 - 0.3 * max(0, -s1);
			q.hrL = max(0, s1) * 4.6; q.hlL = max(0, -s1) * 4.6; q.hlX = -s1 * 2.2; q.hrX = -s1 * 2.2;
			q.yaw = 0.62; q.earL = -0.03 + 0.07 * sin(ph * 2); q.earR = 0.02 - 0.07 * sin(ph * 2 + 1);
			q.lookx = 0.5; q.bellAng = sin(ph * 2) * 0.22;
			setTail(q, 50, -2, 62, -40, 38, -66);
			q.tsw = 0.22; q.tph = ph * 0.5; q.twAmp = 0.8; q.twPh = t * 6.5; q.blush = 0.15; q.stars = 0;
			break;
		case 'windup':
			q.bsy = 0.8; q.bsx = 1.12; q.bx = sin(t * 58) * 0.7; q.hy = 5; q.hx = 2; q.hrot = 0.12; q.rot = -0.1; q.yaw = 0.95;
			q.eye = 0; q.op = 0.78; q.pp = 0.7; q.brow = 1; q.mouth = 7; q.mo = 1; q.blush = 0.25;
			q.earL = q.earR = 0.44 + 0.03 * sin(t * 41);
			q.flA = 0.2; q.frA = -0.2; q.flS = 0.86; q.frS = 0.86; q.hlX = -4; q.hrX = 4;
			q.lookx = 0.95; q.looky = 0.05; q.bellAng = sin(t * 55) * 0.08;
			setTail(q, 38, -4, 56, -16, 60, -36); q.twAmp = 1.2; q.twPh = t * 22;
			break;
		case 'charge':
			ph = t * 17; s1 = sin(ph);
			q.rot = 0.2; q.bsx = 1.04; q.bsy = 0.96; q.bx = -3; q.hx = 6; q.hy = 1.5; q.hrot = 0.14; q.yaw = 1;
			q.flA = s1 * 0.5; q.frA = -s1 * 0.5; q.flS = 1 - 0.35 * max(0, s1); q.frS = 1 - 0.35 * max(0, -s1);
			q.hrL = max(0, s1) * 7; q.hlL = max(0, -s1) * 7; q.hlX = -s1 * 3; q.hrX = -s1 * 3;
			q.eye = 0; q.op = 0.8; q.pp = 0.72; q.brow = 1; q.mouth = 2; q.mo = 0.7; q.blush = 0.2;
			q.earL = q.earR = 0.58 + 0.07 * sin(ph * 1.7);
			q.lookx = 1; q.looky = 0.1; q.lines = 1; q.bellAng = -0.6 + 0.2 * sin(t * 28);
			q.t0x = -16; q.t0y = -14;
			setTail(q, -38, -8, -60, -24, -86, -16); q.tsw = 0.16; q.tph = ph; q.twAmp = 1.5; q.twPh = t * 24;
			break;
		case 'bonk':
			/* 撞击瞬间脸被压向栏杆（头横向压扁、竖向拉长、整体前顶），随后回弹过冲：头后仰、耳朵后甩、铃铛甩动 */
			e = exp(-pt * 8); osc = cos(pt * 28); k = e * osc;
			u = sstep(0.14, 0, pt);
			q.bsx = 1 - 0.1 * k; q.bsy = 1 + 0.07 * k; q.bx = 3 * k - 1; q.rot = -0.07 * k;
			q.hsx = 1 - 0.17 * k; q.hsy = 1 + 0.12 * k; q.hx = 6 * k; q.hy = -1.5 * k + 1; q.hrot = 0.06 - 0.2 * e * sin(pt * 28 + 0.4) + 0.1 * u; q.yaw = 1;
			q.eye = 2; q.mouth = 6; q.mo = 1; q.blush = 1; q.brow = 0;
			q.earL = q.earR = 0.5 + 0.42 * e * osc;
			q.bellAng = 1.1 * e * cos(pt * 30);
			q.flA = 0.3; q.frA = -0.3; q.hlX = -3; q.hrX = 3;
			q.alOn = 1; q.arOn = 1; q.alx = -38; q.aly = -50 + 6 * e * osc; q.arx = 38; q.ary = -46 - 6 * e * osc; q.alB = -1; q.arB = 1; q.armMode = 0;
			setTail(q, 52, -6, 72, -34 - 10 * e * osc, 54, -62 + 14 * e * osc); q.twAmp = 1 + 3 * e; q.twPh = t * 20;
			q.looky = 0.1;
			break;
		case 'dizzy':
			k = sin(t * 3.4);
			q.bx = k * 2.5; q.rot = k * 0.07; q.hrot = 0.09 + sin(t * 3.4 - 0.9) * 0.17; q.hx = sin(t * 3.4 - 0.5) * 2; q.hy = 1.5;
			q.yaw = 0.3 + 0.4 * sin(t * 3.4 - 0.5);
			q.eye = 5; q.mouth = 8; q.mo = 0.8 + 0.2 * sin(t * 6); q.blush = 1;
			q.earL = q.earR = 0.5 + 0.1 * k;
			q.bsy = 0.97; q.bsx = 1.04; q.flA = k * 0.15; q.frA = k * 0.15; q.hlX = -2; q.hrX = 2;
			q.t0x = 18; q.t0y = -8; setTail(q, 40, -2, 52, 0, 60, -6 + 2 * k); q.twAmp = 0.6; q.bellAng = k * 0.35;
			q.stars = 1;
			break;
		case 'plead':
			q.bx = sin(t * 31) * 0.35; q.hrot = 0.1 + sin(t * 2.1) * 0.05; q.hy = -1.2; q.yaw = 0.15; q.bsy = 0.99;
			q.eye = 0; q.ir = 1.06; q.pp = 1.18; q.es = 1.05; q.tear = 1; q.brow = -1; q.mouth = 4; q.mo = 0.5 + 0.5 * sin(t * 17); q.blush = 1;
			q.earL = q.earR = 0.3 + 0.03 * sin(t * 5);
			q.alOn = 1; q.arOn = 1; q.alx = -44; q.aly = -58 + sin(t * 4) * 1; q.arx = 44; q.ary = -58 - sin(t * 4) * 1; q.alG = 1; q.arG = 1; q.alB = -0.6; q.arB = 0.6; q.armMode = 2;
			q.looky = -0.3; q.bellAng = sin(t * 31) * 0.1;
			setTail(q, 46, 2, 56, -12, 40, -22 + 3 * sin(t * 2)); q.twAmp = 1.4; q.twPh = t * 2.5;
			break;
		case 'clap':
			ph = t * 14; c = 0.5 + 0.5 * cos(ph);
			q.by = -abs(sin(t * 7)) * 3.2; q.bsy = 1 + 0.03 * sin(t * 14); q.hrot = 0.03 + sin(t * 7) * 0.05; q.yaw = 0.1;
			q.eye = 1; q.mouth = 3; q.mo = 0.85; q.blush = 1;
			q.alOn = 1; q.arOn = 1; q.alx = -(5 + 12 * c); q.aly = -22 + 2 * c; q.arx = 5 + 12 * c; q.ary = -22 + 2 * c; q.alG = 0; q.arG = 0; q.alB = 0.5; q.arB = -0.5; q.armMode = 1;
			q.earL = q.earR = 0.05 + 0.1 * abs(sin(t * 7));
			setTail(q, 50, -2, 64, -36, 42, -64); q.twAmp = 4; q.twPh = t * 12; q.bellAng = sin(t * 14) * 0.25;
			q.tuck = 0.2;
			break;
		case 'scared':
			/* 受惊先“嗖”地蹦起一下（前 0.36 秒的小抛物线 + 拉长），落回后高频发抖 */
			u = pt < 0.36 ? sin(pt / 0.36 * PI) : 0;
			q.by = -5.5 * u; q.bx = sin(t * 49) * 0.95; q.bsy = 1.06 + 0.07 * u; q.bsx = 0.94 - 0.04 * u; q.hy = -2; q.hrot = 0.03; q.yaw = 0.1;
			q.eye = 0; q.es = 1.08; q.ir = 0.86; q.pp = 0.5; q.mouth = 5; q.mo = 1; q.sweat = 1; q.blush = 0.05; q.brow = -0.8; q.puff = 1; q.tw = 1.45;
			q.earL = q.earR = 0.62 + 0.04 * sin(t * 40);
			q.alOn = 1; q.arOn = 1; q.alx = -36; q.aly = -53; q.arx = 36; q.ary = -53; q.alG = 0; q.arG = 0; q.alB = -0.6; q.arB = 0.6; q.armMode = 1;
			q.hlL = 1.2; q.hrL = 1.2; q.bellAng = sin(t * 47) * 0.2;
			setTail(q, 30, -4, 34, -40, 26, -76); q.twAmp = 2.2; q.twPh = t * 30;
			break;
		case 'jump':
			q.bsy = 1.07; q.bsx = 0.95; q.hy = -1.5; q.hrot = 0.04 + 0.03 * sin(t * 8); q.yaw = 0.2;
			q.eye = 1; q.mouth = 3; q.mo = 1; q.blush = 0.7; q.lookx = 0; q.looky = -0.4;
			q.hlL = 8; q.hrL = 8; q.hlX = -3; q.hrX = 3;
			q.alOn = 1; q.arOn = 1; q.alx = -42; q.aly = -80; q.arx = 42; q.ary = -80; q.alG = 0; q.arG = 0; q.alB = -0.4; q.arB = 0.4; q.armMode = 2;
			q.earL = q.earR = 0.3 + 0.1 * sin(t * 11);
			q.bellAng = sin(t * 12) * 0.35;
			setTail(q, 46, -4, 58, -34, 42, -66 + 8 * sin(t * 8)); q.twAmp = 4; q.twPh = t * 10;
			break;
		case 'cute':
			k = sin(t * 2.6); u = (t * 0.55) % 1;
			q.bx = k * 2.4; q.hx = k * 4; q.hrot = 0.2 + k * 0.07; q.hy = 1; q.by = -abs(sin(t * 3.1)) * 1.4; q.rot = k * 0.04; q.yaw = 0.15 + k * 0.2;
			q.eye = (u < 0.62) ? 3 : 1; q.mouth = 1; q.mo = 0.35; q.blush = 1;
			q.alOn = 1; q.arOn = 1; q.alx = -22; q.aly = -40 + k; q.arx = 22; q.ary = -40 + k; q.alG = 0; q.arG = 0; q.alB = 0.6; q.arB = -0.6; q.armMode = 1;
			q.earL = q.earR = -0.1 + 0.04 * sin(t * 6);
			setTail(q, 54, -2, 70, -34, 48 + 4 * sin(t * 5), -64); q.twAmp = 4.5; q.twPh = t * 7; q.bellAng = k * 0.25;
			break;
		case 'wave':
			k = t * 7.5;
			q.hrot = 0.13 + sin(t * 2) * 0.03; q.yaw = 0.3; q.eye = 1; q.mouth = 3; q.mo = 0.6; q.blush = 0.8;
			q.arOn = 1; q.arx = 59 + 4 * sin(k); q.ary = -64 + 3 * cos(k); q.arG = 0; q.arB = 0.4; q.armMode = 2; q.armMax = 66;
			q.earR = sin(k) * 0.05 + 0.04; q.earL = 0.02; q.bsy = 1 + 0.012 * br; q.hy = -0.5; q.bx = -2; q.hx = -1;
			setTail(q, 50 + 4 * sin(t * 6), -2, 63 + 5 * sin(t * 6 - 0.5), -36, 40 + 8 * sin(t * 6 - 1), -62);
			q.twAmp = 3.5; q.twPh = t * 6.5; q.bellAng = sin(k) * 0.12;
			break;
		case 'sit':
			q.sit = 1; q.bsy = 0.9 + br * 0.012; q.bsx = 1.1; q.hy = 5 + br * 0.5;
			q.hrot = 0.17 + sin(t * 0.8) * 0.03; q.yaw = 0.38; q.bx = 2; q.hx = 1.5;
			q.earL = -0.04 - 0.22 * pulse(t, 4.1, 0.6, 0.25);
			q.earR = 0.02 + 0.12 * pulse(t, 5.7, 1.2, 0.22);
			q.t0x = 22; q.t0y = -4; q.tailFront = 1; q.twAmp = 1.2;
			setTail(q, 46, 3, 20, 6, -9, -3); q.tsw = 0.28; q.tph = t * 2.2;
			q.lookx = sin(t * 0.6) * 0.5; q.bellAng = sin(t * 1.3) * 0.08; q.blush = 0.1;
			break;
		case 'yawn':
			k = (t % 3.8) / 3.8;
			open = sstep(0.08, 0.34, k) * (1 - sstep(0.7, 0.92, k));
			q.mouth = 2; q.mo = open; q.hy = -2.5 * open; q.hsy = 1 + 0.05 * open; q.bsy = 1 + 0.045 * open;
			q.eye = open > 0.55 ? 2 : 0; q.op = 1 - open * 1.5; q.hrot = 0.08 - 0.07 * open; q.yaw = 0.3 - 0.2 * open;
			q.earL = q.earR = 0.34 * open; q.bellDy = -1.2 * open;
			setTail(q, 46, -2, 56, -32 + 6 * open, 40, -56 + 8 * open);
			break;
		default: /* idle：歪头 3/4 站姿，重心偏一侧，右前爪轻抬 */
			q.bsy = 1 + 0.018 * br; q.bsx = 1 - 0.01 * br; q.hy = 0.5 * br; q.rot = sin(t * 1.3) * 0.012;
			q.hrot = 0.1 + sin(t * 1.0 + 0.6) * 0.03; q.yaw = 0.42; q.bx = -2.5; q.hx = 1.5;
			q.pawR = 0.55 + 0.12 * sin(t * 1.9) + 0.25 * pulse(t, 6.3, 0.5, 1.1);
			q.earL = -0.04 - 0.2 * pulse(t, 3.7, 0.4, 0.24); q.earR = 0.02 + 0.14 * pulse(t, 5.1, 1, 0.2);
			q.lookx = sin(t * 0.7) * 0.4; q.looky = sin(t * 0.5 + 1) * 0.15; q.bellAng = sin(t * 1.5) * 0.07 - q.rot * 2;
			break;
		}
		/* 二级运动：弹簧位移（含换 pose 冲量）+ 根运动惯性 LG（draw 从 ctx 状态里算出，anchors 读 ctx 里同一份） */
		springs(name, t, pt);
		var dH = SD[0], dE = SD[1], dT = SD[2], dB = SD[3];
		var lg = LG.v;
		q.hy += clamp(-dH * 0.5, -7, 7);
		h = clamp(1 - dH * 0.004, 0.93, 1.07);
		q.hsy *= h; q.hsx *= 1 / sqrt(h);
		q.hx += clamp(-lg * 0.35, -4, 4);
		q.earL += -dE * 0.06 + clamp(lg * 0.05, -0.35, 0.35); q.earR += -dE * 0.06 + clamp(lg * 0.05, -0.35, 0.35);
		q.tp[3] += clamp(-dT * 0.35, -10, 10); q.tp[5] += clamp(-dT * 0.7, -14, 14); q.tp[4] += clamp(dT * 0.15, -4, 4);
		q.tp[4] -= clamp(lg * 2.5, -12, 12); q.tp[2] -= clamp(lg * 1.2, -6, 6); q.tp[5] += clamp(lg * 0.6, -4, 4);
		q.bellDy += -dB * 0.3; q.bellAng += dB * 0.09 + clamp(lg * 0.1, -0.5, 0.5);
		return q;
	}

	/* ---------------- 缓存的渐变（本地坐标，跨帧复用） ---------------- */
	function cache(c) {
		var g = c.__mwcV2;
		if (!g) { g = { ep: {} }; try { c.__mwcV2 = g; } catch (e) {} }
		return g;
	}
	function irisGrad(c) {
		var g = cache(c);
		if (!g.iris) {
			var gr = c.createLinearGradient(0, -12, 0, 13);
			gr.addColorStop(0, '#5a3a1a'); gr.addColorStop(0.42, '#9b7430'); gr.addColorStop(0.74, '#b5a458'); gr.addColorStop(1, '#dcd496');
			g.iris = gr;
		}
		return g.iris;
	}
	function bellGrad(c) {
		var g = cache(c);
		if (!g.bell) {
			var gr = c.createRadialGradient(-1.2, -1.4, 0.4, 0, 0, 4.6);
			gr.addColorStop(0, '#fff3b4'); gr.addColorStop(0.55, '#f6c43c'); gr.addColorStop(1, '#c88410');
			g.bell = gr;
		}
		return g.bell;
	}
	/* 每个 ctx、每个 pose 的“本次持续秒数”：同 pose 间隔 >0.25 秒或时间倒退就当作新一次 */
	function epTime(G, pose, t) {
		var e = G.ep[pose];
		if (!e) { e = G.ep[pose] = { s: t, l: t }; }
		if (t < e.l - 0.001 || t - e.l > 0.25) e.s = t;
		e.l = t;
		return t - e.s;
	}

	/* ---------------- 头部路径（拷自 plush-v2） ---------------- */
	var RS = [
		[39.8, 1, 43.4, 7, 45.8, 14.6],
		[47, 18, 41.4, 19.6, 37.8, 18.6],
		[42.6, 22.4, 45, 28.6, 41.6, 33],
		[39.8, 35, 34, 31.8, 28.6, 29.2],
		[21.4, 33.4, 10.6, 34.4, 0, 34.4]
	];
	function hp(c, bottom) {
		var i, s, px, py, n = RS.length, i0 = bottom ? 1 : 0;
		if (!bottom) { c.moveTo(0, -34); c.bezierCurveTo(21, -34, 37, -23, 38.4, -6); }
		else c.moveTo(RS[0][4], RS[0][5]);
		for (i = i0; i < n; i++) { s = RS[i]; c.bezierCurveTo(s[0], s[1], s[2], s[3], s[4], s[5]); }
		for (i = n - 1; i >= i0; i--) {
			s = RS[i]; px = i ? RS[i - 1][4] : 38.4; py = i ? RS[i - 1][5] : -6;
			c.bezierCurveTo(-s[2], s[3], -s[0], s[1], -px, py);
		}
		if (!bottom) { c.bezierCurveTo(-37, -23, -21, -34, 0, -34); c.closePath(); }
	}
	function muzzle(c) {
		c.moveTo(-52, 24);
		c.bezierCurveTo(-44, 17, -36, 14, -29, 15.5);
		c.bezierCurveTo(-20, 17, -10, 12, -7, 2);
		c.bezierCurveTo(-5.5, -6, -3.6, -12, -2, -19);
		c.bezierCurveTo(-1.4, -22.6, 1.4, -22.6, 2, -19);
		c.bezierCurveTo(3.6, -12, 5.5, -6, 7, 2);
		c.bezierCurveTo(10, 12, 20, 17, 29, 15.5);
		c.bezierCurveTo(36, 14, 44, 17, 52, 24);
		c.lineTo(52, 45); c.lineTo(-52, 45); c.closePath();
	}
	function stripes(c, sd) {
		c.save(); c.scale(sd, 1); c.translate(1.6, -4.5);
		c.moveTo(-6.2, -29); c.bezierCurveTo(-5.4, -23, -5, -18, -4.6, -12.4); c.bezierCurveTo(-10.4, -15.6, -13.4, -22, -14.6, -29); c.closePath();
		c.moveTo(-18.4, -27.6); c.bezierCurveTo(-15, -26, -13.4, -21.6, -13.4, -16.4); c.bezierCurveTo(-18.4, -17.6, -22, -21.6, -24, -26); c.closePath();
		c.restore();
	}
	function cheekPatch(c, sd, k) {
		c.save(); c.scale(sd * k, k);
		c.moveTo(-44, -6); c.bezierCurveTo(-39, -13, -32, -11, -31, -3); c.bezierCurveTo(-30.6, 4, -35.5, 9, -42, 10.5); c.bezierCurveTo(-44.4, 5, -45.4, 0, -44, -6); c.closePath();
		c.restore();
	}

	/* ---------------- 耳朵（拷自 plush-v2） ---------------- */
	function earOuter(c) {
		c.moveTo(7, -24); c.bezierCurveTo(11, -36, 28, -47, 43, -50); c.bezierCurveTo(46.5, -34, 44, -18, 36, -5); c.lineTo(10, -6); c.closePath();
	}
	function earInner(c) {
		c.moveTo(14, -26); c.bezierCurveTo(18, -33, 28, -40, 38, -43.5); c.bezierCurveTo(40, -32, 38.5, -22, 34, -12); c.lineTo(15, -12); c.closePath();
	}
	function earFluff(c) {
		c.moveTo(9, -10); c.lineTo(11.5, -30); c.lineTo(16.5, -24); c.lineTo(19, -40); c.lineTo(24.5, -29); c.lineTo(28.6, -38); c.lineTo(31, -26);
		c.lineTo(36.6, -22); c.lineTo(37, -4); c.lineTo(9, -4); c.closePath();
	}
	function drawEar(c, P, sd, ang) {
		c.save(); c.scale(sd, 1); c.translate(0, -4); c.translate(24, -17); c.rotate(ang); c.translate(-24, 17);
		c.beginPath(); earOuter(c); c.fillStyle = P.fur; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.moveTo(31, -54); c.lineTo(52, -54); c.lineTo(52, -33); c.bezierCurveTo(45, -37, 38, -39, 31, -40); c.closePath(); c.fillStyle = P.spot; c.fill();
		c.restore();
		c.beginPath(); earInner(c); c.fillStyle = P.pink; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.moveTo(13, -27); c.bezierCurveTo(18, -33, 25, -38, 31, -40); c.bezierCurveTo(24, -31, 20, -23, 19, -12); c.lineTo(13, -12); c.closePath(); c.fillStyle = P.pinkDeep; c.fill();
		c.restore();
		c.beginPath(); earFluff(c); c.fillStyle = P.white; c.fill(); c.strokeStyle = P.inkB; c.lineWidth = lw(0.9); c.stroke();
		c.beginPath(); earOuter(c); rimInk(c, P, 1.2);
		c.restore();
	}

	/* ---------------- 眼睛 ---------------- */
	function lashes(c, P, sd, op) {
		c.save(); c.translate(0, 5 * (1 - op)); c.scale(sd, 1);
		c.lineCap = 'round';
		c.beginPath(); c.moveTo(-10.6, -1); c.bezierCurveTo(-9.6, -9.5, -3, -13.8, 3.5, -13.6); c.bezierCurveTo(9, -13.2, 12, -8.4, 11.2, -1.5);
		c.strokeStyle = P.lash; c.lineWidth = lw(2.1); c.stroke();
		c.beginPath(); c.moveTo(-6.4, -11.8); c.bezierCurveTo(-2, -14.2, 5, -14.4, 9.6, -10.6); c.lineWidth = lw(3.7); c.stroke();
		c.beginPath(); c.moveTo(10.6, -5.6); c.quadraticCurveTo(13.8, -6.4, 16.6, -9.6); c.lineWidth = lw(2.2); c.stroke();
		c.beginPath(); c.moveTo(11.4, -1.6); c.quadraticCurveTo(14.6, -2.2, 17, -4.8); c.lineWidth = lw(1.6); c.stroke();
		c.restore();
	}
	/* 睁眼。q.ir 虹膜大小，q.pp 瞳孔大小，q.es 整眼缩放，q.tear 泪眼（水光带 + 第三高光 + 呼吸闪） */
	function eyeOpen(c, P, sd, q, op, lx, ly, love, t) {
		var ix = lx * 2.3, iy = ly * 1.8 + 0.5, topY = -8.5, ir = q.ir, pp = q.pp, es = q.es, tear = q.tear;
		c.save();
		if (es !== 1) { c.translate(0, -2); c.scale(es, es); c.translate(0, 2); }
		c.save();
		c.translate(0, topY); c.scale(1, op); c.translate(0, -topY);
		c.beginPath(); ell(c, 0, 0, 11.6, 13.6); c.fillStyle = '#fffdf6'; c.fill();
		c.save(); c.clip();
		c.beginPath(); ell(c, ix, iy, 10.4 * ir, 12.4 * ir); c.fillStyle = love ? '#f7d8dc' : irisGrad(c); c.fill();
		if (!love) { c.strokeStyle = 'rgba(70,46,18,0.55)'; c.lineWidth = lw(0.9); c.stroke(); }
		if (love) {
			var hs = 6.2 * (1 + 0.1 * sin(t * 9));
			c.beginPath(); heartPath(c, ix, iy - 0.4, hs); c.fillStyle = '#ff4d7c'; c.fill(); c.strokeStyle = '#b21f4f'; c.lineWidth = lw(0.9); c.stroke();
			c.beginPath(); ell(c, ix - 2.4, iy - 3.2, 1.7, 1.2); c.fillStyle = 'rgba(255,255,255,0.95)'; c.fill();
		} else {
			c.beginPath(); ell(c, ix, iy - 0.5, 5.3 * pp, 7.6 * pp); c.fillStyle = '#2a1a12'; c.fill();
			c.beginPath(); ell(c, 0, -11.5, 14, 6.6); c.fillStyle = 'rgba(56,36,14,0.34)'; c.fill();
			var hl = tear ? 1.14 + 0.08 * sin(t * 9 + sd) : 1;
			c.beginPath(); ell(c, ix - 3.3 * hl, iy - 4.8 * hl, 3.35 * hl, 3.35 * hl); c.fillStyle = '#fff'; c.fill();
			c.beginPath(); ell(c, ix + 3.5, iy + 4.4, 1.55, 1.55); c.fillStyle = 'rgba(255,255,255,0.96)'; c.fill();
			if (tear) {
				c.beginPath(); ell(c, ix + 4.6, iy - 3.4, 1.35, 1.35); c.fillStyle = 'rgba(255,255,255,0.95)'; c.fill();
				c.beginPath(); ell(c, ix, 10.2, 10.6, 4.4); c.fillStyle = 'rgba(176,226,255,0.55)'; c.fill();
				c.beginPath(); c.moveTo(-8.6, 8.4); c.bezierCurveTo(-4, 12.2, 4, 12.2, 8.6, 8.4); c.strokeStyle = 'rgba(255,255,255,0.92)'; c.lineWidth = lw(1.3); c.stroke();
			}
		}
		c.restore();
		c.beginPath(); c.moveTo(-9.8, 4); c.bezierCurveTo(-7, 12, 7, 12, 9.6, 4); c.strokeStyle = P.inkA; c.lineWidth = lw(0.9); c.stroke();
		c.restore();
		lashes(c, P, sd, op);
		c.restore();
	}
	/* 晕眼：白眼珠里转螺旋 */
	function eyeSpiral(c, P, sd, t) {
		var i, a, r;
		c.save();
		c.beginPath(); ell(c, 0, 0, 11.6, 13.6); c.fillStyle = '#fffdf6'; c.fill();
		c.save(); c.clip();
		c.beginPath();
		for (i = 0; i <= 54; i++) {
			a = i * 0.4 * sd + t * 7 * sd; r = 0.6 + i * 0.2;
			if (i) c.lineTo(cos(a) * r, sin(a) * r * 1.12); else c.moveTo(cos(a) * r, sin(a) * r * 1.12);
		}
		c.strokeStyle = '#5a3a1a'; c.lineWidth = lw(1.5); c.lineJoin = 'round'; c.stroke();
		c.restore();
		c.beginPath(); ell(c, 0, 0, 11.6, 13.6); c.strokeStyle = P.inkA; c.lineWidth = lw(0.9); c.stroke();
		lashes(c, P, sd, 1);
		c.restore();
	}
	function eyeHappy(c, P, sd) {
		c.save(); c.scale(sd, 1); c.lineCap = 'round';
		c.beginPath(); c.moveTo(-9.6, 5.4); c.bezierCurveTo(-7.4, -6.6, 7.6, -6.6, 9.8, 5.4);
		c.strokeStyle = P.ink; c.lineWidth = lw(2.8); c.stroke();
		c.beginPath(); c.moveTo(9.4, 1.6); c.quadraticCurveTo(12.8, 0.6, 15.2, -2.6); c.lineWidth = lw(2); c.stroke();
		c.restore();
	}
	function eyeShut(c, P, sd) {
		c.save(); c.scale(sd, 1); c.lineCap = 'round';
		c.beginPath(); c.moveTo(-9.6, 0); c.bezierCurveTo(-6.5, 5.6, 6.5, 5.6, 9.8, 0);
		c.strokeStyle = P.ink; c.lineWidth = lw(2.6); c.stroke();
		c.beginPath(); c.moveTo(9.4, 1.8); c.quadraticCurveTo(12.6, 2.6, 15, 5.4); c.lineWidth = lw(1.8); c.stroke();
		c.restore();
	}
	function eyeChevron(c, P, sd) {
		c.save(); c.scale(sd, 1); c.lineCap = 'round'; c.lineJoin = 'round';
		c.beginPath(); c.moveTo(8, -3.4); c.lineTo(-8, 3.6); c.lineTo(8, 9.4);
		c.strokeStyle = P.ink; c.lineWidth = lw(2.7); c.stroke();
		c.restore();
	}

	/* 眉毛：brow>0 怒（内端下压），<0 愁（内端上扬）。sd=-1 左 / +1 右 */
	function drawBrows(c, P, brow, t) {
		var i, sd, a = abs(brow), ix, iy, ox, oy;
		c.save(); c.lineCap = 'round'; c.strokeStyle = P.ink; c.lineWidth = lw(2.1);
		for (i = 0; i < 2; i++) {
			sd = i ? 1 : -1;
			if (brow > 0) { ix = 9.5; iy = -9 + 5 * a; ox = 25.5; oy = -17 - 2.5 * a; }
			else { ix = 9.5; iy = -17 - 4 * a - 0.8 * sin(t * 9 + i); ox = 25.5; oy = -10 + 1.5 * a; }
			c.beginPath(); c.moveTo(sd * ix, iy); c.quadraticCurveTo(sd * (ix + ox) / 2, (iy + oy) / 2 - 1.6, sd * ox, oy); c.stroke();
		}
		c.restore();
	}
	/* 泪痕与泪珠：从下眼睑沿脸颊滑落（头局部坐标，在 translate(sf,0) 之后） */
	function drawTears(c, t) {
		var i, sd, j, u, x, y, a, off;
		c.save(); c.lineCap = 'round';
		for (i = 0; i < 2; i++) {
			sd = i ? 1 : -1;
			c.beginPath(); c.moveTo(sd * 19, 17); c.quadraticCurveTo(sd * 22.5, 24, sd * 20.5, 33);
			c.strokeStyle = 'rgba(176,226,255,0.55)'; c.lineWidth = lw(2.4); c.stroke();
			for (j = 0; j < 2; j++) {
				off = j * 0.5 + i * 0.23;
				u = (t * 0.85 + off) % 1;
				x = sd * (19.4 + 1.8 * u); y = 15.5 + 22 * u;
				a = u < 0.15 ? u / 0.15 : (1 - sstep(0.7, 1, u));
				c.globalAlpha = a;
				c.beginPath(); c.moveTo(x, y - 3.4); c.bezierCurveTo(x + 2.7, y - 0.4, x + 2.4, y + 2.7, x, y + 2.8); c.bezierCurveTo(x - 2.4, y + 2.7, x - 2.7, y - 0.4, x, y - 3.4); c.closePath();
				c.fillStyle = '#c5ecff'; c.fill(); c.strokeStyle = '#69afd8'; c.lineWidth = lw(0.9); c.stroke();
				c.beginPath(); ell(c, x - 0.8, y - 0.6, 0.7, 1); c.fillStyle = 'rgba(255,255,255,0.95)'; c.fill();
			}
		}
		c.globalAlpha = 1;
		c.restore();
	}
	function drawSweat(c, t) {
		var u = (t * 0.9) % 1, x = 31, y = -24 + 12 * u, a = 1 - sstep(0.6, 1, u);
		c.save(); c.globalAlpha = a;
		c.beginPath(); c.moveTo(x, y - 6.5); c.bezierCurveTo(x + 5.4, y - 0.6, x + 4.6, y + 5.4, x, y + 5.6); c.bezierCurveTo(x - 4.6, y + 5.4, x - 5.4, y - 0.6, x, y - 6.5); c.closePath();
		c.fillStyle = '#c5ecff'; c.fill(); c.strokeStyle = '#5aa0cf'; c.lineWidth = lw(1.1); c.stroke();
		c.beginPath(); ell(c, x - 1.6, y - 0.4, 1, 1.8); c.fillStyle = 'rgba(255,255,255,0.95)'; c.fill();
		c.restore();
	}
	/* 炸毛：头顶一圈小毛尖（画在头底下，只露出外缘） */
	function puffSpikes(c, P, amt, t) {
		/* 三簇：头顶一撮 + 左右脸颊各一簇（避开两只耳朵的根部）；每根间隔留缝，长短不一 */
		var g, sd, am, da = 0.15, len, bx0, by0, bx1, by1, tx, ty, a0, a1;
		var TOP = [1.5 * PI - 0.30, 1.5 * PI, 1.5 * PI + 0.30], CHK = [0.06 * PI, 0.17 * PI, 0.28 * PI, 0.39 * PI];
		c.save(); c.lineJoin = 'round';
		for (g = 0; g < 7; g++) {
			for (sd = (g < 3 ? 0 : -1); sd <= (g < 3 ? 0 : 1); sd += 2) {
				am = g < 3 ? TOP[g] : (sd < 0 ? PI - CHK[g - 3] : CHK[g - 3]);
				a0 = am - da; a1 = am + da;
				len = (g < 3 ? (g === 1 ? 14 : 10) : 8 + (g & 1) * 3) * amt + 1.2 * sin(t * 30 + g * 2);
				bx0 = cos(a0) * 40; by0 = -3 + sin(a0) * 32; bx1 = cos(a1) * 40; by1 = -3 + sin(a1) * 32;
				tx = cos(am) * (40 + len); ty = -3 + sin(am) * (32 + len);
				c.beginPath(); c.moveTo(bx0, by0); c.lineTo(tx, ty); c.lineTo(bx1, by1); c.closePath();
				c.fillStyle = P.fur; c.fill(); c.strokeStyle = P.ink; c.lineWidth = lw(1.3); c.stroke();
			}
		}
		c.restore();
	}

	/* ---------------- 嘴（0-3 拷自 plush-v2；4-8 为笼内表演新增） ----------------
	 * MOUTH_ω=0  微笑小嘴  1 小张嘴  2 哈欠/怒吼(带牙)  3 大笑  4 苦脸(带颤)  5 O 嘴  6 波浪嘴(痛)  7 咬牙  8 吐舌 */
	function noseLine(c, P) {
		c.beginPath(); c.moveTo(0, 20.2); c.lineTo(0, 22);
		c.strokeStyle = P.ink; c.lineWidth = lw(1.4); c.stroke();
	}
	function mouthDraw(c, P, q, t) {
		var m, op, hgt;
		c.lineCap = 'round';
		if (q.mouth === 0) {
			c.beginPath(); c.moveTo(0, 20.2); c.lineTo(0, 22.4);
			c.moveTo(0, 22.4); c.bezierCurveTo(-1.4, 26.4, -6.8, 26.6, -9.4, 21.6);
			c.moveTo(0, 22.4); c.bezierCurveTo(1.4, 26.4, 6.8, 26.6, 9.4, 21.6);
			c.strokeStyle = P.ink; c.lineWidth = lw(1.5); c.stroke();
		} else if (q.mouth === 1 || q.mouth === 3) {
			m = q.mouth === 3 ? 1 : 0.55 + 0.45 * q.mo;
			if (q.mouth === 3) m = 0.5 + 0.5 * q.mo;
			noseLine(c, P);
			c.beginPath(); c.moveTo(-8.6, 22); c.bezierCurveTo(-8.2, 22.4 + 6 * m, 8.2, 22.4 + 6 * m, 8.6, 22); c.bezierCurveTo(4, 20.8, -4, 20.8, -8.6, 22); c.closePath();
			c.fillStyle = P.mouthIn; c.fill();
			c.save(); c.clip(); c.beginPath(); ell(c, 0, 28.6 + 1.5 * m, 5, 3.2 * m + 1); c.fillStyle = P.tongue; c.fill(); c.restore();
			c.strokeStyle = P.ink; c.lineWidth = lw(1.4); c.stroke();
		} else if (q.mouth === 2) {
			op = q.mo; hgt = 3 + 8 * op;
			noseLine(c, P);
			c.beginPath(); c.moveTo(-8.2 - 1.6 * op, 22.2); c.bezierCurveTo(-8.6 - 2 * op, 22.4 + hgt * 1.3, 8.6 + 2 * op, 22.4 + hgt * 1.3, 8.2 + 1.6 * op, 22.2); c.bezierCurveTo(4, 20.8, -4, 20.8, -8.2 - 1.6 * op, 22.2); c.closePath();
			c.fillStyle = P.mouthIn; c.fill();
			c.save(); c.clip(); c.beginPath(); ell(c, 0, 22.6 + hgt, 5.6, 0.55 * hgt + 1); c.fillStyle = P.tongue; c.fill();
			c.beginPath(); c.moveTo(-5.2, 20.6); c.lineTo(-3.4, 24 + 1.4 * op); c.lineTo(-1.6, 20.6); c.moveTo(1.6, 20.6); c.lineTo(3.4, 24 + 1.4 * op); c.lineTo(5.2, 20.6); c.fillStyle = P.white; c.fill();
			c.restore();
			c.strokeStyle = P.ink; c.lineWidth = lw(1.5); c.stroke();
		} else if (q.mouth === 4) {
			/* 苦脸：嘴角下垂，中间一个发颤的小开口 */
			var w4 = 1.25 + 0.12 * q.mo, h4 = 3 + 3 * q.mo;
			noseLine(c, P);
			c.beginPath(); c.moveTo(-7.4 * w4, 27.6); c.bezierCurveTo(-5, 23.2, -2.4, 22.6, 0, 22.8); c.bezierCurveTo(2.4, 22.6, 5, 23.2, 7.4 * w4, 27.6);
			c.bezierCurveTo(4, 25.4 + h4 * 0.4, -4, 25.4 + h4 * 0.4, -7.4 * w4, 27.6); c.closePath();
			c.fillStyle = P.mouthIn; c.fill();
			c.save(); c.clip(); c.beginPath(); ell(c, 0, 28.8 + h4 * 0.2, 4.2, 2.4); c.fillStyle = P.tongue; c.fill(); c.restore();
			c.strokeStyle = P.ink; c.lineWidth = lw(1.9); c.stroke();
		} else if (q.mouth === 5) {
			/* O 嘴（惊） */
			var oy = 1 + 0.08 * sin(t * 40);
			noseLine(c, P);
			c.beginPath(); ell(c, 0, 27, 3.8, 4.9 * oy); c.fillStyle = P.mouthIn; c.fill();
			c.save(); c.clip(); c.beginPath(); ell(c, 0, 30.2, 3, 2); c.fillStyle = P.tongue; c.fill(); c.restore();
			c.beginPath(); ell(c, 0, 27, 3.8, 4.9 * oy); c.strokeStyle = P.ink; c.lineWidth = lw(1.5); c.stroke();
		} else if (q.mouth === 6) {
			/* 撞疼：张开的波浪嘴（两道波，里面深色+舌尖），线条加粗，小尺寸也读得出 */
			noseLine(c, P);
			c.beginPath(); c.moveTo(-11, 24); c.bezierCurveTo(-7.6, 20.6, -4.6, 20.6, -2.4, 24); c.bezierCurveTo(-0.8, 27, 0.8, 27, 2.4, 24); c.bezierCurveTo(4.6, 20.6, 7.6, 20.6, 11, 24);
			c.bezierCurveTo(9, 32.4, -9, 32.4, -11, 24); c.closePath();
			c.fillStyle = P.mouthIn; c.fill();
			c.save(); c.clip(); c.beginPath(); ell(c, 0, 31.4, 5.2, 3.2); c.fillStyle = P.tongue; c.fill(); c.restore();
			c.strokeStyle = P.ink; c.lineWidth = lw(1.8); c.stroke();
		} else if (q.mouth === 7) {
			/* 咬牙：一排牙齿 */
			noseLine(c, P);
			c.beginPath(); c.moveTo(-9.2, 24); c.bezierCurveTo(-8, 22.4, -4, 22.6, 0, 23.6); c.bezierCurveTo(4, 22.6, 8, 22.4, 9.2, 24);
			c.bezierCurveTo(8.6, 28.6, -8.6, 28.6, -9.2, 24); c.closePath();
			c.fillStyle = P.white; c.fill();
			c.strokeStyle = P.ink; c.lineWidth = lw(1.5); c.stroke();
			c.beginPath(); c.moveTo(-4.6, 23.2); c.lineTo(-4.6, 27.6); c.moveTo(0, 23.6); c.lineTo(0, 28); c.moveTo(4.6, 23.2); c.lineTo(4.6, 27.6);
			c.strokeStyle = P.inkA; c.lineWidth = lw(1); c.stroke();
		} else {
			/* 8 吐舌：ω 小嘴 + 垂下的舌头 */
			var tg = 3.6 + 2.4 * q.mo;
			c.beginPath(); c.moveTo(0, 20.2); c.lineTo(0, 22.4);
			c.moveTo(0, 22.4); c.bezierCurveTo(-1.4, 26, -6.8, 26.2, -9.4, 21.6);
			c.moveTo(0, 22.4); c.bezierCurveTo(1.4, 26, 6.8, 26.2, 9.4, 21.6);
			c.strokeStyle = P.ink; c.lineWidth = lw(1.5); c.stroke();
			c.beginPath(); c.moveTo(0.6, 25.2); c.bezierCurveTo(-2.6, 26, -3.4, 31 + tg * 0.3, 1.6, 31.6 + tg * 0.3); c.bezierCurveTo(6.4, 31.8, 5.8, 26, 3.4, 25.2); c.closePath();
			c.fillStyle = P.tongue; c.fill(); c.strokeStyle = P.ink; c.lineWidth = lw(1.2); c.stroke();
			c.beginPath(); c.moveTo(2, 26.4); c.lineTo(2, 29.4); c.strokeStyle = P.inkA; c.lineWidth = lw(0.9); c.stroke();
		}
	}

	/* ---------------- 头（拷自 plush-v2，加入眉/泪/汗/炸毛/新眼型） ---------------- */
	function drawHead(c, P, q, t, blink) {
		var yaw = q.yaw, sh = yaw * 2.2, sf = yaw * 4.6, op;
		drawEar(c, P, -1, q.earL);
		drawEar(c, P, 1, q.earR);
		if (q.puff > 0) puffSpikes(c, P, q.puff, t);
		c.beginPath(); hp(c, false); c.fillStyle = P.fur; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-60, -40, 120, 90); c.save(); c.translate(-3.4, -5); hp(c, false); c.restore(); c.fillStyle = P.furSh; c.fill('evenodd');
		c.save(); c.translate(sh, 0);
		c.beginPath(); stripes(c, -1); stripes(c, 1);
		cheekPatch(c, -1, 1.05); cheekPatch(c, 1, 0.86);
		c.moveTo(-40, -15); c.bezierCurveTo(-36, -22, -28, -20, -29, -14); c.bezierCurveTo(-30, -10, -37, -9, -40, -15); c.closePath();
		c.moveTo(38, -17); c.bezierCurveTo(33, -22, 27, -19, 29, -14); c.bezierCurveTo(31, -10, 36, -10, 38, -17); c.closePath();
		c.fillStyle = P.spot; c.fill();
		c.beginPath(); muzzle(c); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-60, -40, 120, 100); c.save(); c.translate(-2.4, -3.4); hp(c, false); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.restore();
		c.restore();
		c.beginPath(); hp(c, false); rimInk(c, P, 1.2);
		c.beginPath(); hp(c, true); c.strokeStyle = P.ink; c.lineWidth = lw(2); c.stroke();
		c.beginPath();
		c.moveTo(37.6, 17.8); c.lineTo(33.4, 23); c.moveTo(-37.6, 17.8); c.lineTo(-33.4, 23);
		c.moveTo(41.5, 6); c.lineTo(38, 10.6); c.moveTo(-41.5, 6); c.lineTo(-38, 10.6);
		c.strokeStyle = P.inkA; c.lineWidth = lw(1); c.stroke();
		c.save(); c.translate(sf, 0);
		if (q.blush > 0) {
			c.beginPath(); ell(c, -26.5, 17.5, 6.6, 3.8); ell(c, 26.5, 17.5, 6.6, 3.8);
			c.fillStyle = P.blush; c.globalAlpha = min(1, q.blush); c.fill(); c.globalAlpha = 1;
		}
		op = clamp(q.op, 0, 1) * (1 - blink);
		var i, ex, sd;
		for (i = 0; i < 2; i++) {
			sd = i ? 1 : -1; ex = sd * 17.4;
			c.save(); c.translate(ex, 4);
			if (q.eye === 1) eyeHappy(c, P, sd);
			else if (q.eye === 2) eyeChevron(c, P, sd);
			else if (q.eye === 5) eyeSpiral(c, P, sd, t);
			else if (op < 0.16) eyeShut(c, P, sd);
			else eyeOpen(c, P, sd, q, op, q.lookx + (i ? 0.1 : -0.1), q.looky, q.eye === 3, t);
			c.restore();
		}
		if (q.brow !== 0) { c.save(); c.translate(0, 4); drawBrows(c, P, q.brow, t); c.restore(); }
		if (q.tear > 0) drawTears(c, t);
		c.beginPath(); c.moveTo(-3.7, 17.4); c.bezierCurveTo(-3.9, 15.8, 3.9, 15.8, 3.7, 17.4); c.bezierCurveTo(3.5, 19.4, 1.3, 20.4, 0, 20.6); c.bezierCurveTo(-1.3, 20.4, -3.5, 19.4, -3.7, 17.4); c.closePath();
		fillInk(c, P.nose, P.ink, 1);
		c.beginPath(); ell(c, -1.1, 17.2, 1.1, 0.6); c.fillStyle = 'rgba(255,255,255,0.7)'; c.fill();
		mouthDraw(c, P, q, t);
		c.restore();
		c.beginPath();
		c.moveTo(30, 15.5); c.quadraticCurveTo(43, 12, 55, 13);
		c.moveTo(30, 19.5); c.quadraticCurveTo(44, 19, 56, 21.6);
		c.moveTo(-30, 15.5); c.quadraticCurveTo(-43, 12, -55, 13);
		c.moveTo(-30, 19.5); c.quadraticCurveTo(-44, 19, -56, 21.6);
		c.strokeStyle = P.inkB; c.lineWidth = lw(0.8); c.stroke();
		if (q.sweat > 0) drawSweat(c, t);
	}

	/* ---------------- 身体 / 腿（拷自 plush-v2，腿和后脚加了摆角/抬起参数） ---------------- */
	function bodyPath(c) {
		c.moveTo(-13, -44);
		c.bezierCurveTo(-25, -38, -30, -23, -28, -11);
		c.bezierCurveTo(-27, -3, -20, 1, -12, 1);
		c.lineTo(12, 1);
		c.bezierCurveTo(20, 1, 27, -3, 28, -11);
		c.bezierCurveTo(30, -23, 25, -38, 13, -44);
		c.closePath();
	}
	function legPath(c) {
		c.moveTo(-5, 0); c.bezierCurveTo(-5.4, 9, -5.6, 17, -6.4, 22);
		c.bezierCurveTo(-8.6, 24, -8.6, 28.4, -3, 28.6);
		c.lineTo(3, 28.6);
		c.bezierCurveTo(8.6, 28.4, 8.6, 24, 6.4, 22);
		c.bezierCurveTo(5.6, 17, 5.4, 9, 5, 0); c.closePath();
	}
	function legOpen(c) {
		c.moveTo(-5, 0); c.bezierCurveTo(-5.4, 9, -5.6, 17, -6.4, 22);
		c.bezierCurveTo(-8.6, 24, -8.6, 28.4, -3, 28.6);
		c.lineTo(3, 28.6);
		c.bezierCurveTo(8.6, 28.4, 8.6, 24, 6.4, 22);
		c.bezierCurveTo(5.6, 17, 5.4, 9, 5, 0);
	}
	var BIB = [-15, -44, -17.5, -36, -18, -28, -22, -22, -15.5, -20, -18, -12, -11.5, -14, -13, -4, -9, 3, 0, 3.5, 9, 3, 13, -4, 11.5, -14, 18, -12, 15.5, -20, 22, -22, 18, -28, 17.5, -36, 15, -44];
	function bibPath(c) {
		var i; c.moveTo(BIB[0], BIB[1]);
		for (i = 2; i < BIB.length; i += 2) c.lineTo(BIB[i], BIB[i + 1]);
		c.closePath();
	}
	function drawLeg(c, P, x, y, sy, ang, pad, spot, closed, sxw) {
		c.save(); c.translate(x, y); if (ang) c.rotate(ang); c.scale(sxw || 1, sy);
		c.beginPath(); legPath(c); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		if (spot) { c.beginPath(); c.moveTo(-9, 2); c.bezierCurveTo(-4, 0, 3, 3, 4, 10); c.bezierCurveTo(-1, 13, -6, 13, -9, 11); c.closePath(); c.fillStyle = P.spot; c.fill(); }
		c.beginPath(); c.rect(-12, -4, 24, 36); c.save(); c.translate(-2.6, -2.4); legPath(c); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		if (pad) {
			c.beginPath(); ell(c, 0, 19.2, 3.6, 3); ell(c, -3.8, 14.2, 1.7, 1.9); ell(c, 0, 12.6, 1.8, 2); ell(c, 3.8, 14.2, 1.7, 1.9);
			c.fillStyle = P.pinkDeep; c.fill();
		} else {
			c.beginPath(); c.moveTo(-2.2, 21.4); c.lineTo(-2.2, 26); c.moveTo(2.2, 21.4); c.lineTo(2.2, 26); c.strokeStyle = P.inkA; c.lineWidth = lw(0.9); c.stroke();
		}
		c.beginPath(); if (closed) legPath(c); else legOpen(c); rimInk(c, P, 1.3);
		c.restore();
	}
	function mittenPath(c) {
		c.moveTo(-4.8, 0); c.bezierCurveTo(-4.8, -3.8, 4.8, -3.8, 4.8, 0);
		c.bezierCurveTo(5.2, 8, 9.4, 11, 9, 18); c.bezierCurveTo(8.8, 26, -8.8, 26, -9, 18);
		c.bezierCurveTo(-9.4, 11, -5.2, 8, -4.8, 0); c.closePath();
	}
	/* 圆头手套：原点=腕，+y 为指向。kind 0 掌心（粉肉垫）/ 2 手背（无肉垫、几道指缝） */
	function drawMitten(c, P, x, y, ang, sc, kind) {
		c.save(); c.translate(x, y); c.rotate(ang); if (sc !== 1) c.scale(sc, sc);
		c.beginPath(); mittenPath(c); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-14, -6, 28, 36); c.save(); c.translate(-2.6, -2.2); mittenPath(c); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		if (kind === 2) {
			c.beginPath(); c.moveTo(-3, 19); c.lineTo(-3, 23.4); c.moveTo(3, 19); c.lineTo(3, 23.4); c.strokeStyle = P.inkA; c.lineWidth = lw(0.9 / sc); c.stroke();
		} else {
			c.beginPath(); ell(c, 0, 19.2, 4.4, 3.6); ell(c, -5, 12.8, 2, 2.3); ell(c, 0, 11.2, 2.1, 2.4); ell(c, 5, 12.8, 2, 2.3);
			c.fillStyle = P.pinkDeep; c.fill();
		}
		c.beginPath(); mittenPath(c); rimInk(c, P, 1.3 / sc);
		c.restore();
	}
	/* 握拳（抓栏杆）：圆圆的白手套，三道指缝，一个拇指鼓包 */
	function drawFist(c, P, x, y, sd) {
		c.save(); c.translate(x, y); c.scale(sd, 1);
		c.beginPath(); ell(c, 0, 0, 8.6, 9.4); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-14, -14, 28, 28); ell(c, -2.4, -2.2, 8.6, 9.4); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.beginPath(); ell(c, 0, 0, 8.6, 9.4); rimInk(c, P, 1.3);
		c.beginPath(); c.moveTo(-5.2, -4.4); c.quadraticCurveTo(0, -2.6, 5.4, -4.6); c.moveTo(-5.6, 0.4); c.quadraticCurveTo(0, 2.2, 5.6, 0.2); c.moveTo(-5, 4.8); c.quadraticCurveTo(0, 6.4, 5, 4.6);
		c.strokeStyle = P.inkA; c.lineWidth = lw(1); c.stroke();
		c.beginPath(); ell(c, 5.8, -7.2, 3.2, 2.7); c.fillStyle = P.white; c.fill(); c.strokeStyle = P.ink; c.lineWidth = lw(1.1); c.stroke();
		c.restore();
	}
	function hindFoot(c, P, x, y, rx, ry) {
		c.beginPath(); ell(c, x, y, rx, ry); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(x - rx - 2, y - ry - 2, rx * 2 + 4, ry * 2 + 4); ell(c, x - 1.4, y - 1.8, rx, ry); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.beginPath(); ell(c, x, y, rx, ry); rimInk(c, P, 1.3);
		c.beginPath(); c.moveTo(x - 1.6, y + ry - 3.4); c.lineTo(x - 1.6, y + ry - 0.4); c.moveTo(x + 1.8, y + ry - 3.4); c.lineTo(x + 1.8, y + ry - 0.4); c.strokeStyle = P.inkA; c.lineWidth = lw(0.8); c.stroke();
	}
	/* 手臂：毛色上臂（带暖棕描边的圆头胶囊，沿弯曲中线），末端接手套/拳。几何存 AG，两次调用（臂/爪）共用。 */
	var AG = { sx: 0, sy: 0, tx: 0, ty: 0, wx: 0, wy: 0, ux: 0, uy: 1, ang: 0, len: 0, cx: 0, cy: 0 };
	var SHX = 15, SHY = -33;
	function armGeom(sd, tx, ty, bend, maxLen) {
		var sx = sd * SHX, sy = SHY, dx = tx - sx, dy = ty - sy, l = sqrt(dx * dx + dy * dy) || 1;
		if (l > maxLen) { dx *= maxLen / l; dy *= maxLen / l; l = maxLen; tx = sx + dx; ty = sy + dy; }
		AG.sx = sx; AG.sy = sy; AG.tx = tx; AG.ty = ty; AG.len = l; AG.ux = dx / l; AG.uy = dy / l;
		AG.ang = atan2(-dx, dy);
		AG.wx = tx - AG.ux * 11; AG.wy = ty - AG.uy * 11;
		AG.cx = (sx + AG.wx) / 2 - AG.uy * bend * 5 * sd; AG.cy = (sy + AG.wy) / 2 + AG.ux * bend * 5 * sd;
	}
	function armPath(c) {
		c.moveTo(AG.sx, AG.sy); c.quadraticCurveTo(AG.cx, AG.cy, AG.wx + AG.ux * 3, AG.wy + AG.uy * 3);
	}
	function drawArmBase(c, P) {
		c.save(); c.lineCap = 'round';
		if (P.dark) { c.beginPath(); armPath(c); c.strokeStyle = P.rim; c.lineWidth = lw(12.6); c.stroke(); }
		c.beginPath(); armPath(c); c.strokeStyle = P.ink; c.lineWidth = lw(11); c.stroke();
		c.beginPath(); armPath(c); c.strokeStyle = P.fur; c.lineWidth = lw(8.6); c.stroke();
		c.save(); c.translate(1.3, 1.5); c.beginPath(); armPath(c); c.strokeStyle = P.furSh; c.lineWidth = lw(2.4); c.stroke(); c.restore();
		c.restore();
	}
	function drawArmHand(c, P, sd, grip) {
		if (grip === 1) drawFist(c, P, AG.tx, AG.ty, sd);
		else drawMitten(c, P, AG.wx, AG.wy, AG.ang, 0.86, grip);
	}
	function drawBody(c, P, q) {
		var sit = q.sit;
		c.save(); c.scale(q.bsx, q.bsy);
		if (!sit) { hindFoot(c, P, -25.5 + q.hlX, -2.6 - q.hlL, 7.4, 4.4); hindFoot(c, P, 25.5 + q.hrX, -2.6 - q.hrL, 7.4, 4.4); }
		c.beginPath(); bodyPath(c); c.fillStyle = P.fur; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-40, -50, 80, 60); c.save(); c.translate(-4.4, -4.4); bodyPath(c); c.restore(); c.fillStyle = P.furSh; c.fill('evenodd');
		c.beginPath();
		c.moveTo(-33, -36); c.bezierCurveTo(-24, -42, -13, -34, -15.5, -23); c.bezierCurveTo(-18, -12, -26, -5, -33, -10); c.closePath();
		c.moveTo(31, -22); c.bezierCurveTo(27, -26, 20, -24, 20.5, -16); c.bezierCurveTo(21, -8, 27, -5, 32, -9); c.closePath();
		c.moveTo(-12, -42); c.bezierCurveTo(-9, -46, 0, -46, 4, -43); c.bezierCurveTo(2, -38, -6, -37, -12, -42); c.closePath();
		c.moveTo(20, -40); c.bezierCurveTo(23, -43, 29, -39, 28, -33); c.bezierCurveTo(25, -31, 20, -34, 20, -40); c.closePath();
		c.fillStyle = P.spot; c.fill();
		c.beginPath(); ell(c, -24, -19, 3.4, 5); ell(c, 25.5, -13, 2.2, 3.2); ell(c, -5, -35, 2.2, 1.6); ell(c, 24.6, -29, 1.8, 1.8);
		c.fillStyle = P.spotDk; c.fill();
		c.beginPath(); bibPath(c); c.fillStyle = P.white; c.fill();
		c.beginPath(); c.rect(-24, -46, 48, 50); c.save(); c.translate(-1.6, -1.2); bibPath(c); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.beginPath(); bodyPath(c); rimInk(c, P, 1.3);
		c.beginPath(); c.moveTo(-27.8, -8); c.bezierCurveTo(-27, -3, -20, 1, -12, 1); c.lineTo(12, 1); c.bezierCurveTo(20, 1, 27, -3, 27.8, -8); c.strokeStyle = P.ink; c.lineWidth = lw(2.2); c.stroke();
		c.beginPath(); bibPath(c); c.strokeStyle = P.inkB; c.lineWidth = lw(1); c.stroke();
		if (sit) { hindFoot(c, P, -23, -3, 9, 5.2); hindFoot(c, P, 23, -3, 9, 5.2); }
		/* 前腿：举臂的一侧不画腿 */
		var ty = 1 - 0.3 * q.tuck, pr = q.pawR;
		if (!q.alOn) drawLeg(c, P, -9.6, -29, ty * q.flS, 0.05 + q.flA, false, true);
		if (!q.arOn) drawLeg(c, P, 9.6, -29, ty * q.frS * (1 - 0.18 * pr), -0.05 - 0.42 * pr + q.frA, false, false);
		c.restore();
	}

	/* ---------------- 尾巴（拷自 plush-v2，加粗细倍率 q.tw） ---------------- */
	var TX = [], TY = [], NT = 20, OX = [], OY = [];
	function tailPts(q) {
		var p = q.tp, x0 = q.t0x, y0 = q.t0y, i, u, v, dx, dy, ln, a;
		for (i = 0; i <= NT; i++) {
			u = i / NT; v = 1 - u;
			TX[i] = v * v * v * x0 + 3 * v * v * u * p[0] + 3 * v * u * u * p[2] + u * u * u * p[4];
			TY[i] = v * v * v * y0 + 3 * v * v * u * p[1] + 3 * v * u * u * p[3] + u * u * u * p[5];
		}
		if (q.tsw) {
			var rx = TX[0], ry = TY[0], cs, sn2;
			for (i = 1; i <= NT; i++) {
				u = i / NT; a = q.tsw * sin(q.tph - 5.5 * u) * u; cs = cos(a); sn2 = sin(a);
				dx = TX[i] - rx; dy = TY[i] - ry; TX[i] = rx + dx * cs - dy * sn2; TY[i] = ry + dx * sn2 + dy * cs;
			}
		}
		if (q.twAmp > 0) {
			for (i = 1; i <= NT; i++) {
				dx = TX[min(NT, i + 1)] - TX[i - 1]; dy = TY[min(NT, i + 1)] - TY[i - 1]; ln = sqrt(dx * dx + dy * dy) || 1;
				u = i / NT; a = q.twAmp * u * u * sin(q.twPh - 5.5 * u);
				OX[i] = -dy / ln * a; OY[i] = dx / ln * a;
			}
			for (i = 1; i <= NT; i++) { TX[i] += OX[i]; TY[i] += OY[i]; }
		}
	}
	function tailStroke(c, i0, i1) {
		var i; c.beginPath(); c.moveTo(TX[i0], TY[i0]);
		for (i = i0 + 1; i <= i1; i++) c.lineTo(TX[i], TY[i]);
	}
	function drawTail(c, P, tw) {
		c.save(); c.lineJoin = 'round';
		if (P.dark) { tailStroke(c, 0, NT); c.lineCap = 'round'; c.strokeStyle = P.rim; c.lineWidth = lw(12.4 * tw); c.stroke(); }
		tailStroke(c, 0, NT); c.lineCap = 'round'; c.strokeStyle = P.ink; c.lineWidth = lw(10.4 * tw); c.stroke();
		tailStroke(c, 0, NT); c.strokeStyle = P.fur; c.lineWidth = lw(8.6 * tw); c.stroke();
		c.lineCap = 'butt';
		c.strokeStyle = P.spot; c.lineWidth = lw(8.6 * tw);
		tailStroke(c, 4, 6); c.stroke();
		tailStroke(c, 8, 10); c.stroke();
		tailStroke(c, 12, 14); c.stroke();
		tailStroke(c, 16, 17); c.stroke();
		c.lineCap = 'round'; tailStroke(c, 18, NT); c.strokeStyle = P.white; c.lineWidth = lw(8.6 * tw); c.stroke();
		tailStroke(c, 1, 17); c.strokeStyle = P.furSh; c.lineWidth = lw(2.2 * tw); c.translate(1.4, 1.8); c.stroke();
		c.restore();
	}

	/* ---------------- 铃铛（拷自 plush-v2） ---------------- */
	function drawBell(c, q) {
		c.save(); c.translate(q.hx * 0.3, -38 + q.bellDy * 0.2); c.rotate(q.bellAng);
		c.beginPath(); c.moveTo(-8.5, -3); c.quadraticCurveTo(0, 5, 8.5, -3); c.strokeStyle = '#a5681a'; c.lineWidth = lw(1.1); c.stroke();
		c.translate(0, 13 + q.bellDy * 0.5);
		c.beginPath(); c.arc(0, -4.6, 1.3, 0, TAU); c.strokeStyle = '#7a4912'; c.lineWidth = lw(0.9); c.stroke();
		c.beginPath(); c.arc(0, 0, 3.9, 0, TAU); c.fillStyle = bellGrad(c); c.fill(); c.strokeStyle = '#7a4912'; c.lineWidth = lw(1.2); c.stroke();
		c.beginPath(); c.moveTo(-3.4, 1.1); c.lineTo(3.4, 1.1); c.moveTo(0, 1.1); c.lineTo(0, 3.3); c.strokeStyle = '#7a4912'; c.lineWidth = lw(0.9); c.stroke();
		c.restore();
	}
	function star(c, x, y, r, rot) {
		var i, a, rr;
		for (i = 0; i < 10; i++) {
			a = rot + i * PI / 5 - PI / 2; rr = (i & 1) ? r * 0.5 : r;
			if (i) c.lineTo(x + cos(a) * rr, y + sin(a) * rr); else c.moveTo(x + cos(a) * rr, y + sin(a) * rr);
		}
		c.closePath();
	}
	/* 晕星：绕头顶旋转的三颗星，椭圆轨道，后半圈画小一点（在头后面感觉由 draw 决定先后）。cx,cy=设计坐标 */
	function drawStars(c, P, t, cx, cy, front) {
		var i, a, sn, r;
		c.save(); c.lineJoin = 'round';
		for (i = 0; i < 3; i++) {
			a = t * 3.2 + i * TAU / 3; sn = sin(a);
			if ((sn >= 0) !== front) continue;
			r = 5.4 + 1.6 * sn;
			c.beginPath(); star(c, cx + cos(a) * 44, cy + sn * 10, r, a * 0.6);
			c.fillStyle = P.star; c.fill(); c.strokeStyle = '#e07a35'; c.lineWidth = lw(1.2); c.stroke();
		}
		c.restore();
	}
	/* 冲刺速度线（画在猫身后，设计坐标；方向 -x） */
	function drawSpeedLines(c, P, t) {
		var li, ly0, off;
		c.save(); c.lineCap = 'round';
		c.strokeStyle = P.dark ? 'rgba(255,255,255,0.7)' : 'rgba(106,59,36,0.5)'; c.lineWidth = lw(2.4);
		for (li = 0; li < 4; li++) {
			ly0 = -18 - li * 20; off = ((t * 9 + li * 0.37) % 1) * 16;
			c.beginPath(); c.moveTo(-50 - off, ly0); c.lineTo(-78 - off - li * 5, ly0 + (li - 1.5) * 0.5); c.stroke();
		}
		c.restore();
	}

	/* ---------------- 坐标换算、锚点与包围盒 ---------------- */
	var TMP = { s: 1, sxx: 1, syy: 1, fl: 1, pv: 0, rot: 0, x: 0, y: 0, lift: 0 };
	var PTMP = { x: 0, y: 0 };
	var BB = { l: 0, t: 0, r: 0, b: 0 };
	function pt(oo, lx, ly, out) {
		var s = oo.s, X = lx * K * s * oo.sxx * oo.fl, Y = ly * K * s * oo.syy;
		var pyW = oo.pv * K * s * oo.syy, r = oo.rot, cr = cos(r), sr = sin(r), yy = Y - pyW;
		out.x = oo.x + X * cr - yy * sr; out.y = oo.y + oo.lift + pyW + X * sr + yy * cr;
		return out;
	}
	function bbAdd(oo, lx, ly) {
		pt(oo, lx, ly, PTMP);
		if (PTMP.x < BB.l) BB.l = PTMP.x; if (PTMP.x > BB.r) BB.r = PTMP.x;
		if (PTMP.y < BB.t) BB.t = PTMP.y; if (PTMP.y > BB.b) BB.b = PTMP.y;
	}
	var HT = { x: 0, y: 0 };
	function headPt(q, hcy, lx, ly, out) {
		var sx2 = q.hsx * (1 + (q.bsx - 1) * 0.5), sy2 = q.hsy * (1 + (q.bsy - 1) * 0.6), cr = cos(q.hrot), sr = sin(q.hrot);
		var X = lx * sx2, Y = ly * sy2;
		out.x = (q.bx + q.hx) + X * cr - Y * sr; out.y = hcy + X * sr + Y * cr;
		return out;
	}
	/* 手臂爪心（设计坐标，已含 bx、bsx、bsy）；没举臂时是前脚掌 */
	function handDesign(q, sd, out) {
		var on = sd < 0 ? q.alOn : q.arOn, tx, ty;
		if (on) {
			tx = sd < 0 ? q.alx : q.arx; ty = sd < 0 ? q.aly : q.ary;
			armGeom(sd, tx, ty, 0, q.armMax);
			out.x = q.bx + AG.tx * q.bsx; out.y = AG.ty * q.bsy;
		} else {
			out.x = q.bx + sd * 9.6 * q.bsx; out.y = (-29 + 24 * (sd < 0 ? q.flS : q.frS)) * q.bsy;
		}
		return out;
	}
	var HD = { x: 0, y: 0 };
	function boundsOf(oo, q, hcy, showStars) {
		var i, u, e, pad = 4;
		BB.l = 1e9; BB.r = -1e9; BB.t = 1e9; BB.b = -1e9;
		bbAdd(oo, q.bx - 38 * q.bsx, 6); bbAdd(oo, q.bx + 38 * q.bsx, 6); bbAdd(oo, q.bx - 31 * q.bsx, -46 * q.bsy); bbAdd(oo, q.bx + 31 * q.bsx, -46 * q.bsy);
		tailPts(q);
		for (i = 0; i <= NT; i += 2) {
			e = TX[i] * q.tsx + q.bx;
			bbAdd(oo, e - pad * q.tw - 2, TY[i] - pad * q.tw - 2); bbAdd(oo, e + pad * q.tw + 2, TY[i] + pad * q.tw + 2);
		}
		var hp2 = [-58, -64, 58, -64, 0, -40, -60, 26, 60, 26, 0, 38, -48, 16, 48, 16, -50, -30, 50, -30];
		for (i = 0; i < hp2.length; i += 2) { headPt(q, hcy, hp2[i], hp2[i + 1], HT); bbAdd(oo, HT.x, HT.y); }
		if (q.alOn) { handDesign(q, -1, HD); bbAdd(oo, HD.x - 12, HD.y - 12); bbAdd(oo, HD.x + 12, HD.y + 12); }
		if (q.arOn) { handDesign(q, 1, HD); bbAdd(oo, HD.x - 12, HD.y - 12); bbAdd(oo, HD.x + 12, HD.y + 12); }
		if (q.lines) { bbAdd(oo, -110, -80); bbAdd(oo, -50, -10); }
		if (q.puff) { headPt(q, hcy, -54, -48, HT); bbAdd(oo, HT.x, HT.y); headPt(q, hcy, 54, -48, HT); bbAdd(oo, HT.x, HT.y); }
		if (showStars) { headPt(q, hcy, -56, -70, HT); bbAdd(oo, HT.x, HT.y); headPt(q, hcy, 56, -52, HT); bbAdd(oo, HT.x, HT.y); }
		u = 4 * K * oo.s;
		return { l: BB.l - u, t: BB.t - u, r: BB.r + u, b: BB.b + u };
	}
	function mkP(oo, lx, ly) { return pt(oo, lx, ly, { x: 0, y: 0 }); }
	function anchorsOf(oo, q, hcy, showStars) {
		var hxx = q.bx + q.hx, sf = q.yaw * 4.6, a = {};
		a.feet = mkP(oo, q.bx, 0);
		a.body = mkP(oo, q.bx, -22 * q.bsy);
		a.head = mkP(oo, hxx, hcy);
		headPt(q, hcy, 0, -54, HT); a.top = mkP(oo, HT.x, HT.y);
		headPt(q, hcy, 0, -74, HT); a.crown = mkP(oo, HT.x, HT.y);
		headPt(q, hcy, 0, -66, HT); a.stars = mkP(oo, HT.x, HT.y);
		headPt(q, hcy, sf, 22, HT); a.mouth = mkP(oo, HT.x, HT.y);
		headPt(q, hcy, sf - 17.4, 4, HT); a.eyeL = mkP(oo, HT.x, HT.y);
		headPt(q, hcy, sf + 17.4, 4, HT); a.eyeR = mkP(oo, HT.x, HT.y);
		handDesign(q, -1, HD); a.handL = mkP(oo, HD.x, HD.y);
		handDesign(q, 1, HD); a.handR = mkP(oo, HD.x, HD.y);
		a.bell = mkP(oo, hxx * 0.3 + q.bx, -25);
		a.tail = mkP(oo, q.bx + TX[NT] * q.tsx, TY[NT]);
		return a;
	}

	/* ---------------- 主入口 ---------------- */
	function sstepPulse(t) {
		var b = max(pulse(t, 3.9, 0.35, 0.2), pulse(t, 9.1, 1.2, 0.18), pulse(t, 9.1, 1.58, 0.18));
		return b * b * (3 - 2 * b);
	}
	function poseOf(o) {
		var pose = o.pose || 'idle', i; for (i = 0; i < POSES.length && POSES[i] !== pose; i++);
		return i === POSES.length ? 'idle' : pose;
	}
	/* 把 o 里的各种覆盖项写进 q（look、mouth/eye 覆盖、extra） */
	function applyOverrides(q, o, pose) {
		var ex = o.extra || {};
		if (o.look && o.look.x != null) {
			q.lookx = o.look.x;
			/* 头略转向光标（不影响固定朝向的冲刺/撞击/晕） */
			if (pose !== 'charge' && pose !== 'windup' && pose !== 'bonk' && pose !== 'dizzy' && pose !== 'walk') q.yaw = clamp(q.yaw + o.look.x * 0.32, -0.7, 1);
		}
		if (o.look && o.look.y != null) q.looky = o.look.y;
		if (o.mouth != null && typeof o.mouth === 'number') q.mouth = o.mouth;
		if (o.eye != null && typeof o.eye === 'number') q.eye = o.eye;
		if (ex.tailAmp != null) q.twAmp = ex.tailAmp * 12;
		if (ex.blush != null) q.blush = ex.blush;
	}

	function draw(ctx, o) {
		o = o || {};
		var c = ctx, pose = poseOf(o), t = o.t || 0, P = o.palette || (isDark(o) ? PD : PL);
		var G = cache(c);
		var ptm = (o.pt != null) ? o.pt : epTime(G, pose, t);
		var s = o.s == null ? 1 : o.s, sxx = o.sx == null ? 1 : o.sx, syy = o.sy == null ? 1 : o.sy;
		lagStep(G, o, s, o.flip ? -1 : 1, t);
		var q = solve(pose, t, o, ptm);
		applyOverrides(q, o, pose);
		var base = 1, m = null;
		try { m = c.getTransform ? c.getTransform() : null; } catch (e) { m = null; }
		base = m ? sqrt(m.a * m.a + m.b * m.b) : (o.dpr || 1);
		MINW = 1.1 / (base * s * K * max(0.2, min(sxx, syy)));
		var blink = (o.blink != null) ? o.blink : sstepPulse(t);
		if (q.eye !== 0 || q.tear > 0 || q.brow !== 0) blink = (o.blink != null) ? o.blink : 0;
		var lift = q.by * K * s * syy;
		var pv = (o.pivotY || 0) / K;
		var rot = (o.rot || 0) + q.rot;
		var x = o.x || 0, y = o.y || 0;
		if (o.shadow) {
			c.save(); c.translate(x, y); c.scale(s * sxx * K, s * syy * K);
			c.beginPath(); ell(c, 5 + q.bx * 0.6, 1.2, 36, 6.2); c.fillStyle = P.shadow; c.fill(); c.restore();
		}
		c.save();
		c.translate(x, y + lift);
		var pyW = pv * K * s * syy;
		if (rot) { c.translate(0, pyW); c.rotate(rot); c.translate(0, -pyW); }
		var fl = (o.flip ? -1 : 1) * q.turn;
		c.scale(s * sxx * K * fl, s * syy * K);
		c.lineJoin = 'round'; c.lineCap = 'round';
		if (o.peek) {
			/* 探脸：只重画头部椭圆里的内容（不含耳朵尖），给调用方在“前景栏杆之后”再调一次，让脸从栏杆间鼓出来 */
			c.save(); c.translate(q.bx + q.hx, -44 * q.bsy - 22 + q.hy); c.rotate(q.hrot); c.scale(q.hsx * (1 + (q.bsx - 1) * 0.5), q.hsy * (1 + (q.bsy - 1) * 0.6));
			c.beginPath(); ell(c, 0, 3, 41, 33); c.restore(); c.clip();
		}
		if (q.lines && !o.peek) drawSpeedLines(c, P, t);
		tailPts(q);
		c.save(); c.translate(q.bx, 0);
		if (!q.tailFront) { c.save(); c.scale(q.tsx, 1); drawTail(c, P, q.tw); c.restore(); }
		drawBody(c, P, q);
		if (q.tailFront) { c.save(); c.scale(q.tsx, 1); drawTail(c, P, q.tw); c.restore(); }
		c.restore();
		/* 臂：armMode 0/2 画在头后（爪也在头后/头前），1 全在头前 */
		var gl = 0, gr = 0;
		if (q.alOn) { armGeom(-1, q.alx, q.aly, q.alB, q.armMax); }
		if (q.armMode !== 1) {
			c.save(); c.translate(q.bx, 0); c.scale(q.bsx, q.bsy);
			if (q.alOn) { armGeom(-1, q.alx, q.aly, q.alB, q.armMax); drawArmBase(c, P); if (q.armMode === 0) drawArmHand(c, P, -1, q.alG); }
			if (q.arOn) { armGeom(1, q.arx, q.ary, q.arB, q.armMax); drawArmBase(c, P); if (q.armMode === 0) drawArmHand(c, P, 1, q.arG); }
			c.restore();
		}
		c.save(); c.translate(q.bx, 0); drawBell(c, q); c.restore();
		var hcy = -44 * q.bsy - 22 + q.hy;
		var showStars = !!o.stars;
		if (showStars) drawStars(c, P, t, q.bx + q.hx, hcy - 50, false);
		c.save(); c.translate(q.bx + q.hx, hcy); c.rotate(q.hrot); c.scale(q.hsx * (1 + (q.bsx - 1) * 0.5), q.hsy * (1 + (q.bsy - 1) * 0.6));
		drawHead(c, P, q, t, blink);
		c.restore();
		if (q.armMode !== 0 && (q.alOn || q.arOn)) {
			c.save(); c.translate(q.bx, 0); c.scale(q.bsx, q.bsy);
			if (q.alOn) { armGeom(-1, q.alx, q.aly, q.alB, q.armMax); if (q.armMode === 1) drawArmBase(c, P); drawArmHand(c, P, -1, q.alG); }
			if (q.arOn) { armGeom(1, q.arx, q.ary, q.arB, q.armMax); if (q.armMode === 1) drawArmBase(c, P); drawArmHand(c, P, 1, q.arG); }
			c.restore();
		}
		void gl; void gr;
		if (showStars) drawStars(c, P, t, q.bx + q.hx, hcy - 50, true);
		c.restore();
		TMP.s = s; TMP.sxx = sxx; TMP.syy = syy; TMP.fl = fl; TMP.pv = pv; TMP.rot = rot; TMP.x = x; TMP.y = y; TMP.lift = lift;
		var an = anchorsOf(TMP, q, hcy, showStars);
		an.bounds = boundsOf(TMP, q, hcy, showStars);
		return an;
	}

	/* 不绘制，只算锚点（与 draw 同一套 solve/弹簧）。o.pt 可选。 */
	function anchors(o, ctx) {
		o = o || {};
		var pose = poseOf(o), t = o.t || 0, ptm = (o.pt != null) ? o.pt : 0.3;
		var s = o.s == null ? 1 : o.s, sxx = o.sx == null ? 1 : o.sx, syy = o.sy == null ? 1 : o.sy;
		LG.v = (ctx && ctx.__mwcV2 && ctx.__mwcV2.lp) || 0;
		var q = solve(pose, t, o, ptm);
		applyOverrides(q, o, pose);
		var pv = (o.pivotY || 0) / K, rot = (o.rot || 0) + q.rot;
		TMP.s = s; TMP.sxx = sxx; TMP.syy = syy; TMP.fl = (o.flip ? -1 : 1) * q.turn; TMP.pv = pv; TMP.rot = rot;
		TMP.x = o.x || 0; TMP.y = o.y || 0; TMP.lift = q.by * K * s * syy;
		var hcy = -44 * q.bsy - 22 + q.hy, showStars = !!o.stars;
		tailPts(q);
		var an = anchorsOf(TMP, q, hcy, showStars);
		an.bounds = boundsOf(TMP, q, hcy, showStars);
		return an;
	}
	/* 包围盒，相对脚底。worst:true 取一个周期内的并集 */
	function bounds(o) {
		o = o || {};
		var i, n = o.worst ? 20 : 1, r = { l: 1e9, t: 1e9, r: -1e9, b: -1e9 }, oo, b, k;
		for (i = 0; i < n; i++) {
			oo = {}; for (k in o) { if (o.hasOwnProperty(k)) oo[k] = o[k]; }
			oo.x = 0; oo.y = 0; if (o.worst) oo.t = (o.t || 0) + i * 0.173;
			b = anchors(oo).bounds;
			if (b.l < r.l) r.l = b.l; if (b.r > r.r) r.r = b.r; if (b.t < r.t) r.t = b.t; if (b.b > r.b) r.b = b.b;
		}
		return { l: r.l, t: r.t, r: r.r, b: r.b, w: r.r - r.l, h: r.b - r.t };
	}

	root.MwrtCatV2 = { draw: draw, anchors: anchors, bounds: bounds, poses: POSES, HEIGHT: HEIGHT, WIDTH: WIDTH, K: K, palette: function (d) { return d ? PD : PL; } };
})(typeof window !== 'undefined' ? window : this);

/* plush-v2.js —— 解锁后「玩偶猫」v2 终版（日系 chibi 小猫，对照 _s90/ref/plush-ref.png）
 *
 * 沙橙底色 + 棕褐豹纹斑，白脸下半/白胸/白手套，暖棕金绿大眼，粉三角耳（耳内白绒），
 * 小粉鼻 + ω 嘴，脸颊长鬓毛，胸前小金铃铛，尾巴棕环 + 白尖。赛璐璐平涂 + 一阶阴影 + 暖棕变粗细描边。
 * 纯 ES5、无依赖、不碰 DOM。
 *
 * 导出：window.MwrtPlushV2 = { draw(ctx,o), bounds(o), hopLift(phase,pose), poses, HEIGHT, WIDTH, K }
 *
 * draw(ctx,o) 与旧 drawPlush 同签名、同锚点语义：
 *   o = { x, y,        // (x,y) = 脚底中点（猫站着的地面点），向上为 -y，脸朝 +x
 *         s,           // 缩放，默认 1。s=1 时脚底到耳尖约 60 逻辑单位（旧版约 110）
 *         t,           // 秒，驱动呼吸/眨眼/摆尾；也是二级运动（弹簧）的时间轴，请传连续时钟
 *         pose,        // idle | hop | flip | sit | wave | yawn | chase-tail | love | land
 *         phase,       // 0..1，hop/flip/land 由它驱动（缺省时按 t 循环）
 *         dur,         // hop/flip/land 一个 phase 周期对应的秒数（弹簧回看用），默认 0.62/0.82/0.5
 *         sx, sy,      // 调用方追加的整体挤压拉伸（以脚底为原点），默认 1（hop/flip/land 自带挤压，建议不传）
 *         rot,         // 调用方追加的旋转（弧度）；flip 自带旋转，调用方传 0
 *         flip,        // true=水平镜像（脸朝 -x）
 *         blink,       // 0..1，给了就覆盖自动眨眼
 *         look:{x,y},  // -1..1，瞳孔朝向
 *         dark,        // true=深色侧栏（加一圈很淡的轮廓光、阴影改黑）
 *         shadow,      // false=不画地面影子（调用方自己画）
 *         noLift,      // true=hop/flip 不做内部腾空（调用方已按抛物线改 y）；selfLift:false 与之等价
 *         hopH,        // 内部腾空高度（设计单位，默认 30，约 15 逻辑单位）
 *         stars,       // true/false 强制开关橙色小星星；缺省只在 idle/sit/wave/love 显示
 *         pivotY,      // 旋转中心（设计单位，默认 -58，头身之间）
 *         dpr }        // 取不到 ctx.getTransform 时用来估算最细描边，默认 1
 *   返回 { feet, head, top, body, mouth, face, bounds:{l,t,r,b} }，全是画布坐标。
 *   bounds 是本帧猫的包围盒（画布坐标，含旋转、腾空、尾巴、耳朵、星星）。
 *
 * bounds(o)：不绘制。返回相对锚点 (o.x,o.y)=脚底 的包围盒 {l,t,r,b,w,h,extraTop}，t 为负表示在脚底之上。
 *   给 phase/t 时是该 pose 在那一刻的盒；给 worst:true 时是整个周期的并集（一次性给画布留顶部余量）。
 *   已乘 s、sx、sy。不含 noLift 时调用方自己的抛物线高度。extraTop = 比站立耳尖多出的顶部余量。
 *
 * 设计坐标系：脚底中点为原点，-y 向上，整只猫高约 120 设计单位，内部乘 K=0.5 变成约 60 逻辑单位。
 */
(function (root) {
	'use strict';

	var PI = Math.PI, TAU = PI * 2, sin = Math.sin, cos = Math.cos, abs = Math.abs, min = Math.min, max = Math.max, sqrt = Math.sqrt, atan2 = Math.atan2;
	var K = 0.5;
	var DESIGN_H = 120;
	var HEIGHT = DESIGN_H * K;
	var WIDTH = 124 * K;
	var PIVOT = -58;

	function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
	function lerp(a, b, t) { return a + (b - a) * t; }
	function sstep(a, b, x) { var t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
	function pulse(t, period, offset, width) {
		var p = (((t + offset) % period) + period) % period;
		if (p >= width) return 0;
		return sin(p / width * PI);
	}
	function ell(c, x, y, rx, ry) {
		c.moveTo(x + rx, y);
		c.save(); c.translate(x, y); c.scale(rx, ry); c.arc(0, 0, 1, 0, TAU); c.restore();
	}
	function heartPath(c, x, y, s) {
		c.moveTo(x, y + s * 0.92);
		c.bezierCurveTo(x - s * 1.35, y + s * 0.05, x - s * 0.95, y - s * 0.95, x, y - s * 0.38);
		c.bezierCurveTo(x + s * 0.95, y - s * 0.95, x + s * 1.35, y + s * 0.05, x, y + s * 0.92);
		c.closePath();
	}

	/* ---------------- 调色板 ---------------- */
	function mk(dark) {
		return {
			dark: dark,
			ink: '#6a3b24', inkA: 'rgba(106,59,36,0.55)', inkB: 'rgba(106,59,36,0.32)',
			fur: '#f4cb95', furSh: 'rgba(188,106,46,0.30)', lash: '#43251a',
			spot: '#a8693a', spotDk: '#86532a',
			white: '#fffaf2', whiteSh: 'rgba(104,126,186,0.22)',
			pink: '#f8b4ba', pinkDeep: '#ec8190',
			nose: '#f2829a', mouthIn: '#8b3245', tongue: '#f6919f',
			blush: 'rgba(255,112,130,0.52)',
			rim: dark ? 'rgba(255,238,214,0.20)' : 'rgba(255,255,255,0)',
			shadow: dark ? 'rgba(0,0,0,0.42)' : 'rgba(86,110,168,0.38)',
			star: '#f8a458'
		};
	}
	var PL = mk(false), PD = mk(true);

	/* ---------------- 线宽（每次 draw 重算，保证最细约 1.1 设备像素） ---------------- */
	var MINW = 0.5;
	function lw(w) { return w > MINW ? w : MINW; }
	function fillInk(c, fill, ink, w) {
		if (fill) { c.fillStyle = fill; c.fill(); }
		if (ink) { c.strokeStyle = ink; c.lineWidth = lw(w); c.stroke(); }
	}
	function rimInk(c, P, w) {
		if (P.dark) { c.strokeStyle = P.rim; c.lineWidth = lw(w + 1.7); c.stroke(); }
		c.strokeStyle = P.ink; c.lineWidth = lw(w); c.stroke();
	}

	/* ---------------- 姿态参数（单例复用，不在每帧分配） ---------------- */
	var Q = {
		bsx: 1, bsy: 1, by: 0, rot: 0, bx: 0, hx: 0, hy: 0, hrot: 0.08, hsx: 1, hsy: 1,
		earL: 0, earR: 0, eye: 0, mouth: 0, mo: 0, blush: 0, sit: 0, tuck: 0, vy: 0, pawR: 0,
		armOn: 0, armAng: 0, tailFront: 0, t0x: 20, t0y: -12, tp: [0, 0, 0, 0, 0, 0], twAmp: 2, twPh: 0,
		bellAng: 0, bellDy: 0, tsx: 1, yaw: 0.3, lookx: 0, looky: 0, op: 1, stars: 0, turn: 1, loveP: 0, pivot: PIVOT,
		dg: 0, hang: 0, legL: 0, legR: 0, armW: 0, gx: 0, gy: 0, gsp: 5
	};
	function resetQ(q) {
		q.bsx = 1; q.bsy = 1; q.by = 0; q.rot = 0; q.bx = 0; q.hx = 0; q.hy = 0; q.hrot = 0.09; q.hsx = 1; q.hsy = 1;
		q.earL = 0; q.earR = 0; q.eye = 0; q.mouth = 0; q.mo = 0; q.blush = 0; q.sit = 0; q.tuck = 0; q.vy = 0; q.pawR = 0;
		q.armOn = 0; q.armAng = 0; q.tailFront = 0; q.t0x = 20; q.t0y = -12; q.twAmp = 2; q.twPh = 0;
		q.bellAng = 0; q.bellDy = 0; q.tsx = 1; q.yaw = 0.3; q.lookx = 0; q.looky = 0; q.op = 1; q.stars = 0; q.turn = 1; q.loveP = 0; q.pivot = PIVOT;
		q.dg = 0; q.hang = 0; q.legL = 0; q.legR = 0; q.armW = 0; q.gx = 0; q.gy = 0; q.gsp = 5;
	}
	function setTail(q, a, b, c2, d, e, f) { var p = q.tp; p[0] = a; p[1] = b; p[2] = c2; p[3] = d; p[4] = e; p[5] = f; }

	var POSES = ['idle', 'hop', 'flip', 'sit', 'wave', 'yawn', 'chase-tail', 'love', 'land', 'drag'];
	var DURS = { hop: 0.62, flip: 0.82, land: 0.5 };

	/* ---------------- 二级运动：无状态弹簧（回看 0.8 秒的一级驱动信号） ----------------
	 * 驱动信号 yb = 身体“顶部”的垂直位置（设计单位，向上为正），由 pose 的相位/时间解析给出。
	 * 头、耳、尾、铃铛各是一个阻尼弹簧追着它走，位移差 d = 跟随者 - 驱动 就是滞后和过冲。
	 * 半隐式欧拉，32 个采样。切 pose 时，窗口里切换点之前的采样按旧 pose 的延续值算，
	 * 切换后 0.12 秒内线性混合到新 pose，所以不会跳变。 */
	var SPR_W = [22, 28, 12, 16], SPR_Z = [0.6, 0.3, 0.3, 0.2];
	var SD = [0, 0, 0, 0];
	var WIN = 0.8, NS = 32, BLEND = 0.12;

	function hasDrive(p) { return p === 'hop' || p === 'flip' || p === 'land' || p === 'love' || p === 'chase-tail'; }
	function ybPhase(pose, p, hopH) {
		var w, k, sq;
		if (pose === 'hop') {
			if (p < 0.2) return -8.8 * sstep(0, 0.2, p);
			if (p < 0.82) { w = (p - 0.2) / 0.62; return hopH * 4 * w * (1 - w); }
			k = (p - 0.82) / 0.18; return -9.7 * sin(k * PI) * (1 - k * 0.3);
		}
		if (pose === 'flip') {
			if (p < 0.16) return -9.7 * sstep(0, 0.16, p);
			if (p < 0.86) { w = (p - 0.16) / 0.7; return hopH * 1.25 * 4 * w * (1 - w); }
			k = (p - 0.86) / 0.14; return -10 * sin(k * PI);
		}
		/* land */
		if (p < 0.34) return -11 * sstep(0, 0.34, p);
		if (p < 0.7) { w = (p - 0.34) / 0.36; sq = 1 - sstep(0, 1, w); return -11 * sq + 2.8 * sin(w * PI); }
		return 0;
	}
	function ybTime(pose, tm) {
		if (pose === 'love') return abs(sin(tm * 4.2)) * 3.4;
		return abs(sin(tm * 4.4)) * 2.6;           /* chase-tail */
	}
	function isPhasePose(p) { return p === 'hop' || p === 'flip' || p === 'land'; }
	/* ago 秒前的驱动值。cur/prev 的相位按各自 dur 倒推 */
	function ybAt(G, pose, t, ph, cyc, dur, hopH, ago) {
		var tm = t - ago, p, yc = 0, yp = 0, w;
		if (hasDrive(pose)) {
			if (isPhasePose(pose)) {
				p = cyc ? ((((tm) * 0.9) % 1) + 1) % 1 : clamp(ph - ago / dur, 0, 1);
				yc = ybPhase(pose, p, hopH);
			} else yc = ybTime(pose, tm);
		}
		if (G && G.prev && tm < G.sw + BLEND) {
			if (hasDrive(G.prev)) {
				if (isPhasePose(G.prev)) { p = clamp(G.prevPh + (tm - G.sw) / G.prevDur, 0, 1); yp = ybPhase(G.prev, p, hopH); }
				else yp = ybTime(G.prev, tm);
			}
			w = clamp((tm - G.sw) / BLEND, 0, 1);
			return lerp(yp, yc, w);
		}
		return yc;
	}
	function springs(G, pose, t, ph, cyc, dur, hopH) {
		var j, i, dt = WIN / NS, y, y0, x0 = 0, x1 = 0, x2 = 0, x3 = 0, v0 = 0, v1 = 0, v2 = 0, v3 = 0, a, w;
		SD[0] = SD[1] = SD[2] = SD[3] = 0;
		if (!hasDrive(pose) && !(G && G.prev && hasDrive(G.prev) && t < G.sw + WIN)) return;
		y0 = ybAt(G, pose, t, ph, cyc, dur, hopH, WIN);
		x0 = x1 = x2 = x3 = y0;
		for (i = 1; i <= NS; i++) {
			y = ybAt(G, pose, t, ph, cyc, dur, hopH, WIN - i * dt);
			w = SPR_W[0]; a = w * w * (y - x0) - 2 * SPR_Z[0] * w * v0; v0 += a * dt; x0 += v0 * dt;
			w = SPR_W[1]; a = w * w * (y - x1) - 2 * SPR_Z[1] * w * v1; v1 += a * dt; x1 += v1 * dt;
			w = SPR_W[2]; a = w * w * (y - x2) - 2 * SPR_Z[2] * w * v2; v2 += a * dt; x2 += v2 * dt;
			w = SPR_W[3]; a = w * w * (y - x3) - 2 * SPR_Z[3] * w * v3; v3 += a * dt; x3 += v3 * dt;
		}
		SD[0] = x0 - y; SD[1] = x1 - y; SD[2] = x2 - y; SD[3] = x3 - y;
	}

	/* ---------------- 拖拽姿态 'drag'（草稿 b 新增）----------------
	 * DG 由 drag.js 的物理每帧填写；w=0 时 'drag' 姿态 == idle（用于落地后无缝交回待机）。 */
	var DG = {
		w: 0, bsx: 1, bsy: 1, by: 0, bx: 0, hx: 0, hy: 0, hrot: 0.09, yaw: 0.3, earL: 0, earR: 0,
		eye: 0, mouth: 0, mo: 0, blush: 1, op: 1, lookx: 0, looky: 0, bellAng: 0, bellDy: 0,
		hang: 0, armW: 0, gx: 0, gy: -120, gsp: 4.5, legL: 0, legR: 0, twAmp: 1.5, twPh: 0,
		tp: [50, -2, 62, -40, 38, -66]
	};
	function applyDrag(q) {
		var w = DG.w, i;
		if (w <= 0) return;
		q.dg = 1;
		q.bsx = lerp(q.bsx, DG.bsx, w); q.bsy = lerp(q.bsy, DG.bsy, w); q.by = lerp(q.by, DG.by, w);
		q.hx = lerp(q.hx, DG.hx, w); q.hy = lerp(q.hy, DG.hy, w); q.bx = lerp(q.bx, DG.bx, w);
		q.hrot = lerp(q.hrot, DG.hrot, w); q.yaw = lerp(q.yaw, DG.yaw, w);
		q.earL = lerp(q.earL, DG.earL, w); q.earR = lerp(q.earR, DG.earR, w);
		q.lookx = lerp(q.lookx, DG.lookx, w); q.looky = lerp(q.looky, DG.looky, w);
		q.bellAng = lerp(q.bellAng, DG.bellAng, w); q.bellDy = lerp(q.bellDy, DG.bellDy, w);
		q.blush = lerp(q.blush, DG.blush, w); q.op = lerp(q.op, DG.op, w);
		q.twAmp = lerp(q.twAmp, DG.twAmp, w); q.twPh = DG.twPh;
		for (i = 0; i < 6; i++) q.tp[i] = lerp(q.tp[i], DG.tp[i], w);
		if (w > 0.5) { q.eye = DG.eye; q.mouth = DG.mouth; q.mo = DG.mo; }
		q.hang = DG.hang * w; q.legL = DG.legL * w; q.legR = DG.legR * w;
		q.armW = DG.armW * w; q.gx = DG.gx; q.gy = DG.gy; q.gsp = DG.gsp;
	}
	/* 被提起的双臂：pass 0 画在头后面（粗白色软管，从肩出发，经头顶两耳之间微向内收，再略张开伸向抓点），
	 * pass 1 画在头前面（两只白手套并排握住抓点）。坐标为设计单位、身体坐标系（脚底为原点）。
	 * armW 0..1 控制伸缩（松手后收回肩旁）。手套中心在抓点正下 1px 内，两手套中点的横坐标 = 抓点。 */
	function drawArms(c, P, q, pass) {
		var i, sd, ax, ay, gx, gy, dx, dy, ln, ux, uy, nx, ny, bl, c1x, c1y, c2x, c2y, ex, ey, tx, ty, tl, w = q.armW;
		c.save(); c.translate(q.bx, 0); c.lineCap = 'round'; c.lineJoin = 'round';
		for (i = 0; i < 2; i++) {
			sd = i ? 1 : -1;
			ax = sd * 15 * q.bsx; ay = -40 * q.bsy;
			gx = ax + (q.gx + sd * q.gsp - ax) * w; gy = ay + (q.gy - ay) * w - 4 * (1 - w);
			dx = gx - ax; dy = gy - ay; ln = sqrt(dx * dx + dy * dy) || 1; ux = dx / ln; uy = dy / ln;
			nx = -uy; ny = ux; if (nx * sd < 0) { nx = -nx; ny = -ny; }          /* 向外的法向 */
			bl = -ln * 0.05 * w;                                                   /* 两臂向内微收的量 */
			c1x = ax + ux * ln * 0.3 - nx * bl * 0.8; c1y = ay + uy * ln * 0.3 - ny * bl * 0.8;
			c2x = ax + ux * ln * 0.72 - nx * bl; c2y = ay + uy * ln * 0.72 - ny * bl;
			tx = gx - c2x; ty = gy - c2y; tl = sqrt(tx * tx + ty * ty) || 1; tx /= tl; ty /= tl;
			ex = gx - tx * 13; ey = gy - ty * 13;                                  /* 手腕：手套中心在腕前 13 */
			if (pass === 0) {
				c.beginPath(); c.moveTo(ax, ay); c.bezierCurveTo(c1x, c1y, c2x, c2y, ex, ey);
				if (P.dark) { c.strokeStyle = P.rim; c.lineWidth = lw(20.2); c.stroke(); }
				c.strokeStyle = P.ink; c.lineWidth = lw(18.4); c.stroke();
				c.strokeStyle = P.white; c.lineWidth = lw(15.6); c.stroke();
				c.save(); c.translate(-sd * 2.2, 1.2); c.strokeStyle = P.whiteSh; c.lineWidth = lw(3.6); c.stroke(); c.restore();
			} else {
				drawMitten(c, P, ex, ey, atan2(-tx, ty));
			}
		}
		c.restore();
	}

	/* ---------------- 各 pose 的解析动作 ---------------- */
	function solve(name, t, o, G) {
		var q = Q, k, w, h, dh, sq, open, th, c, ph, cyc, dur, hopH;
		resetQ(q);
		cyc = (o.phase == null);
		ph = cyc ? ((t * 0.9) % 1) : clamp(o.phase, 0, 1);
		dur = (o.dur != null && o.dur > 0.05) ? o.dur : (DURS[name] || 0.6);
		hopH = (o.hopH != null) ? o.hopH : 30;
		var br = sin(t * 2.1), sw = sin(t * 1.7), sl = sin(t * 1.7 - 0.9);
		q.twPh = t * 3.3;
		/* 默认尾巴：向上翘并在末端钩住 */
		setTail(q, 50 + 2 * sw, -2, 62 + 3 * sw, -40 + 1.5 * sw, 38 + 6 * sl, -66 + 2 * sl);
		switch (name) {
		case 'hop':
			if (ph < 0.2) {
				k = sstep(0, 0.2, ph);
				q.bsy = 1 - 0.2 * k; q.bsx = 1 + 0.13 * k; q.hy = 2.5 * k; q.earL = q.earR = 0.1 * k; q.looky = -0.5 * k;
				setTail(q, 48, 1, 60, -30, 42, -56);
			} else if (ph < 0.82) {
				w = (ph - 0.2) / 0.62; h = 4 * w * (1 - w); dh = 1 - 2 * w; sq = (1 - w) * (1 - w);
				q.by = -hopH * h; q.vy = dh;
				q.bsy = 1 + 0.1 * h + 0.1 * sq; q.bsx = 1 - 0.07 * h - 0.06 * sq;
				q.hy = 1.2 * dh; q.tuck = h;
				q.mouth = 1; q.mo = 0.8; q.hrot = 0.08 - 0.04 * dh;
				q.earL = q.earR = 0.06;
			} else {
				k = (ph - 0.82) / 0.18; sq = sin(k * PI) * (1 - k * 0.3);
				q.bsy = 1 - 0.22 * sq; q.bsx = 1 + 0.15 * sq; q.hy = 3.2 * sq;
				q.earL = q.earR = 0.3 * sq; q.eye = (sq > 0.5) ? 1 : 0; q.mouth = 0;
			}
			break;
		case 'flip':
			if (ph < 0.16) {
				k = sstep(0, 0.16, ph);
				q.bsy = 1 - 0.22 * k; q.bsx = 1 + 0.15 * k; q.hy = 2.8 * k; q.earL = q.earR = 0.1 * k;
			} else if (ph < 0.86) {
				w = (ph - 0.16) / 0.7; h = 4 * w * (1 - w);
				q.by = -hopH * 1.25 * h; q.vy = 1 - 2 * w;
				th = w * w * (3 - 2 * w) * 0.35 + w * 0.65;
				q.rot = -TAU * th;
				q.bsy = 0.97; q.bsx = 1.02; q.tuck = 1;
				q.earL = q.earR = 0.2; q.eye = 1; q.mouth = 1; q.mo = 0.7;
				q.hrot = 0.02; q.hy = -1; q.bellAng = 0.5 * sin(w * PI * 2);
			} else {
				k = (ph - 0.86) / 0.14; sq = sin(k * PI);
				q.bsy = 1 - 0.24 * sq; q.bsx = 1 + 0.17 * sq; q.hy = 3.4 * sq; q.earL = q.earR = 0.3 * sq; q.eye = sq > 0.5 ? 1 : 0;
			}
			break;
		case 'sit':
			q.sit = 1; q.bsy = 0.9 + br * 0.012; q.bsx = 1.1; q.hy = 5 + br * 0.5;
			q.hrot = 0.17 + sin(t * 0.8) * 0.03; q.yaw = 0.38; q.bx = 2; q.hx = 1.5;
			q.earL = -0.04 - 0.22 * pulse(t, 4.1, 0.6, 0.25);
			q.earR = 0.02 + 0.12 * pulse(t, 5.7, 1.2, 0.22);
			q.t0x = 22; q.t0y = -4; q.tailFront = 1; q.twAmp = 1.2;
			setTail(q, 46, 3, 20 + sw * 1.5, 6, -9, -3 + sin(t * 2.4) * 2.4);
			q.stars = 1; q.lookx = sin(t * 0.6) * 0.5; q.bellAng = sin(t * 1.3) * 0.08;
			break;
		case 'wave':
			k = t * 7.5;
			q.hrot = 0.13 + sin(t * 2) * 0.03; q.yaw = 0.3; q.armOn = 1; q.armAng = -2.05 + sin(k) * 0.3;
			q.eye = 1; q.mouth = 0; q.earR = sin(k) * 0.05 + 0.04; q.earL = 0.02;
			q.bsy = 1 + 0.012 * br; q.hy = -0.5; q.bx = -2; q.hx = -1;
			setTail(q, 50 + 4 * sin(t * 6), -2, 63 + 5 * sin(t * 6 - 0.5), -36, 40 + 8 * sin(t * 6 - 1), -62);
			q.twAmp = 3.5; q.twPh = t * 6.5;
			q.stars = 1; q.bellAng = sin(k) * 0.12;
			break;
		case 'yawn':
			k = (t % 3.8) / 3.8;
			open = sstep(0.08, 0.34, k) * (1 - sstep(0.7, 0.92, k));
			q.mouth = 2; q.mo = open; q.hy = -2.5 * open; q.hsy = 1 + 0.05 * open; q.bsy = 1 + 0.045 * open;
			q.eye = open > 0.55 ? 2 : 0; q.op = 1 - open * 1.5; q.hrot = 0.08 - 0.07 * open; q.yaw = 0.3 - 0.2 * open;
			q.earL = q.earR = 0.34 * open; q.bellDy = -1.2 * open;
			setTail(q, 46, -2, 56, -32 + 6 * open, 40, -56 + 8 * open);
			break;
		case 'chase-tail':
			/* 沿小圆转圈：x 随 sin 位移，cos 决定朝向与侧身宽度；尾巴从身后绕到身前，尾尖在脸颊旁画小圈被追 */
			k = t * 4.0; c = cos(k);
			q.turn = (c < 0 ? -1 : 1);
			q.bx = 13 * sin(k); q.hx = 0;
			q.bsx = 0.58 + 0.42 * abs(c); q.tsx = 0.5 + 0.5 * abs(c); q.hsx = 0.7 + 0.3 * abs(c);
			q.by = -(abs(sin(k * 2)) * 2.4 + 5 * max(0, 1 - abs(c) * 6));       /* 翻面瞬间小起跳 */
			q.hrot = 0.34 * q.turn * q.turn; q.rot = sin(k) * 0.09;
			q.eye = 0; q.mouth = 1; q.mo = 0.5; q.lookx = 0.95; q.looky = 0.55; q.yaw = 0.85;
			q.earL = q.earR = 0.18; q.bellAng = 0.45 * sin(k);
			setTail(q, 66, -6, 70, -38, 36 + 5 * cos(k * 3), -42 + 7 * sin(k * 3));
			q.twAmp = 4; q.twPh = t * 9; q.bellDy = 1.2;
			break;
		case 'love':
			q.eye = 3; q.mouth = 3; q.mo = 0.8; q.blush = 1; q.loveP = 1;
			q.bsy = 1 + 0.02 * sin(t * 6); q.by = -abs(sin(t * 4.2)) * 3.4;
			q.hrot = 0.14 + sin(t * 3) * 0.04; q.earL = q.earR = -0.16 + 0.04 * sin(t * 9);
			q.hy = 0.8 * sin(t * 4.2); q.stars = 1; q.yaw = 0.1; q.rot = sin(t * 3.6) * 0.04;
			setTail(q, 46 + 3 * sin(t * 7), -4, 54 + 5 * sin(t * 7), -38, 38 + 7 * sin(t * 7 - 0.8), -70);
			q.twAmp = 4; q.twPh = t * 8;
			q.bellAng = 0.2 * sin(t * 8);
			break;
		case 'land':
			k = ph;
			if (k < 0.34) { sq = sstep(0, 0.34, k); q.bsy = 1 - 0.26 * sq; q.bsx = 1 + 0.18 * sq; q.hy = 3.6 * sq; q.earL = q.earR = 0.3 * sq; q.eye = sq > 0.4 ? 1 : 0; }
			else if (k < 0.7) { w = (k - 0.34) / 0.36; sq = 1 - sstep(0, 1, w); q.bsy = 1 - 0.26 * sq + 0.07 * sin(w * PI); q.bsx = 1 + 0.18 * sq - 0.04 * sin(w * PI); q.by = -2.8 * sin(w * PI); q.hy = 3.6 * sq - 1.2 * sin(w * PI); q.earL = q.earR = 0.3 * sq - 0.1 * sin(w * PI); q.eye = 0; q.tuck = 0.3 * sin(w * PI); }
			else { w = (k - 0.7) / 0.3; q.bsy = 1 + 0.03 * sin(w * PI * 2) * (1 - w); q.bsx = 1 - 0.02 * sin(w * PI * 2) * (1 - w); }
			q.mouth = 0;
			break;
		case 'drag':
			solve('idle', t, o, G);
			applyDrag(q);
			break;
		default: /* idle：歪头 3/4 站姿，重心偏一侧，右前爪轻抬 */
			q.bsy = 1 + 0.018 * br; q.bsx = 1 - 0.01 * br; q.hy = 0.5 * br; q.rot = sin(t * 1.3) * 0.012;
			q.hrot = 0.1 + sin(t * 1.0 + 0.6) * 0.03; q.yaw = 0.42; q.bx = -2.5; q.hx = 1.5;
			q.pawR = 0.55 + 0.12 * sin(t * 1.9) + 0.25 * pulse(t, 6.3, 0.5, 1.1);
			q.earL = -0.04 - 0.2 * pulse(t, 3.7, 0.4, 0.24); q.earR = 0.02 + 0.14 * pulse(t, 5.1, 1, 0.2);
			q.stars = 1; q.lookx = sin(t * 0.7) * 0.4; q.looky = sin(t * 0.5 + 1) * 0.15; q.bellAng = sin(t * 1.5) * 0.07 - q.rot * 2;
			break;
		}
		/* 二级运动：弹簧位移叠加到头/耳/尾/铃铛 */
		springs(G, name, t, ph, cyc, dur, hopH);
		var dH = SD[0], dE = SD[1], dT = SD[2], dB = SD[3];
		q.hy += clamp(-dH * 0.18, -5, 5);
		h = clamp(1 - dH * 0.004, 0.93, 1.07);
		q.hsy *= h; q.hsx *= 1 / sqrt(h);
		q.earL += -dE * 0.05; q.earR += -dE * 0.05;
		q.tp[3] += clamp(-dT * 0.35, -10, 10); q.tp[5] += clamp(-dT * 0.7, -14, 14); q.tp[4] += clamp(dT * 0.15, -4, 4);
		q.bellDy += -dB * 0.3; q.bellAng += dB * 0.04;
		if (o.noLift || o.selfLift === false) { if (name === 'hop' || name === 'flip') q.by = 0; }
		return q;
	}

	/* ---------------- 缓存的渐变（本地坐标，跨帧复用） ---------------- */
	function cache(c) {
		var g = c.__mwpV2;
		if (!g) { g = {}; try { c.__mwpV2 = g; } catch (e) {} }
		return g;
	}
	function irisGrad(c) {
		var g = cache(c);
		if (!g.iris) {
			var gr = c.createLinearGradient(0, -12, 0, 13);
			gr.addColorStop(0, '#5a3a1a'); gr.addColorStop(0.42, '#9b7430'); gr.addColorStop(0.74, '#b5a458'); gr.addColorStop(1, '#dcd496');
			g.iris = gr;
		}
		return g.iris;
	}
	function bellGrad(c) {
		var g = cache(c);
		if (!g.bell) {
			var gr = c.createRadialGradient(-1.2, -1.4, 0.4, 0, 0, 4.6);
			gr.addColorStop(0, '#fff3b4'); gr.addColorStop(0.55, '#f6c43c'); gr.addColorStop(1, '#c88410');
			g.bell = gr;
		}
		return g.bell;
	}

	/* ---------------- 头部路径（右半边用分段表，左半边镜像倒走） ---------------- */
	var RS = [
		[39.8, 1, 43.4, 7, 45.8, 14.6],
		[47, 18, 41.4, 19.6, 37.8, 18.6],
		[42.6, 22.4, 45, 28.6, 41.6, 33],
		[39.8, 35, 34, 31.8, 28.6, 29.2],
		[21.4, 33.4, 10.6, 34.4, 0, 34.4]
	];
	function hp(c, bottom) {
		var i, s, px, py, n = RS.length, i0 = bottom ? 1 : 0;
		if (!bottom) { c.moveTo(0, -34); c.bezierCurveTo(21, -34, 37, -23, 38.4, -6); }
		else c.moveTo(RS[0][4], RS[0][5]);
		for (i = i0; i < n; i++) { s = RS[i]; c.bezierCurveTo(s[0], s[1], s[2], s[3], s[4], s[5]); }
		for (i = n - 1; i >= i0; i--) {
			s = RS[i]; px = i ? RS[i - 1][4] : 38.4; py = i ? RS[i - 1][5] : -6;
			c.bezierCurveTo(-s[2], s[3], -s[0], s[1], -px, py);
		}
		if (!bottom) { c.bezierCurveTo(-37, -23, -21, -34, 0, -34); c.closePath(); }
	}
	function muzzle(c) {
		c.moveTo(-52, 24);
		c.bezierCurveTo(-44, 17, -36, 14, -29, 15.5);
		c.bezierCurveTo(-20, 17, -10, 12, -7, 2);
		c.bezierCurveTo(-5.5, -6, -3.6, -12, -2, -19);
		c.bezierCurveTo(-1.4, -22.6, 1.4, -22.6, 2, -19);
		c.bezierCurveTo(3.6, -12, 5.5, -6, 7, 2);
		c.bezierCurveTo(10, 12, 20, 17, 29, 15.5);
		c.bezierCurveTo(36, 14, 44, 17, 52, 24);
		c.lineTo(52, 45); c.lineTo(-52, 45); c.closePath();
	}
	/* 额头虎斑（sd=-1 左 / +1 右）：内侧粗、外侧短 */
	function stripes(c, sd) {
		c.save(); c.scale(sd, 1); c.translate(1.6, -4.5);
		c.moveTo(-6.2, -29); c.bezierCurveTo(-5.4, -23, -5, -18, -4.6, -12.4); c.bezierCurveTo(-10.4, -15.6, -13.4, -22, -14.6, -29); c.closePath();
		c.moveTo(-18.4, -27.6); c.bezierCurveTo(-15, -26, -13.4, -21.6, -13.4, -16.4); c.bezierCurveTo(-18.4, -17.6, -22, -21.6, -24, -26); c.closePath();
		c.restore();
	}
	function cheekPatch(c, sd, k) {
		c.save(); c.scale(sd * k, k);
		c.moveTo(-44, -6); c.bezierCurveTo(-39, -13, -32, -11, -31, -3); c.bezierCurveTo(-30.6, 4, -35.5, 9, -42, 10.5); c.bezierCurveTo(-44.4, 5, -45.4, 0, -44, -6); c.closePath();
		c.restore();
	}

	/* ---------------- 耳朵 ---------------- */
	function earOuter(c) {
		c.moveTo(7, -24); c.bezierCurveTo(11, -36, 28, -47, 43, -50); c.bezierCurveTo(46.5, -34, 44, -18, 36, -5); c.lineTo(10, -6); c.closePath();
	}
	function earInner(c) {
		c.moveTo(14, -26); c.bezierCurveTo(18, -33, 28, -40, 38, -43.5); c.bezierCurveTo(40, -32, 38.5, -22, 34, -12); c.lineTo(15, -12); c.closePath();
	}
	function earFluff(c) {
		c.moveTo(9, -10); c.lineTo(11.5, -30); c.lineTo(16.5, -24); c.lineTo(19, -40); c.lineTo(24.5, -29); c.lineTo(28.6, -38); c.lineTo(31, -26);
		c.lineTo(36.6, -22); c.lineTo(37, -4); c.lineTo(9, -4); c.closePath();
	}
	function drawEar(c, P, sd, ang) {
		c.save(); c.scale(sd, 1); c.translate(0, -4); c.translate(24, -17); c.rotate(ang); c.translate(-24, 17);
		c.beginPath(); earOuter(c); c.fillStyle = P.fur; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.moveTo(31, -54); c.lineTo(52, -54); c.lineTo(52, -33); c.bezierCurveTo(45, -37, 38, -39, 31, -40); c.closePath(); c.fillStyle = P.spot; c.fill();
		c.restore();
		c.beginPath(); earInner(c); c.fillStyle = P.pink; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.moveTo(13, -27); c.bezierCurveTo(18, -33, 25, -38, 31, -40); c.bezierCurveTo(24, -31, 20, -23, 19, -12); c.lineTo(13, -12); c.closePath(); c.fillStyle = P.pinkDeep; c.fill();
		c.restore();
		c.beginPath(); earFluff(c); c.fillStyle = P.white; c.fill(); c.strokeStyle = P.inkB; c.lineWidth = lw(0.9); c.stroke();
		c.beginPath(); earOuter(c); rimInk(c, P, 1.2);
		c.restore();
	}

	/* ---------------- 眼睛 ---------------- */
	function eyeOpen(c, P, sd, op, lx, ly, love, t) {
		var ix = lx * 2.3, iy = ly * 1.8 + 0.5, topY = -8.5;
		c.save();
		c.translate(0, topY); c.scale(1, op); c.translate(0, -topY);
		c.beginPath(); ell(c, 0, 0, 11.6, 13.6); c.fillStyle = '#fffdf6'; c.fill();
		c.save(); c.clip();
		c.beginPath(); ell(c, ix, iy, 10.4, 12.4); c.fillStyle = love ? '#f7d8dc' : irisGrad(c); c.fill();
		if (!love) { c.strokeStyle = 'rgba(70,46,18,0.55)'; c.lineWidth = lw(0.9); c.stroke(); }
		if (love) {
			var hs = 6.2 * (1 + 0.1 * sin(t * 9));
			c.beginPath(); heartPath(c, ix, iy - 0.4, hs); c.fillStyle = '#ff4d7c'; c.fill(); c.strokeStyle = '#b21f4f'; c.lineWidth = lw(0.9); c.stroke();
			c.beginPath(); ell(c, ix - 2.4, iy - 3.2, 1.7, 1.2); c.fillStyle = 'rgba(255,255,255,0.95)'; c.fill();
		} else {
			c.beginPath(); ell(c, ix, iy - 0.5, 5.3, 7.6); c.fillStyle = '#2a1a12'; c.fill();
			c.beginPath(); ell(c, 0, -11.5, 14, 6.6); c.fillStyle = 'rgba(56,36,14,0.34)'; c.fill();
			c.beginPath(); ell(c, ix - 3.3, iy - 4.8, 3.35, 3.35); c.fillStyle = '#fff'; c.fill();
			c.beginPath(); ell(c, ix + 3.5, iy + 4.4, 1.55, 1.55); c.fillStyle = 'rgba(255,255,255,0.96)'; c.fill();
		}
		c.restore();
		c.beginPath(); c.moveTo(-9.8, 4); c.bezierCurveTo(-7, 12, 7, 12, 9.6, 4); c.strokeStyle = P.inkA; c.lineWidth = lw(0.9); c.stroke();
		c.restore();
		/* 上眼睑：上粗下细的睫毛 + 外翘小尾 */
		c.save(); c.translate(0, 5 * (1 - op)); c.scale(sd, 1);
		c.lineCap = 'round';
		c.beginPath(); c.moveTo(-10.6, -1); c.bezierCurveTo(-9.6, -9.5, -3, -13.8, 3.5, -13.6); c.bezierCurveTo(9, -13.2, 12, -8.4, 11.2, -1.5);
		c.strokeStyle = P.lash; c.lineWidth = lw(2.1); c.stroke();
		c.beginPath(); c.moveTo(-6.4, -11.8); c.bezierCurveTo(-2, -14.2, 5, -14.4, 9.6, -10.6); c.lineWidth = lw(3.7); c.stroke();
		c.beginPath(); c.moveTo(10.6, -5.6); c.quadraticCurveTo(13.8, -6.4, 16.6, -9.6); c.lineWidth = lw(2.2); c.stroke();
		c.beginPath(); c.moveTo(11.4, -1.6); c.quadraticCurveTo(14.6, -2.2, 17, -4.8); c.lineWidth = lw(1.6); c.stroke();
		c.restore();
	}
	function eyeHappy(c, P, sd) {
		c.save(); c.scale(sd, 1); c.lineCap = 'round';
		c.beginPath(); c.moveTo(-9.6, 5.4); c.bezierCurveTo(-7.4, -6.6, 7.6, -6.6, 9.8, 5.4);
		c.strokeStyle = P.ink; c.lineWidth = lw(2.8); c.stroke();
		c.beginPath(); c.moveTo(9.4, 1.6); c.quadraticCurveTo(12.8, 0.6, 15.2, -2.6); c.lineWidth = lw(2); c.stroke();
		c.restore();
	}
	function eyeShut(c, P, sd) {
		c.save(); c.scale(sd, 1); c.lineCap = 'round';
		c.beginPath(); c.moveTo(-9.6, 0); c.bezierCurveTo(-6.5, 5.6, 6.5, 5.6, 9.8, 0);
		c.strokeStyle = P.ink; c.lineWidth = lw(2.6); c.stroke();
		c.beginPath(); c.moveTo(9.4, 1.8); c.quadraticCurveTo(12.6, 2.6, 15, 5.4); c.lineWidth = lw(1.8); c.stroke();
		c.restore();
	}
	function eyeChevron(c, P, sd) {
		c.save(); c.scale(sd, 1); c.lineCap = 'round'; c.lineJoin = 'round';
		c.beginPath(); c.moveTo(8, -3.4); c.lineTo(-8, 3.6); c.lineTo(8, 9.4);
		c.strokeStyle = P.ink; c.lineWidth = lw(2.7); c.stroke();
		c.restore();
	}

	/* ---------------- 嘴 ---------------- */
	function mouthDraw(c, P, q) {
		c.lineCap = 'round';
		if (q.mouth === 0) {
			c.beginPath(); c.moveTo(0, 20.2); c.lineTo(0, 22.4);
			c.moveTo(0, 22.4); c.bezierCurveTo(-1.4, 26.4, -6.8, 26.6, -9.4, 21.6);
			c.moveTo(0, 22.4); c.bezierCurveTo(1.4, 26.4, 6.8, 26.6, 9.4, 21.6);
			c.strokeStyle = P.ink; c.lineWidth = lw(1.5); c.stroke();
		} else if (q.mouth === 1 || q.mouth === 3) {
			var m = q.mouth === 3 ? 1 : 0.55 + 0.45 * q.mo;
			c.beginPath(); c.moveTo(0, 20.2); c.lineTo(0, 22);
			c.strokeStyle = P.ink; c.lineWidth = lw(1.4); c.stroke();
			c.beginPath(); c.moveTo(-8.6, 22); c.bezierCurveTo(-8.2, 22.4 + 6 * m, 8.2, 22.4 + 6 * m, 8.6, 22); c.bezierCurveTo(4, 20.8, -4, 20.8, -8.6, 22); c.closePath();
			c.fillStyle = P.mouthIn; c.fill();
			c.save(); c.clip(); c.beginPath(); ell(c, 0, 28.6 + 1.5 * m, 5, 3.2 * m + 1); c.fillStyle = P.tongue; c.fill(); c.restore();
			c.strokeStyle = P.ink; c.lineWidth = lw(1.4); c.stroke();
		} else {
			var op = q.mo, hgt = 3 + 8 * op;
			c.beginPath(); c.moveTo(0, 20.2); c.lineTo(0, 22);
			c.strokeStyle = P.ink; c.lineWidth = lw(1.4); c.stroke();
			c.beginPath(); c.moveTo(-8.2 - 1.6 * op, 22.2); c.bezierCurveTo(-8.6 - 2 * op, 22.4 + hgt * 1.3, 8.6 + 2 * op, 22.4 + hgt * 1.3, 8.2 + 1.6 * op, 22.2); c.bezierCurveTo(4, 20.8, -4, 20.8, -8.2 - 1.6 * op, 22.2); c.closePath();
			c.fillStyle = P.mouthIn; c.fill();
			c.save(); c.clip(); c.beginPath(); ell(c, 0, 22.6 + hgt, 5.6, 0.55 * hgt + 1); c.fillStyle = P.tongue; c.fill();
			c.beginPath(); c.moveTo(-5.2, 20.6); c.lineTo(-3.4, 24 + 1.4 * op); c.lineTo(-1.6, 20.6); c.moveTo(1.6, 20.6); c.lineTo(3.4, 24 + 1.4 * op); c.lineTo(5.2, 20.6); c.fillStyle = P.white; c.fill();
			c.restore();
			c.strokeStyle = P.ink; c.lineWidth = lw(1.5); c.stroke();
		}
	}

	/* ---------------- 头 ---------------- */
	function drawHead(c, P, q, t, blink) {
		var yaw = q.yaw, sh = yaw * 2.2, sf = yaw * 4.6, op;
		drawEar(c, P, -1, q.earL);
		drawEar(c, P, 1, q.earR);
		c.beginPath(); hp(c, false); c.fillStyle = P.fur; c.fill();
		c.save(); c.clip();
		/* 一阶阴影：右下新月 */
		c.beginPath(); c.rect(-60, -40, 120, 90); c.save(); c.translate(-3.4, -5); hp(c, false); c.restore(); c.fillStyle = P.furSh; c.fill('evenodd');
		/* 斑纹（跟着头走，随偏航平移） */
		c.save(); c.translate(sh, 0);
		c.beginPath(); stripes(c, -1); stripes(c, 1);
		cheekPatch(c, -1, 1.05); cheekPatch(c, 1, 0.86);
		c.moveTo(-40, -15); c.bezierCurveTo(-36, -22, -28, -20, -29, -14); c.bezierCurveTo(-30, -10, -37, -9, -40, -15); c.closePath();
		c.moveTo(38, -17); c.bezierCurveTo(33, -22, 27, -19, 29, -14); c.bezierCurveTo(31, -10, 36, -10, 38, -17); c.closePath();
		c.fillStyle = P.spot; c.fill();
		/* 白脸下半 */
		c.beginPath(); muzzle(c); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-60, -40, 120, 100); c.save(); c.translate(-2.4, -3.4); hp(c, false); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.restore();
		c.restore(); /* 退出 clip */
		/* 描边：全圈细 + 下半圈粗 */
		c.beginPath(); hp(c, false); rimInk(c, P, 1.2);
		c.beginPath(); hp(c, true); c.strokeStyle = P.ink; c.lineWidth = lw(2); c.stroke();
		/* 鬓毛分叉小刻线 */
		c.beginPath();
		c.moveTo(37.6, 17.8); c.lineTo(33.4, 23); c.moveTo(-37.6, 17.8); c.lineTo(-33.4, 23);
		c.moveTo(41.5, 6); c.lineTo(38, 10.6); c.moveTo(-41.5, 6); c.lineTo(-38, 10.6);
		c.strokeStyle = P.inkA; c.lineWidth = lw(1); c.stroke();
		/* 五官：整体向朝向偏移 sf */
		c.save(); c.translate(sf, 0);
		if (q.blush > 0) {
			c.beginPath(); ell(c, -26.5, 17.5, 6.6, 3.8); ell(c, 26.5, 17.5, 6.6, 3.8);
			c.fillStyle = P.blush; c.globalAlpha = q.blush; c.fill(); c.globalAlpha = 1;
		}
		op = clamp(q.op, 0, 1) * (1 - blink);
		var i, ex, sd;
		for (i = 0; i < 2; i++) {
			sd = i ? 1 : -1; ex = sd * 17.4;
			c.save(); c.translate(ex, 4);
			if (q.eye === 1) eyeHappy(c, P, sd);
			else if (q.eye === 2) eyeChevron(c, P, sd);
			else if (op < 0.16) eyeShut(c, P, sd);
			else eyeOpen(c, P, sd, op, q.lookx + (i ? 0.1 : -0.1), q.looky, q.eye === 3, t);
			c.restore();
		}
		c.beginPath(); c.moveTo(-3.7, 17.4); c.bezierCurveTo(-3.9, 15.8, 3.9, 15.8, 3.7, 17.4); c.bezierCurveTo(3.5, 19.4, 1.3, 20.4, 0, 20.6); c.bezierCurveTo(-1.3, 20.4, -3.5, 19.4, -3.7, 17.4); c.closePath();
		fillInk(c, P.nose, P.ink, 1);
		c.beginPath(); ell(c, -1.1, 17.2, 1.1, 0.6); c.fillStyle = 'rgba(255,255,255,0.7)'; c.fill();
		mouthDraw(c, P, q);
		c.restore();
		/* 胡须 */
		c.beginPath();
		c.moveTo(30, 15.5); c.quadraticCurveTo(43, 12, 55, 13);
		c.moveTo(30, 19.5); c.quadraticCurveTo(44, 19, 56, 21.6);
		c.moveTo(-30, 15.5); c.quadraticCurveTo(-43, 12, -55, 13);
		c.moveTo(-30, 19.5); c.quadraticCurveTo(-44, 19, -56, 21.6);
		c.strokeStyle = P.inkB; c.lineWidth = lw(0.8); c.stroke();
	}

	/* ---------------- 身体 ---------------- */
	function bodyPath(c) {
		c.moveTo(-13, -44);
		c.bezierCurveTo(-25, -38, -30, -23, -28, -11);
		c.bezierCurveTo(-27, -3, -20, 1, -12, 1);
		c.lineTo(12, 1);
		c.bezierCurveTo(20, 1, 27, -3, 28, -11);
		c.bezierCurveTo(30, -23, 25, -38, 13, -44);
		c.closePath();
	}
	function legPath(c) {
		c.moveTo(-5, 0); c.bezierCurveTo(-5.4, 9, -5.6, 17, -6.4, 22);
		c.bezierCurveTo(-8.6, 24, -8.6, 28.4, -3, 28.6);
		c.lineTo(3, 28.6);
		c.bezierCurveTo(8.6, 28.4, 8.6, 24, 6.4, 22);
		c.bezierCurveTo(5.6, 17, 5.4, 9, 5, 0); c.closePath();
	}
	function legOpen(c) {
		c.moveTo(-5, 0); c.bezierCurveTo(-5.4, 9, -5.6, 17, -6.4, 22);
		c.bezierCurveTo(-8.6, 24, -8.6, 28.4, -3, 28.6);
		c.lineTo(3, 28.6);
		c.bezierCurveTo(8.6, 28.4, 8.6, 24, 6.4, 22);
		c.bezierCurveTo(5.6, 17, 5.4, 9, 5, 0);
	}
	/* 蓬松白胸：外缘一串小绒尖，下缘垂到两腿之间 */
	var BIB = [-15, -44, -17.5, -36, -18, -28, -22, -22, -15.5, -20, -18, -12, -11.5, -14, -13, -4, -9, 3, 0, 3.5, 9, 3, 13, -4, 11.5, -14, 18, -12, 15.5, -20, 22, -22, 18, -28, 17.5, -36, 15, -44];
	function bibPath(c) {
		var i; c.moveTo(BIB[0], BIB[1]);
		for (i = 2; i < BIB.length; i += 2) c.lineTo(BIB[i], BIB[i + 1]);
		c.closePath();
	}
	function drawLeg(c, P, x, y, sy, ang, pad, spot, closed, sxw) {
		c.save(); c.translate(x, y); if (ang) c.rotate(ang); c.scale(sxw || 1, sy);
		c.beginPath(); legPath(c); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		if (spot) { c.beginPath(); c.moveTo(-9, 2); c.bezierCurveTo(-4, 0, 3, 3, 4, 10); c.bezierCurveTo(-1, 13, -6, 13, -9, 11); c.closePath(); c.fillStyle = P.spot; c.fill(); }
		c.beginPath(); c.rect(-12, -4, 24, 36); c.save(); c.translate(-2.6, -2.4); legPath(c); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		if (pad) {
			c.beginPath(); ell(c, 0, 19.2, 3.6, 3); ell(c, -3.8, 14.2, 1.7, 1.9); ell(c, 0, 12.6, 1.8, 2); ell(c, 3.8, 14.2, 1.7, 1.9);
			c.fillStyle = P.pinkDeep; c.fill();
		} else {
			c.beginPath(); c.moveTo(-2.2, 21.4); c.lineTo(-2.2, 26); c.moveTo(2.2, 21.4); c.lineTo(2.2, 26); c.strokeStyle = P.inkA; c.lineWidth = lw(0.9); c.stroke();
		}
		c.beginPath(); if (closed) legPath(c); else legOpen(c); rimInk(c, P, 1.3);
		c.restore();
	}
	/* 挥爪用的圆头手套：臂根圆帽 + 大掌 + 粉肉垫（原点=肩，向 +y 伸出） */
	function mittenPath(c) {
		c.moveTo(-4.8, 0); c.bezierCurveTo(-4.8, -3.8, 4.8, -3.8, 4.8, 0);
		c.bezierCurveTo(5.2, 8, 9.4, 11, 9, 18); c.bezierCurveTo(8.8, 26, -8.8, 26, -9, 18);
		c.bezierCurveTo(-9.4, 11, -5.2, 8, -4.8, 0); c.closePath();
	}
	function drawMitten(c, P, x, y, ang) {
		c.save(); c.translate(x, y); c.rotate(ang);
		c.beginPath(); mittenPath(c); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-14, -6, 28, 36); c.save(); c.translate(-2.6, -2.2); mittenPath(c); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.beginPath(); ell(c, 0, 19.2, 4.4, 3.6); ell(c, -5, 12.8, 2, 2.3); ell(c, 0, 11.2, 2.1, 2.4); ell(c, 5, 12.8, 2, 2.3);
		c.fillStyle = P.pinkDeep; c.fill();
		c.beginPath(); mittenPath(c); rimInk(c, P, 1.3);
		c.restore();
	}
	function hindFoot(c, P, x, y, rx, ry) {
		c.beginPath(); ell(c, x, y, rx, ry); c.fillStyle = P.white; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(x - rx - 2, y - ry - 2, rx * 2 + 4, ry * 2 + 4); ell(c, x - 1.4, y - 1.8, rx, ry); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.beginPath(); ell(c, x, y, rx, ry); rimInk(c, P, 1.3);
		c.beginPath(); c.moveTo(x - 1.6, y + ry - 3.4); c.lineTo(x - 1.6, y + ry - 0.4); c.moveTo(x + 1.8, y + ry - 3.4); c.lineTo(x + 1.8, y + ry - 0.4); c.strokeStyle = P.inkA; c.lineWidth = lw(0.8); c.stroke();
	}
	function drawBody(c, P, q) {
		var sit = q.sit;
		c.save(); c.scale(q.bsx, q.bsy);
		if (!sit) { hindFoot(c, P, -25.5 + 8 * q.hang - 10 * q.legL, -2.6 + 9 * q.hang, 7.4, 4.4); hindFoot(c, P, 25.5 - 8 * q.hang - 10 * q.legR, -2.6 + 9 * q.hang, 7.4, 4.4); }
		c.beginPath(); bodyPath(c); c.fillStyle = P.fur; c.fill();
		c.save(); c.clip();
		c.beginPath(); c.rect(-40, -50, 80, 60); c.save(); c.translate(-4.4, -4.4); bodyPath(c); c.restore(); c.fillStyle = P.furSh; c.fill('evenodd');
		/* 豹纹：不对称的大小斑块 */
		c.beginPath();
		c.moveTo(-33, -36); c.bezierCurveTo(-24, -42, -13, -34, -15.5, -23); c.bezierCurveTo(-18, -12, -26, -5, -33, -10); c.closePath();
		c.moveTo(31, -22); c.bezierCurveTo(27, -26, 20, -24, 20.5, -16); c.bezierCurveTo(21, -8, 27, -5, 32, -9); c.closePath();
		c.moveTo(-12, -42); c.bezierCurveTo(-9, -46, 0, -46, 4, -43); c.bezierCurveTo(2, -38, -6, -37, -12, -42); c.closePath();
		c.moveTo(20, -40); c.bezierCurveTo(23, -43, 29, -39, 28, -33); c.bezierCurveTo(25, -31, 20, -34, 20, -40); c.closePath();
		c.fillStyle = P.spot; c.fill();
		c.beginPath(); ell(c, -24, -19, 3.4, 5); ell(c, 25.5, -13, 2.2, 3.2); ell(c, -5, -35, 2.2, 1.6); ell(c, 24.6, -29, 1.8, 1.8);
		c.fillStyle = P.spotDk; c.fill();
		/* 白胸 */
		c.beginPath(); bibPath(c); c.fillStyle = P.white; c.fill();
		c.beginPath(); c.rect(-24, -46, 48, 50); c.save(); c.translate(-1.6, -1.2); bibPath(c); c.restore(); c.fillStyle = P.whiteSh; c.fill('evenodd');
		c.restore();
		c.beginPath(); bodyPath(c); rimInk(c, P, 1.3);
		c.beginPath(); c.moveTo(-27.8, -8); c.bezierCurveTo(-27, -3, -20, 1, -12, 1); c.lineTo(12, 1); c.bezierCurveTo(20, 1, 27, -3, 27.8, -8); c.strokeStyle = P.ink; c.lineWidth = lw(2.2); c.stroke();
		c.beginPath(); bibPath(c); c.strokeStyle = P.inkB; c.lineWidth = lw(1); c.stroke();
		if (sit) { hindFoot(c, P, -23, -3, 9, 5.2); hindFoot(c, P, 23, -3, 9, 5.2); }
		/* 前腿：右前爪（idle）可轻抬；wave 时右腿换成抬起的大手套 */
		var ty = 1 - 0.3 * q.tuck, pr = q.pawR;
		drawLeg(c, P, -9.6, -29 + 17 * q.hang, ty, 0.05 + q.legL, false, true);
		if (!q.armOn) drawLeg(c, P, 9.6, -29 + 17 * q.hang, ty * (1 - 0.18 * pr), -0.05 - 0.42 * pr + q.legR, false, false);
		c.restore();
	}

	/* ---------------- 尾巴：贝塞尔中线 + 沿法线的行波（软尾） ---------------- */
	var TX = [], TY = [], NT = 20;
	function tailPts(q) {
		var p = q.tp, x0 = q.t0x, y0 = q.t0y, i, u, v, dx, dy, ln, a;
		for (i = 0; i <= NT; i++) {
			u = i / NT; v = 1 - u;
			TX[i] = v * v * v * x0 + 3 * v * v * u * p[0] + 3 * v * u * u * p[2] + u * u * u * p[4];
			TY[i] = v * v * v * y0 + 3 * v * v * u * p[1] + 3 * v * u * u * p[3] + u * u * u * p[5];
		}
		if (q.twAmp > 0) {
			var ox = [], oy = [];
			for (i = 1; i <= NT; i++) {
				dx = TX[min(NT, i + 1)] - TX[i - 1]; dy = TY[min(NT, i + 1)] - TY[i - 1]; ln = sqrt(dx * dx + dy * dy) || 1;
				u = i / NT; a = q.twAmp * u * u * sin(q.twPh - 5.5 * u);
				ox[i] = -dy / ln * a; oy[i] = dx / ln * a;
			}
			for (i = 1; i <= NT; i++) { TX[i] += ox[i]; TY[i] += oy[i]; }
		}
	}
	function tailStroke(c, i0, i1) {
		var i; c.beginPath(); c.moveTo(TX[i0], TY[i0]);
		for (i = i0 + 1; i <= i1; i++) c.lineTo(TX[i], TY[i]);
	}
	function drawTail(c, P) {
		c.save(); c.lineJoin = 'round';
		if (P.dark) { tailStroke(c, 0, NT); c.lineCap = 'round'; c.strokeStyle = P.rim; c.lineWidth = lw(12.4); c.stroke(); }
		tailStroke(c, 0, NT); c.lineCap = 'round'; c.strokeStyle = P.ink; c.lineWidth = lw(10.4); c.stroke();
		tailStroke(c, 0, NT); c.strokeStyle = P.fur; c.lineWidth = lw(8.6); c.stroke();
		c.lineCap = 'butt';
		c.strokeStyle = P.spot; c.lineWidth = lw(8.6);
		tailStroke(c, 4, 6); c.stroke();
		tailStroke(c, 8, 10); c.stroke();
		tailStroke(c, 12, 14); c.stroke();
		tailStroke(c, 16, 17); c.stroke();
		c.lineCap = 'round'; tailStroke(c, 18, NT); c.strokeStyle = P.white; c.lineWidth = lw(8.6); c.stroke();
		tailStroke(c, 1, 17); c.strokeStyle = P.furSh; c.lineWidth = lw(2.2); c.translate(1.4, 1.8); c.stroke();
		c.restore();
	}

	/* ---------------- 铃铛 ---------------- */
	function drawBell(c, q) {
		c.save(); c.translate(q.hx * 0.3, -38 + q.bellDy * 0.2); c.rotate(q.bellAng);
		c.beginPath(); c.moveTo(-8.5, -3); c.quadraticCurveTo(0, 5, 8.5, -3); c.strokeStyle = '#a5681a'; c.lineWidth = lw(1.1); c.stroke();
		c.translate(0, 13 + q.bellDy * 0.5);
		c.beginPath(); c.arc(0, -4.6, 1.3, 0, TAU); c.strokeStyle = '#7a4912'; c.lineWidth = lw(0.9); c.stroke();
		c.beginPath(); c.arc(0, 0, 3.9, 0, TAU); c.fillStyle = bellGrad(c); c.fill(); c.strokeStyle = '#7a4912'; c.lineWidth = lw(1.2); c.stroke();
		c.beginPath(); c.moveTo(-3.4, 1.1); c.lineTo(3.4, 1.1); c.moveTo(0, 1.1); c.lineTo(0, 3.3); c.strokeStyle = '#7a4912'; c.lineWidth = lw(0.9); c.stroke();
		c.restore();
	}

	function star(c, x, y, r, rot) {
		var i, a, rr;
		for (i = 0; i < 10; i++) {
			a = rot + i * PI / 5 - PI / 2; rr = (i & 1) ? r * 0.5 : r;
			if (i) c.lineTo(x + cos(a) * rr, y + sin(a) * rr); else c.moveTo(x + cos(a) * rr, y + sin(a) * rr);
		}
		c.closePath();
	}
	function drawStars(c, P, t) {
		var a = 0.85 + 0.15 * sin(t * 3), b = 0.85 + 0.15 * sin(t * 3 + 2);
		c.save(); c.lineJoin = 'round';
		c.beginPath(); star(c, -66, -90, 7.4 * a, sin(t) * 0.2); c.fillStyle = P.star; c.fill(); c.strokeStyle = '#e07a35'; c.lineWidth = lw(1.3); c.stroke();
		c.beginPath(); star(c, 70, -104, 6.2 * b, -sin(t * 1.2) * 0.2); c.fill(); c.stroke();
		c.restore();
	}

	/* ---------------- 坐标换算与包围盒 ---------------- */
	function pt(oo, lx, ly, out) {
		var s = oo.s, X = lx * K * s * oo.sxx * oo.fl, Y = ly * K * s * oo.syy;
		var pyW = oo.pv * K * s * oo.syy, r = oo.rot, cr = cos(r), sr = sin(r), yy = Y - pyW;
		out.x = oo.x + X * cr - yy * sr; out.y = oo.y + oo.lift + pyW + X * sr + yy * cr;
		return out;
	}
	var TMP = { s: 1, sxx: 1, syy: 1, fl: 1, pv: PIVOT, rot: 0, x: 0, y: 0, lift: 0 };
	var PTMP = { x: 0, y: 0 };
	var BB = { l: 0, t: 0, r: 0, b: 0 };
	function bbAdd(oo, lx, ly) {
		pt(oo, lx, ly, PTMP);
		if (PTMP.x < BB.l) BB.l = PTMP.x; if (PTMP.x > BB.r) BB.r = PTMP.x;
		if (PTMP.y < BB.t) BB.t = PTMP.y; if (PTMP.y > BB.b) BB.b = PTMP.y;
	}
	/* 头局部点 → 设计坐标（头的平移/旋转/缩放） */
	function headPt(q, hcy, lx, ly, out) {
		var sx2 = q.hsx * (1 + (q.bsx - 1) * 0.5), sy2 = q.hsy * (1 + (q.bsy - 1) * 0.6), cr = cos(q.hrot), sr = sin(q.hrot);
		var X = lx * sx2, Y = ly * sy2;
		out.x = (q.bx + q.hx) + X * cr - Y * sr; out.y = hcy + X * sr + Y * cr;
		return out;
	}
	var HT = { x: 0, y: 0 };
	function boundsOf(oo, q, hcy, showStars) {
		var i, u, e, pad = 4;
		BB.l = 1e9; BB.r = -1e9; BB.t = 1e9; BB.b = -1e9;
		/* 身体：脚与肩 */
		bbAdd(oo, q.bx - 38 * q.bsx, 6); bbAdd(oo, q.bx + 38 * q.bsx, 6); bbAdd(oo, q.bx - 31 * q.bsx, -46 * q.bsy); bbAdd(oo, q.bx + 31 * q.bsx, -46 * q.bsy);
		/* 尾巴：中线上若干点，向外各加半宽 */
		tailPts(q);
		for (i = 0; i <= NT; i += 2) {
			e = TX[i] * q.tsx + q.bx;
			bbAdd(oo, e - pad - 2, TY[i] - pad - 2); bbAdd(oo, e + pad + 2, TY[i] + pad + 2);
		}
		/* 头：耳尖、脸颊、胡须、下巴、头顶（含耳朵旋转的余量） */
		var hp2 = [-52, -62, 52, -62, 0, -38, -57, 24, 57, 24, 0, 37, -46, 16, 46, 16];
		for (i = 0; i < hp2.length; i += 2) { headPt(q, hcy, hp2[i], hp2[i + 1], HT); bbAdd(oo, HT.x, HT.y); }
		if (q.armOn) { bbAdd(oo, q.bx + 56, -58 * q.bsy); bbAdd(oo, q.bx + 56, -22 * q.bsy); }
		if (showStars) { bbAdd(oo, -76, -100); bbAdd(oo, -58, -82); bbAdd(oo, 80, -114); bbAdd(oo, 62, -96); }
		/* 描边半宽 + 轮廓光余量 */
		u = 4 * K * oo.s;
		return { l: BB.l - u, t: BB.t - u, r: BB.r + u, b: BB.b + u };
	}

	/* ---------------- 主入口 ---------------- */
	function sstepPulse(t) {
		var b = max(pulse(t, 3.9, 0.35, 0.2), pulse(t, 9.1, 1.2, 0.18), pulse(t, 9.1, 1.58, 0.18));
		return b * b * (3 - 2 * b);
	}
	/* 每个 ctx 一份切 pose 记录（给弹簧做新旧混合） */
	function track(G, pose, t, o) {
		var ph = (o.phase != null) ? clamp(o.phase, 0, 1) : 0;
		var dur = (o.dur != null && o.dur > 0.05) ? o.dur : (DURS[pose] || 0.6);
		if (G.cur !== pose || t < G.lastT - 0.001 || t - G.lastT > 0.4) {
			if (G.cur && G.cur !== pose && t >= G.lastT && t - G.lastT <= 0.4) { G.prev = G.cur; G.prevPh = G.lastPh; G.prevDur = G.lastDur; G.sw = t; }
			else if (t < G.lastT - 0.001 || t - G.lastT > 0.4) G.prev = null;
			G.cur = pose;
		}
		if (G.prev && t > G.sw + WIN + 0.1) G.prev = null;
		G.lastT = t; G.lastPh = ph; G.lastDur = dur;
	}

	function draw(ctx, o) {
		o = o || {};
		var c = ctx, pose = o.pose || 'idle', t = o.t || 0, P = o.dark ? PD : PL;
		var G = cache(c);
		var i; for (i = 0; i < POSES.length && POSES[i] !== pose; i++); if (i === POSES.length) pose = 'idle';
		track(G, pose, t, o);
		var s = o.s == null ? 1 : o.s, sxx = o.sx == null ? 1 : o.sx, syy = o.sy == null ? 1 : o.sy;
		var q = solve(pose, t, o, G);
		var base = 1, m = null;
		try { m = c.getTransform ? c.getTransform() : null; } catch (e) { m = null; }
		base = m ? sqrt(m.a * m.a + m.b * m.b) : (o.dpr || 1);
		MINW = 1.1 / (base * s * K * max(0.2, min(sxx, syy)));
		var blink = (o.blink != null) ? o.blink : sstepPulse(t);
		if (q.eye === 1 || q.eye === 2) blink = 0;
		var lift = q.by * K * s * syy;
		var rot = (o.rot || 0) + q.rot;
		var pv = (o.pivotY != null) ? o.pivotY : q.pivot;
		var x = o.x || 0, y = o.y || 0;
		if (o.look && o.look.x != null) q.lookx = o.look.x;
		if (o.look && o.look.y != null) q.looky = o.look.y;
		if (o.shadow !== false) {
			var lf = clamp(-q.by / 60, 0, 1), sc = 1 - lf * 0.45;
			c.save(); c.translate(x, y); c.scale(s * sxx * K, s * syy * K);
			c.beginPath(); ell(c, 5 + q.bx * 0.6, 1.2, 36 * sc, 6.2 * sc); c.fillStyle = P.shadow; c.globalAlpha = 1 - lf * 0.45; c.fill(); c.restore();
		}
		c.save();
		c.translate(x, y + lift);
		var pyW = pv * K * s * syy;
		if (rot) { c.translate(0, pyW); c.rotate(rot); c.translate(0, -pyW); }
		var fl = (o.flip ? -1 : 1) * q.turn;
		c.scale(s * sxx * K * fl, s * syy * K);
		c.lineJoin = 'round'; c.lineCap = 'round';
		tailPts(q);
		c.save(); c.translate(q.bx, 0);
		if (!q.tailFront) { c.save(); c.scale(q.tsx, 1); drawTail(c, P); c.restore(); }
		drawBody(c, P, q);
		if (q.tailFront) { c.save(); c.scale(q.tsx, 1); drawTail(c, P); c.restore(); }
		c.restore();
		if (q.dg && q.armW > 0.02) drawArms(c, P, q, 0);
		c.save(); c.translate(q.bx, 0); drawBell(c, q); c.restore();
		var hcy = -44 * q.bsy - 22 + q.hy;
		c.save(); c.translate(q.bx + q.hx, hcy); c.rotate(q.hrot); c.scale(q.hsx * (1 + (q.bsx - 1) * 0.5), q.hsy * (1 + (q.bsy - 1) * 0.6));
		drawHead(c, P, q, t, blink);
		c.restore();
		if (q.dg && q.armW > 0.02) drawArms(c, P, q, 1);
		/* 挥爪：抬起的右手套（短臂、大掌）画在头前 */
		if (q.armOn) {
			c.save(); c.translate(q.bx, 0); c.scale(q.bsx, q.bsy);
			drawMitten(c, P, 14, -31, q.armAng);
			c.restore();
		}
		var showStars = (o.stars != null) ? !!o.stars : !!q.stars;
		if (showStars) drawStars(c, P, t);
		c.restore();
		TMP.s = s; TMP.sxx = sxx; TMP.syy = syy; TMP.fl = fl; TMP.pv = pv; TMP.rot = rot; TMP.x = x; TMP.y = y; TMP.lift = lift;
		var hxx = q.bx + q.hx;
		return {
			feet: pt(TMP, 0, 0, { x: 0, y: 0 }),
			head: pt(TMP, hxx, hcy, { x: 0, y: 0 }),
			top: pt(TMP, hxx, hcy - 54 * q.hsy, { x: 0, y: 0 }),
			body: pt(TMP, q.bx, -22 * q.bsy, { x: 0, y: 0 }),
			mouth: pt(TMP, hxx + q.yaw * 4.6 * fl * fl, hcy + 22 * q.hsy, { x: 0, y: 0 }),
			face: pt(TMP, hxx, hcy + 4, { x: 0, y: 0 }),
			bounds: boundsOf(TMP, q, hcy, showStars)
		};
	}

	/* 不绘制、只算包围盒。坐标相对 (o.x,o.y)=脚底。
	 * 默认按 o.t/o.phase 算“这一刻”的盒；worst:true 则取整个周期的并集（一次性给画布留顶部余量）。 */
	function boundsOne(o, t, phase) {
		var pose = o.pose || 'idle', i; for (i = 0; i < POSES.length && POSES[i] !== pose; i++); if (i === POSES.length) pose = 'idle';
		var s = o.s == null ? 1 : o.s, sxx = o.sx == null ? 1 : o.sx, syy = o.sy == null ? 1 : o.sy;
		var oo = { phase: phase, dur: o.dur, hopH: o.hopH, noLift: o.noLift, selfLift: o.selfLift };
		var q = solve(pose, t, oo, null);
		var rot = (o.rot || 0) + q.rot, pv = (o.pivotY != null) ? o.pivotY : q.pivot;
		TMP.s = s; TMP.sxx = sxx; TMP.syy = syy; TMP.fl = (o.flip ? -1 : 1) * q.turn; TMP.pv = pv; TMP.rot = rot; TMP.x = 0; TMP.y = 0; TMP.lift = q.by * K * s * syy;
		var showStars = (o.stars != null) ? !!o.stars : !!q.stars;
		return boundsOf(TMP, q, -44 * q.bsy - 22 + q.hy, showStars);
	}
	function bounds(o) {
		o = o || {};
		var pose = o.pose || 'idle', r, b, i, n = 24, ph;
		if (o.worst) {
			r = { l: 1e9, t: 1e9, r: -1e9, b: -1e9 };
			for (i = 0; i < n; i++) {
				ph = i / (n - 1);
				b = boundsOne(o, i * 0.17, isPhasePose(pose) ? ph : null);
				if (b.l < r.l) r.l = b.l; if (b.r > r.r) r.r = b.r; if (b.t < r.t) r.t = b.t; if (b.b > r.b) r.b = b.b;
			}
		} else {
			r = boundsOne(o, o.t || 0, (o.phase != null) ? o.phase : (isPhasePose(pose) ? (((o.t || 0) * 0.9) % 1) : null));
		}
		var s = o.s == null ? 1 : o.s, syy = o.sy == null ? 1 : o.sy;
		return { l: r.l, t: r.t, r: r.r, b: r.b, w: r.r - r.l, h: r.b - r.t, extraTop: max(0, -r.t - HEIGHT * s * syy) };
	}
	/* 内部腾空占 hopH 的比例 0..1（给想自己算 y 的调用方；只在腾空段非零） */
	function hopLift(phase, pose) {
		var p = clamp(phase, 0, 1), a = pose === 'flip' ? 0.16 : 0.2, e = pose === 'flip' ? 0.86 : 0.82, w;
		if (p <= a || p >= e) return 0;
		w = (p - a) / (e - a); return 4 * w * (1 - w);
	}

	root.MwrtPlushV2 = { DG: DG, draw: draw, bounds: bounds, hopLift: hopLift, poses: POSES, HEIGHT: HEIGHT, WIDTH: WIDTH, K: K };
})(typeof window !== 'undefined' ? window : this);

/* ====================================================================
 * 玩偶猫「被抓住拖拽」（s90 第四轮；来源 _s90/out/drag/final/drag.js 终稿，物理与表情逐字沿用，只改了 setHome 加 lift、attach 加 cancel）
 * window.MwrtDrag = { create(opts), attach(el, host), version }；依赖上面的 window.MwrtPlushV2（已加 'drag' 姿态与 DG）。
 * 约定见 mwrt-catcage.js 的「玩偶拖拽」一节。纯 ES5，无外部资源。
 * ==================================================================== */
(function (root) {
	'use strict';
	var PBNS = { MwrtPlushV2: root.MwrtPlushV2 };
	if (!PBNS.MwrtPlushV2 || !PBNS.MwrtPlushV2.DG) return;
/* ====================================================================
 * 拖拽玩偶猫 · 终稿「关节链 / 轻量布娃娃」物理与绘制胶水
 *
 * 链（世界坐标，单位 px，y 向下）：
 *   H(手，被指针钉住) -绳- S(肩) -杆- Pv(臀) -杆- FL/FR(两只脚)
 *   尾巴 T1-T2-T3 三节链（根部钉在躯干坐标系的尾根）
 *   双耳各一节（根部钉在头坐标系的耳根）
 * 全部用位置 Verlet + 固定迭代次数的距离约束，固定子步长 SUB。
 * 链的姿态再映射成玩偶 v2 的绘制参数（经 plush.DG 交给派生副本的 'drag' 姿态）。
 * 本文件无 DOM 依赖（render 只用传入的 ctx），可在 node 里直接跑数值断言。
 * ==================================================================== */

var PB = PBNS.MwrtPlushV2;
var DG = PB.DG;

var PI = Math.PI, sin = Math.sin, cos = Math.cos, sqrt = Math.sqrt, abs = Math.abs, exp = Math.exp, atan2 = Math.atan2;
function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
function lerp(a, b, t) { return a + (b - a) * t; }
function tanh1(x) { if (x > 9) return 1; var e = exp(2 * x); return (e - 1) / (e + 1); }

/* ---------------- 物理常量 ---------------- */
var SUB = 1 / 240;                   /* 固定子步长（秒） */
var ITER = 8;                        /* 每个子步的约束迭代次数（固定） */
var MAXFRAME = 0.05;                 /* 单帧最大推进时间（dt 钳制） */
var G = 2300;                        /* 重力 px/s^2：猫高 60px≈0.25m，按 9.8m/s^2 换算 */
var E_GROUND = 0.35;                 /* 落地恢复系数 */
var E_WALL = 0.5;                    /* 撞墙/天花板恢复系数 */
var VMAX_THROW = 1500;               /* 松手抛出速度限幅 px/s */
var CLICK_PX = 5, HOLD_SEC = 0.28;   /* 点击/拖拽判定 */
var SWING_MAX = 0.85;                /* 手到肩的摆角限幅（弧度），约 49 度 */
var TORSO_SOFT = 0.42;               /* 躯干倾角软限幅起点，约 24 度：超过后用 tanh 压缩，越接近硬上限越难再倾 */
var TORSO_MAX = 0.68;                /* 躯干倾角硬上限，约 39 度（验收要求：软限约 40 度，硬上限不超 45 度） */

/* 几何（s=1 时，对应玩偶 v2 的 60px 猫）：肩到脚底 21，肩-臀 13，臀-脚 8，臂长（手到肩）52：手套高出耳尖约 12px，双臂才露得出来 */
var L_SF = 21, L_SP = 13, L_PF = 8, L_ARM = 52;
/* 节点编号 */
var nH = 0, nS = 1, nP = 2, nFL = 3, nFR = 4, nT1 = 5, nT2 = 6, nT3 = 7, nEL = 8, nER = 9, NN = 10;
var MASS = [0, 1.6, 2.2, 0.5, 0.5, 0.25, 0.2, 0.15, 0.1, 0.1];
var DAMP_C = [0, 0.9, 0.9, 1.6, 1.6, 2.6, 2.6, 2.6, 3, 3];       /* 每节点速度阻尼系数 1/s */
var GSCALE = [0, 1, 1, 1, 1, 0.85, 0.85, 0.85, 0.9, 0.9];

/* 尾巴静止形（idle 的贝塞尔控制点在 t=1/3,2/3,1 处的采样，设计单位） */
var TAIL_P0 = [20, -12], TAIL_IDLE = [50, -2, 62, -40, 38, -66];
function bez(t, p0, c1, c2, p3) { var v = 1 - t; return v * v * v * p0 + 3 * v * v * t * c1 + 3 * v * t * t * c2 + t * t * t * p3; }

/* 粒子种类 */
var K_DUST = 1, K_STAR = 2, K_DROP = 3, K_BUBBLE = 4;
var NPART = 40;
var BUB_DX = 34, BUB_HW = 17, BUB_HH = 15;   /* 气泡中心相对头的水平偏移、半宽、半高（设计 px，s=1，含放大余量） */

function create(opts) {
	opts = opts || {};
	var s = opts.s || 1;
	var zone = { l: 0, t: 0, r: 220, b: 260 };
	var ground = 246;
	var dark = !!opts.dark;

	/* ---------- 状态（全部预分配，运行期不分配） ---------- */
	var X = new Float64Array(NN), Y = new Float64Array(NN), QX = new Float64Array(NN), QY = new Float64Array(NN);
	var IM = new Float64Array(NN), DMP = new Float64Array(NN);
	var i;
	var DMPF = exp(-0.05 * SUB);
	for (i = 0; i < NN; i++) { IM[i] = MASS[i] > 0 ? 1 / MASS[i] : 0; DMP[i] = exp(-DAMP_C[i] * SUB); }
	var lSF = L_SF * s, lSP = L_SP * s, lPF = L_PF * s, lArm = L_ARM * s;
	var tailRest = [0, 0, 0, 0, 0, 0];            /* 尾巴 3 节点静止位置（猫坐标系 px） */
	var tailLen = [0, 0, 0];
	(function () {
		var k = 0.5 * s, p0x = TAIL_P0[0] * k, p0y = TAIL_P0[1] * k, t, px = p0x, py = p0y, nx, ny, j;
		for (j = 0; j < 3; j++) {
			t = (j + 1) / 3;
			nx = bez(t, TAIL_P0[0], TAIL_IDLE[0], TAIL_IDLE[2], TAIL_IDLE[4]) * k;
			ny = bez(t, TAIL_P0[1], TAIL_IDLE[1], TAIL_IDLE[3], TAIL_IDLE[5]) * k;
			tailRest[j * 2] = nx; tailRest[j * 2 + 1] = ny;
			tailLen[j] = sqrt((nx - px) * (nx - px) + (ny - py) * (ny - py));
			px = nx; py = ny;
		}
	})();
	var earLen = 19 * s, earBaseX = 12 * s, earBaseY = -43.5 * s;
	var earRestAng = 0.5;                          /* 耳朵静止方向：偏离竖直向外 0.5 弧度 */

	var mode = 0;            /* 0 空闲(宿主自己画) 1 被抓 2 下落 3 落地弹跳 4 稳态收尾 */
	var tSim = 0;            /* 物理时钟（秒） */
	var acc = 0;
	var homeX = 110;
	var homeLift = 0;        /* 待机时脚离地的高度(<=0, 向上为负): 半空中被抓时从当前高度提起, 不先瞬移到地面 */
	/* 指针 */
	var pressed = false, pdx = 0, pdy = 0, pT = 0, ptx = 0, pty = 0, pointerId = -1;
	var tgtX = 0, tgtY = 0;  /* 指针（已按空闲区限幅前的原始值在 rawX/rawY） */
	var rawX = 0, rawY = 0;
	var grabT = 0;           /* 抓取后一小段更慢的“伸手”平滑 */
	var SN = 12, sT = new Float64Array(SN), sX = new Float64Array(SN), sY = new Float64Array(SN), sI = 0, sCount = 0;
	/* 落地 */
	var rootX = 0, rootY = 0, rootVX = 0, rootVY = 0, bounces = 0, vinMax = 0, lastVin = 0;
	var settleT = 0, settleLen = 1.35, doneFlag = false, bigFall = false;   /* settleLen: 落稳阶段时长, 进入落稳越晚越短, 使落地到回待机总长不超过约 2 秒 */
	/* 二级运动 */
	var sq = 0, sqv = 0;                       /* 落地挤压弹簧 */
	var hA = 0, hV = 0;                        /* 头相对躯干的角度弹簧 */
	var bA = 0, bV = 0;                        /* 铃铛 */
	var thF = 0, thVF = 0, thVPrev = 0, thAcc = 0, thPrev = 0;
	var aSx = 0, aSy = 0, vSx = 0, vSy = 0;    /* S 的低通加速度 */
	var hang = 0, armW = 0, tension = 0;
	var gxL = 0, gyL = -120;                   /* 手在猫坐标系（设计单位） */
	var earAngL = 0, earAngR = 0;
	var th = 0, cs = 1, sn = 0, f0x = 0, f0y = 0;   /* 猫坐标系：旋转角、原点（脚底中点） */
	var swing = 0, swingV = 0, swingPrev = 0;
	var rotDraw = 0;
	var lastHandVX = 0;
	/* 粒子池 */
	var P_k = new Uint8Array(NPART), P_x = new Float64Array(NPART), P_y = new Float64Array(NPART), P_vx = new Float64Array(NPART), P_vy = new Float64Array(NPART);
	var P_l = new Float64Array(NPART), P_m = new Float64Array(NPART), P_s = new Float64Array(NPART), P_r = new Float64Array(NPART);
	var pNext = 0;
	var meowT = 0, dropT = 0, bubbleOn = 0;
	var dizzyT = 0;
	var wave = 0;

	var D = { s: s };
	var REL = { vx: 0, vy: 0 };            /* 松手初速度（复用，不在抓放循环里新建对象） */

	function spawn(k, x, y, vx, vy, life, size, rot) {
		var n = NPART, j = pNext, c;
		for (c = 0; c < n; c++) { if (P_l[(j + c) % n] <= 0) { j = (j + c) % n; break; } }
		if (c === n) j = pNext;          /* 池满则覆盖最旧的 */
		pNext = (j + 1) % n;
		P_k[j] = k; P_x[j] = x; P_y[j] = y; P_vx[j] = vx; P_vy[j] = vy; P_l[j] = life; P_m[j] = life; P_s[j] = size; P_r[j] = rot || 0;
		return j;
	}
	function clearParts() { var j; for (j = 0; j < NPART; j++) P_l[j] = 0; bubbleOn = 0; }

	/* ---------- 猫坐标系 ---------- */
	function frame() {
		var dx = X[nP] - X[nS], dy = Y[nP] - Y[nS], d = sqrt(dx * dx + dy * dy) || 1e-6;
		dx /= d; dy /= d;                         /* 躯干朝下的单位向量 */
		th = atan2(-dx, dy);
		cs = cos(th); sn = sin(th);
		f0x = X[nS] + dx * lSF; f0y = Y[nS] + dy * lSF;
	}
	var WX = 0, WY = 0;
	function toWorld(lx, ly) { WX = f0x + lx * cs - ly * sn; WY = f0y + lx * sn + ly * cs; }
	var LX = 0, LY = 0;
	function toLocal(wx, wy) { var dx = wx - f0x, dy = wy - f0y; LX = dx * cs + dy * sn; LY = -dx * sn + dy * cs; }

	/* ---------- 初始化为站姿 ---------- */
	function standAt(x) {
		var g = ground, j;
		X[nH] = x; Y[nH] = g - lSF - lArm;
		X[nS] = x; Y[nS] = g - lSF;
		X[nP] = x; Y[nP] = g - lPF;
		X[nFL] = x - 4 * s; Y[nFL] = g; X[nFR] = x + 4 * s; Y[nFR] = g;
		th = 0; cs = 1; sn = 0; f0x = x; f0y = g;
		for (j = 0; j < 3; j++) { X[nT1 + j] = x + tailRest[j * 2]; Y[nT1 + j] = g + tailRest[j * 2 + 1]; }
		X[nEL] = x - earBaseX - sin(earRestAng) * earLen; Y[nEL] = g + earBaseY - cos(earRestAng) * earLen;
		X[nER] = x + earBaseX + sin(earRestAng) * earLen; Y[nER] = g + earBaseY - cos(earRestAng) * earLen;
		for (j = 0; j < NN; j++) { QX[j] = X[j]; QY[j] = Y[j]; }
		sq = sqv = 0; hA = hV = 0; bA = bV = 0; thF = thVF = thVPrev = thAcc = 0; thPrev = 0; aSx = aSy = vSx = vSy = 0;
		swing = swingV = swingPrev = 0; hang = 0; armW = 0; tension = 0;
		earAngL = earAngR = 0;
	}

	/* ---------- 约束 ---------- */
	function dist(a, b, len) {
		var dx = X[b] - X[a], dy = Y[b] - Y[a], d = sqrt(dx * dx + dy * dy) || 1e-6, df = (d - len) / d, wa = IM[a], wb = IM[b], ws = wa + wb, f;
		if (ws === 0) return;
		f = df / ws;
		X[a] += dx * f * wa; Y[a] += dy * f * wa; X[b] -= dx * f * wb; Y[b] -= dy * f * wb;
	}
	function distFix(a, bx, by, len) {         /* 节点 a 与固定点 (bx,by) 保持距离 len（节点全量移动） */
		var dx = X[a] - bx, dy = Y[a] - by, d = sqrt(dx * dx + dy * dy) || 1e-6, f = len / d;
		X[a] = bx + dx * f; Y[a] = by + dy * f;
	}
	function ropeMax(a, b, len) {             /* 只限最大距离：b 被拉向钉住的 a */
		var dx = X[b] - X[a], dy = Y[b] - Y[a], d = sqrt(dx * dx + dy * dy);
		if (d > len) { var f = len / d; X[b] = X[a] + dx * f; Y[b] = Y[a] + dy * f; }
	}
	/* 绕 (cx,cy) 把节点 a 转 ang 弧度 */
	function rotAbout(a, cx, cy, ang) {
		var c = cos(ang), n = sin(ang), dx = X[a] - cx, dy = Y[a] - cy;
		X[a] = cx + dx * c - dy * n; Y[a] = cy + dx * n + dy * c;
	}
	/* 让线段 a->b 绕质心转向“b 在 a 正下方”，比例 k；位置修正不改质心 → 动量守恒 */
	function rightPair(a, b, k) {
		var wa = IM[a] > 0 ? 1 / IM[a] : 1, wb = IM[b] > 0 ? 1 / IM[b] : 1, cx = (X[a] * wa + X[b] * wb) / (wa + wb), cy = (Y[a] * wa + Y[b] * wb) / (wa + wb);
		var ang = atan2(-(X[b] - X[a]), Y[b] - Y[a]);   /* 线段偏离“向下”的角（顺时针为正） */
		if (ang > PI) ang -= 2 * PI; if (ang < -PI) ang += 2 * PI;
		ang = -ang * k;
		rotAbout(a, cx, cy, ang); rotAbout(b, cx, cy, ang);
	}

	/* ---------- 子步 ---------- */
	function pointerClamp() {
		var x = rawX, y = rawY;
		var xl = zone.l + 26 * s, xr = zone.r - 26 * s, yt = zone.t + 25 * s, yb = ground - lSF - lArm;
		yt += clamp((yb - yt) * 0.5, 0, 9 * s);        /* 区域够高时手的上限再让出一点, 免得手套在大幅倾斜时被画布上沿切掉 */
		tgtX = clamp(x, xl, xr); tgtY = clamp(y, yt, yb);
	}

	function secondary() {
		/* 低通的 S 加速度 → 铃铛“有效重力”；躯干角加速度 → 头的角度弹簧 */
		var vx = (X[nS] - QX[nS]) / SUB, vy = (Y[nS] - QY[nS]) / SUB, ax = (vx - vSx) / SUB, ay = (vy - vSy) / SUB, k = 1 - exp(-SUB / 0.02);
		vSx = vx; vSy = vy;
		aSx += (ax - aSx) * k; aSy += (ay - aSy) * k;
		var thv = (th - thPrev); if (thv > PI) thv -= 2 * PI; if (thv < -PI) thv += 2 * PI;
		thv /= SUB; thPrev = th;
		thVF += (thv - thVF) * (1 - exp(-SUB / 0.012));
		thAcc = (thVF - thVPrev) / SUB; thVPrev = thVF;
		thAcc = clamp(thAcc, -400, 400);
		var wh = 17, zh = 0.28, acc2;
		acc2 = -wh * wh * hA - 2 * zh * wh * hV - 2.2 * thAcc * 0.5;
		hV += acc2 * SUB; hA += hV * SUB; hA = clamp(hA, -0.5, 0.5);
		/* 铃铛：朝“有效重力”方向悬挂 */
		var egx = -aSx, egy = G - aSy, eg = sqrt(egx * egx + egy * egy), lx = egx * cs + egy * sn, ly = -egx * sn + egy * cs, tgt, wgt;
		wgt = clamp(eg / (0.35 * G), 0, 1);
		tgt = atan2(-lx, ly) * wgt;
		tgt = clamp(tgt, -1.0, 1.0);
		var wb = 14, zb = 0.2;
		bV += (-wb * wb * (bA - tgt) - 2 * zb * wb * bV) * SUB; bA += bV * SUB; bA = clamp(bA, -1.3, 1.3);
		/* 落地挤压弹簧 */
		var ws = 26, zs = 0.26;
		sqv += (-ws * ws * sq - 2 * zs * ws * sqv) * SUB; sq += sqv * SUB; sq = clamp(sq, -0.3, 0.46);
		/* 摆角（手到质心）及其角速度，用于断言与表情 */
		var cmx = (X[nS] * MASS[nS] + X[nP] * MASS[nP] + X[nFL] * MASS[nFL] + X[nFR] * MASS[nFR]) / (MASS[nS] + MASS[nP] + MASS[nFL] + MASS[nFR]);
		var cmy = (Y[nS] * MASS[nS] + Y[nP] * MASS[nP] + Y[nFL] * MASS[nFL] + Y[nFR] * MASS[nFR]) / (MASS[nS] + MASS[nP] + MASS[nFL] + MASS[nFR]);
		swing = atan2(cmx - X[nH], cmy - Y[nH]);
		swingV = (swing - swingPrev) / SUB; swingPrev = swing;
	}

	function kinematics(held) {
		/* 尾根、耳根随猫坐标系；尾巴和耳朵节点做距离约束并向静止形回弹 */
		var j, kt = 1 - exp(-SUB * (held ? 3.2 : 16)), ke = 1 - exp(-SUB * (held ? 9 : 22)), bx, by, rx, ry, nx, ny;
		/* 尾巴 */
		toWorld(TAIL_P0[0] * 0.5 * s, TAIL_P0[1] * 0.5 * s);
		bx = WX; by = WY;
		for (j = 0; j < 3; j++) {
			toWorld(tailRest[j * 2], tailRest[j * 2 + 1]);
			X[nT1 + j] += (WX - X[nT1 + j]) * kt; Y[nT1 + j] += (WY - Y[nT1 + j]) * kt;
		}
		for (i = 0; i < 2; i++) {
			distFix(nT1, bx, by, tailLen[0]);
			dist(nT1, nT2, tailLen[1]);
			dist(nT2, nT3, tailLen[2]);
			distFix(nT1, bx, by, tailLen[0]);
		}
		/* 耳朵（头坐标系 = 猫坐标系 + 头相对角 + 头自身倾角，粗略取 hA） */
		for (j = 0; j < 2; j++) {
			var sd = j ? 1 : -1, ni = j ? nER : nEL, ha = hA + 0.09;
			var ex = sd * earBaseX, ey = earBaseY, ca = cos(ha), sa = sin(ha);
			toWorld(ex * ca - ey * sa, ex * sa + ey * ca + 0);   /* 以猫坐标原点旋转，近似头绕颈部的摆动 */
			rx = WX; ry = WY;
			var ang = sd * earRestAng + ha;
			var ux = sin(ang), uy = -cos(ang);
			nx = rx + (ux * cs - uy * sn) * earLen; ny = ry + (ux * sn + uy * cs) * earLen;
			X[ni] += (nx - X[ni]) * ke; Y[ni] += (ny - Y[ni]) * ke;
			distFix(ni, rx, ry, earLen);
		}
		clampLimbs();
	}
	/* 尾巴 3 节与双耳尖始终夹在空闲区内（含笔画粗细的余量），不会越出虚线 */
	function clampLimbs() {
		var j, pad;
		for (j = nT1; j <= nER; j++) {
			pad = (j <= nT3 ? PAD_TAIL : PAD_EAR);
			if (X[j] < zone.l + pad) X[j] = zone.l + pad; else if (X[j] > zone.r - pad) X[j] = zone.r - pad;
			if (Y[j] < zone.t + pad) Y[j] = zone.t + pad; else if (Y[j] > zone.b - pad) Y[j] = zone.b - pad;
		}
	}
	/* 头的中心（肩沿躯干向上 HEAD_UP），用来把头也挡在墙内；结果放在 HX/HY */
	var HX = 0, HY = 0;
	function headPoint() {
		var ux = X[nS] - X[nP], uy = Y[nS] - Y[nP], d = sqrt(ux * ux + uy * uy) || 1;
		HX = X[nS] + ux / d * HEAD_UP * s; HY = Y[nS] + uy / d * HEAD_UP * s;
	}
	function headShiftX() {
		headPoint();
		if (HX < zone.l + HEAD_R * s) return zone.l + HEAD_R * s - HX;
		if (HX > zone.r - HEAD_R * s) return zone.r - HEAD_R * s - HX;
		return 0;
	}

	var WN = [nS, nP, nFL, nFR], WM = [22 * s, 18 * s, 8 * s, 8 * s];
	var PAD_TAIL = 10 * s, PAD_EAR = 8 * s;        /* 尾巴/耳尖节点到空闲区边线的最小距离（含笔画粗细） */
	var HEAD_UP = 26, HEAD_R = 35;                /* 头中心在肩上方的距离、头+耳的半宽（设计 px，s=1） */
	function wallClamp() {
		var j, n, sx;
		for (j = 0; j < 4; j++) {
			n = WN[j];
			if (X[n] < zone.l + WM[j]) X[n] = zone.l + WM[j];
			if (X[n] > zone.r - WM[j]) X[n] = zone.r - WM[j];
		}
		/* 头先碰墙：整条身体平移（位置与上一步位置同移，不产生反弹速度），猫贴边但不出界 */
		sx = headShiftX();
		if (sx !== 0) for (j = 0; j < 4; j++) { n = WN[j]; X[n] += sx; QX[n] += sx; }
	}
	var footHit = false;
	function floorClamp() {
		var j, n, lim;
		for (j = 0; j < 4; j++) {
			n = WN[j]; lim = j === 0 ? ground - lSF + 0.5 : (j === 1 ? ground - lPF + 0.5 : ground);
			if (Y[n] > lim) { Y[n] = lim; footHit = true; if (j > 1) QX[n] += (X[n] - QX[n]) * 0.12; }   /* 肩/髋先触地（脚贴地被提着再松手）也算落地 */
		}
	}
	/* 下落中撞左右墙/天花板：整体平移，把该方向速度反向并乘恢复系数（刚体式，约束不被破坏） */
	function wallRigid() {
		var j, n, shiftX = 0, shiftY = 0, v, ceil = zone.t + 42 * s;
		for (j = 0; j < 4; j++) {
			n = WN[j];
			if (X[n] < zone.l + WM[j]) shiftX = Math.max(shiftX, zone.l + WM[j] - X[n]);
			if (X[n] > zone.r - WM[j]) shiftX = Math.min(shiftX, zone.r - WM[j] - X[n]);
		}
		if (shiftX === 0) shiftX = headShiftX();
		if (shiftX !== 0) {
			for (j = 1; j < NN; j++) { v = X[j] - QX[j]; X[j] += shiftX; QX[j] = X[j] - (shiftX > 0 ? abs(v) : -abs(v)) * E_WALL; }
		}
		if (Y[nS] < ceil) {
			shiftY = ceil - Y[nS];
			for (j = 1; j < NN; j++) { v = Y[j] - QY[j]; Y[j] += shiftY; QY[j] = Y[j] - abs(v) * E_WALL; }
		}
	}

	var preVy = 0;
	function substep() {
		var j, it, d, g;
		tSim += SUB; footHit = false;
		if (mode === 1) {
			/* ---- 被抓：手钉在（限幅后的）指针上，带轻微跟手平滑 ---- */
			pointerClamp();
			var tau = grabT > 0 ? 0.045 : 0.012, kh = 1 - exp(-SUB / tau), mx, my, ml;
			mx = (tgtX - X[nH]) * kh; my = (tgtY - Y[nH]) * kh;
			ml = sqrt(mx * mx + my * my); if (ml > 14 * s) { mx *= 14 * s / ml; my *= 14 * s / ml; }
			X[nH] += mx; Y[nH] += my; QX[nH] = X[nH]; QY[nH] = Y[nH];
			if (grabT > 0) grabT -= SUB;
		}
		/* Verlet 积分 */
		for (j = 1; j < NN; j++) {
			var dm = (mode === 2 && j <= 4) ? DMPF : DMP[j];     /* 自由下落时身体几乎无空气阻力，保证 sqrt(2h/g) */
			var vx = (X[j] - QX[j]) * dm, vy = (Y[j] - QY[j]) * dm;
			QX[j] = X[j]; QY[j] = Y[j];
			X[j] += vx; Y[j] += vy + G * GSCALE[j] * SUB * SUB;
		}
		/* 约束修正之前的下落速度（落地瞬间约束会把它压掉，所以这里先记下，用来判断落差轻重） */
		preVy = ((Y[nP] - QY[nP]) + (Y[nS] - QY[nS])) * 0.5 / SUB;
		if (mode === 3 || mode === 4) {
			groundMode();
		} else {
			for (it = 0; it < ITER; it++) {
				if (mode === 1) {
					ropeMax(nH, nS, lArm);
					/* 摆角限幅：S 必须在 H 下方的锥形内 */
					var dx = X[nS] - X[nH], dy = Y[nS] - Y[nH], dd = sqrt(dx * dx + dy * dy), a = atan2(dx, dy);
					if (dd > 1e-6 && abs(a) > SWING_MAX) {
						var a2 = a > 0 ? SWING_MAX : -SWING_MAX;
						X[nS] = X[nH] + sin(a2) * dd; Y[nS] = Y[nH] + cos(a2) * dd;
					}
				}
				dist(nS, nP, lSP);
				dist(nP, nFL, lPF); dist(nP, nFR, lPF);
				dist(nFL, nFR, 8 * s);              /* 两脚不并成一点，也不分太开：用等式取小间距 */
				if (mode === 1) {
					/* 躯干倾角上限 */
					var tx = X[nP] - X[nS], ty = Y[nP] - Y[nS], ta = atan2(-tx, ty), aa = abs(ta);
					if (aa > TORSO_SOFT) {
						/* 软限幅：超出软限的部分用 tanh 压进 (软限, 硬上限) 之间，没有硬撞的顿挫 */
						var span = TORSO_MAX - TORSO_SOFT, want = TORSO_SOFT + span * tanh1((aa - TORSO_SOFT) / span);
						if (want > TORSO_MAX) want = TORSO_MAX;
						rotAbout(nP, X[nS], Y[nS], (ta > 0 ? want : -want) - ta);
					}
				}
				floorClamp(); if (mode === 1) wallClamp();
			}
			if (mode === 2) {
				wallRigid();
				/* 下落中：躯干和腿缓慢转向“脚朝下”（质心不动，动量守恒） */
				var kr = 1 - exp(-SUB * 9);
				rightPair(nS, nP, kr);
				rightPair(nP, nFL, kr * 0.6); rightPair(nP, nFR, kr * 0.6);
			}
		}
		frame();
		kinematics(mode === 1);
		secondary();
		/* 下落中检测落地 */
		if (mode === 2 && footHit) landed();
	}

	/* ---------- 落地 ---------- */
	function landed() {
		var vy = ((Y[nP] - QY[nP]) + (Y[nS] - QY[nS])) * 0.5 / SUB, vx = ((X[nP] - QX[nP]) + (X[nS] - QX[nS])) * 0.5 / SUB;
		if (preVy > vy) vy = preVy;
		lastVin = vy > 0 ? vy : 0;
		rootX = (X[nFL] + X[nFR]) * 0.5; rootY = ground; rootVX = vx; rootVY = -E_GROUND * lastVin;
		bounces = 0; vinMax = lastVin; bigFall = lastVin > 650;
		mode = 3; D.landT = tSim;
		impact(lastVin);
		if (lastVin < 150) { rootVY = 0; }
		dizzyT = bigFall ? 1.5 : 0;
		settleT = 0;
	}
	function impact(vin) {
		var n, j, f = clamp(vin / 900, 0, 1.3);
		sqv += 0.0105 * vin;
		n = 3 + ((vin / 220) | 0); if (n > 9) n = 9;
		for (j = 0; j < n; j++) {
			var side = (j & 1) ? 1 : -1, sp = 70 + 90 * f * (0.4 + 0.6 * ((j * 37) % 10) / 10);
			spawn(K_DUST, rootX + side * (6 + 4 * (j >> 1)) * s, ground - 2 * s, side * sp, -30 - 30 * f, 0.34 + 0.08 * f, (2.4 + 2.0 * f) * s, 0);
		}
		if (vin > 650) {
			for (j = 0; j < 3; j++) spawn(K_STAR, rootX, ground - 34 * s, (j - 1) * 70, -150 - 30 * j, 0.9, 5 * s, j);
		}
	}
	function groundMode() {
		var j, kk = 1 - exp(-SUB * 38);
		/* 根点（脚）弹道 */
		if (rootY > ground) {      /* 落稳时空闲区缩小(窗口/布局高度变化): 地面上移了, 整条关节链连同上一步位置一起平移到新地面, 不停在区外也不抖 */
			var gdy = ground - rootY;
			for (j = 0; j < NN; j++) { Y[j] += gdy; QY[j] += gdy; }
			f0y += gdy; rootY = ground; if (rootVY > 0) rootVY = 0;
		}
		var onG = (rootY >= ground - 1e-6 && rootVY === 0);
		if (!onG) {
			rootVY += G * SUB; rootY += rootVY * SUB;
			if (rootY >= ground && rootVY > 0) {
				rootY = ground;
				if (rootVY > 150) { var v = rootVY; bounces++; rootVY = -E_GROUND * v; impact(v * 0.7); if (bounces > 3) rootVY = 0; }
				else rootVY = 0;
			}
		}
		rootX += rootVX * SUB;
		rootVX *= exp(-(rootY >= ground - 1e-6 ? 7 : 0.4) * SUB);
		if (!hbOff) D.hitBox();
		if (rootX < zone.l - hbOff.l + 1) { rootX = zone.l - hbOff.l + 1; rootVX = abs(rootVX) * E_WALL; }          /* 站姿含耳尖尾巴的实际宽度，落稳后整只猫（含尾巴）不出虚线 */
		if (rootX > zone.r - hbOff.r - 1) { rootX = zone.r - hbOff.r - 1; rootVX = -abs(rootVX) * E_WALL; }
		/* 脚钉在根点；臀和肩向“站姿目标”平滑靠拢（滞后 = 二级运动） */
		X[nFL] = rootX - 4 * s; Y[nFL] = rootY; X[nFR] = rootX + 4 * s; Y[nFR] = rootY;
		var kp = 1 - exp(-SUB * 30), ks = 1 - exp(-SUB * 24);
		X[nP] += (rootX - X[nP]) * kp; Y[nP] += (rootY - lPF - Y[nP]) * kp;
		X[nS] += (rootX - X[nS]) * ks; Y[nS] += (rootY - lPF - lSP - Y[nS]) * ks;
		dist(nS, nP, lSP);
		if (mode === 3 && rootY >= ground - 1e-6 && rootVY === 0 && abs(rootVX) < 12) { mode = 4; settleT = 0; settleLen = clamp(1.95 - (tSim - D.landT), 0.6, 1.35); }
		if (mode === 4) {
			settleT += SUB;
			if (settleT > settleLen) { finish(); }
		}
	}
	function finish() {
		mode = 0; doneFlag = true; homeX = rootX; homeLift = 0; clearParts();
		standAt(homeX);
		DG.w = 0;
	}

	/* ---------- 输入 ---------- */
	function startHeld() {
		if (mode === 0) {
			standAt(homeX);
			if (homeLift < 0) { for (var jj = 0; jj < NN; jj++) { Y[jj] += homeLift; QY[jj] += homeLift; } f0y += homeLift; }
		}
		mode = 1; grabT = 0.2; armW = armW; doneFlag = false;
		sI = 0; sCount = 0; rootVY = 0; meowT = 0.5; dropT = 0.4;
		clearParts();
		X[nH] = clamp(X[nS], zone.l + 26 * s, zone.r - 26 * s); Y[nH] = Y[nS] - lArm * 0.7; QX[nH] = X[nH]; QY[nH] = Y[nH];
		sqv += 3;                                /* 被提起的惊喜小弹跳 */
		/* 被抓前如果在地面/下落，保留当前速度；在地面站着则速度为 0 */
	}
	function sample(x, y, tms) {
		var k = sI % SN; sT[k] = tms / 1000; sX[k] = x; sY[k] = y; sI++; if (sCount < SN) sCount++;
	}
	function pointerVel(nowT) {
		/* 取最近 100ms 内最老的样本到最新样本的平均速度；最新样本太旧则视为已停手 */
		var last = (sI - 1) % SN, j, k, t0, x0, y0, n = sCount, vx = 0, vy = 0;
		if (!n) { D.vx = 0; D.vy = 0; return; }
		if (nowT - sT[last] > 0.07) { lastHandVX = 0; D.vx = 0; D.vy = 0; return; }
		k = last; t0 = sT[last]; x0 = sX[last]; y0 = sY[last];
		for (j = 1; j < n; j++) {
			var q = (last - j + SN * 4) % SN;
			if (sT[last] - sT[q] > 0.1) break;
			k = q; t0 = sT[q]; x0 = sX[q]; y0 = sY[q];
		}
		if (sT[last] - t0 > 0.012) { vx = (sX[last] - x0) / (sT[last] - t0); vy = (sY[last] - y0) / (sT[last] - t0); }
		D.vx = vx; D.vy = vy;
	}
	D.vx = 0; D.vy = 0; D.landT = 0; D.relT = 0;

	D.setZone = function (z, groundY) {
		zone.l = z.l; zone.t = z.t; zone.r = z.r; zone.b = z.b;
		ground = groundY != null ? groundY : z.b - 14;
		if (mode === 0) standAt(homeX);
	};
	D.setHome = function (x, lift) {
		var l = lift < 0 ? lift : 0;
		if (x === homeX && l === homeLift) return;
		homeX = x; homeLift = l; if (mode === 0) standAt(x);
	};
	D.setDark = function (v) { dark = !!v; };

	/* 命中判定：用玩偶实际包围盒（站姿，含耳尖/尾巴），坐标为宿主画布坐标 */
	var HB = { l: 0, t: 0, r: 0, b: 0 }, hbOff = null;
	D.hitBox = function () {
		var fx = mode === 0 ? homeX : f0x, fy = mode === 0 ? ground : f0y;
		if (!hbOff) { var b = PB.bounds({ pose: 'idle', s: s, worst: true }); hbOff = { l: b.l, r: b.r, t: b.t, b: b.b }; }   /* 站姿包围盒与位置无关，算一次 */
		HB.l = fx + hbOff.l; HB.r = fx + hbOff.r; HB.t = fy + hbOff.t; HB.b = fy + hbOff.b;
		return HB;
	};
	D.hit = function (x, y) { var b = D.hitBox(); return x >= b.l && x <= b.r && y >= b.t && y <= b.b; };

	/* 宿主调用：按下。返回 true=接管。tms 为毫秒时间戳 */
	D.press = function (x, y, tms, id) {
		pressed = true; pdx = x; pdy = y; pT = tSim; rawX = x; rawY = y; pointerId = id == null ? -1 : id;
		sI = 0; sCount = 0; sample(x, y, tms);
		return true;
	};
	D.move = function (x, y, tms) {
		rawX = x; rawY = y;
		if (pressed || mode === 1) sample(x, y, tms);
		if (pressed && mode !== 1) {
			var dx = x - pdx, dy = y - pdy;
			if (dx * dx + dy * dy > CLICK_PX * CLICK_PX) { pressed = false; startHeld(); }
		}
	};
	/* 抬起：返回 'click' | 'drop' | 'none' */
	D.release = function (tms, cancel) {
		var r = 'none';
		if (mode === 1) {
			var j, nowT = (tms == null ? tSim * 1000 : tms) / 1000, vcx = 0, vcy = 0, tm = 0, vx, vy, sp, m;
			pointerVel(nowT); vx = cancel ? 0 : D.vx; vy = cancel ? 0 : D.vy;
			sp = sqrt(vx * vx + vy * vy); if (sp > VMAX_THROW) { vx *= VMAX_THROW / sp; vy *= VMAX_THROW / sp; }
			/* 质心速度替换为（限幅后的）指针速度，保留各节点相对速度 */
			for (j = 1; j <= 4; j++) { m = MASS[j]; tm += m; vcx += m * (X[j] - QX[j]) / SUB; vcy += m * (Y[j] - QY[j]) / SUB; }
			vcx /= tm; vcy /= tm;
			for (j = 1; j < NN; j++) { QX[j] -= (vx - vcx) * SUB; QY[j] -= (vy - vcy) * SUB; }
			toLocal(X[nH], Y[nH]); gxL = LX * 2 / s; gyL = LY * 2 / s;
			mode = 2; REL.vx = vx; REL.vy = vy; D.rel = REL; D.relT = tSim;
			r = 'drop';
		} else if (pressed) {
			r = cancel ? 'none' : 'click';
		}
		pressed = false; pointerId = -1;
		return r;
	};
	D.cancel = function () { return D.release(null, true); };

	/* ---------- 推进 ---------- */
	D.step = function (dt) {
		if (!(dt > 0)) return;
		if (dt > MAXFRAME) dt = MAXFRAME;
		acc += dt;
		/* 长按：不动也算抓 */
		if (pressed && mode !== 1 && (tSim - pT) > HOLD_SEC) { pressed = false; startHeld(); }
		var n = 0;
		while (acc >= SUB && n < 64) {
			if (mode !== 0) substep(); else tSim += SUB;
			acc -= SUB; n++;
		}
		if (mode !== 0) {
			updateExpr(dt);
			updateParticles(dt);
			placeBubbles(); keepParts();
		} else if (hasParts) updateParticles(dt);
	};
	var hasParts = false;

	function updateParticles(dt) {
		var j, any = false;
		for (j = 0; j < NPART; j++) {
			if (P_l[j] <= 0) continue;
			any = true;
			P_l[j] -= dt;
			if (P_k[j] === K_DUST) { P_x[j] += P_vx[j] * dt; P_y[j] += P_vy[j] * dt; P_vx[j] *= exp(-3.4 * dt); P_vy[j] *= exp(-2.4 * dt); }   /* 向两侧扩散并缓缓上浮（阻尼按时间算，与帧率无关） */
			else if (P_k[j] === K_STAR) { P_vy[j] += 380 * dt; P_x[j] += P_vx[j] * dt; P_y[j] += P_vy[j] * dt; P_vx[j] *= exp(-0.9 * dt); P_r[j] += 5 * dt; if (P_y[j] > ground - 2) { P_y[j] = ground - 2; P_vy[j] *= -0.4; } }
			else if (P_k[j] === K_DROP) { P_vy[j] += 700 * dt; P_x[j] += P_vx[j] * dt; P_y[j] += P_vy[j] * dt; }
			else if (P_k[j] === K_BUBBLE) { P_vy[j] += 10 * dt; }   /* P_vy 当作已上浮的距离 */
		}
		hasParts = any;
	}

	/* ---------- 表情与参数 ---------- */
	var armPhase = 0;
	function updateExpr(dt) {
		var ka = 1 - exp(-dt / 0.07), kh = 1 - exp(-dt / 0.06), hT, aT, j;
		aT = mode === 1 ? 1 : 0;
		armW += (aT - armW) * ka;
		if (mode === 1) {
			/* 手在猫坐标系里的位置 */
			toLocal(X[nH], Y[nH]); gxL = LX * 2 / s; gyL = LY * 2 / s;
			var fy = Math.max(Y[nFL], Y[nFR]);
			hT = clamp((ground - fy) / 10, 0, 1);
			tension = clamp((sqrt((X[nS] - X[nH]) * (X[nS] - X[nH]) + (Y[nS] - Y[nH]) * (Y[nS] - Y[nH])) / lArm - 0.9) / 0.1, 0, 1);
		} else if (mode === 2) { hT = 0.35; tension = 0; }
		else { hT = 0; tension = 0; }
		hang += (hT - hang) * kh;
		/* 小气泡与汗滴 */
		if (mode === 1) {
			meowT -= dt; dropT -= dt;
			var sw = abs(swing);
			if (meowT <= 0) {
				meowT = 2.2 + 1.6 * ((tSim * 7) % 1);
				toWorld(0, -52 * s);
				spawn(K_BUBBLE, WX, WY, 0, 0, 1.15, 1, (WX + (BUB_DX + BUB_HW) * s > zone.r) ? -1 : 1);   /* P_r 存左右侧：靠右墙就翻到头的左边 */
			}
			if (dropT <= 0 && sw > 0.18) {
				dropT = 0.55 + 0.4 * ((tSim * 13) % 1);
				spawn(K_DROP, (swing > 0 ? 26 : -26) * s, -48 * s, (swing > 0 ? 25 : -25), -90, 0.55, 1.6 * s, 0);   /* 汗滴用躯干局部坐标，跟着头走 */
			}
		}
		if (mode === 4 && dizzyT > 0) dizzyT -= dt;
	}

	/* 把当前链姿态写进玩偶派生副本的 DG（每帧一次） */
	var tw = [0, 0, 0, 0, 0, 0];
	function fillDG(t) {
		var j, w = 1, k = 0.5 * s, sk = 2 / s;
		/* 尾巴：猫坐标系节点 → 贝塞尔控制点（过 t=1/3, 2/3, 1 的反解） */
		var p0x = TAIL_P0[0], p0y = TAIL_P0[1], n1x, n1y, n2x, n2y, n3x, n3y, Ax, Ay, Bx, By;
		toLocal(X[nT1], Y[nT1]); n1x = LX * sk; n1y = LY * sk;
		toLocal(X[nT2], Y[nT2]); n2x = LX * sk; n2y = LY * sk;
		toLocal(X[nT3], Y[nT3]); n3x = LX * sk; n3y = LY * sk;
		Ax = 27 * n1x - 8 * p0x - n3x; Ay = 27 * n1y - 8 * p0y - n3y;
		Bx = 27 * n2x - p0x - 8 * n3x; By = 27 * n2y - p0y - 8 * n3y;
		DG.tp[0] = (2 * Ax - Bx) / 18; DG.tp[1] = (2 * Ay - By) / 18;
		DG.tp[2] = (2 * Bx - Ax) / 18; DG.tp[3] = (2 * By - Ay) / 18;
		DG.tp[4] = n3x; DG.tp[5] = n3y;
		/* 耳朵 */
		var ha = hA + 0.09, wl, wr, rel;
		for (j = 0; j < 2; j++) {
			var sd = j ? 1 : -1, ni = j ? nER : nEL, bxl = sd * earBaseX, byl = earBaseY, ca = cos(ha), sa = sin(ha);
			toLocal(X[ni], Y[ni]);
			var vx = LX - (bxl * ca - byl * sa), vy = LY - (bxl * sa + byl * ca);
			var ang = atan2(vx, -vy);                                 /* 从竖直向上起顺时针 */
			rel = ang - (sd * earRestAng + ha);
			rel = clamp(rel, -0.9, 0.9);
			if (j) earAngR = rel; else earAngL = -rel;
		}
		/* 表情与姿态 */
		var cheer = (mode === 4 || mode === 3);
		DG.w = 1;
		if (mode === 4 && settleT > settleLen - 0.4) DG.w = clamp(1 - (settleT - (settleLen - 0.4)) / 0.4, 0, 1);
		DG.bsx = 1 + 0.38 * sq - 0.03 * tension;
		DG.bsy = 1 - 0.55 * sq + 0.07 * tension * (mode === 1 ? 1 : 0);
		DG.by = 0;
		DG.hx = 0; DG.bx = 0;
		DG.hy = 3.6 * clamp(sq, -0.2, 0.46) / 0.4 - 1.2 * tension;
		DG.hrot = 0.09 - hA - (mode === 1 ? 0.06 : 0);
		DG.yaw = mode === 1 ? 0.12 : 0.3;
		DG.earL = 0.12 + earAngL + clamp(sq, 0, 0.5) * 0.7 - (mode === 2 ? 0.15 : 0);
		DG.earR = 0.12 + earAngR + clamp(sq, 0, 0.5) * 0.7 - (mode === 2 ? 0.15 : 0);
		DG.bellAng = bA; DG.bellDy = clamp(-sqv * 0.04, -2, 2);
		DG.hang = hang; DG.armW = armW;
		toLocalLegs();
		DG.gx = gxL; DG.gy = gyL; DG.gsp = 27;
		DG.twAmp = mode === 1 ? 1.2 : 2.5; DG.twPh = t * 5;
		DG.blush = 1;
		DG.op = 1;
		DG.lookx = 0; DG.looky = 0;
		if (mode === 1) {
			DG.eye = 0; DG.looky = -0.85; DG.lookx = clamp(-swing * 0.5, -0.5, 0.5);
			var pulseM = (sin(tSim * 2.3) > 0.55) ? 1 : 0;
			DG.mouth = pulseM ? 2 : 0; DG.mo = pulseM ? 0.28 : 0;
			if (tSim - pT < 0) DG.mouth = 0;
		} else if (mode === 2) {
			DG.eye = 0; DG.looky = 0.3; DG.mouth = 2; DG.mo = 0.55;
		} else {
			/* 落地：挤压最深时 >_<，之后按落差选“开心”或“头晕” */
			if (sq > 0.18 || (mode === 3 && settleT === 0 && bounces === 0)) { DG.eye = 2; DG.mouth = 0; DG.mo = 0; }
			else if (bigFall && dizzyT > 0) { DG.eye = 0; DG.lookx = cos(t * 9) * 0.8; DG.looky = sin(t * 9) * 0.55; DG.mouth = 2; DG.mo = 0.18; DG.op = 1; }
			else { DG.eye = 1; DG.mouth = 1; DG.mo = 0.6; }
		}
		return cheer;
	}
	function toLocalLegs() {
		/* 两脚相对躯干轴的偏转角 → 腿摆动（设计里“顺时针为正”） */
		var j, ni, a, base = atan2(X[nP] - X[nS], Y[nP] - Y[nS]);
		for (j = 0; j < 2; j++) {
			ni = j ? nFR : nFL;
			a = atan2(-(X[ni] - X[nP]), Y[ni] - Y[nP]);          /* 偏离“向下”，顺时针为正 */
			a -= th;                                              /* 去掉躯干本身的旋转 */
			if (a > PI) a -= 2 * PI; if (a < -PI) a += 2 * PI;
			a = clamp(a, -0.75, 0.75);
			if (j) DG.legR = a; else DG.legL = a;
		}
	}

	/* ---------- 绘制 ---------- */
	function drawDrop(ctx, x, y, r, a) {
		ctx.save(); ctx.globalAlpha = a; ctx.translate(x, y);
		ctx.beginPath(); ctx.moveTo(0, -r * 1.9); ctx.bezierCurveTo(r * 1.1, -r * 0.5, r * 1.1, r, 0, r); ctx.bezierCurveTo(-r * 1.1, r, -r * 1.1, -r * 0.5, 0, -r * 1.9); ctx.closePath();
		ctx.fillStyle = '#bfe6ff'; ctx.fill(); ctx.strokeStyle = '#5aa3d6'; ctx.lineWidth = 1; ctx.stroke();
		ctx.restore();
	}
	function drawStar(ctx, x, y, r, rot, a) {
		var j, an, rr;
		ctx.save(); ctx.globalAlpha = a; ctx.beginPath();
		for (j = 0; j < 10; j++) { an = rot + j * PI / 5 - PI / 2; rr = (j & 1) ? r * 0.5 : r; if (j) ctx.lineTo(x + cos(an) * rr, y + sin(an) * rr); else ctx.moveTo(x + cos(an) * rr, y + sin(an) * rr); }
		ctx.closePath(); ctx.fillStyle = '#f8a458'; ctx.fill(); ctx.strokeStyle = '#e07a35'; ctx.lineWidth = 1; ctx.stroke(); ctx.restore();
	}
	function drawBubble(ctx, x, y, a, sc, side) {
		ctx.save(); ctx.globalAlpha = a; ctx.translate(x, y); ctx.scale(sc * side, sc);      /* side=-1：整体镜像，气泡尾巴指向头的另一侧 */
		ctx.beginPath(); ctx.moveTo(-14, -9); ctx.quadraticCurveTo(-14, -15, -8, -15); ctx.lineTo(8, -15); ctx.quadraticCurveTo(14, -15, 14, -9);
		ctx.lineTo(14, 1); ctx.quadraticCurveTo(14, 6, 8, 6); ctx.lineTo(-6, 6); ctx.lineTo(-11, 12); ctx.lineTo(-10, 6); ctx.quadraticCurveTo(-14, 5, -14, 1); ctx.closePath();
		ctx.fillStyle = '#fffaf2'; ctx.fill(); ctx.strokeStyle = '#6a3b24'; ctx.lineWidth = 1.1; ctx.stroke();
		ctx.restore();
		ctx.save(); ctx.globalAlpha = a; ctx.translate(x, y); ctx.scale(sc, sc);               /* 文字不镜像 */
		ctx.fillStyle = '#6a3b24'; ctx.font = 'bold 11px "Microsoft YaHei", "PingFang SC", sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
		ctx.fillText('喵~', 0, -4.5);
		ctx.restore();
	}
	/* 烟尘贴图：只建一次的小离屏画布（径向渐变），之后每帧只 drawImage，不再分配渐变对象。无 document（node）时退回纯色圆。 */
	var sprites = null;
	function mkSprite(r, g, bl) {
		var c = document.createElement('canvas'), x;
		c.width = 32; c.height = 32; x = c.getContext('2d');
		var gr = x.createRadialGradient(16, 16, 0, 16, 16, 16);
		gr.addColorStop(0, 'rgba(' + r + ',' + g + ',' + bl + ',1)');
		gr.addColorStop(0.6, 'rgba(' + r + ',' + g + ',' + bl + ',0.85)');
		gr.addColorStop(1, 'rgba(' + r + ',' + g + ',' + bl + ',0)');
		x.fillStyle = gr; x.fillRect(0, 0, 32, 32);
		return c;
	}
	function getSprites() {
		if (sprites === null) {
			sprites = (typeof document !== 'undefined' && document.createElement) ? [mkSprite(214, 186, 146), mkSprite(246, 228, 196)] : false;   /* 0 浅色底（暖褐米），1 深色底（暖米白） */
		}
		return sprites;
	}

	/* 气泡每帧位置：以头为基准，出生时锁定的一侧，上浮，并把整个气泡夹进空闲区（靠边时不会越线） */
	function placeBubbles() {
		var j, lo, hi;
		for (j = 0; j < NPART; j++) {
			if (P_l[j] <= 0 || P_k[j] !== K_BUBBLE) continue;
			headPoint();
			P_x[j] = HX + P_r[j] * BUB_DX * s;
			P_y[j] = HY - 22 * s - P_vy[j];
			lo = zone.l + BUB_HW * s; hi = zone.r - BUB_HW * s; if (P_x[j] < lo) P_x[j] = lo; else if (P_x[j] > hi) P_x[j] = hi;
			lo = zone.t + BUB_HH * s; if (P_y[j] < lo) P_y[j] = lo;
		}
	}
	/* 小星星、烟尘不许飞出空闲区：碰到左右/上边界就反弹 */
	function keepParts() {
		var j;
		for (j = 0; j < NPART; j++) {
			if (P_l[j] <= 0 || (P_k[j] !== K_STAR && P_k[j] !== K_DUST)) continue;
			if (P_x[j] < zone.l + 6) { P_x[j] = zone.l + 6; P_vx[j] = abs(P_vx[j]) * 0.5; }
			else if (P_x[j] > zone.r - 6) { P_x[j] = zone.r - 6; P_vx[j] = -abs(P_vx[j]) * 0.5; }
			if (P_y[j] < zone.t + 6) { P_y[j] = zone.t + 6; P_vy[j] = abs(P_vy[j]) * 0.5; }
		}
	}

	/* 画影子 + 猫 + 粒子。返回 plush.draw 的返回值（画布坐标的 head/top 等） */
	var DO = { x: 0, y: 0, s: 1, t: 0, pose: 'drag', shadow: false, dark: false, stars: false, look: null };   /* 传给玩偶 draw 的参数对象，复用 */
	D.render = function (ctx, o) {
		var t = (o && o.t != null) ? o.t : tSim, dk = (o && o.dark != null) ? o.dark : dark, j, ret, hgt, sc, lf, spr, r, pop, sc2, an;
		if (mode === 0) { return null; }
		fillDG(t);
		/* 影子：随离地高度变小变淡（颜色固定，只改 globalAlpha，不拼字符串） */
		hgt = clamp((ground - f0y) / (90 * s), 0, 1);
		sc = 1 - 0.5 * hgt;
		ctx.save(); ctx.translate(clamp(f0x, zone.l + 23 * s * sc + 1, zone.r - 23 * s * sc - 1), ground); ctx.scale(s * sc, s * sc * 0.18);   /* 影子也不越出虚线 */
		ctx.beginPath(); ctx.arc(2, 0, 20, 0, PI * 2); ctx.globalAlpha = (dk ? 0.42 : 0.38) * (1 - 0.5 * hgt); ctx.fillStyle = dk ? '#000000' : '#566ea8'; ctx.fill();
		ctx.restore();
		ctx.save(); ctx.translate(f0x, f0y); ctx.rotate(th);
		DO.s = s; DO.t = t; DO.dark = dk;
		ret = PB.draw(ctx, DO);
		ctx.restore();
		rotDraw = th;
		/* 头晕星星绕头转 */
		if (mode === 4 && bigFall && dizzyT > 0) {
			toWorld(0, -62 * s);
			for (j = 0; j < 3; j++) {
				an = t * 5 + j * 2.094;
				drawStar(ctx, WX + cos(an) * 20 * s, WY - 8 * s + sin(an) * 5 * s, 4.5 * s, an, clamp(dizzyT / 0.4, 0, 1));
			}
		}
		/* 粒子 */
		spr = getSprites();
		for (j = 0; j < NPART; j++) {
			if (P_l[j] <= 0) continue;
			lf = P_l[j] / P_m[j];
			if (P_k[j] === K_DUST) {
				r = P_s[j] * (1.0 + (1 - lf) * 1.2);                 /* 边飘边散：半径由小变大，同时淡出 */
				ctx.globalAlpha = 0.9 * lf;
				if (spr) ctx.drawImage(spr[dk ? 1 : 0], P_x[j] - r, P_y[j] - r, r * 2, r * 2);
				else { ctx.beginPath(); ctx.arc(P_x[j], P_y[j], r * 0.6, 0, PI * 2); ctx.fillStyle = dk ? '#eedec2' : '#cfb48e'; ctx.fill(); }
				ctx.globalAlpha = 1;
			} else if (P_k[j] === K_STAR) drawStar(ctx, P_x[j], P_y[j], P_s[j], P_r[j], clamp(lf * 1.6, 0, 1));
			else if (P_k[j] === K_DROP) {
				toWorld(P_x[j], P_y[j]);
				drawDrop(ctx, clamp(WX, zone.l + 3, zone.r - 3), clamp(WY, zone.t + 4, zone.b - 3), P_s[j], clamp(lf * 2, 0, 1));
			} else if (P_k[j] === K_BUBBLE) {
				pop = clamp((1 - lf) / 0.15, 0, 1); sc2 = 0.6 + 0.4 * (1 - (1 - pop) * (1 - pop)) + 0.08 * sin(pop * PI);
				drawBubble(ctx, P_x[j], P_y[j], clamp(lf * 3, 0, 1), sc2 * s, P_r[j]);
			}
		}
		return ret;
	};

	/* 包围盒（保守估计，宿主坐标）：身体、头（含耳）、尾巴、耳尖、手套、气泡、汗滴的并集。写进复用对象并返回。
	 * 待机（mode 0）返回站姿包围盒。 */
	var BB = { l: 0, t: 0, r: 0, b: 0 };
	function bbAdd(x0, y0, x1, y1) { if (x0 < BB.l) BB.l = x0; if (y0 < BB.t) BB.t = y0; if (x1 > BB.r) BB.r = x1; if (y1 > BB.b) BB.b = y1; }
	D.bounds = function () {
		var j, n, hb;
		if (mode === 0) { hb = D.hitBox(); BB.l = hb.l; BB.t = hb.t; BB.r = hb.r; BB.b = hb.b; return BB; }
		BB.l = 1e9; BB.t = 1e9; BB.r = -1e9; BB.b = -1e9;
		headPoint();
		bbAdd(HX - HEAD_R * s, HY - 24 * s, HX + HEAD_R * s, HY + 20 * s);
		for (j = 0; j < 4; j++) { n = WN[j]; bbAdd(X[n] - WM[j], Y[n] - WM[j] * 0.5, X[n] + WM[j], Y[n] + (j > 1 ? 4 * s : WM[j] * 0.5)); }
		for (j = nT1; j <= nER; j++) { n = (j <= nT3 ? PAD_TAIL : PAD_EAR); bbAdd(X[j] - n, Y[j] - n, X[j] + n, Y[j] + n); }
		if (mode === 1) bbAdd(X[nH] - 12 * s, Y[nH] - 12 * s, X[nH] + 12 * s, Y[nH] + 12 * s);
		for (j = 0; j < NPART; j++) {
			if (P_l[j] <= 0) continue;
			if (P_k[j] === K_BUBBLE) bbAdd(P_x[j] - BUB_HW * s, P_y[j] - BUB_HH * s, P_x[j] + BUB_HW * s, P_y[j] + 13 * s);
			else if (P_k[j] === K_DROP) { toWorld(P_x[j], P_y[j]); bbAdd(clamp(WX, zone.l + 3, zone.r - 3) - 3, clamp(WY, zone.t + 4, zone.b - 3) - 5, clamp(WX, zone.l + 3, zone.r - 3) + 3, clamp(WY, zone.t + 4, zone.b - 3) + 3); }
		}
		return BB;
	};

	/* 快照（测试钩子用） */
	D.snap = function () {
		return {
			mode: mode, t: tSim,
			hand: { x: X[nH], y: Y[nH] }, pointer: { x: tgtX, y: tgtY }, raw: { x: rawX, y: rawY },
			feet: { x: f0x, y: f0y }, shoulder: { x: X[nS], y: Y[nS] }, hip: { x: X[nP], y: Y[nP] },
			theta: th, swing: swing, swingV: swingV, torsoDeg: th * 57.2958,
			sq: sq, hang: hang, armW: armW, headA: hA, bellA: bA, bounces: bounces, vin: lastVin, big: bigFall,
			root: { x: rootX, y: rootY, vx: rootVX, vy: rootVY },
			vel: { x: D.vx, y: D.vy }
		};
	};
	D.mode = function () { return mode; };
	D.modeName = function () { return ['idle', 'held', 'fall', 'bounce', 'settle'][mode]; };
	D.active = function () { return mode !== 0 || pressed; };
	D.isHeld = function () { return mode === 1; };
	D.consumeDone = function () { var d = doneFlag; doneFlag = false; return d; };
	D.reset = function (x) {
		mode = 0; pressed = false; acc = 0; clearParts(); hasParts = false; doneFlag = false; DG.w = 0;
		homeLift = 0; if (x != null) homeX = x; standAt(homeX);
	};
	D.params = function () {
		return { SUB: SUB, ITER: ITER, G: G, E_GROUND: E_GROUND, E_WALL: E_WALL, L_ARM: lArm, L_SP: lSP, L_PF: lPF, L_SF: lSF, DAMP_C: DAMP_C.slice(), VMAX_THROW: VMAX_THROW, SWING_MAX: SWING_MAX, TORSO_MAX: TORSO_MAX, TORSO_SOFT: TORSO_SOFT, ground: ground };
	};
	D.dispose = function () { D.reset(); };
	/* 测试用：直接把手放到某处并进入抓取态（不走指针事件） */
	D._grabAt = function (x, y, tms) { D.press(x, y, tms || 0); rawX = x + 10; D.move(x + 10, y, tms || 0); D.move(x, y, tms || 0); };

	standAt(homeX);
	return D;
}




/* ====================================================================
 * attach(el, host)：把 Pointer Events（鼠标 + 触摸 + 笔）接到控制器上
 *   el   —— 盖在猫身上的命中元素（透明 div，大小 = ctl.hitBox()）。touch-action:none 只设在它身上，
 *           空闲区其余部分保持 pointer-events:none，所以侧栏菜单的滚动和点击不受影响。
 *   host —— {
 *     ctl:      create() 返回的控制器
 *     toLocal:  function (e, out) —— 把事件的 clientX/clientY 换成宿主画布坐标写进 out.x/out.y（不要新建对象）
 *     onClick:  function (x, y) —— 位移 < 5px 且按下很短 → 原来的“点它冒爱心、喵~”
 *     onGrab:   function () —— 进入被抓态时（可选）
 *     onDrop:   function () —— 松手/取消，开始下落时（可选）
 *     clock:    function (e) —— 毫秒时间戳（可选，默认用 e.timeStamp / performance.now()；测试时可换成虚拟时钟）
 *   }
 * 返回 { detach(), listeners(), dragging(), syncBox(ox, oy) }。没有定时器；长按判定由控制器的 step 完成。
 * 安全落下：pointerup / pointercancel / lostpointercapture / 窗口 blur / 页面 hidden 都会走 cancel，不会卡在抓取态。
 * ==================================================================== */
function attach(el, host) {
	var ctl = host.ctl, tracking = -1, nListen = 0, held = false, P = { x: 0, y: 0 };
	var win = (typeof window !== 'undefined') ? window : null, doc = (typeof document !== 'undefined') ? document : null;
	function now(e) {
		if (host.clock) return host.clock(e);                         /* 测试用：虚拟时钟 */
		if (e && typeof e.timeStamp === 'number' && e.timeStamp > 0) return e.timeStamp;
		return (win && win.performance && win.performance.now) ? win.performance.now() : Date.now();
	}
	function setCursor(g) { try { el.style.cursor = g ? 'grabbing' : 'grab'; } catch (x) { } }
	function finish(cancel, e) {
		var id = tracking, r;
		if (id === -1) return;
		tracking = -1;
		try { if (el.releasePointerCapture && el.hasPointerCapture && el.hasPointerCapture(id)) el.releasePointerCapture(id); } catch (x0) { }
		r = ctl.release(now(e), cancel);
		setCursor(false);
		if (r === 'click') { if (host.onClick) host.onClick(P.x, P.y); }
		else if (r === 'drop') { held = false; if (host.onDrop) host.onDrop(); }
		held = false;
	}
	function onDown(e) {
		if (tracking !== -1) return;                                  /* 只跟踪一根手指/一个指针 */
		if (e.pointerType === 'mouse' && e.button !== 0) return;
		if (ctl.mode() !== 0) return;                                 /* 落下、落稳期间不接新的抓取 */
		host.toLocal(e, P);
		tracking = e.pointerId;
		try { el.setPointerCapture(e.pointerId); } catch (x) { }
		ctl.press(P.x, P.y, now(e), e.pointerId);
		if (e.cancelable) e.preventDefault();
	}
	function onMove(e) {
		if (e.pointerId !== tracking) return;
		host.toLocal(e, P);
		ctl.move(P.x, P.y, now(e));
		if (ctl.isHeld()) {
			if (!held) { held = true; setCursor(true); if (host.onGrab) host.onGrab(); }
			if (e.cancelable) e.preventDefault();                       /* 只在被抓取期间拦截（防止触摸时页面跟着滚） */
		}
	}
	function onUp(e) {
		if (e.pointerId !== tracking) return;
		host.toLocal(e, P);
		if (ctl.isHeld()) ctl.move(P.x, P.y, now(e));
		finish(false, e);
	}
	function onCancel(e) { if (e.pointerId === tracking) finish(true, e); }
	function onLost(e) { if (e.pointerId === tracking) finish(true, e); }
	function onCtx(e) { if (tracking !== -1) e.preventDefault(); }       /* 抓取时屏蔽右键菜单/触摸长按菜单 */
	function onBlur() { finish(true, null); }
	function onVis() { if (doc.hidden) finish(true, null); }
	function add(t, f, target) { target.addEventListener(t, f, false); nListen++; }
	function rem(t, f, target) { target.removeEventListener(t, f, false); nListen--; }
	add('pointerdown', onDown, el); add('pointermove', onMove, el); add('pointerup', onUp, el); add('pointercancel', onCancel, el);
	add('lostpointercapture', onLost, el); add('contextmenu', onCtx, el);
	if (win) add('blur', onBlur, win);
	if (doc) add('visibilitychange', onVis, doc);
	try { el.style.touchAction = 'none'; el.style.cursor = 'grab'; el.style.webkitUserSelect = 'none'; el.style.userSelect = 'none'; } catch (x1) { }
	return {
		listeners: function () { return nListen; },
		dragging: function () { return held; },
		/* 每帧调用：把命中元素对齐到 ctl.hitBox()（宿主坐标加偏移 ox,oy）；非待机/被抓时不接收事件 */
		syncBox: function (ox, oy) {
			var b = ctl.hitBox(), st = el.style, m = ctl.mode();
			st.left = (b.l + (ox || 0)) + 'px'; st.top = (b.t + (oy || 0)) + 'px';
			st.width = (b.r - b.l) + 'px'; st.height = (b.b - b.t) + 'px';
			st.pointerEvents = (m === 0 || m === 1) ? 'auto' : 'none';
		},
		/* 宿主强制松手(区域被隐藏、状态切换等)：走与 pointercancel 相同的安全路径 */
		cancel: function () { finish(true, null); },
		detach: function () {
			finish(true, null);
			rem('pointerdown', onDown, el); rem('pointermove', onMove, el); rem('pointerup', onUp, el); rem('pointercancel', onCancel, el);
			rem('lostpointercapture', onLost, el); rem('contextmenu', onCtx, el);
			if (win) rem('blur', onBlur, win);
			if (doc) rem('visibilitychange', onVis, doc);
		}
	};
}

	root.MwrtDrag = { create: create, attach: attach, version: 'i1' };
})(typeof window !== 'undefined' ? window : this);

(function (root) {
	'use strict';

	var PI = Math.PI, TAU = PI * 2, sin = Math.sin, cos = Math.cos, abs = Math.abs,
		min = Math.min, max = Math.max, floor = Math.floor, sqrt = Math.sqrt, pow = Math.pow, atan2 = Math.atan2;

	function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
	function lerp(a, b, t) { return a + (b - a) * t; }
	function sstep(a, b, x) { var t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
	function hash(n) { var s = sin(n * 127.1 + 311.7) * 43758.5453; return s - floor(s); }
	/* 周期脉冲：每 period 秒一次，宽度 width 秒，0..1 的圆润凸起 */
	function pulse(t, period, offset, width) {
		var p = (((t + offset) % period) + period) % period;
		if (p >= width) return 0;
		return sin(p / width * PI);
	}
	function easeOutBack(x) { var c1 = 1.70158, c3 = c1 + 1; return 1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2); }

	/* ---------- 颜色 ---------- */
	var _rgbCache = {};
	function hex2rgb(h) {
		var c = _rgbCache[h];
		if (c) return c;
		if (h.charAt(0) === 'r') { var m = h.match(/\d+/g); c = [+m[0], +m[1], +m[2]]; _rgbCache[h] = c; return c; }
		var s = h.charAt(0) === '#' ? h.slice(1) : h;
		if (s.length === 3) s = s.charAt(0) + s.charAt(0) + s.charAt(1) + s.charAt(1) + s.charAt(2) + s.charAt(2);
		c = [parseInt(s.slice(0, 2), 16), parseInt(s.slice(2, 4), 16), parseInt(s.slice(4, 6), 16)];
		_rgbCache[h] = c;
		return c;
	}
	function mix(a, b, t) {
		if (t <= 0) return a;
		if (t >= 1) return b;
		var x = hex2rgb(a), y = hex2rgb(b);
		return 'rgb(' + Math.round(x[0] + (y[0] - x[0]) * t) + ',' + Math.round(x[1] + (y[1] - x[1]) * t) + ',' + Math.round(x[2] + (y[2] - x[2]) * t) + ')';
	}
	function rgba(h, a) { var x = hex2rgb(h); return 'rgba(' + x[0] + ',' + x[1] + ',' + x[2] + ',' + a + ')'; }

	/* ---------- 调色板（深/浅侧栏各一套） ---------- */
	function makePal(dark) {
		return {
			dark: dark,
			ink: '#4a2a1f', ink2: '#6e4331',
			fur: '#f59a2f', furHi: '#ffbe62', furSh: '#d9741a', furStripe: '#b8581a',
			cream: '#fff5e4', creamSh: '#ecd5b0',
			pink: '#ff8fa6', pinkIn: '#ffc4cf', pinkDeep: '#ee6a89', nose: '#ff6f8f',
			blush: 'rgba(255,96,124,0.5)',
			iris1: '#d6f26a', iris2: '#2c9a62', pupil: '#2a1710',
			tear: '#c8ecff',
			/* 笼子：浅色侧栏上用更深更饱和的蓝灰（不发灰消失），深色侧栏上提亮并加轮廓光 */
			steelHi: dark ? '#d3e3f8' : '#bcd0ea',
			steel: dark ? '#7f98bb' : '#5f7898',
			steelLo: dark ? '#4a5f7e' : '#364a66',
			steelOut: dark ? '#141c29' : '#1f2c40',
			rim: dark ? 'rgba(170,215,255,0.55)' : 'rgba(255,255,255,0.0)',
			panel: dark ? 'rgba(4,10,20,0.40)' : 'rgba(30,50,84,0.20)',
			floor: dark ? '#3b4d68' : '#8aa0bf',
			gold: '#ffd75a', goldMid: '#f2ab1d', goldLo: '#b36c0a', goldHi: '#fff3b0',
			rivet: dark ? '#e8f1ff' : '#dbe8fa',
			scorch: 'rgba(20,14,12,0.55)',
			/* 布偶 */
			pFab: '#f1c48a', pFabSh: '#d9a468', pPatch: '#e2975a', pCream: '#fff1d8',
			pThread: '#6b4331', pBtn: '#3a2418', pBtnHi: '#8a6a58', pScarf: '#e8505b', pScarfSh: '#b8323f',
			pCheek: '#ffa5ae', pEar: '#ffb7b8'
		};
	}
	var PAL_L = makePal(false), PAL_D = makePal(true);
	var API = { isDark: false };
	function palette(isDark) { return isDark ? PAL_D : PAL_L; }
	function palOf(o) { return (o && o.dark != null) ? palette(!!o.dark) : palette(API.isDark); }

	/* ---------- 路径小工具 ---------- */
	function ell(c, x, y, rx, ry) {
		c.moveTo(x + rx, y);
		c.save(); c.translate(x, y); c.scale(rx, ry); c.arc(0, 0, 1, 0, TAU); c.restore();
	}
	function heartPath(c, x, y, s) {
		c.moveTo(x, y + s * 0.92);
		c.bezierCurveTo(x - s * 1.35, y + s * 0.05, x - s * 0.95, y - s * 0.95, x, y - s * 0.38);
		c.bezierCurveTo(x + s * 0.95, y - s * 0.95, x + s * 1.35, y + s * 0.05, x, y + s * 0.92);
		c.closePath();
	}
	function starPath(c, x, y, r, rot, inner) {
		var i, a, rr, k = inner || 0.48;
		for (i = 0; i < 10; i++) {
			a = rot + i * PI / 5 - PI / 2;
			rr = (i & 1) ? r * k : r;
			if (i === 0) c.moveTo(x + cos(a) * rr, y + sin(a) * rr);
			else c.lineTo(x + cos(a) * rr, y + sin(a) * rr);
		}
		c.closePath();
	}
	function glintPath(c, x, y, r) {
		c.moveTo(x, y - r);
		c.quadraticCurveTo(x, y, x + r, y);
		c.quadraticCurveTo(x, y, x, y + r);
		c.quadraticCurveTo(x, y, x - r, y);
		c.quadraticCurveTo(x, y, x, y - r);
		c.closePath();
	}
	function rrect(c, x, y, w, h, r) {
		r = min(r, w / 2, h / 2);
		c.moveTo(x + r, y);
		c.lineTo(x + w - r, y); c.quadraticCurveTo(x + w, y, x + w, y + r);
		c.lineTo(x + w, y + h - r); c.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
		c.lineTo(x + r, y + h); c.quadraticCurveTo(x, y + h, x, y + h - r);
		c.lineTo(x, y + r); c.quadraticCurveTo(x, y, x + r, y);
		c.closePath();
	}
	/* 先描边（粗）再填色 */
	function fs(c, fill, ink, lw) {
		if (fill) { c.fillStyle = fill; c.fill(); }
		if (ink) { c.strokeStyle = ink; c.lineWidth = lw || 2.2; c.lineJoin = 'round'; c.lineCap = 'round'; c.stroke(); }
	}
	/* 阴影月牙：在当前 clip 内，以(lx,ly)为光源偏移的椭圆之外涂暗色（2 阶 cel 光影） */
	function shadeCrescent(c, cx, cy, rx, ry, col) {
		c.beginPath();
		c.rect(cx - rx * 2.5, cy - ry * 2.5, rx * 5, ry * 5);
		ell(c, cx, cy, rx, ry);
		c.fillStyle = col;
		try { c.fill('evenodd'); } catch (e) { c.fill(); }
	}
	/* 渐尖条纹（虎斑）：底宽 w 在 (x0,y0)，尖在 (x1,y1)，略带弧度 */
	function stripe(c, x0, y0, x1, y1, w, col) {
		var dx = x1 - x0, dy = y1 - y0, l = sqrt(dx * dx + dy * dy) || 1, nx = -dy / l, ny = dx / l;
		c.beginPath();
		c.moveTo(x0 + nx * w, y0 + ny * w);
		c.quadraticCurveTo((x0 + x1) / 2 + nx * w * 0.6 + ny * 0.8, (y0 + y1) / 2 + ny * w * 0.6 - nx * 0.8, x1, y1);
		c.quadraticCurveTo((x0 + x1) / 2 - nx * w * 0.3, (y0 + y1) / 2 - ny * w * 0.3, x0 - nx * w, y0 - ny * w);
		c.closePath();
		c.fillStyle = col; c.fill();
	}

	/* =====================================================================
	 *  猫
	 * ===================================================================== */
	var CAT_POSES = ['idle', 'walk', 'windup', 'charge', 'bonk', 'dizzy', 'plead', 'clap', 'scared', 'jump', 'cute', 'wave', 'sit', 'yawn'];

	function newParams() {
		return {
			bx: 0, by: 0, bsx: 1, bsy: 1, brot: 0,
			hx: 0, hy: 0, hrot: 0, hsx: 1, hsy: 1,
			eye: 'open', brow: 0, mouth: 'smile', mo: 0, lookx: 0, looky: 0,
			earL: 0, earR: 0,
			aLx: -23, aLy: -19, aRx: 23, aRy: -19, armsFront: false, palm: 0,
			fLx: -11, fLy: 0, fRx: 11, fRy: 0,
			tbx: 11, tby: -9, tailAng: -0.25, tailAmp: 0.16, tailFreq: 2.2, tailCurl: -1.0, tailHook: 0.6, tailPuff: 1, tailFront: false,
			blush: 0.55, tear: 0, sweat: 0, stars: 0, haunch: 0, whisk: 0, lines: 0, heartEye: 0, gritt: 0
		};
	}

	function catPose(name, t, o) {
		var p = newParams(), k, ph, s1, br, c, u, open;
		switch (name) {
		case 'walk':
			ph = t * 9; s1 = sin(ph);
			p.by = -abs(cos(ph)) * 2.6; p.brot = s1 * 0.055; p.hrot = -s1 * 0.05; p.hy = -abs(cos(ph)) * 0.8;
			p.fLx = -11 + s1 * 2.4; p.fRx = 11 - s1 * 2.4;
			p.fLy = -max(0, s1) * 4.6; p.fRy = -max(0, -s1) * 4.6;
			p.aLx = -22 - s1 * 1.5; p.aLy = -19 + s1 * 3.2; p.aRx = 22 + s1 * 1.5; p.aRy = -19 - s1 * 3.2;
			p.tailAmp = 0.22; p.tailFreq = 4.6; p.tailAng = 0.0;
			p.earL = sin(ph * 2) * 0.05; p.earR = -sin(ph * 2 + 1) * 0.05;
			break;
		case 'windup':
			p.bsy = 0.84; p.bsx = 1.13; p.brot = -0.15; p.bx = sin(t * 58) * 0.8;
			p.hy = 3.5; p.hrot = 0.1; p.hx = 1;
			p.brow = 0.9; p.mouth = 'flat'; p.gritt = 1; p.blush = 0.25;
			p.earL = -0.55; p.earR = 0.5;
			p.fLx = -15; p.fRx = 15; p.fLy = 0; p.fRy = 0;
			p.aLx = -27; p.aLy = -14; p.aRx = 25; p.aRy = -12;
			p.tailAng = -1.0; p.tailAmp = 0.06; p.tailFreq = 9; p.tailCurl = -0.5; p.tailHook = 0.2; p.tailPuff = 1.12;
			break;
		case 'charge':
			ph = t * 17; s1 = sin(ph);
			p.brot = 0.22; p.bx = -4; p.bsx = 1.06; p.bsy = 0.95; p.by = -abs(cos(ph)) * 2; p.hx = 2; p.hy = 1.5; p.hrot = 0.12;
			p.fLx = -11 + s1 * 6; p.fRx = 11 - s1 * 6; p.fLy = -max(0, s1) * 7; p.fRy = -max(0, -s1) * 7;
			p.aLx = -27 + s1 * 2; p.aLy = -26 - s1 * 3; p.aRx = 17; p.aRy = -24 + s1 * 3;
			p.brow = 1; p.mouth = 'open'; p.mo = 1; p.blush = 0.2;
			p.earL = -0.7; p.earR = 0.55;
			p.tbx = 2; p.tby = -12; p.tailAng = PI + 0.2; p.tailAmp = 0.1; p.tailFreq = 14; p.tailCurl = 0.25; p.tailHook = 0; p.tailPuff = 0.95;
			p.lines = 1; p.lookx = 1; p.looky = 0.1;
			break;
		case 'bonk':
			p.bsx = 1.32; p.bsy = 0.72; p.brot = 0.06; p.bx = -3; p.hx = 3; p.hy = 5; p.hsx = 1.1; p.hsy = 0.9; p.hrot = 0.12;
			p.eye = 'squint'; p.mouth = 'wavy'; p.blush = 0.35;
			p.earL = -0.7; p.earR = 0.7;
			p.aLx = -30; p.aLy = -30; p.aRx = 31; p.aRy = -22; p.fLx = -15; p.fRx = 14;
			p.tailAng = -1.35; p.tailAmp = 0.02; p.tailCurl = -0.3; p.tailHook = 0.1; p.tailPuff = 1.25;
			break;
		case 'dizzy':
			k = sin(t * 3.4);
			p.brot = k * 0.1; p.bx = k * 2.2; p.hrot = sin(t * 3.4 - 0.9) * 0.14; p.hx = sin(t * 3.4 - 0.5) * 1.5;
			p.eye = 'dizzy'; p.mouth = 'tongue'; p.stars = 1; p.blush = 0.7;
			p.earL = -0.55; p.earR = 0.55;
			p.aLx = -23; p.aLy = -13 + k; p.aRx = 23; p.aRy = -13 - k;
			p.fLx = -13; p.fRx = 13;
			p.tailAng = 0.3; p.tailAmp = 0.07; p.tailCurl = 0.25; p.tailHook = 0; p.tailFreq = 2.4;
			break;
		case 'plead':
			p.bx = sin(t * 31) * 0.35; p.hrot = 0.1 + sin(t * 2.1) * 0.05; p.hy = -1.2;
			p.eye = 'plead'; p.brow = -1; p.mouth = 'sad'; p.mo = 0.5 + 0.5 * sin(t * 17); p.tear = 1; p.blush = 0.8;
			p.earL = -0.32; p.earR = 0.32;
			p.aLx = -27; p.aLy = -45 + sin(t * 4) * 0.8; p.aRx = 27; p.aRy = -45 - sin(t * 4) * 0.8; p.armsFront = true;
			p.fLx = -10; p.fRx = 10;
			p.tailAng = 0.7; p.tailAmp = 0.1; p.tailFreq = 1.6; p.tailCurl = -0.5; p.tailHook = 0.6;
			p.looky = -0.3;
			break;
		case 'clap':
			ph = t * 14; c = 0.5 + 0.5 * cos(ph);
			p.by = -abs(sin(t * 7)) * 3.2; p.bsy = 1 + 0.03 * sin(t * 14);
			p.eye = 'happy'; p.mouth = 'open'; p.mo = 0.8; p.blush = 0.75;
			p.aLx = -(3 + 11 * c); p.aLy = -31 - 2 * (1 - c); p.aRx = 3 + 11 * c; p.aRy = p.aLy; p.armsFront = true; p.palm = 1 - c;
			p.earL = 0.05; p.earR = -0.05; p.hrot = sin(t * 7) * 0.05;
			p.tailAmp = 0.34; p.tailFreq = 7; p.tailAng = -0.2; p.fLy = -abs(sin(t * 7)) * 1.5; p.fRy = p.fLy;
			break;
		case 'scared':
			p.bx = sin(t * 49) * 0.95; p.bsy = 1.07; p.bsx = 0.93; p.hy = -2;
			p.eye = 'wide'; p.mouth = 'o'; p.mo = 1; p.sweat = 1; p.blush = 0.08; p.brow = -0.5;
			p.earL = -0.78; p.earR = 0.78;
			p.aLx = -21; p.aLy = -51; p.aRx = 21; p.aRy = -51; p.armsFront = true; p.palm = 0.4;
			p.fLy = -1.5; p.fRy = -1.5;
			p.tailAng = -1.25; p.tailAmp = 0.05; p.tailFreq = 9; p.tailCurl = -0.3; p.tailHook = 0.2; p.tailPuff = 1.55;
			break;
		case 'jump':
			p.bsy = 1.1; p.bsx = 0.92; p.hy = -1.5;
			p.fLx = -8; p.fRx = 8; p.fLy = -6; p.fRy = -6;
			p.aLx = -43; p.aLy = -66; p.aRx = 43; p.aRy = -66; p.armsFront = true;
			p.earL = -0.55 + sin(t * 11) * 0.12; p.earR = 0.55 - sin(t * 11 + 1) * 0.12;
			p.mouth = 'open'; p.mo = 1; p.blush = 0.6; p.looky = -0.4;
			p.tailAng = -1.0; p.tailAmp = 0.28; p.tailFreq = 8; p.tailCurl = -0.9; p.tailHook = 0.6;
			break;
		case 'cute':
			k = sin(t * 2.6);
			p.bx = k * 2.4; p.brot = k * 0.05; p.hrot = 0.2 + k * 0.06; p.hy = 1; p.by = -abs(sin(t * 3.1)) * 1.4;
			u = (t * 0.55) % 1;
			p.eye = (u < 0.62) ? 'heart' : 'happy'; p.heartEye = 1;
			p.blush = 1; p.mouth = 'cat'; p.mo = 0.5;
			p.aLx = -8; p.aLy = -34 + k; p.aRx = 8; p.aRy = -34 + k; p.armsFront = true;
			p.earL = 0.08; p.earR = -0.08;
			p.fLx = -9; p.fRx = 9;
			p.tailAng = -0.45; p.tailAmp = 0.3; p.tailFreq = 5; p.tailCurl = -1.5; p.tailHook = 0.9;
			break;
		case 'wave':
			ph = t * 9;
			p.hrot = 0.07 + sin(t * 2) * 0.03; p.brot = sin(t * 2) * 0.02;
			p.aRx = 43 + sin(ph) * 5; p.aRy = -66 + cos(ph) * 2.5; p.armsFront = true; p.palm = 0.5;
			p.aLx = -22; p.aLy = -19;
			p.eye = 'open'; p.mouth = 'open'; p.mo = 0.45; p.blush = 0.7; p.looky = -0.1;
			p.tailAmp = 0.3; p.tailFreq = 4.2; p.tailAng = -0.1;
			break;
		case 'sit':
			br = sin(t * 1.8);
			p.bsy = 0.88 + br * 0.012; p.bsx = 1.14; p.hy = 5 - br * 0.6; p.haunch = 1;
			p.fLx = -9; p.fRx = 9; p.fLy = 0.5; p.fRy = 0.5;
			p.aLx = -11; p.aLy = -13; p.aRx = 11; p.aRy = -13; p.armsFront = true;
			p.tbx = 17; p.tby = -3; p.tailAng = 0.12; p.tailCurl = 2.7; p.tailHook = -0.3; p.tailAmp = 0.06; p.tailFreq = 1.6; p.tailFront = true;
			p.earL = -0.3 * pulse(t, 4.4, 0.5, 0.25);
			p.hrot = sin(t * 0.8) * 0.03; p.blush = 0.6;
			break;
		case 'yawn':
			c = (t % 3.8) / 3.8;
			open = sstep(0.08, 0.34, c) * (1 - sstep(0.7, 0.92, c));
			p.mouth = 'yawn'; p.mo = open;
			p.eye = open > 0.35 ? 'closed' : 'open';
			p.hy = -2.5 * open; p.hrot = -0.07 * open; p.bsy = 1 + 0.05 * open; p.bsx = 1 - 0.02 * open;
			p.earL = -0.4 * open; p.earR = 0.4 * open;
			p.aRx = lerp(23, 13, open); p.aRy = lerp(-19, -47, open); p.armsFront = open > 0.05;
			p.aLx = lerp(-23, -27, open); p.aLy = lerp(-19, -15, open);
			p.tailAng = -0.25 - 0.5 * open; p.tailAmp = 0.1;
			p.blush = 0.5;
			break;
		default: /* idle */
			br = sin(t * 2.3);
			p.bsy = 1 + 0.018 * br; p.bsx = 1 - 0.01 * br; p.hy = -0.9 * br; p.hrot = sin(t * 0.9) * 0.025;
			p.earL = -0.4 * pulse(t, 3.9, 0.7, 0.24); p.earR = 0.12 * pulse(t, 5.3, 1.2, 0.2);
			p.aLy = -19 + br * 0.5; p.aRy = -19 + br * 0.5;
			p.lookx = sin(t * 0.7) * 0.45; p.looky = sin(t * 0.53) * 0.2;
			p.tailAmp = 0.2; p.tailFreq = 2.2;
			break;
		}
		return p;
	}

	/* ---------- 猫的各部件 ---------- */
	function headPath(c) {
		c.moveTo(0, -26);
		c.bezierCurveTo(19, -26.5, 33, -16.5, 34, -1);
		c.bezierCurveTo(34.5, 4, 39.5, 7, 39, 12);
		c.bezierCurveTo(38.5, 16, 34.5, 16.5, 30.5, 20.5);
		c.bezierCurveTo(23.5, 26.5, 11, 28, 0, 28);
		c.bezierCurveTo(-11, 28, -23.5, 26.5, -30.5, 20.5);
		c.bezierCurveTo(-34.5, 16.5, -38.5, 16, -39, 12);
		c.bezierCurveTo(-39.5, 7, -34.5, 4, -34, -1);
		c.bezierCurveTo(-33, -16.5, -19, -26.5, 0, -26);
		c.closePath();
	}
	function earPath(c, sd) {
		c.moveTo(sd * 31, -8);
		c.bezierCurveTo(sd * 34, -22, sd * 32.5, -36, sd * 27, -45.5);
		c.bezierCurveTo(sd * 24, -48, sd * 20, -46, sd * 16, -42);
		c.bezierCurveTo(sd * 11, -37, sd * 7, -31, sd * 5, -26);
		c.closePath();
	}
	function earInPath(c, sd) {
		c.moveTo(sd * 27.5, -15);
		c.bezierCurveTo(sd * 29.5, -24, sd * 28.5, -33, sd * 25.5, -39.5);
		c.bezierCurveTo(sd * 21.5, -37, sd * 16.5, -32, sd * 12.5, -27);
		c.closePath();
	}
	function bodyPath(c) {
		c.moveTo(-14, -43);
		c.bezierCurveTo(-22, -40, -24, -24, -22, -13);
		c.bezierCurveTo(-20.5, -3.5, -12, -0.5, 0, -0.5);
		c.bezierCurveTo(12, -0.5, 20.5, -3.5, 22, -13);
		c.bezierCurveTo(24, -24, 22, -40, 14, -43);
		c.closePath();
	}

	/* 尾巴：一串变半径的圆沿“行波”脊线排布，先铺描边圆再铺毛色圆，条纹与白尖最后叠 */
	function drawTail(c, p, t, pal, thick) {
		var N = 18, seg = 2.85, i, a = p.tailAng, x = p.tbx, y = p.tby, pts = [], f, r, curv, ph;
		for (i = 0; i <= N; i++) {
			f = i / N;
			ph = sin(t * p.tailFreq - i * 0.34);
			curv = p.tailCurl / N + ph * p.tailAmp * (0.25 + f * 0.9) / 3.2 * (i > 0 ? 1 : 0);
			if (p.tailHook && f > 0.72) curv += p.tailHook * 0.11 * ((f - 0.72) / 0.28);
			a += curv;
			x += cos(a) * seg; y += sin(a) * seg;
			r = lerp(4.6, 3.1, f) * (thick || 1) * p.tailPuff;
			pts.push([x, y, r, a]);
		}
		c.fillStyle = pal.ink;
		for (i = 0; i < pts.length; i++) { c.beginPath(); c.arc(pts[i][0], pts[i][1], pts[i][2] + 1.7, 0, TAU); c.fill(); }
		for (i = 0; i < pts.length; i++) {
			c.beginPath(); c.arc(pts[i][0], pts[i][1], pts[i][2], 0, TAU);
			c.fillStyle = (i >= N - 3) ? pal.cream : pal.fur; c.fill();
		}
		/* 下沿阴影 + 条纹 */
		c.strokeStyle = pal.furStripe; c.lineWidth = 2.1; c.lineCap = 'round';
		for (i = 3; i < N - 3; i += 4) {
			var q = pts[i], nx = -sin(q[3]), ny = cos(q[3]), rr = q[2] * 0.95;
			c.beginPath(); c.moveTo(q[0] - nx * rr, q[1] - ny * rr); c.lineTo(q[0] + nx * rr, q[1] + ny * rr); c.stroke();
		}
		/* 高光条 */
		c.strokeStyle = 'rgba(255,236,190,0.55)'; c.lineWidth = 1.5;
		c.beginPath();
		for (i = 1; i < N - 3; i++) {
			var w = pts[i];
			var hx = w[0] - sin(w[3]) * w[2] * -0.45, hy = w[1] + cos(w[3]) * w[2] * -0.45;
			if (i === 1) c.moveTo(hx, hy); else c.lineTo(hx, hy);
		}
		c.stroke();
		return pts;
	}

	function drawFoot(c, x, y, pal, front) {
		c.beginPath(); ell(c, x, y - 4.2, 8.6, 5.6);
		var g = c.createLinearGradient(0, y - 10, 0, y + 2);
		g.addColorStop(0, '#ffffff'); g.addColorStop(1, pal.creamSh);
		fs(c, g, pal.ink, 2.2);
		c.beginPath();
		c.moveTo(x - 2.6, y - 2.4); c.lineTo(x - 2.6, y - 5.4);
		c.moveTo(x + 2.6, y - 2.4); c.lineTo(x + 2.6, y - 5.4);
		c.strokeStyle = pal.ink2; c.lineWidth = 1.2; c.lineCap = 'round'; c.stroke();
	}

	function drawArm(c, sx, sy, hx, hy, bendDir, pal, palm) {
		var mx = (sx + hx) / 2, my = (sy + hy) / 2, dx = hx - sx, dy = hy - sy, l = sqrt(dx * dx + dy * dy) || 1;
		var nx = -dy / l * bendDir, ny = dx / l * bendDir, bend = min(7, l * 0.22);
		c.lineCap = 'round'; c.lineJoin = 'round';
		c.beginPath(); c.moveTo(sx, sy); c.quadraticCurveTo(mx + nx * bend, my + ny * bend, hx, hy);
		c.strokeStyle = pal.ink; c.lineWidth = 11.4; c.stroke();
		c.strokeStyle = pal.fur; c.lineWidth = 7.4; c.stroke();
		c.strokeStyle = pal.furSh; c.lineWidth = 2.2;
		c.beginPath(); c.moveTo(sx + 2, sy + 2); c.quadraticCurveTo(mx + nx * bend + 2.4, my + ny * bend + 2, hx + 1, hy + 1); c.stroke();
		/* 条纹 */
		var q, qx, qy;
		c.strokeStyle = pal.furStripe; c.lineWidth = 1.8;
		for (q = 0.3; q < 0.8; q += 0.28) {
			qx = (1 - q) * (1 - q) * sx + 2 * q * (1 - q) * (mx + nx * bend) + q * q * hx;
			qy = (1 - q) * (1 - q) * sy + 2 * q * (1 - q) * (my + ny * bend) + q * q * hy;
			c.beginPath(); c.moveTo(qx - ny * 3.2, qy + nx * 3.2); c.lineTo(qx + ny * 3.2, qy - nx * 3.2); c.stroke();
		}
		/* 白手套 */
		c.beginPath(); c.arc(hx, hy, 6.5, 0, TAU);
		var g = c.createRadialGradient(hx - 2, hy - 2.5, 1, hx, hy, 7);
		g.addColorStop(0, '#ffffff'); g.addColorStop(1, pal.creamSh);
		fs(c, g, pal.ink, 2.2);
		if (palm > 0.4) {
			c.beginPath(); ell(c, hx, hy + 0.6, 3.1, 2.6); c.fillStyle = pal.pink; c.fill();
			c.beginPath(); ell(c, hx - 3, hy - 3.4, 1.1, 1.4); ell(c, hx, hy - 4.2, 1.1, 1.4); ell(c, hx + 3, hy - 3.4, 1.1, 1.4); c.fill();
		} else {
			c.beginPath(); c.moveTo(hx - 2.4, hy + 1.2); c.lineTo(hx - 2.4, hy + 5); c.moveTo(hx + 2.2, hy + 1.2); c.lineTo(hx + 2.2, hy + 5);
			c.strokeStyle = pal.ink2; c.lineWidth = 1.15; c.stroke();
		}
	}

	function drawEar(c, sd, rot, pal, inner) {
		c.save();
		c.translate(sd * 19, -20); c.rotate(rot); c.translate(-sd * 19, 20);
		c.beginPath(); earPath(c, sd);
		var g = c.createLinearGradient(0, -48, 0, -8);
		g.addColorStop(0, pal.furSh); g.addColorStop(0.35, pal.fur); g.addColorStop(1, pal.fur);
		fs(c, g, pal.ink, 2.6);
		c.beginPath(); earInPath(c, sd);
		var g2 = c.createLinearGradient(sd * 26, -40, sd * 14, -18);
		g2.addColorStop(0, pal.pinkIn); g2.addColorStop(1, pal.pink);
		c.fillStyle = g2; c.fill();
		c.strokeStyle = 'rgba(160,60,80,0.55)'; c.lineWidth = 1.1; c.lineCap = 'round';
		c.beginPath(); c.moveTo(sd * 25, -35); c.lineTo(sd * 26.5, -30);
		c.moveTo(sd * 21, -31); c.lineTo(sd * 24.5, -27);
		c.stroke();
		/* 耳尖毛 */
		c.strokeStyle = pal.cream; c.lineWidth = 1.2;
		c.beginPath(); c.moveTo(sd * 21, -26); c.quadraticCurveTo(sd * 24, -30, sd * 23, -36);
		c.moveTo(sd * 17, -28); c.quadraticCurveTo(sd * 20, -32, sd * 20, -37); c.stroke();
		c.restore();
	}

	/* 眼睛。sd=-1 左眼，+1 右眼；中心(sd*14, 3) */
	function drawEye(c, sd, p, pal, blink, t, look) {
		var ex = sd * 14, ey = 3, rx = 8.7, ry = 10.6, mode = p.eye, i;
		var lx = look.x * 2.4, ly = look.y * 2.2;
		var lid = p.brow; /* >0 愤怒(内角下压) <0 担忧(内角上提) */
		c.save();
		c.translate(ex, ey);
		if (mode === 'happy') {
			c.beginPath(); c.moveTo(-7.4, 3.2); c.quadraticCurveTo(0, -8.4, 7.4, 3.2);
			c.strokeStyle = pal.ink; c.lineWidth = 3.2; c.lineCap = 'round'; c.stroke();
			c.restore(); return;
		}
		if (mode === 'closed' || blink > 0.86) {
			c.beginPath(); c.moveTo(-7.4, 0.4); c.quadraticCurveTo(0, 6.6, 7.4, 0.4);
			c.strokeStyle = pal.ink; c.lineWidth = 3; c.lineCap = 'round'; c.stroke();
			/* 睫毛 */
			c.beginPath(); c.moveTo(sd * 7.2, 0.6); c.lineTo(sd * 10.4, -1.6); c.lineWidth = 2; c.stroke();
			c.restore(); return;
		}
		if (mode === 'squint') {
			c.beginPath();
			c.moveTo(-sd * 6.5, -6); c.lineTo(sd * 6.5, 0.4); c.lineTo(-sd * 6.5, 6.6);
			c.strokeStyle = pal.ink; c.lineWidth = 3.2; c.lineJoin = 'round'; c.lineCap = 'round'; c.stroke();
			c.restore(); return;
		}
		if (mode === 'heart') {
			var hs = 8.4 + 1.4 * sin(t * 9 + sd);
			c.beginPath(); heartPath(c, 0, 0.4, hs);
			var gh = c.createLinearGradient(0, -hs, 0, hs); gh.addColorStop(0, '#ff8fb0'); gh.addColorStop(1, '#e83c6d');
			fs(c, gh, pal.ink, 2);
			c.beginPath(); ell(c, -hs * 0.42, -hs * 0.34, 1.9, 1.3); c.fillStyle = 'rgba(255,255,255,0.9)'; c.fill();
			c.restore(); return;
		}
		var sc = (mode === 'wide') ? 1.12 : 1;
		var eh = ry * sc * (1 - blink * 0.92);
		/* 眼白 */
		c.save();
		c.beginPath(); ell(c, 0, 0, rx * sc, eh);
		c.fillStyle = '#ffffff'; c.fill();
		c.clip();
		if (mode === 'dizzy') {
			c.strokeStyle = pal.ink; c.lineWidth = 1.7; c.lineCap = 'round';
			c.beginPath();
			var sp = t * 7 * sd;
			for (i = 0; i <= 30; i++) {
				var aa = sp + i * 0.62, rr = 0.45 + i * 0.27;
				var qx = cos(aa) * rr, qy = sin(aa) * rr * 1.1;
				if (i === 0) c.moveTo(qx, qy); else c.lineTo(qx, qy);
			}
			c.stroke();
		} else {
			var ir = (mode === 'plead') ? 1.0 : (mode === 'wide' ? 0.55 : 0.88);
			var irx = 6.8 * ir, iry = 8.8 * ir;
			if (mode === 'wide') { irx = 3.4; iry = 3.4; }
			var cx = lx * (mode === 'wide' ? 1.2 : 1), cy = ly * (mode === 'wide' ? 1.2 : 1) + (mode === 'plead' ? 0.8 : 0);
			if (mode === 'wide') {
				c.beginPath(); ell(c, cx, cy, irx, iry); c.fillStyle = pal.pupil; c.fill();
				c.beginPath(); ell(c, cx - 0.9, cy - 1, 1.1, 1.1); c.fillStyle = '#fff'; c.fill();
			} else {
				var gi = c.createRadialGradient(cx, cy - iry * 0.5, 1, cx, cy, iry * 1.05);
				gi.addColorStop(0, pal.iris1); gi.addColorStop(1, pal.iris2);
				c.beginPath(); ell(c, cx, cy, irx, iry); c.fillStyle = gi; c.fill();
				c.strokeStyle = 'rgba(30,70,40,0.55)'; c.lineWidth = 1; c.stroke();
				var prx = (mode === 'plead') ? 4.5 : 3.5, pry = (mode === 'plead') ? 6.7 : 5.6;
				c.beginPath(); ell(c, cx, cy + 0.3, prx, pry); c.fillStyle = pal.pupil; c.fill();
				/* 高光：大 + 小 + 泪眼的第三颗 */
				c.fillStyle = '#ffffff';
				c.beginPath(); ell(c, cx - irx * 0.36, cy - iry * 0.36, 2.9, 3.2); c.fill();
				c.beginPath(); ell(c, cx + irx * 0.38, cy + iry * 0.42, 1.5, 1.5); c.fill();
				if (mode === 'plead') {
					c.beginPath(); ell(c, cx + irx * 0.12, cy - iry * 0.62, 1.2, 1.2); c.fill();
					/* 泪膜：下眼睑一条亮水带 */
					c.beginPath(); c.moveTo(-rx, eh * 0.55); c.quadraticCurveTo(0, eh * 0.2, rx, eh * 0.55); c.lineTo(rx, eh + 2); c.lineTo(-rx, eh + 2); c.closePath();
					c.fillStyle = 'rgba(190,232,255,0.78)'; c.fill();
					c.beginPath(); c.moveTo(-rx * 0.6, eh * 0.64); c.quadraticCurveTo(0, eh * 0.45, rx * 0.5, eh * 0.66);
					c.strokeStyle = 'rgba(255,255,255,0.95)'; c.lineWidth = 1.1; c.stroke();
				}
				/* 上眼睑投影 */
				c.fillStyle = 'rgba(60,30,20,0.18)';
				c.beginPath(); ell(c, 0, -eh * 0.92, rx * 1.1, eh * 0.38); c.fill();
			}
			/* 眼睑切角：愤怒/担忧 */
			if (abs(lid) > 0.05) {
				var inner = -sd, drop = lid * 7.5; /* inner 方向 x 符号 */
				c.beginPath();
				/* 切掉上方三角：外角 y=-eh*0.55-?，内角 y 随 lid 变化 */
				var yo = -eh * 0.4 - lid * 3.2, yi = -eh * 0.4 + lid * 7.2 + 0;
				/* 外角在 -inner 侧 */
				c.moveTo(-inner * 12, yo); c.lineTo(inner * 12, yi); c.lineTo(inner * 12, -20); c.lineTo(-inner * 12, -20); c.closePath();
				c.fillStyle = pal.fur; c.fill();
				c.strokeStyle = pal.ink; c.lineWidth = 2.8; c.lineCap = 'round';
				c.beginPath(); c.moveTo(-inner * 12, yo); c.lineTo(inner * 12, yi); c.stroke();
			}
		}
		c.restore();
		/* 眼眶描边 + 上睫毛线 */
		c.beginPath(); ell(c, 0, 0, rx * sc, eh);
		c.strokeStyle = pal.ink; c.lineWidth = 2.1; c.stroke();
		c.beginPath();
		c.moveTo(-rx * sc * 0.96, -eh * 0.28); c.bezierCurveTo(-rx * 0.8, -eh * 1.02, rx * 0.8, -eh * 1.02, rx * sc * 0.96, -eh * 0.28);
		c.lineWidth = 3.2; c.stroke();
		/* 外眼角睫毛 */
		c.beginPath(); c.moveTo(sd * rx * 0.9, -eh * 0.5); c.lineTo(sd * (rx + 3.2), -eh * 0.78); c.lineWidth = 2.1; c.stroke();
		if (mode !== 'dizzy' && mode !== 'wide' && abs(lid) > 0.3) {
			/* 小眉毛 */
			c.beginPath(); var inn = -sd;
			c.moveTo(-inn * 7, -eh - 3.4 - lid * 1.0); c.lineTo(inn * 6, -eh - 4.6 + lid * 3.6 * 1);
			c.strokeStyle = pal.furStripe; c.lineWidth = 2.1; c.stroke();
		}
		c.restore();
	}

	function drawMouth(c, p, pal, t, mode, mo) {
		var i;
		c.lineCap = 'round'; c.lineJoin = 'round';
		/* 鼻-人中 */
		c.strokeStyle = pal.ink; c.lineWidth = 1.9;
		if (mode === 'open' || mode === 'yawn' || mode === 'o') {
			if (mode === 'open') {
				var oh = 5 + 7 * mo;
				c.beginPath();
				c.moveTo(-7.5, 15.6); c.quadraticCurveTo(0, 17.8, 7.5, 15.6);
				c.bezierCurveTo(8, 15.6 + oh, 3.5, 17 + oh + 1.5, 0, 17 + oh + 1.5);
				c.bezierCurveTo(-3.5, 17 + oh + 1.5, -8, 15.6 + oh, -7.5, 15.6);
				c.closePath();
				c.fillStyle = '#7b2535'; c.fill();
				c.save(); c.clip();
				c.beginPath(); ell(c, 0, 17.4 + oh + 1.2, 5.6, 3.6 + mo * 2); c.fillStyle = '#ff7f93'; c.fill();
				c.restore();
				c.beginPath();
				c.moveTo(-7.5, 15.6); c.quadraticCurveTo(0, 17.8, 7.5, 15.6);
				c.bezierCurveTo(8, 15.6 + oh, 3.5, 17 + oh + 1.5, 0, 17 + oh + 1.5);
				c.bezierCurveTo(-3.5, 17 + oh + 1.5, -8, 15.6 + oh, -7.5, 15.6);
				c.closePath(); c.stroke();
				/* 小尖牙 */
				c.beginPath(); c.moveTo(-5.4, 16.2); c.lineTo(-4.3, 19.4); c.lineTo(-3.2, 16.8); c.moveTo(5.4, 16.2); c.lineTo(4.3, 19.4); c.lineTo(3.2, 16.8);
				c.fillStyle = '#fff'; c.fill(); c.lineWidth = 1; c.stroke();
			} else if (mode === 'o') {
				c.beginPath(); ell(c, 0, 20.4, 4.8 + mo * 1.2, 5.6 + mo * 2.2);
				c.fillStyle = '#7b2535'; c.fill(); c.lineWidth = 2; c.stroke();
				c.beginPath(); ell(c, 0, 23.6, 3, 1.8); c.fillStyle = '#ff7f93'; c.fill();
			} else {
				var yh = 3 + mo * 11, yw = 7 + mo * 3.2;
				c.beginPath(); ell(c, 0, 17.5 + yh * 0.5, yw, yh);
				c.fillStyle = '#7b2535'; c.fill();
				c.save(); c.clip();
				c.beginPath(); ell(c, 0, 17.5 + yh * 1.35, yw * 0.72, yh * 0.62); c.fillStyle = '#ff7f93'; c.fill();
				c.restore();
				c.beginPath(); ell(c, 0, 17.5 + yh * 0.5, yw, yh); c.lineWidth = 2; c.stroke();
				if (mo > 0.4) { c.beginPath(); c.moveTo(-4, 18.3); c.lineTo(-3, 21.8); c.lineTo(-2, 18.5); c.moveTo(4, 18.3); c.lineTo(3, 21.8); c.lineTo(2, 18.5); c.fillStyle = '#fff'; c.fill(); c.lineWidth = 0.9; c.stroke(); }
			}
			c.beginPath(); c.moveTo(0, 14); c.lineTo(0, 15.8); c.lineWidth = 1.9; c.strokeStyle = pal.ink; c.stroke();
			return;
		}
		c.beginPath(); c.moveTo(0, 13.8); c.lineTo(0, 16.6); c.stroke();
		if (mode === 'sad') {
			var wob = sin(t * 17) * 0.5 * mo;
			c.beginPath(); c.moveTo(-6.2, 21.4 + wob); c.quadraticCurveTo(-3, 16.4 + wob, 0, 17.2); c.quadraticCurveTo(3, 16.4 - wob, 6.2, 21.4 - wob);
			c.lineWidth = 2; c.stroke();
		} else if (mode === 'flat') {
			c.beginPath(); rrect(c, -7.5, 16.4, 15, 6.4, 2.4); c.fillStyle = '#fff'; c.fill(); c.lineWidth = 1.9; c.stroke();
			c.beginPath(); for (i = -4; i <= 4; i += 2.5) { c.moveTo(i, 16.6); c.lineTo(i, 22.6); } c.lineWidth = 1; c.stroke();
			c.beginPath(); c.moveTo(-7.5, 19.6); c.lineTo(7.5, 19.6); c.stroke();
		} else if (mode === 'wavy') {
			c.beginPath(); c.moveTo(-8, 19.5);
			for (i = 0; i < 4; i++) c.quadraticCurveTo(-8 + i * 4 + 1, 17 + (i & 1 ? 5 : 0) + 0.0, -8 + i * 4 + 4, 19.5);
			c.lineWidth = 2; c.stroke();
		} else if (mode === 'tongue') {
			c.beginPath(); c.moveTo(0, 16.6); c.quadraticCurveTo(-3.5, 21, -7.4, 17.4); c.moveTo(0, 16.6); c.quadraticCurveTo(3.5, 21, 7.4, 17.4); c.lineWidth = 1.9; c.stroke();
			c.beginPath(); c.moveTo(-3.2, 19.4); c.bezierCurveTo(-3.6, 26, 3.6, 26, 3.2, 19.4); c.closePath();
			c.fillStyle = '#ff7f93'; c.fill(); c.lineWidth = 1.6; c.stroke();
			c.beginPath(); c.moveTo(0, 20.4); c.lineTo(0, 23.2); c.lineWidth = 1; c.strokeStyle = 'rgba(160,40,70,0.7)'; c.stroke();
		} else if (mode === 'cat') {
			c.beginPath(); c.moveTo(0, 16.6); c.quadraticCurveTo(-3.4, 21.6, -8, 16.8); c.moveTo(0, 16.6); c.quadraticCurveTo(3.4, 21.6, 8, 16.8); c.lineWidth = 2.1; c.stroke();
			/* 小酒窝 */
			c.beginPath(); c.moveTo(-9.4, 17.4); c.lineTo(-10.6, 15.8); c.moveTo(9.4, 17.4); c.lineTo(10.6, 15.8); c.lineWidth = 1.2; c.stroke();
		} else { /* smile ω */
			c.beginPath(); c.moveTo(0, 16.6); c.quadraticCurveTo(-3.2, 20.8, -7.6, 17.2); c.moveTo(0, 16.6); c.quadraticCurveTo(3.2, 20.8, 7.6, 17.2); c.lineWidth = 2; c.stroke();
		}
	}

	function drawWhiskers(c, sd, p, pal, t) {
		var i, y0, spread = [-5.5, 0.5, 6.5], dr = p.whisk;
		var flick = sin(t * 3.1 + sd) * 0.7;
		for (i = 0; i < 3; i++) {
			y0 = 12.5 + i * 3.1;
			c.beginPath();
			c.moveTo(sd * 22, y0);
			c.quadraticCurveTo(sd * 36, y0 + spread[i] * 0.5 + flick * (i - 1) * 0.5, sd * (47 + (i === 1 ? 2 : 0)), y0 + spread[i] + 1 + dr * 4);
			c.strokeStyle = 'rgba(70,40,30,0.55)'; c.lineWidth = 2.6; c.lineCap = 'round'; c.stroke();
			c.strokeStyle = '#ffffff'; c.lineWidth = 1.2; c.stroke();
		}
	}

	function drawHead(c, p, pal, t, blink, look) {
		var i, fx = look.x * 3.6, fy = look.y * 1.8;
		/* 耳朵（后）：earL 负值=向外下垂 */
		drawEar(c, -1, p.earL, pal);
		drawEar(c, 1, p.earR, pal);
		/* 头 */
		c.save();
		c.beginPath(); headPath(c);
		var g = c.createLinearGradient(0, -26, 0, 28);
		g.addColorStop(0, pal.furHi); g.addColorStop(0.55, pal.fur); g.addColorStop(1, pal.fur);
		c.fillStyle = g; c.fill();
		c.clip();
		/* 白脸下半 + 脸颊白毛 */
		c.beginPath();
		c.moveTo(-40, 8); c.bezierCurveTo(-33, 3, -26, 9, -19, 11); c.bezierCurveTo(-12, 12, -6, 6, 0, 8);
		c.bezierCurveTo(6, 6, 12, 12, 19, 11); c.bezierCurveTo(26, 9, 33, 3, 40, 8); c.lineTo(40, 40); c.lineTo(-40, 40); c.closePath();
		var g2 = c.createLinearGradient(0, 4, 0, 28); g2.addColorStop(0, '#ffffff'); g2.addColorStop(1, pal.cream);
		c.fillStyle = g2; c.fill();
		/* 虎斑：额头三道 + 脸颊各两道 */
		stripe(c, 0, -27, 0, -13.5, 2.3, pal.furStripe);
		stripe(c, -10, -25, -8, -14.5, 1.9, pal.furStripe);
		stripe(c, 10, -25, 8, -14.5, 1.9, pal.furStripe);
		stripe(c, -35, -6, -26, -3.5, 2.1, pal.furStripe);
		stripe(c, -35.5, 1.5, -27.5, 2.2, 1.9, pal.furStripe);
		stripe(c, 35, -6, 26, -3.5, 2.1, pal.furStripe);
		stripe(c, 35.5, 1.5, 27.5, 2.2, 1.9, pal.furStripe);
		/* cel 阴影 + 高光 */
		shadeCrescent(c, -4, -5, 36, 30, 'rgba(180,70,10,0.2)');
		c.beginPath(); ell(c, -17, -19, 9, 3.4); c.fillStyle = 'rgba(255,240,200,0.5)'; c.fill();
		c.restore();
		c.beginPath(); headPath(c); c.strokeStyle = pal.ink; c.lineWidth = 2.9; c.lineJoin = 'round'; c.stroke();
		/* 脸部随视线平移（伪 3D） */
		c.save();
		c.translate(fx, fy);
		/* 口鼻部白垫 */
		c.beginPath(); ell(c, -5.6, 16.4, 8.6, 6.4); ell(c, 5.6, 16.4, 8.6, 6.4);
		c.fillStyle = 'rgba(255,255,255,0.55)'; c.fill();
		/* 脸颊红晕 */
		if (p.blush > 0.02) {
			c.beginPath(); ell(c, -24, 11.5, 7.2, 4.4); ell(c, 24, 11.5, 7.2, 4.4);
			c.fillStyle = 'rgba(255,92,120,' + (0.55 * p.blush) + ')'; c.fill();
			c.strokeStyle = 'rgba(255,255,255,' + (0.5 * p.blush) + ')'; c.lineWidth = 1; c.lineCap = 'round';
			c.beginPath(); for (i = 0; i < 3; i++) { c.moveTo(-26.5 + i * 2.6, 13.2); c.lineTo(-25.2 + i * 2.6, 9.4); c.moveTo(21.4 + i * 2.6, 13.2); c.lineTo(22.7 + i * 2.6, 9.4); } c.stroke();
		}
		/* 眼睛 */
		drawEye(c, -1, p, pal, blink, t, look);
		drawEye(c, 1, p, pal, blink, t, look);
		/* 鼻子 */
		c.beginPath();
		c.moveTo(-4.6, 9.8); c.quadraticCurveTo(0, 8.6, 4.6, 9.8); c.quadraticCurveTo(4.6, 12.4, 0.4, 14.2); c.quadraticCurveTo(-0.4, 14.4, -0.8, 14.2); c.quadraticCurveTo(-4.6, 12.4, -4.6, 9.8);
		c.closePath();
		var gn = c.createLinearGradient(0, 9, 0, 14.4); gn.addColorStop(0, '#ff9db4'); gn.addColorStop(1, pal.nose);
		fs(c, gn, pal.ink, 1.8);
		c.beginPath(); ell(c, -1.6, 10.5, 1.5, 0.8); c.fillStyle = 'rgba(255,255,255,0.8)'; c.fill();
		drawMouth(c, p, pal, t, p.mouth, p.mo);
		drawWhiskers(c, -1, p, pal, t);
		drawWhiskers(c, 1, p, pal, t);
		c.restore();
	}

	/* 头顶晕星（深浅通吃，描边） */
	function drawOrbitStars(c, t, pal) {
		var i, a, x, y, z, order = [];
		for (i = 0; i < 4; i++) {
			a = t * 3.2 + i * TAU / 4;
			order.push({ z: sin(a), a: a });
		}
		order.sort(function (A, B) { return A.z - B.z; });
		for (i = 0; i < order.length; i++) {
			a = order[i].a; z = order[i].z;
			x = cos(a) * 29; y = -31 + sin(a) * 7;
			c.beginPath(); starPath(c, x, y, 5.6 + z * 1.4, a * 0.8, 0.5);
			fs(c, '#ffe14d', pal.ink, 1.7);
			c.beginPath(); ell(c, x - 1.2, y - 1.5, 1.1, 1.1); c.fillStyle = 'rgba(255,255,255,0.9)'; c.fill();
		}
	}
	function drawSweat(c, t, pal) {
		var i, k, y;
		for (i = 0; i < 2; i++) {
			k = (t * 1.6 + i * 0.5) % 1;
			var x = (i ? 1 : -1) * (37 + k * 3), yy = -14 + k * 24 + (i ? 4 : 0);
			c.save(); c.translate(x, yy); c.scale(0.8 + (1 - k) * 0.3, 0.8 + (1 - k) * 0.3);
			c.beginPath(); c.moveTo(0, -6.5); c.bezierCurveTo(4.2, -1, 4.8, 1.2, 4.8, 2.6); c.bezierCurveTo(4.8, 6, -4.8, 6, -4.8, 2.6); c.bezierCurveTo(-4.8, 1.2, -4.2, -1, 0, -6.5);
			c.closePath(); c.globalAlpha = 1 - k * k;
			fs(c, '#bfe8ff', pal.ink, 1.5);
			c.beginPath(); ell(c, -1.6, 1.8, 1, 1.7); c.fillStyle = 'rgba(255,255,255,0.9)'; c.fill();
			c.restore();
		}
		c.globalAlpha = 1;
	}
	function drawTears(c, t, pal) {
		var i, k, x, y;
		for (i = 0; i < 2; i++) {
			k = (t * 0.8 + i * 0.47) % 1;
			x = (i ? 1 : -1) * (15.5 + k * 3.6); y = 12 + k * 14;
			c.save(); c.translate(x, y); c.globalAlpha = k > 0.8 ? (1 - k) * 5 : 1;
			c.beginPath(); c.moveTo(0, -3.6); c.bezierCurveTo(2.6, -0.6, 3, 0.6, 3, 1.4); c.bezierCurveTo(3, 3.6, -3, 3.6, -3, 1.4); c.bezierCurveTo(-3, 0.6, -2.6, -0.6, 0, -3.6);
			c.closePath(); fs(c, pal.tear, 'rgba(60,110,160,0.8)', 1.1);
			c.beginPath(); ell(c, -0.9, 1, 0.7, 1.1); c.fillStyle = '#fff'; c.fill();
			c.restore();
		}
	}

	function autoBlink(t) {
		return max(pulse(t, 3.7, 0.35, 0.2), pulse(t, 8.9, 1.9, 0.18), pulse(t, 8.9, 2.25, 0.18));
	}

	function xfPoint(o, pivotY, lx, ly) {
		var s = o.s == null ? 1 : o.s, sx = (o.sx == null ? 1 : o.sx) * s * (o.flip ? -1 : 1), sy = (o.sy == null ? 1 : o.sy) * s;
		var py = pivotY * s * (o.sy == null ? 1 : o.sy);
		var X = lx * sx, Y = ly * sy - py;
		var r = o.rot || 0, cr = cos(r), sr = sin(r);
		return { x: (o.x || 0) + X * cr - Y * sr, y: (o.y || 0) + py + X * sr + Y * cr };
	}

	function anchorsOf(o, p) {
		var pv = o.pivotY || 0, bsy = p ? p.bsy : 1, by = p ? p.by : 0, hy = p ? p.hy : 0, bx = p ? p.bx : 0;
		var headY = -(40 * bsy) - 18 + hy + by;
		return {
			feet: xfPoint(o, pv, bx, by),
			body: xfPoint(o, pv, bx, by - 22 * bsy),
			head: xfPoint(o, pv, bx, headY),
			top: xfPoint(o, pv, bx, headY - 36),
			mouth: xfPoint(o, pv, bx, headY + 17),
			handL: xfPoint(o, pv, p ? p.aLx + bx : -23, p ? p.aLy + by : -19),
			handR: xfPoint(o, pv, p ? p.aRx + bx : 23, p ? p.aRy + by : -19),
			eyeL: xfPoint(o, pv, bx - 14, headY + 3),
			eyeR: xfPoint(o, pv, bx + 14, headY + 3)
		};
	}

	function drawCat(ctx, o) {
		o = o || {};
		var pal = palOf(o), t = o.t || 0, pose = o.pose || 'idle', p = catPose(pose, t, o), c = ctx;
		var look = o.look || { x: p.lookx, y: p.looky };
		var blink = (o.blink != null) ? o.blink : autoBlink(t);
		if (p.eye === 'closed' || p.eye === 'happy' || p.eye === 'heart') blink = 0;
		if (pose === 'plead' || pose === 'scared' || pose === 'bonk' || pose === 'dizzy' || pose === 'charge' || pose === 'windup') blink = (o.blink != null) ? o.blink : 0;
		if (o.mouth) p.mouth = o.mouth;
		if (o.eye) p.eye = o.eye;
		var ex = o.extra || {};
		if (ex.tailAmp != null) p.tailAmp = ex.tailAmp;
		if (ex.blush != null) p.blush = ex.blush;
		var s = o.s == null ? 1 : o.s, pv = o.pivotY || 0;
		var sxx = (o.sx == null ? 1 : o.sx), syy = (o.sy == null ? 1 : o.sy);

		c.save();
		c.translate(o.x || 0, o.y || 0);
		var pyW = pv * s * syy;
		if (o.rot) { c.translate(0, pyW); c.rotate(o.rot); c.translate(0, -pyW); }
		c.scale(s * sxx * (o.flip ? -1 : 1), s * syy);
		c.lineJoin = 'round'; c.lineCap = 'round';

		if (o.shadow) {
			c.beginPath(); ell(c, 0, 0, 27, 5.5); c.fillStyle = 'rgba(0,0,0,0.22)'; c.fill();
		}

		/* 冲刺速度线 */
		if (p.lines) {
			c.strokeStyle = pal.dark ? 'rgba(255,255,255,0.75)' : 'rgba(80,60,50,0.6)'; c.lineWidth = 2.2;
			var li;
			for (li = 0; li < 4; li++) {
				var ly0 = -14 - li * 16, off = ((t * 9 + li * 0.37) % 1) * 14;
				c.beginPath(); c.moveTo(-34 - off, ly0); c.lineTo(-58 - off - li * 4, ly0 + (li - 1.5) * 0.4); c.stroke();
			}
		}

		/* —— 身体空间（带挤压） —— */
		c.save();
		c.translate(p.bx, p.by);
		c.rotate(p.brot);
		c.scale(p.bsx, p.bsy);
		var tail = null;
		if (!p.tailFront) drawTail(c, p, t, pal);
		/* 坐姿的后腿 */
		if (p.haunch) {
			var hs = [-1, 1], hi;
			for (hi = 0; hi < 2; hi++) {
				c.beginPath(); ell(c, hs[hi] * 19, -11, 12.5, 11.5);
				var gh = c.createLinearGradient(0, -22, 0, 0); gh.addColorStop(0, pal.fur); gh.addColorStop(1, pal.furSh);
				fs(c, gh, pal.ink, 2.4);
				stripe(c, hs[hi] * 28.5, -15, hs[hi] * 21, -13, 1.8, pal.furStripe);
				stripe(c, hs[hi] * 28, -8, hs[hi] * 21.5, -8.5, 1.6, pal.furStripe);
			}
		}
		/* 脚 */
		drawFoot(c, p.fLx, p.fLy, pal);
		drawFoot(c, p.fRx, p.fRy, pal);
		/* 躯干 */
		c.save();
		c.beginPath(); bodyPath(c);
		var gb = c.createLinearGradient(-20, -44, 20, 0);
		gb.addColorStop(0, pal.furHi); gb.addColorStop(0.5, pal.fur); gb.addColorStop(1, pal.furSh);
		c.fillStyle = gb; c.fill();
		c.clip();
		/* 白肚皮 */
		c.beginPath(); ell(c, 0, -16, 11.5, 15.5);
		var gw = c.createLinearGradient(0, -32, 0, 0); gw.addColorStop(0, '#ffffff'); gw.addColorStop(1, pal.creamSh);
		c.fillStyle = gw; c.fill();
		c.strokeStyle = 'rgba(150,100,60,0.35)'; c.lineWidth = 1.2;
		c.beginPath(); c.arc(0, -16, 12.6, 0.15 * PI, 0.85 * PI); c.stroke();
		/* 侧身虎斑 */
		stripe(c, -23, -33, -14, -30, 2.1, pal.furStripe); stripe(c, -23, -25, -14.5, -23.5, 2, pal.furStripe); stripe(c, -22, -17, -14, -16.5, 1.8, pal.furStripe);
		stripe(c, 23, -33, 14, -30, 2.1, pal.furStripe); stripe(c, 23, -25, 14.5, -23.5, 2, pal.furStripe); stripe(c, 22, -17, 14, -16.5, 1.8, pal.furStripe);
		shadeCrescent(c, -4, -24, 22, 24, 'rgba(160,60,10,0.2)');
		c.restore();
		c.beginPath(); bodyPath(c); c.strokeStyle = pal.ink; c.lineWidth = 2.7; c.stroke();
		if (p.tailFront) drawTail(c, p, t, pal);
		/* 手臂（背景层） */
		if (!p.armsFront) {
			drawArm(c, -16, -33, p.aLx, p.aLy, 1, pal, p.palm);
			drawArm(c, 16, -33, p.aRx, p.aRy, -1, pal, p.palm);
		}
		c.restore();

		/* —— 头（不随躯干挤压变形；跟随躯干的位置与倾斜） —— */
		var neckY = -(40 * p.bsy) - 18;
		c.save();
		c.translate(p.bx, p.by);
		c.rotate(p.brot * 0.7);
		c.translate(p.hx, neckY + p.hy);
		c.rotate(p.hrot);
		c.scale(p.hsx, p.hsy);
		drawHead(c, p, pal, t, blink, look);
		if (p.tear) drawTears(c, t, pal);
		if (p.sweat) drawSweat(c, t, pal);
		if (p.stars) drawOrbitStars(c, t, pal);
		c.restore();

		/* —— 前景手臂（抱脸/鼓掌/扒栏杆） —— */
		if (p.armsFront) {
			c.save();
			c.translate(p.bx, p.by);
			c.rotate(p.brot);
			c.scale(p.bsx, p.bsy);
			drawArm(c, -16, -33, p.aLx, p.aLy, 1, pal, p.palm);
			drawArm(c, 16, -33, p.aRx, p.aRy, -1, pal, p.palm);
			c.restore();
		}
		/* 挥手的手臂（右手单独抬高） */
		c.restore();

		return anchorsOf(o, p);
	}

	function catAnchors(o) {
		o = o || {};
		var p = catPose(o.pose || 'idle', o.t || 0, o);
		return anchorsOf(o, p);
	}

	/* =====================================================================
	 *  笼子
	 * ===================================================================== */
	var CAGE = { w: 160, h: 150, bars: 7, spacing: 21, barW: 7, baseH: 18, roofH: 13, ringH: 13, floorY: 21, x0: -63, catScale: 0.8 };  /* catScale: 笼内猫的建议缩放 = 笼子 k × catScale（猫耳不顶到笼顶） */
	function barX(i) { return CAGE.x0 + CAGE.spacing * i; }

	function mkBars() {
		var a = [], i;
		for (i = 0; i < CAGE.bars; i++) a.push({ hp: 1, bend: 0, gone: false, lean: 0 });
		return a;
	}
	function cageStage(n) {
		n = clamp(Math.round(n || 0), 0, 6);
		var s = { bars: mkBars(), lock: 0, topDamage: 0, baseDamage: 0, tilt: 0, flash: 0, gone: false, rot: 0, shake: 0 }, b = s.bars;
		if (n >= 1) { /* ① 栏杆弯曲变形 */
			b[4].bend = 12; b[5].bend = 19; b[5].hp = 0.72; b[2].bend = -4;
		}
		if (n >= 2) { /* ② 一根栏杆崩断 */
			b[5].gone = true; b[4].bend = 18; b[4].hp = 0.7; b[3].hp = 0.8; b[2].hp = 0.84; b[6].bend = 7;
		}
		if (n >= 3) { /* ③ 锁头裂开，崩飞一半 */
			s.lock = 2; s.tilt = 0.02; b[2].bend = -7; b[2].hp = 0.7; b[3].hp = 0.62;
		}
		if (n >= 4) { /* ④ 顶塌，整体歪斜 */
			s.topDamage = 1; s.tilt = 0.07; s.lock = 3; s.baseDamage = 0.5; b[1].bend = -10; b[1].hp = 0.6;
		}
		if (n >= 5) { /* ⑤ 多根栏杆炸开，底座裂开 */
			b[0].lean = -0.62; b[0].bend = -12; b[1].gone = true;
			b[2].lean = -0.34; b[2].bend = -15; b[3].hp = 0.28; b[3].lean = 0.08; b[4].gone = true;
			b[6].lean = 0.8; b[6].bend = 10;
			s.topDamage = 1.4; s.baseDamage = 1; s.tilt = 0.1;
		}
		if (n >= 6) { /* ⑥ 整体爆散 */
			var i;
			for (i = 0; i < b.length; i++) { b[i].gone = true; b[i].lean = 0; b[i].bend = 0; }
			s.gone = true; s.topDamage = 2; s.baseDamage = 2; s.tilt = 0; s.lock = 3;
		}
		return s;
	}

	function cageLayout(c) {
		var k = (c.w || 160) / 160, H = (c.h || 150) / k;
		return { k: k, H: H, yb: -CAGE.baseH + 1, yt: -(H - CAGE.ringH - CAGE.roofH) };
	}
	function cageOrigin(c) {
		var ox = c.x || 0, oy = c.y || 0, w = c.w || 160, h = c.h || 150;
		if (c.origin === 'center') oy += h / 2;
		else if (c.origin === 'topleft') { ox += w / 2; oy += h; }
		return { x: ox, y: oy };
	}
	function cageBegin(ctx, c) {
		var L = cageLayout(c), o = cageOrigin(c), t = c.t || 0, sh = c.shake || 0;
		ctx.save();
		ctx.translate(o.x + sh * sin(t * 53) * 2.4 * L.k, o.y + sh * cos(t * 47) * 1.5 * L.k);
		if (c.rot) ctx.rotate(c.rot);
		if (c.tilt) ctx.transform(1, 0, -c.tilt, 1, 0, 0);
		ctx.scale(L.k, L.k);
		ctx.lineJoin = 'round'; ctx.lineCap = 'round';
		return L;
	}
	/* 逻辑坐标 → 画布像素（不含 shake 抖动） */
	function cagePoint(c, lx, ly) {
		var L = cageLayout(c), o = cageOrigin(c);
		var x = lx - (c.tilt || 0) * ly, y = ly;
		x *= L.k; y *= L.k;
		var r = c.rot || 0, cr = cos(r), sr = sin(r);
		return { x: o.x + x * cr - y * sr, y: o.y + x * sr + y * cr };
	}
	function barPoint(c, i, u) {
		var L = cageLayout(c), b = (c.bars && c.bars[i]) || {};
		var y = L.yb + (L.yt - L.yb) * u;
		var x = barX(i) + (b.lean || 0) * (L.yb - y) + (b.bend || 0) * sin(PI * u);
		return cagePoint(c, x, y);
	}
	function cageRect(c) {
		var o = cageOrigin(c), w = c.w || 160, h = c.h || 150, k = w / 160;
		var tl = cagePoint(c, -80, -(h / k)), tr = cagePoint(c, 80, -(h / k)), bl = cagePoint(c, -80, 0), br = cagePoint(c, 80, 0);
		var x0 = min(tl.x, tr.x, bl.x, br.x), x1 = max(tl.x, tr.x, bl.x, br.x), y0 = min(tl.y, tr.y, bl.y, br.y), y1 = max(tl.y, tr.y, bl.y, br.y);
		return { x: x0, y: y0, w: x1 - x0, h: y1 - y0, cx: (x0 + x1) / 2, cy: (y0 + y1) / 2, bottomX: o.x, bottomY: o.y };
	}
	/* 猫脚应站的地面点（画布像素） */
	function cageFloor(c) {
		var k = (c.w || 160) / 160;
		return cagePoint(c, 0, -(CAGE.floorY - 1));
	}

	function barPts(L, i, b, t, shake, back) {
		var n = 14, pts = [], j, u, y, x, vib, bx = barX(i) * (back ? 0.9 : 1);
		var lean = b.lean || 0, bend = b.bend || 0;
		for (j = 0; j <= n; j++) {
			u = j / n;
			y = L.yb + (L.yt - L.yb) * u - (back ? 6 : 0) * u * 0;
			vib = shake * sin(t * 44 + i * 1.37) * 2.6 * sin(PI * u);
			x = bx + lean * (L.yb - y) + bend * sin(PI * u) * (back ? 0.6 : 1) + vib;
			pts.push([x, y]);
		}
		return pts;
	}
	function polyline(c, pts, a, z, off) {
		var j;
		c.beginPath();
		for (j = a; j <= z; j++) { if (j === a) c.moveTo(pts[j][0] + off, pts[j][1]); else c.lineTo(pts[j][0] + off, pts[j][1]); }
	}
	function fl(col, f) { return f > 0 ? mix(col, '#ffffff', f) : col; }

	function drawBarSeg(c, pal, pts, a, z, w, flash, back) {
		c.lineCap = 'butt'; c.lineJoin = 'round';
		if (pal.dark && !back) { polyline(c, pts, a, z, 0); c.strokeStyle = pal.rim; c.lineWidth = w + 5.4; c.stroke(); }
		polyline(c, pts, a, z, 0); c.strokeStyle = back ? mix(pal.steelOut, pal.steelLo, 0.5) : pal.steelOut; c.lineWidth = w + 2.8; c.stroke();
		polyline(c, pts, a, z, 0); c.strokeStyle = fl(back ? pal.steelLo : pal.steel, flash); c.lineWidth = w; c.stroke();
		if (!back) {
			polyline(c, pts, a, z, w * 0.28); c.strokeStyle = fl(pal.steelLo, flash); c.lineWidth = w * 0.4; c.stroke();
			polyline(c, pts, a, z, -w * 0.2); c.strokeStyle = fl(pal.steelHi, flash); c.lineWidth = w * 0.22; c.stroke();
		}
		c.lineCap = 'round';
	}
	function jag(c, pal, x, y, dir, w, seed, flash) {
		/* 断口锯齿 */
		var h1 = 4 + hash(seed) * 3, h2 = 5 + hash(seed + 1) * 4, h3 = 3 + hash(seed + 2) * 3;
		c.beginPath();
		c.moveTo(x - w / 2 - 1.2, y);
		c.lineTo(x - w / 2 + 0.4, y + dir * h1); c.lineTo(x - w * 0.12, y + dir * 1.2); c.lineTo(x + w * 0.1, y + dir * h2); c.lineTo(x + w / 2 - 0.2, y + dir * h3); c.lineTo(x + w / 2 + 1.2, y);
		c.closePath();
		c.fillStyle = fl(pal.steel, flash); c.fill();
		c.strokeStyle = pal.steelOut; c.lineWidth = 1.8; c.stroke();
		c.beginPath(); c.moveTo(x - w * 0.12, y + dir * 1.2); c.lineTo(x + w * 0.1, y + dir * h2); c.strokeStyle = fl(pal.steelHi, flash); c.lineWidth = 1; c.stroke();
	}
	function cracks(c, pal, pts, hp, seed) {
		if (hp >= 0.9) return;
		var n = hp < 0.35 ? 3 : (hp < 0.65 ? 2 : 1), q, j, y, x;
		c.strokeStyle = pal.steelOut; c.lineWidth = 1.5; c.lineCap = 'round';
		for (q = 0; q < n; q++) {
			j = 3 + floor(hash(seed + q * 3.1) * 8);
			x = pts[j][0]; y = pts[j][1];
			c.beginPath(); c.moveTo(x - 3.2, y); c.lineTo(x - 0.5, y + 1.8); c.lineTo(x + 0.8, y - 0.8); c.lineTo(x + 3.2, y + 1.2);
			c.stroke();
		}
	}

	function drawCageBack(ctx, c) {
		var pal = palOf(c), t = c.t || 0, L = cageBegin(ctx, c), bars = c.bars || mkBars(), i, gone = 0, sh = c.shake || 0, fl0 = c.flash || 0;
		for (i = 0; i < bars.length; i++) if (bars[i].gone) gone++;
		if (c.gone) { ctx.restore(); return; }
		var pa = 1 - gone / bars.length * 0.8;
		/* 内壁暗色 */
		ctx.globalAlpha = pa;
		ctx.beginPath(); rrect(ctx, -70, L.yt, 140, L.yb - L.yt - 9, 6);
		var gp = ctx.createLinearGradient(0, L.yt, 0, L.yb);
		gp.addColorStop(0, pal.panel); gp.addColorStop(1, pal.dark ? 'rgba(4,10,20,0.22)' : 'rgba(30,50,84,0.10)');
		ctx.fillStyle = gp; ctx.fill();
		ctx.globalAlpha = 1;
		/* 地台上表面 */
		if (!(c.baseDamage >= 2)) {
			ctx.beginPath(); ctx.moveTo(-77, -CAGE.baseH + 1); ctx.lineTo(77, -CAGE.baseH + 1); ctx.lineTo(70, -CAGE.baseH - 10); ctx.lineTo(-70, -CAGE.baseH - 10); ctx.closePath();
			var gf = ctx.createLinearGradient(0, -CAGE.baseH - 10, 0, -CAGE.baseH);
			gf.addColorStop(0, fl(mix(pal.floor, pal.steelLo, 0.35), fl0)); gf.addColorStop(1, fl(pal.floor, fl0));
			fs(ctx, gf, pal.steelOut, 2);
			ctx.strokeStyle = 'rgba(0,0,0,0.18)'; ctx.lineWidth = 1;
			ctx.beginPath(); for (i = 1; i < 6; i++) { var fx0 = -77 + i * 25.7; ctx.moveTo(fx0, -CAGE.baseH + 1); ctx.lineTo(fx0 * 0.91, -CAGE.baseH - 10); } ctx.stroke();
		}
		/* 后排栏杆（细、暗） */
		for (i = 0; i < bars.length; i++) {
			if (bars[i].gone) continue;
			var bp = barPts(L, i, bars[i], t, sh * 0.6, true);
			drawBarSeg(ctx, pal, bp, 0, bp.length - 1, 5, fl0 * 0.5, true);
		}
		/* 后顶梁 */
		if (!(c.topDamage >= 1)) {
			ctx.beginPath(); rrect(ctx, -72, L.yt - 4, 144, 6, 2);
			fs(ctx, mix(pal.steelLo, pal.steelOut, 0.3), pal.steelOut, 1.6);
		}
		ctx.restore();
	}

	function roofPoly(L, dmg) {
		var xs = [], i, x, top = [], bot = [], yt = L.yt, rh = CAGE.roofH, d;
		for (i = 0; i <= 26; i++) xs.push(-78 + i * 6);
		for (i = 0; i < xs.length; i++) {
			x = xs[i];
			var dome = 9 * (1 - pow(x / 78, 2));
			var yT = yt - rh - dome + 3;
			var yB = yt;
			if (dmg > 0) {
				d = dmg > 1 ? 1 : dmg;
				var crater = exp1(-pow((x + 18) / 38, 2));
				yT += d * (16 * crater + (x < -50 ? 9 * (-50 - x) / 28 : 0) * 0.9) + (dmg - d) * 10 * crater;
				yB += d * (6 * crater + (x < -50 ? 8 * (-50 - x) / 28 : 0)) + (dmg - d) * 6;
			}
			top.push([x, yT]); bot.push([x, yB]);
		}
		return { top: top, bot: bot };
	}
	function exp1(x) { return Math.exp(x); }

	function drawRoof(c, pal, L, dmg, flash, t) {
		var rp = roofPoly(L, dmg), i, yt = L.yt;
		var pathFn = function () {
			c.beginPath();
			c.moveTo(rp.top[0][0], rp.bot[0][1] + 0);
			for (i = 0; i < rp.top.length; i++) c.lineTo(rp.top[i][0], rp.top[i][1]);
			for (i = rp.bot.length - 1; i >= 0; i--) c.lineTo(rp.bot[i][0], rp.bot[i][1]);
			c.closePath();
		};
		if (pal.dark) { pathFn(); c.strokeStyle = pal.rim; c.lineWidth = 5; c.stroke(); }
		pathFn();
		var g = c.createLinearGradient(0, yt - CAGE.roofH - 10, 0, yt + 2);
		g.addColorStop(0, fl(pal.steelHi, flash)); g.addColorStop(0.22, fl(pal.steel, flash)); g.addColorStop(0.75, fl(pal.steelLo, flash)); g.addColorStop(1, fl(mix(pal.steelLo, pal.steelOut, 0.5), flash));
		fs(c, g, pal.steelOut, 2.4);
		/* 高光条与铆钉 */
		c.save(); pathFn(); c.clip();
		c.beginPath();
		for (i = 2; i < rp.top.length - 2; i++) { if (i === 2) c.moveTo(rp.top[i][0], rp.top[i][1] + 3.6); else c.lineTo(rp.top[i][0], rp.top[i][1] + 3.6); }
		c.strokeStyle = 'rgba(255,255,255,0.55)'; c.lineWidth = 2; c.stroke();
		c.beginPath();
		for (i = 1; i < rp.bot.length - 1; i++) { if (i === 1) c.moveTo(rp.bot[i][0], rp.bot[i][1] - 3); else c.lineTo(rp.bot[i][0], rp.bot[i][1] - 3); }
		c.strokeStyle = 'rgba(10,20,40,0.35)'; c.lineWidth = 2.2; c.stroke();
		/* 撞坏的缺口 */
		if (dmg >= 1) {
			c.beginPath();
			c.moveTo(28, yt - 30);
			var zz = [[34, yt - 9], [38, yt - 17], [43, yt - 6], [48, yt - 15], [53, yt - 4], [58, yt - 12], [62, yt - 30]];
			for (i = 0; i < zz.length; i++) c.lineTo(zz[i][0], zz[i][1]);
			c.closePath();
			c.fillStyle = 'rgba(8,14,26,0.9)'; c.fill();
			c.strokeStyle = pal.steelOut; c.lineWidth = 2; c.stroke();
			/* 凹痕阴影 */
			c.beginPath(); ell(c, -18, yt - 4, 26, 8); c.fillStyle = 'rgba(10,20,40,0.28)'; c.fill();
			c.strokeStyle = pal.steelOut; c.lineWidth = 1.6;
			c.beginPath(); c.moveTo(-40, yt - 7); c.lineTo(-30, yt - 3); c.lineTo(-27, yt - 10); c.lineTo(-14, yt - 5); c.stroke();
		}
		c.restore();
		for (i = 0; i < 6; i++) {
			var rx = -62 + i * 24.8; if (dmg >= 1 && rx > 22 && rx < 66) continue;
			var ry = lerpPoly(rp.top, rx) + 6;
			rivet(c, pal, rx, ry, 2.1);
		}
	}
	function lerpPoly(arr, x) {
		var i;
		for (i = 0; i < arr.length - 1; i++) if (x >= arr[i][0] && x <= arr[i + 1][0]) return lerp(arr[i][1], arr[i + 1][1], (x - arr[i][0]) / (arr[i + 1][0] - arr[i][0]));
		return arr[0][1];
	}
	function rivet(c, pal, x, y, r) {
		c.beginPath(); c.arc(x, y, r, 0, TAU);
		var g = c.createRadialGradient(x - r * 0.4, y - r * 0.4, 0.2, x, y, r);
		g.addColorStop(0, '#ffffff'); g.addColorStop(0.45, pal.rivet); g.addColorStop(1, pal.steelLo);
		fs(c, g, pal.steelOut, 1.3);
	}

	function drawRing(c, pal, L, dmg, flash) {
		var rp = roofPoly(L, dmg), cy = lerpPoly(rp.top, 0) - 6, x = 0;
		c.save();
		if (dmg >= 1) { c.translate(-10, 6); c.rotate(-0.55); x = 0; }
		c.translate(0, cy);
		/* 底座 */
		c.beginPath(); c.moveTo(-9, 5); c.lineTo(-6, -1.5); c.lineTo(6, -1.5); c.lineTo(9, 5); c.closePath();
		fs(c, fl(pal.steel, flash), pal.steelOut, 2);
		/* 环 */
		c.beginPath(); ell(c, 0, -8, 8.6, 9.6);
		if (pal.dark) { c.strokeStyle = pal.rim; c.lineWidth = 8.4; c.stroke(); }
		c.strokeStyle = pal.steelOut; c.lineWidth = 7; c.stroke();
		c.strokeStyle = fl(pal.steel, flash); c.lineWidth = 4; c.stroke();
		c.beginPath(); c.arc(0, -8, 8.6 * 0.92, PI * 1.05, PI * 1.55);
		c.strokeStyle = fl(pal.steelHi, flash); c.lineWidth = 1.6; c.stroke();
		c.restore();
	}

	function drawBase(c, pal, L, dmg, flash, t) {
		var bh = CAGE.baseH, i;
		if (dmg >= 2) { /* 爆散后只剩焦痕与两三块底座残片 */
			c.beginPath(); ell(c, 0, -3, 70, 7); c.fillStyle = pal.scorch; c.fill();
			c.beginPath(); ell(c, 0, -3, 42, 4); c.fillStyle = 'rgba(10,6,4,0.4)'; c.fill();
			var sh = [[-58, 0.4, 14], [-30, -0.2, 9], [44, 0.3, 12]], q;
			for (q = 0; q < sh.length; q++) {
				c.save(); c.translate(sh[q][0], -3); c.rotate(sh[q][1]);
				c.beginPath(); c.moveTo(-sh[q][2], 0); c.lineTo(-sh[q][2] * 0.5, -6); c.lineTo(sh[q][2] * 0.6, -4); c.lineTo(sh[q][2], 1); c.closePath();
				fs(c, fl(pal.steelLo, flash), pal.steelOut, 1.8);
				c.beginPath(); c.moveTo(-sh[q][2] * 0.7, -2); c.lineTo(sh[q][2] * 0.4, -3.2); c.strokeStyle = pal.steelHi; c.lineWidth = 1; c.stroke();
				c.restore();
			}
			return;
		}
		function slab(x0, x1, yOff, rot, leftRound, rightRound) {
			c.save(); c.translate((x0 + x1) / 2, yOff); c.rotate(rot); c.translate(-(x0 + x1) / 2, 0);
			var w = x1 - x0;
			c.beginPath();
			c.moveTo(x0 + (leftRound ? 6 : 0), -bh);
			c.lineTo(x1 - (rightRound ? 6 : 0), -bh);
			if (rightRound) c.quadraticCurveTo(x1, -bh, x1, -bh + 6); else c.lineTo(x1, -bh + 2);
			c.lineTo(x1 - (rightRound ? 2 : 0), -1);
			c.lineTo(x0 + (leftRound ? 2 : 0), -1);
			c.lineTo(x0, -bh + (leftRound ? 6 : 2));
			if (leftRound) c.quadraticCurveTo(x0, -bh, x0 + 6, -bh);
			c.closePath();
			if (pal.dark) { c.strokeStyle = pal.rim; c.lineWidth = 5; c.stroke(); }
			var g = c.createLinearGradient(0, -bh, 0, 0);
			g.addColorStop(0, fl(pal.steelHi, flash)); g.addColorStop(0.2, fl(pal.steel, flash)); g.addColorStop(0.8, fl(pal.steelLo, flash)); g.addColorStop(1, fl(mix(pal.steelLo, pal.steelOut, 0.55), flash));
			fs(c, g, pal.steelOut, 2.6);
			/* 凹槽压纹 */
			c.beginPath(); c.moveTo(x0 + 5, -bh + 5.4); c.lineTo(x1 - 5, -bh + 5.4);
			c.strokeStyle = 'rgba(255,255,255,0.5)'; c.lineWidth = 1.4; c.stroke();
			c.beginPath(); c.moveTo(x0 + 5, -bh + 7.4); c.lineTo(x1 - 5, -bh + 7.4);
			c.strokeStyle = 'rgba(10,20,40,0.35)'; c.stroke();
			/* 铆钉 */
			var n = max(1, Math.round(w / 19.3)), j;
			for (j = 0; j < n; j++) rivet(c, pal, x0 + (j + 0.5) * w / n, -bh * 0.36, 2.5);
			c.restore();
		}
		if (dmg >= 1) {
			/* 底座从中间裂成两半，右半翘起 */
			slab(-80, 6, 0, 0, true, false);
			slab(10, 80, -1, -0.045, false, true);
			c.beginPath(); c.moveTo(6, -bh); c.lineTo(8.6, -bh * 0.62); c.lineTo(5.2, -bh * 0.34); c.lineTo(9.6, -1); c.lineTo(10, -1); c.lineTo(10, -bh); c.closePath();
			c.fillStyle = 'rgba(6,10,18,0.85)'; c.fill();
		} else slab(-80, 80, 0, 0, true, true);
		if (dmg > 0.2 && dmg < 1.9) {
			/* 裂纹 + 缺角 */
			c.strokeStyle = pal.steelOut; c.lineWidth = 1.8; c.lineCap = 'round';
			c.beginPath(); c.moveTo(-34, -bh); c.lineTo(-30, -bh * 0.7); c.lineTo(-36, -bh * 0.5); c.lineTo(-31, -2); c.stroke();
			if (dmg < 1) { c.beginPath(); c.moveTo(38, -bh); c.lineTo(42, -bh * 0.6); c.lineTo(36, -bh * 0.45); c.stroke(); }
			c.beginPath(); c.moveTo(62, -bh); c.lineTo(72, -bh); c.lineTo(80, -bh * 0.4); c.lineTo(74, -bh * 0.5); c.lineTo(68, -bh * 0.18); c.lineTo(64, -bh * 0.45); c.closePath();
			c.fillStyle = 'rgba(6,10,18,0.78)'; c.fill();
		}
	}

	function drawLock(c, pal, L, state, t, shake, flash) {
		/* s90r3: 锁挂在笼子右侧外立柱上(栏杆之外, x=70)并缩小, 猫在笼内任何位置都碰不到它, 不再遮猫身 */
		var ly = L.yb - (L.yb - L.yt) * 0.3;
		var sw = sin(t * 31) * shake * 0.14;
		/* 门栓扣带 */
		c.save();
		c.translate(70, ly);
		c.scale(0.62, 0.62);
		if (state === 3) { c.translate(-15, -12); c.rotate(0.26 + sin(t * 2.1) * 0.02 + sw); c.translate(15, 12); }
		c.beginPath(); rrect(c, -15, -16, 30, 9, 3);
		var gb = c.createLinearGradient(0, -16, 0, -7);
		gb.addColorStop(0, fl(pal.steelHi, flash)); gb.addColorStop(0.4, fl(pal.steel, flash)); gb.addColorStop(1, fl(pal.steelLo, flash));
		if (pal.dark) { c.strokeStyle = pal.rim; c.lineWidth = 5; c.stroke(); }
		fs(c, gb, pal.steelOut, 2.2);
		rivet(c, pal, -9.5, -11.5, 2); rivet(c, pal, 9.5, -11.5, 2);
		if (state === 3) { /* 空扣环 */
			c.beginPath(); c.arc(0, -3, 6, PI * 0.15, PI * 0.9, true); c.strokeStyle = pal.steelOut; c.lineWidth = 5.4; c.stroke();
			c.strokeStyle = fl(pal.steel, flash); c.lineWidth = 3; c.stroke();
			c.restore();
			return;
		}
		/* 锁梁 */
		c.translate(0, -11); c.rotate(sw);
		var open = state === 2;
		c.beginPath(); c.moveTo(-7.4, 16); c.lineTo(-7.4, 7); c.arc(0, 7, 7.4, PI, 0, false); c.lineTo(7.4, open ? 8 : 16);
		c.strokeStyle = pal.steelOut; c.lineWidth = 6.6; c.lineJoin = 'round'; c.stroke();
		c.strokeStyle = fl('#c9d5e6', flash); c.lineWidth = 3.4; c.stroke();
		c.beginPath(); c.moveTo(-5.6, 12); c.lineTo(-5.6, 7); c.arc(0, 7, 5.6, PI, PI * 1.55, false);
		c.strokeStyle = 'rgba(255,255,255,0.85)'; c.lineWidth = 1.2; c.stroke();
		/* 锁身 */
		var bx = -13, by = 14, bw = 26, bh = 22;
		c.save();
		c.beginPath();
		if (state === 2) { /* 右半崩飞：锯齿断口 */
			c.moveTo(bx + 5, by); c.lineTo(3, by); c.lineTo(5.5, by + 4); c.lineTo(1.5, by + 8); c.lineTo(5, by + 12); c.lineTo(0.5, by + 16.5); c.lineTo(4, by + 19.5); c.lineTo(2, by + bh);
			c.lineTo(bx + 5, by + bh); c.quadraticCurveTo(bx, by + bh, bx, by + bh - 5); c.lineTo(bx, by + 5); c.quadraticCurveTo(bx, by, bx + 5, by);
		} else rrect(c, bx, by, bw, bh, 5.5);
		c.closePath();
		var gg = c.createLinearGradient(bx, by, bx + bw, by + bh);
		gg.addColorStop(0, fl(pal.goldHi, flash)); gg.addColorStop(0.28, fl(pal.gold, flash)); gg.addColorStop(0.7, fl(pal.goldMid, flash)); gg.addColorStop(1, fl(pal.goldLo, flash));
		fs(c, gg, '#5a3207', 2.4);
		c.clip();
		/* 高光与下沿阴影 */
		c.fillStyle = 'rgba(255,255,255,0.55)'; c.fillRect(bx + 3, by + 2.4, 3.2, bh - 6);
		c.fillStyle = 'rgba(120,60,0,0.3)'; c.fillRect(bx, by + bh - 5, bw, 5);
		/* 锁孔 */
		c.beginPath(); c.arc(-0.2, by + 9, 3, 0, TAU); c.moveTo(-1.6, by + 9); c.lineTo(-2.4, by + 16.4); c.lineTo(2, by + 16.4); c.lineTo(1.2, by + 9);
		c.fillStyle = '#3a1d04'; c.fill();
		if (state === 2) { /* 断口里露出暗色内腔 */
			c.fillStyle = 'rgba(70,35,4,0.55)'; c.fillRect(-3, by, 6, bh);
		}
		c.restore();
		if (state === 1) { /* 裂纹 */
			c.strokeStyle = '#4a2504'; c.lineWidth = 1.7; c.lineCap = 'round';
			c.beginPath(); c.moveTo(-0.4, by + 1); c.lineTo(2.4, by + 5); c.lineTo(-1.6, by + 9); c.lineTo(3.2, by + 14); c.lineTo(0, by + 21);
			c.moveTo(2.4, by + 5); c.lineTo(7.4, by + 4.4); c.moveTo(-1.6, by + 9); c.lineTo(-6.4, by + 11); c.stroke();
		}
		c.restore();
	}

	function drawCageFront(ctx, c) {
		var pal = palOf(c), t = c.t || 0, L = cageBegin(ctx, c), bars = c.bars || mkBars(), i, sh = c.shake || 0, flash = c.flash || 0;
		var lock = c.lock || 0, topD = c.topDamage || 0, baseD = c.baseDamage || 0;
		/* 栏杆 */
		for (i = 0; i < bars.length; i++) {
			var b = bars[i], bw = (i === 0 || i === bars.length - 1) ? 9 : 7, pts = barPts(L, i, b, t, sh, false), n = pts.length - 1, lo = 0, hi = n;
			if (c.gone) continue;
			if (b.gone) {
				var botN = 3 + floor(hash(i * 3.3) * 3), topN = 2 + floor(hash(i * 5.7 + 2) * 3);
				drawBarSeg(ctx, pal, pts, 0, botN, bw, flash, false);
				jag(ctx, pal, pts[botN][0], pts[botN][1], -1, bw, i * 7, flash);
				if (topD < 1.9) {
					drawBarSeg(ctx, pal, pts, n - topN, n, bw, flash, false);
					jag(ctx, pal, pts[n - topN][0], pts[n - topN][1], 1, bw, i * 9 + 3, flash);
				}
				continue;
			}
			drawBarSeg(ctx, pal, pts, 0, n, bw, flash, false);
			if (b.hp < 0.9) cracks(ctx, pal, pts, b.hp, i * 11.3);
			if (b.bend && abs(b.bend) > 8) { /* 弯折处的压痕高光 */
				var mj = floor(n / 2);
				ctx.beginPath(); ctx.moveTo(pts[mj][0] - bw * 0.5, pts[mj][1] - 1); ctx.lineTo(pts[mj][0] + bw * 0.5, pts[mj][1] + 1);
				ctx.strokeStyle = pal.steelOut; ctx.lineWidth = 1.5; ctx.stroke();
			}
		}
		/* 底座 */
		drawBase(ctx, pal, L, baseD, flash, t);
		/* 门锁 */
		if (!c.gone && !(bars[5] && bars[5].gone && bars[6] && bars[6].gone)) drawLock(ctx, pal, L, lock, t, sh, flash);
		/* 顶 */
		if (topD < 1.9 && !c.gone) { drawRoof(ctx, pal, L, topD, flash, t); drawRing(ctx, pal, L, topD, flash); }
		else if (c.gone || topD >= 1.9) { /* 只剩顶梁的两个断角 */
			if (!c.gone) { /* 无 */ }
		}
		ctx.restore();
	}
	function drawCage(ctx, c) { drawCageBack(ctx, c); drawCageFront(ctx, c); }

	/* =====================================================================
	 *  布偶猫（解锁后的玩偶）：缝线、纽扣眼睛、软乎乎的填充感
	 * ===================================================================== */
	var PLUSH_POSES = ['idle', 'hop', 'flip', 'sit', 'wave', 'yawn', 'chase-tail'];

	function plushPose(name, t, o) {
		var q = {
			bx: 0, by: 0, bsx: 1, bsy: 1, rot: 0, hrot: 0, hx: 0, hy: 0,
			aLx: -27, aLy: -20, aRx: 27, aRy: -20,
			fLx: -12, fLy: 0, fRx: 12, fRy: 0,
			mouth: 'smile', mo: 0, tbx: 14, tby: -9, tailAng: 0.1, tailAmp: 0.12, tailFreq: 2.0, tailCurl: -1.1, tailHook: 0.5,
			earL: 0, earR: 0, armsFront: false, sit: false, pivotY: -26, bowWag: 0, tailChase: false
		};
		var open, ph = (o.phase != null) ? clamp(o.phase, 0, 1) : ((t * 0.9) % 1), hopH = (o.hopH != null) ? o.hopH : 34, br, k, w, h, air, ex;
		switch (name) {
		case 'hop':
			if (ph < 0.2) { /* 预备：蹲 */
				k = sstep(0, 0.2, ph);
				q.bsy = 1 - 0.22 * k; q.bsx = 1 + 0.15 * k; q.hy = 3 * k;
				q.aLx = -25 - 3 * k; q.aLy = -16 + 3 * k; q.aRx = 25 + 3 * k; q.aRy = -16 + 3 * k;
				q.earL = 0.12 * k; q.earR = -0.12 * k; q.mouth = 'smile';
			} else if (ph < 0.82) {
				w = (ph - 0.2) / 0.62; h = 4 * w * (1 - w);
				q.by = -hopH * h;
				ex = (1 - w) * (1 - w);
				q.bsy = 1 + 0.12 * h + 0.1 * ex; q.bsx = 1 - 0.08 * h - 0.06 * ex;
				q.aLx = -31; q.aLy = -42 + h * -6; q.aRx = 31; q.aRy = -42 + h * -6;
				q.fLy = -5 * h - 2; q.fRy = q.fLy; q.fLx = -10; q.fRx = 10;
				q.earL = -0.4 * h - 0.2 * sin(w * 14); q.earR = 0.4 * h + 0.2 * sin(w * 14 + 1);
				q.mouth = 'open'; q.mo = 0.7; q.tailAng = -0.9; q.tailAmp = 0.2; q.tailCurl = -0.8;
				q.hrot = -0.06 * (w - 0.5);
			} else { /* 落地：压扁回弹 */
				k = (ph - 0.82) / 0.18;
				var sq = sin(k * PI) * (1 - k * 0.3);
				q.bsy = 1 - 0.24 * sq; q.bsx = 1 + 0.16 * sq; q.hy = 3.5 * sq;
				q.aLx = -28 - 3 * sq; q.aLy = -20 + 4 * sq; q.aRx = 28 + 3 * sq; q.aRy = -20 + 4 * sq;
				q.earL = -0.3 * sq; q.earR = 0.3 * sq; q.mouth = 'smile';
			}
			break;
		case 'flip':
			if (ph < 0.16) {
				k = sstep(0, 0.16, ph);
				q.bsy = 1 - 0.24 * k; q.bsx = 1 + 0.16 * k; q.hy = 3 * k; q.aLy = -14; q.aRy = -14;
			} else if (ph < 0.86) {
				w = (ph - 0.16) / 0.7; h = 4 * w * (1 - w);
				q.by = -hopH * 1.25 * h;
				q.rot = -TAU * sstep(0.0, 1.0, w);
				q.bsy = 0.95; q.bsx = 1.02;
				q.aLx = -17; q.aLy = -30; q.aRx = 17; q.aRy = -30;
				q.fLx = -9; q.fRx = 9; q.fLy = -9; q.fRy = -9;
				q.mouth = 'open'; q.mo = 0.5; q.earL = -0.5; q.earR = 0.5;
				q.tailAng = -1.2; q.tailCurl = -0.6;
			} else {
				k = (ph - 0.86) / 0.14; var sq2 = sin(k * PI);
				q.bsy = 1 - 0.26 * sq2; q.bsx = 1 + 0.18 * sq2; q.hy = 3.5 * sq2; q.aLx = -30; q.aRx = 30; q.aLy = -16; q.aRy = -16;
				q.mouth = 'open'; q.mo = 0.5;
			}
			break;
		case 'sit':
			br = sin(t * 1.7);
			q.sit = true; q.bsy = 0.9 + br * 0.01; q.bsx = 1.12; q.hy = 4;
			q.aLx = -14; q.aLy = -14; q.aRx = 14; q.aRy = -14;
			q.fLx = -15; q.fRx = 15; q.fLy = 0; q.fRy = 0;
			q.tbx = 16; q.tby = -5; q.tailAng = 0.1; q.tailCurl = 2.2; q.tailAmp = 0.05; q.tailHook = -0.2;
			q.hrot = sin(t * 0.8) * 0.04; q.earL = -0.1 * pulse(t, 4.1, 0.6, 0.25);
			break;
		case 'wave':
			k = t * 8;
			q.hrot = 0.08 + sin(t * 2) * 0.03; q.rot = sin(t * 2) * 0.02;
			q.aRx = 47 + sin(k) * 5; q.aRy = -66 + cos(k) * 2; q.armsFront = true; q.mouth = 'smile';
			q.tailAmp = 0.3; q.tailFreq = 4;
			q.earR = sin(k) * 0.05;
			break;
		case 'yawn':
			k = (t % 3.8) / 3.8;
			open = sstep(0.08, 0.34, k) * (1 - sstep(0.7, 0.92, k));
			q.mouth = 'yawn'; q.mo = open;
			q.hy = -2 * open; q.hrot = -0.06 * open; q.bsy = 1 + 0.05 * open;
			q.aLx = lerp(-27, -43, open); q.aLy = lerp(-20, -64, open); q.aRx = lerp(27, 43, open); q.aRy = lerp(-20, -64, open); q.armsFront = open > 0.08;
			q.earL = -0.3 * open; q.earR = 0.3 * open;
			break;
		case 'chase-tail':
			k = t * 4.6;
			q.bsx = 0.66 + 0.34 * cos(k); q.rot = sin(k) * 0.12; q.hrot = 0.26 * cos(k * 0.5 + 0.4); q.by = -abs(sin(k)) * 2.2;
			q.fLy = -max(0, sin(k * 2)) * 3; q.fRy = -max(0, -sin(k * 2)) * 3;
			q.tailAng = -0.75; q.tailCurl = -0.5; q.tailAmp = 0.12; q.tailFreq = 9; q.tailHook = 0.9; q.tailChase = true; q.tbx = 17;
			q.mouth = 'open'; q.mo = 0.3; q.aLx = -25; q.aRx = 25;
			q.aLy = -24 + sin(k) * 3; q.aRy = -24 - sin(k) * 3;
			break;
		default: /* idle */
			br = sin(t * 2.0);
			q.bsy = 1 + 0.014 * br; q.bsx = 1 - 0.008 * br; q.rot = sin(t * 1.3) * 0.025; q.hrot = sin(t * 1.0 + 0.6) * 0.04;
			q.earL = -0.2 * pulse(t, 3.7, 0.4, 0.24); q.earR = 0.08 * pulse(t, 5.1, 1, 0.2);
			q.aLy = -20 + br * 0.5; q.aRy = -20 + br * 0.5;
			break;
		}
		return q;
	}
	
	function stitchEdge(c, pal, fn, w) {
		c.save();
		c.beginPath(); fn(c); c.clip();
		c.beginPath(); fn(c);
		c.setLineDash([1.5, 3.7]); c.lineWidth = w || 6.4; c.strokeStyle = pal.pThread; c.lineCap = 'butt'; c.stroke();
		c.setLineDash([]);
		c.restore();
	}
	function seamLine(c, pal, x0, y0, cx, cy, x1, y1) {
		c.save();
		c.beginPath(); c.moveTo(x0, y0); c.quadraticCurveTo(cx, cy, x1, y1);
		c.setLineDash([3.2, 2.6]); c.strokeStyle = pal.pThread; c.lineWidth = 1.5; c.lineCap = 'round'; c.stroke();
		c.setLineDash([]);
		c.restore();
	}
	function pHeadPath(c) {
		c.moveTo(0, -29);
		c.bezierCurveTo(21, -29, 37, -19, 37.5, -2);
		c.bezierCurveTo(38, 14, 25, 28, 0, 28);
		c.bezierCurveTo(-25, 28, -38, 14, -37.5, -2);
		c.bezierCurveTo(-37, -19, -21, -29, 0, -29);
		c.closePath();
	}
	function pBodyPath(c) {
		c.moveTo(-13, -44);
		c.bezierCurveTo(-23, -40, -26, -24, -25, -13);
		c.bezierCurveTo(-24, -3.5, -14, -0.5, 0, -0.5);
		c.bezierCurveTo(14, -0.5, 24, -3.5, 25, -13);
		c.bezierCurveTo(26, -24, 23, -40, 13, -44);
		c.closePath();
	}
	function pEarPath(c, sd) {
		c.moveTo(sd * 33, -8);
		c.bezierCurveTo(sd * 37, -22, sd * 36, -36, sd * 30, -44);
		c.bezierCurveTo(sd * 25, -48, sd * 18, -44, sd * 13, -38);
		c.bezierCurveTo(sd * 9, -33, sd * 5, -29, sd * 4, -26);
		c.closePath();
	}
	function pEarInPath(c, sd) {
		c.moveTo(sd * 29.5, -14);
		c.bezierCurveTo(sd * 32, -23, sd * 31, -32, sd * 28, -38);
		c.bezierCurveTo(sd * 23, -35, sd * 18, -31, sd * 14, -27);
		c.closePath();
	}

	function pLimb(c, pal, x0, y0, x1, y1, w, padCol) {
		c.lineCap = 'round'; c.lineJoin = 'round';
		c.beginPath(); c.moveTo(x0, y0); c.lineTo(x1, y1);
		c.strokeStyle = pal.ink; c.lineWidth = w + 3.8; c.stroke();
		c.strokeStyle = pal.pFab; c.lineWidth = w; c.stroke();
		c.strokeStyle = pal.pFabSh; c.lineWidth = w * 0.3;
		c.beginPath(); c.moveTo(x0 + 2, y0 + 2); c.lineTo(x1 + 1.5, y1 + 2); c.stroke();
		/* 手掌/脚掌：奶油色布片 + 缝线圈 */
		c.beginPath(); c.arc(x1, y1, w * 0.46, 0, TAU);
		c.fillStyle = padCol || pal.pCream; c.fill();
		c.strokeStyle = pal.ink2; c.lineWidth = 1.3; c.stroke();
		c.save(); c.setLineDash([1.6, 1.8]); c.beginPath(); c.arc(x1, y1, w * 0.46 - 2.2, 0, TAU); c.strokeStyle = pal.pThread; c.lineWidth = 1; c.stroke(); c.restore();
	}

	function pTail(c, pal, q, t) {
		var N = 14, seg = 3.0, a = q.tailAng, x = q.tbx, y = q.tby, i, f, pts = [], ph, curv, r;
		for (i = 0; i <= N; i++) {
			f = i / N;
			ph = sin(t * q.tailFreq - i * 0.36);
			curv = q.tailCurl / N + ph * q.tailAmp * (0.25 + f * 0.9) / 3.2 * (i > 0 ? 1 : 0);
			if (q.tailHook && f > 0.72) curv += q.tailHook * 0.1 * ((f - 0.72) / 0.28);
			a += curv; x += cos(a) * seg; y += sin(a) * seg;
			r = lerp(5.6, 4.3, f);
			pts.push([x, y, r, a]);
		}
		c.fillStyle = pal.ink;
		for (i = 0; i < pts.length; i++) { c.beginPath(); c.arc(pts[i][0], pts[i][1], pts[i][2] + 1.9, 0, TAU); c.fill(); }
		for (i = 0; i < pts.length; i++) { c.beginPath(); c.arc(pts[i][0], pts[i][1], pts[i][2], 0, TAU); c.fillStyle = (i > N - 5) ? pal.pPatch : pal.pFab; c.fill(); }
		/* 缝合线（横跨尾巴的两道） + 中心缝 */
		c.strokeStyle = pal.pThread; c.lineWidth = 1.4; c.lineCap = 'round';
		var idx = [7, 12], q2, nx, ny, rr;
		for (i = 0; i < idx.length; i++) {
			q2 = pts[idx[i]]; nx = -sin(q2[3]); ny = cos(q2[3]); rr = q2[2] * 0.8;
			c.beginPath(); c.moveTo(q2[0] - nx * rr, q2[1] - ny * rr); c.lineTo(q2[0] + nx * rr, q2[1] + ny * rr);
			c.setLineDash([2, 2]); c.stroke(); c.setLineDash([]);
		}
		c.beginPath();
		for (i = 1; i < N - 1; i++) { if (i === 1) c.moveTo(pts[i][0], pts[i][1]); else c.lineTo(pts[i][0], pts[i][1]); }
		c.setLineDash([3, 2.6]); c.strokeStyle = pal.pThread; c.lineWidth = 1.2; c.stroke(); c.setLineDash([]);
		c.beginPath();
		for (i = 1; i < N - 4; i++) { var hh = pts[i]; var hxx = hh[0] + sin(hh[3]) * hh[2] * 0.5, hyy = hh[1] - cos(hh[3]) * hh[2] * 0.5; if (i === 1) c.moveTo(hxx, hyy); else c.lineTo(hxx, hyy); }
		c.strokeStyle = 'rgba(255,248,230,0.5)'; c.lineWidth = 1.6; c.stroke();
		return pts;
	}

	function pButton(c, pal, x, y, r, kind, hiAmt) {
		var g = c.createRadialGradient(x - r * 0.35, y - r * 0.4, 0.5, x, y, r);
		if (kind === 'amber') { g.addColorStop(0, '#d98a3a'); g.addColorStop(1, '#6a3410'); }
		else { g.addColorStop(0, '#5d4234'); g.addColorStop(1, '#1d110a'); }
		c.beginPath(); c.arc(x, y, r, 0, TAU);
		c.fillStyle = g; c.fill(); c.strokeStyle = pal.ink; c.lineWidth = 1.8; c.stroke();
		/* 内圈凹槽 */
		c.beginPath(); c.arc(x, y, r * 0.72, 0, TAU); c.strokeStyle = 'rgba(255,255,255,0.18)'; c.lineWidth = 1; c.stroke();
		c.beginPath(); c.arc(x, y, r * 0.72, PI * 1.1, PI * 1.55); c.strokeStyle = 'rgba(0,0,0,0.5)'; c.lineWidth = 0.9; c.stroke();
		/* 线孔 + 十字线 */
		var d = r * 0.3, hole = [[-d, -d], [d, -d], [-d, d], [d, d]], i;
		c.fillStyle = '#0a0503';
		if (kind === 'amber') {
			c.beginPath(); c.arc(x - d * 1.1, y, r * 0.16, 0, TAU); c.arc(x + d * 1.1, y, r * 0.16, 0, TAU); c.fill();
			c.beginPath(); c.moveTo(x - d * 1.1, y); c.lineTo(x + d * 1.1, y); c.strokeStyle = '#f5e6c8'; c.lineWidth = 1.5; c.stroke();
		} else {
			for (i = 0; i < 4; i++) { c.beginPath(); c.arc(x + hole[i][0], y + hole[i][1], r * 0.14, 0, TAU); c.fill(); }
			c.beginPath(); c.moveTo(x - d, y - d); c.lineTo(x + d, y + d); c.moveTo(x + d, y - d); c.lineTo(x - d, y + d);
			c.strokeStyle = '#f5e6c8'; c.lineWidth = 1.4; c.stroke();
		}
		/* 高光 */
		c.beginPath(); ell(c, x - r * 0.42, y - r * 0.5, r * 0.28, r * 0.18); c.fillStyle = 'rgba(255,255,255,0.75)'; c.fill();
		c.beginPath(); ell(c, x + r * 0.42, y + r * 0.46, r * 0.12, r * 0.1); c.fillStyle = 'rgba(255,255,255,0.45)'; c.fill();
	}

	function pHead(c, pal, q, t, o) {
		var i;
		/* 耳 */
		var sd, rotE;
		for (i = 0; i < 2; i++) {
			sd = i ? 1 : -1; rotE = i ? q.earR : q.earL;
			c.save();
			c.translate(sd * 20, -20); c.rotate(rotE); c.translate(-sd * 20, 20);
			var fn = (function (s) { return function (cc) { pEarPath(cc, s); }; })(sd);
			c.beginPath(); fn(c);
			fs(c, (sd > 0) ? pal.pPatch : pal.pFab, pal.ink, 2.6);
			stitchEdge(c, pal, fn, 6);
			c.beginPath(); pEarInPath(c, sd); c.fillStyle = pal.pEar; c.fill();
			c.strokeStyle = pal.ink2; c.lineWidth = 1.2; c.stroke();
			c.restore();
		}
		/* 头布 */
		c.beginPath(); pHeadPath(c);
		var g = c.createLinearGradient(-30, -28, 30, 28);
		g.addColorStop(0, '#f8d9a6'); g.addColorStop(0.55, pal.pFab); g.addColorStop(1, pal.pFabSh);
		c.fillStyle = g; c.fill();
		c.save(); c.clip();
		shadeCrescent(c, -4, -6, 38, 32, 'rgba(150,80,30,0.22)');
		/* 右眼处的橘色补丁（缝线边） */
		var patch = function (cc) { ell(cc, 17, -3, 17, 16.5); };
		c.beginPath(); patch(c); c.fillStyle = pal.pPatch; c.fill();
		c.strokeStyle = pal.pThread; c.lineWidth = 1.2; c.setLineDash([2.4, 2.2]); c.stroke(); c.setLineDash([]);
		/* 奶油色嘴套 */
		c.beginPath(); ell(c, -6, 16, 11, 9); ell(c, 6, 16, 11, 9); c.fillStyle = pal.pCream; c.fill();
		c.strokeStyle = pal.pThread; c.lineWidth = 1.2; c.setLineDash([2.4, 2.2]);
		c.beginPath(); c.moveTo(-17, 14); c.quadraticCurveTo(-14, 25, -2, 24); c.moveTo(17, 14); c.quadraticCurveTo(14, 25, 2, 24); c.stroke(); c.setLineDash([]);
		c.beginPath(); ell(c, -16, -20, 9, 3.5); c.fillStyle = 'rgba(255,248,224,0.5)'; c.fill();
		c.restore();
		c.beginPath(); pHeadPath(c); c.strokeStyle = pal.ink; c.lineWidth = 2.9; c.stroke();
		stitchEdge(c, pal, pHeadPath, 6.6);
		/* 额头缝线（Y 形） */
		seamLine(c, pal, 0, -28, 0, -20, 0, -11);
		seamLine(c, pal, 0, -11, -4, -8, -7, -5);
		seamLine(c, pal, 0, -11, 4, -8, 7, -5);
		/* 脸颊 */
		c.beginPath(); ell(c, -26, 11, 6.2, 4.2); ell(c, 26, 11, 6.2, 4.2);
		c.fillStyle = 'rgba(255,128,148,0.62)'; c.fill();
		/* 纽扣眼睛：左黑 4 孔，右琥珀 2 孔 */
		pButton(c, pal, -15, 1, 8.2, 'black');
		pButton(c, pal, 15, 1, 7.4, 'amber');
		/* 鼻子：粉色毛毡三角 */
		c.beginPath(); c.moveTo(-5.2, 9.2); c.quadraticCurveTo(0, 7.6, 5.2, 9.2); c.quadraticCurveTo(4.6, 12.6, 0, 14.4); c.quadraticCurveTo(-4.6, 12.6, -5.2, 9.2);
		c.closePath(); fs(c, pal.pCheek, pal.ink, 1.7);
		c.beginPath(); ell(c, -1.6, 10.2, 1.5, 0.8); c.fillStyle = 'rgba(255,255,255,0.7)'; c.fill();
		/* 嘴（缝线） */
		var m = q.mouth;
		c.lineCap = 'round'; c.lineJoin = 'round';
		if (m === 'yawn' || m === 'open') {
			var mh = (m === 'yawn') ? (3 + q.mo * 11) : (3 + q.mo * 6), mw = (m === 'yawn') ? 7.5 + q.mo * 2.5 : 6.5;
			c.beginPath(); ell(c, 0, 19 + mh * 0.5, mw, mh);
			c.fillStyle = '#7b2535'; c.fill();
			c.save(); c.clip();
			c.beginPath(); ell(c, 0, 19 + mh * 1.4, mw * 0.7, mh * 0.6); c.fillStyle = '#ff8195'; c.fill();
			c.restore();
			c.beginPath(); ell(c, 0, 19 + mh * 0.5, mw, mh); c.strokeStyle = pal.ink; c.lineWidth = 1.9; c.stroke();
			c.beginPath(); ell(c, 0, 19 + mh * 0.5, mw + 2.2, mh + 2.2); c.setLineDash([1.8, 1.8]); c.strokeStyle = pal.pThread; c.lineWidth = 1; c.stroke(); c.setLineDash([]);
			c.beginPath(); c.moveTo(0, 14); c.lineTo(0, 17.5); c.strokeStyle = pal.ink; c.lineWidth = 1.7; c.stroke();
		} else {
			c.beginPath(); c.moveTo(0, 14); c.lineTo(0, 18);
			c.moveTo(0, 18); c.quadraticCurveTo(-3.6, 23, -8, 18.4); c.moveTo(0, 18); c.quadraticCurveTo(3.6, 23, 8, 18.4);
			c.setLineDash([2.2, 1.7]); c.strokeStyle = pal.ink; c.lineWidth = 1.9; c.stroke(); c.setLineDash([]);
		}
		/* 胡须缝线 */
		c.strokeStyle = pal.pThread; c.lineWidth = 1.4;
		c.beginPath();
		for (i = 0; i < 3; i++) {
			var yy = 12 + i * 3.4;
			c.moveTo(-23, yy); c.lineTo(-33 - (i === 1 ? 2 : 0), yy + (i - 1) * 3.4);
			c.moveTo(23, yy); c.lineTo(33 + (i === 1 ? 2 : 0), yy + (i - 1) * 3.4);
		}
		c.stroke();
		/* 小线头 */
		c.strokeStyle = pal.pThread; c.lineWidth = 1.2;
		c.beginPath(); c.moveTo(30, 20); c.quadraticCurveTo(34, 21, 33, 25); c.moveTo(30, 20); c.quadraticCurveTo(31, 24, 28, 25); c.stroke();
	}

	function pBow(c, pal, wag) {
		c.save();
		c.translate(0, 28.5);
		/* 领口带 */
		c.beginPath(); c.moveTo(-17, -3); c.quadraticCurveTo(0, 6.5, 17, -3); c.lineTo(17, 2.4); c.quadraticCurveTo(0, 11.5, -17, 2.4); c.closePath();
		fs(c, pal.pScarf, pal.ink, 2.2);
		c.beginPath(); c.moveTo(-14, -1); c.quadraticCurveTo(0, 7, 14, -1); c.strokeStyle = 'rgba(255,255,255,0.35)'; c.lineWidth = 1.4; c.stroke();
		/* 蝴蝶结 */
		c.translate(9, 4.4);
		c.rotate(-0.18);
		c.beginPath(); c.moveTo(0, 0); c.bezierCurveTo(-4, -9, -15, -9, -15, -1); c.bezierCurveTo(-15, 7, -4, 7, 0, 0); c.closePath(); fs(c, pal.pScarf, pal.ink, 2);
		c.beginPath(); c.moveTo(0, 0); c.bezierCurveTo(4, -9, 15, -9, 15, -1); c.bezierCurveTo(15, 7, 4, 7, 0, 0); c.closePath(); fs(c, pal.pScarf, pal.ink, 2);
		c.beginPath(); c.moveTo(-9, -3); c.quadraticCurveTo(-11, 0, -9, 3); c.moveTo(9, -3); c.quadraticCurveTo(11, 0, 9, 3); c.strokeStyle = pal.pScarfSh; c.lineWidth = 1.2; c.stroke();
		c.beginPath(); ell(c, 0, 0, 3.8, 3.8); fs(c, pal.pScarf, pal.ink, 1.8);
		c.beginPath(); ell(c, -1, -1, 1.1, 0.8); c.fillStyle = 'rgba(255,255,255,0.6)'; c.fill();
		/* 小铃铛 */
		c.beginPath(); c.arc(0, 10.5, 3.9, 0, TAU);
		var gg = c.createRadialGradient(-1, 9, 0.5, 0, 10.5, 4.4); gg.addColorStop(0, '#fff3b0'); gg.addColorStop(1, '#e0a010');
		fs(c, gg, '#5a3207', 1.6);
		c.beginPath(); c.moveTo(-2.8, 11); c.lineTo(2.8, 11); c.strokeStyle = '#5a3207'; c.lineWidth = 1; c.stroke();
		c.restore();
	}

	function drawPlush(ctx, o) {
		o = o || {};
		var pal = palOf(o), t = o.t || 0, pose = o.pose || 'idle', q = plushPose(pose, t, o), c = ctx;
		var s = o.s == null ? 1 : o.s, pv = (o.pivotY != null) ? o.pivotY : q.pivotY, sxx = (o.sx == null ? 1 : o.sx), syy = (o.sy == null ? 1 : o.sy);
		var rot = (o.rot || 0) + q.rot, lift = q.by;
		q.by = 0; /* 起跳高度在世界坐标里抬升，不随翻滚旋转 */
		if (o.shadow !== false && pose !== 'flip') {
			var lf = clamp(-lift / 40, 0, 1);
			c.save(); c.translate(o.x || 0, o.y || 0); c.scale(s * sxx, s * syy);
			c.beginPath(); ell(c, 0, 0, 27 * (1 - lf * 0.4), 5.5 * (1 - lf * 0.4)); c.fillStyle = 'rgba(0,0,0,' + (0.22 * (1 - lf * 0.5)) + ')'; c.fill();
			c.restore();
		}
		c.save();
		c.translate(o.x || 0, (o.y || 0) + lift * s * syy);
		var pyW = pv * s * syy;
		if (rot) { c.translate(0, pyW); c.rotate(rot); c.translate(0, -pyW); }
		c.scale(s * sxx * (o.flip ? -1 : 1), s * syy);
		c.lineJoin = 'round'; c.lineCap = 'round';
		c.save();
		c.translate(q.bx, 0);
		c.scale(q.bsx, q.bsy);
		/* 尾 */
		pTail(c, pal, q, t);
		/* 腿 */
		var legY = q.sit ? -6 : -7;
		pLimb(c, pal, -12, legY, q.fLx, q.fLy - 5.5, q.sit ? 15 : 14.5, pal.pCream);
		pLimb(c, pal, 12, legY, q.fRx, q.fRy - 5.5, q.sit ? 15 : 14.5, pal.pCream);
		/* 躯干 */
		c.beginPath(); pBodyPath(c);
		var gb = c.createLinearGradient(-24, -44, 24, 0);
		gb.addColorStop(0, '#f8d9a6'); gb.addColorStop(0.55, pal.pFab); gb.addColorStop(1, pal.pFabSh);
		c.fillStyle = gb; c.fill();
		c.save(); c.clip();
		shadeCrescent(c, -4, -24, 25, 26, 'rgba(150,80,30,0.2)');
		/* 奶油肚皮布片 + 缝线 */
		var belly = function (cc) { ell(cc, 0, -15, 13, 17); };
		c.beginPath(); belly(c); c.fillStyle = pal.pCream; c.fill();
		c.strokeStyle = pal.pThread; c.lineWidth = 1.3; c.setLineDash([2.4, 2.2]); c.stroke(); c.setLineDash([]);
		/* 背部补丁 */
		c.beginPath(); ell(c, -20, -29, 9, 7); c.fillStyle = pal.pPatch; c.fill();
		c.strokeStyle = pal.pThread; c.lineWidth = 1.1; c.setLineDash([2, 2]); c.stroke(); c.setLineDash([]);
		/* 肚脐小十字线 */
		c.beginPath(); c.moveTo(-2, -17); c.lineTo(2, -13); c.moveTo(2, -17); c.lineTo(-2, -13); c.strokeStyle = pal.pThread; c.lineWidth = 1.2; c.stroke();
		c.restore();
		c.beginPath(); pBodyPath(c); c.strokeStyle = pal.ink; c.lineWidth = 2.7; c.stroke();
		stitchEdge(c, pal, pBodyPath, 6);
		/* 手臂 */
		if (!q.armsFront) {
			pLimb(c, pal, -20, -31, q.aLx, q.aLy, 13, pal.pCream);
			pLimb(c, pal, 20, -31, q.aRx, q.aRy, 13, pal.pCream);
		}
		c.restore();
		/* 头 */
		var neckY = -(41 * q.bsy) - 16;
		c.save();
		c.translate(q.bx, 0);
		c.translate(q.hx, neckY + q.hy);
		c.rotate(q.hrot);
		pHead(c, pal, q, t, o);
		pBow(c, pal, 0);
		c.restore();
		if (q.armsFront) {
			c.save();
			c.translate(q.bx, 0); c.scale(q.bsx, q.bsy);
			pLimb(c, pal, -20, -31, q.aLx, q.aLy, 13, pal.pCream);
			pLimb(c, pal, 20, -31, q.aRx, q.aRy, 13, pal.pCream);
			c.restore();
		}
		c.restore();
		var pvv = o.pivotY != null ? o.pivotY : q.pivotY;
		return {
			feet: xfPoint({ x: o.x, y: (o.y || 0) + lift * s * syy, s: s, sx: sxx, sy: syy, flip: o.flip, rot: rot }, pvv, 0, 0),
			head: xfPoint({ x: o.x, y: (o.y || 0) + lift * s * syy, s: s, sx: sxx, sy: syy, flip: o.flip, rot: rot }, pvv, q.bx, neckY + q.hy),
			top: xfPoint({ x: o.x, y: (o.y || 0) + lift * s * syy, s: s, sx: sxx, sy: syy, flip: o.flip, rot: rot }, pvv, q.bx, neckY + q.hy - 42),
			body: xfPoint({ x: o.x, y: (o.y || 0) + lift * s * syy, s: s, sx: sxx, sy: syy, flip: o.flip, rot: rot }, pvv, q.bx, -22)
		};
	}

	/* =====================================================================
	 *  粒子（对象池）
	 * ===================================================================== */
	var POOL_MAX = 768, DEF_LIST_MAX = 512;
	var pool = [], made = 0, dropped = 0, FX_DARK = false;
	function newParticle() {
		made++;
		return { kind: '', x: 0, y: 0, vx: 0, vy: 0, ax: 0, ay: 0, life: 1, age: 0, size: 6, rot: 0, vr: 0, col: '#fff', col2: '#fff',
			text: '', shape: '', alpha: 1, wl: null, wr: null, floor: 1e9, bounce: 0.4, drag: 0, seed: 0, grow: 0, r0: 0, r1: 0, w: 1, h: 1, delay: 0, sway: 0, tx: 0, ty: 0, n: 0 };
	}
	function acquire() { return pool.length ? pool.pop() : newParticle(); }
	function release(p) { if (pool.length < POOL_MAX) pool.push(p); else dropped++; }
	function rnd(a, b) { return a + Math.random() * (b - a); }

	var TEXT_COLS = ['#ffd93d', '#ff7a5c', '#fff', '#7be0ff', '#ff8fc0'];
	var CONF_COLS = ['#ff5d8f', '#ffd93d', '#4fd1c5', '#7aa2ff', '#ff9f43', '#b57aff', '#7be07a'];

	function initKind(p, kind, o) {
		var a, sp;
		p.kind = kind; p.alpha = 1; p.age = 0; p.delay = 0; p.rot = 0; p.vr = 0; p.drag = 0; p.ax = 0; p.ay = 0; p.floor = 1e9; p.bounce = 0.4; p.grow = 0;
		p.text = ''; p.shape = ''; p.sway = 0; p.wl = null; p.wr = null; p.seed = Math.random() * 100; p.vx = 0; p.vy = 0; p.n = 0; p.col2 = '#fff';
		switch (kind) {
		case 'heart':
			p.life = rnd(1.1, 1.5); p.size = rnd(9, 13); p.vx = rnd(-8, 8); p.vy = rnd(-36, -26); p.col = '#ff5d8f'; p.sway = rnd(5, 9); p.rot = rnd(-0.3, 0.3); break;
		case 'star':
			p.life = rnd(0.7, 1.0); p.size = rnd(6.5, 9.5); a = rnd(0, TAU); sp = rnd(30, 90); p.vx = cos(a) * sp; p.vy = sin(a) * sp - 30; p.ay = 160; p.vr = rnd(-8, 8); p.col = '#ffe14d'; p.drag = 1.5; break;
		case 'spark':
			p.life = rnd(0.25, 0.5); p.size = rnd(1.6, 2.6); a = rnd(0, TAU); sp = rnd(90, 230); p.vx = cos(a) * sp; p.vy = sin(a) * sp; p.ay = 360; p.col = '#ffb347'; p.col2 = '#fff6c8'; p.drag = 1.2; break;
		case 'dust':
			p.life = rnd(0.5, 0.85); p.size = rnd(5, 9); p.vx = rnd(-22, 22); p.vy = rnd(-26, -6); p.grow = rnd(12, 22); p.col = pal_dust(); p.drag = 2; break;
		case 'debris':
			p.life = rnd(1.2, 1.8); p.size = rnd(6, 12); a = rnd(PI * 1.1, PI * 1.9); sp = rnd(80, 220); p.vx = cos(a) * sp; p.vy = sin(a) * sp; p.ay = 620; p.vr = rnd(-12, 12); p.shape = 'chip'; p.bounce = 0.38; p.w = rnd(0.6, 1.2); p.h = rnd(0.5, 1.0); break;
		case 'ring':
			p.life = 0.45; p.size = 5; p.r0 = 4; p.r1 = rnd(34, 46); p.col = '#ffffff'; break;
		case 'text':
			p.life = 0.95; p.size = 26; p.vy = -22; p.rot = rnd(-0.22, 0.22); p.col = TEXT_COLS[floor(Math.random() * TEXT_COLS.length)]; p.text = '咚!'; break;
		case 'confetti':
			p.life = rnd(1.8, 2.6); p.size = rnd(4, 7); a = rnd(PI * 1.15, PI * 1.85); sp = rnd(120, 300); p.vx = cos(a) * sp; p.vy = sin(a) * sp; p.ay = 150; p.drag = 1.6; p.vr = rnd(-9, 9);
			p.col = CONF_COLS[floor(Math.random() * CONF_COLS.length)]; p.sway = rnd(5, 9); p.w = rnd(0.5, 1); break;
		case 'poof':
			p.life = 0.65; p.size = rnd(14, 20); p.col = '#fff'; p.grow = 1; break;
		case 'sweat':
			p.life = 0.8; p.size = rnd(3.5, 5); a = rnd(-PI * 0.9, -PI * 0.1); sp = rnd(30, 70); p.vx = cos(a) * sp; p.vy = sin(a) * sp; p.ay = 260; p.col = '#bfe8ff'; break;
		case 'note':
			p.life = rnd(1.2, 1.6); p.size = rnd(7, 9); p.vx = rnd(-10, 10); p.vy = rnd(-34, -24); p.sway = rnd(6, 10); p.col = ['#ff8fc0', '#7be0ff', '#ffd93d', '#9bf06a'][floor(Math.random() * 4)]; break;
		case 'glint':
			p.life = rnd(0.4, 0.6); p.size = rnd(5, 9); p.col = '#fffbe0'; p.vr = rnd(-3, 3); break;
		default:
			p.life = 0.6; p.size = 6; p.col = '#fff';
		}
		if (o) {
			var k;
			for (k in o) { if (o.hasOwnProperty(k) && k !== 'ang' && k !== 'spd' && k !== 'spread') p[k] = o[k]; }
			if (o.ang != null || o.spd != null) {
				a = (o.ang != null ? o.ang : rnd(0, TAU)) + (o.spread ? rnd(-o.spread, o.spread) : 0);
				sp = o.spd != null ? o.spd * rnd(0.7, 1.15) : sqrt(p.vx * p.vx + p.vy * p.vy);
				p.vx = cos(a) * sp; p.vy = sin(a) * sp;
			}
		}
		if (kind === 'ring' && o && o.size == null) p.size = 5;
		p.tx = p.x; p.ty = p.y;
	}
	function pal_dust() { return FX_DARK ? '#c9bfae' : '#b9a98e'; }

	function fxSpawn(list, kind, x, y, opt) {
		var max_ = list.max || DEF_LIST_MAX;
		if (list.length >= max_) return null;
		var p = acquire();
		p.x = x; p.y = y;
		initKind(p, kind, opt);
		p.x = x + (opt && opt.jx ? rnd(-opt.jx, opt.jx) : 0);
		p.y = y + (opt && opt.jy ? rnd(-opt.jy, opt.jy) : 0);
		list.push(p);
		return p;
	}
	function fxBurst(list, kind, x, y, n, opt) {
		var i;
		for (i = 0; i < n; i++) fxSpawn(list, kind, x, y, opt);
	}
	function fxStep(list, dt) {
		var i = 0, p, n = list.length, d;
		if (dt > 0.1) dt = 0.1;
		while (i < n) {
			p = list[i];
			if (p.delay > 0) { p.delay -= dt; i++; continue; }
			p.age += dt;
			if (p.age >= p.life) {
				n--; list[i] = list[n]; list.pop(); release(p);
				continue;
			}
			if (p.drag) { d = 1 - p.drag * dt; if (d < 0) d = 0; p.vx *= d; p.vy *= d; }
			p.vx += p.ax * dt; p.vy += p.ay * dt;
			p.x += p.vx * dt; p.y += p.vy * dt;
			p.rot += p.vr * dt;
			if (p.kind === 'debris' && p.wr != null) {   /* 侧栏左右边缘: 碰壁反弹并损失动能, 不再被画布硬裁 */
				if (p.x > p.wr) { p.x = p.wr; if (p.vx > 0) { p.vx = -p.vx * 0.4; p.vr *= 0.6; } }
				else if (p.x < p.wl) { p.x = p.wl; if (p.vx < 0) { p.vx = -p.vx * 0.4; p.vr *= 0.6; } }
			}
			if (p.kind === 'debris' && p.y > p.floor) {
				p.y = p.floor; p.vy = -p.vy * p.bounce; p.vx *= 0.7; p.vr *= 0.6;
				if (abs(p.vy) < 24) { p.vy = 0; p.ay = 0; p.vr *= 0.3; }
			}
			if (p.kind === 'confetti' && p.vy > 40) p.vy = 40 + (p.vy - 40) * (1 - 3 * dt);
			i++;
		}
		return list.length;
	}
	function fxClear(list) { var p; while ((p = list.pop())) release(p); }
	function fxStats() { return { made: made, pooled: pool.length, dropped: dropped, poolMax: POOL_MAX }; }

	/* ---------- 粒子绘制 ---------- */
	var FONT = '"Microsoft YaHei","PingFang SC","Noto Sans CJK SC","Hiragino Sans GB",sans-serif';
	function drawDebris(c, p, pal, a) {
		var s = p.size, sh = p.shape;
		c.lineJoin = 'round';
		if (sh === 'bar') {
			var L2 = s * 2.8, w = 5.4;
			c.beginPath(); c.moveTo(-w / 2, -L2 / 2); c.lineTo(w / 2, -L2 / 2 + 2); c.lineTo(w / 2 + 0.6, L2 / 2 - 3); c.lineTo(1, L2 / 2 + 1); c.lineTo(-1.8, L2 / 2 - 3.4); c.lineTo(-w / 2 - 0.4, L2 / 2 - 0.5); c.closePath();
			fs(c, pal.steel, pal.steelOut, 1.9);
			c.fillStyle = pal.steelHi; c.fillRect(-w / 2 + 0.7, -L2 / 2 + 2, 1.3, L2 - 6);
			c.fillStyle = pal.steelLo; c.fillRect(w / 2 - 1.8, -L2 / 2 + 3, 1.4, L2 - 7);
		} else if (sh === 'lock') {
			c.beginPath(); c.moveTo(-s * 0.9, -s * 0.8); c.lineTo(s * 0.8, -s * 0.9); c.lineTo(s * 0.55, -s * 0.1); c.lineTo(s * 0.95, s * 0.35); c.lineTo(s * 0.2, s * 0.9); c.lineTo(-s * 0.9, s * 0.7); c.closePath();
			var g = c.createLinearGradient(-s, -s, s, s); g.addColorStop(0, '#fff3b0'); g.addColorStop(0.4, '#ffd75a'); g.addColorStop(1, '#b36c0a');
			fs(c, g, '#5a3207', 2);
			c.fillStyle = 'rgba(255,255,255,0.55)'; c.fillRect(-s * 0.7, -s * 0.5, s * 0.25, s * 0.9);
		} else if (sh === 'base') {
			c.beginPath(); c.moveTo(-s, -s * 0.5); c.lineTo(s * 0.6, -s * 0.7); c.lineTo(s * 1.05, s * 0.2); c.lineTo(-s * 0.3, s * 0.65); c.lineTo(-s * 1.0, s * 0.3); c.closePath();
			fs(c, pal.steelLo, pal.steelOut, 2);
			c.fillStyle = pal.steelHi; c.fillRect(-s * 0.8, -s * 0.4, s * 1.2, 1.4);
			c.beginPath(); c.arc(s * 0.1, 0, 1.6, 0, TAU); c.fillStyle = pal.rivet; c.fill();
		} else if (sh === 'roof') {
			c.beginPath(); c.moveTo(-s * 1.2, -s * 0.5); c.lineTo(s * 0.4, -s * 0.8); c.lineTo(s * 1.3, s * 0.1); c.lineTo(s * 0.6, s * 0.8); c.lineTo(-s * 0.7, s * 0.55); c.closePath();
			fs(c, pal.steel, pal.steelOut, 2);
			c.fillStyle = pal.steelHi; c.fillRect(-s * 1.0, -s * 0.38, s * 1.4, 1.6);
		} else {
			c.beginPath(); c.moveTo(-s * 0.5, -s * 0.3); c.lineTo(s * 0.3, -s * 0.5); c.lineTo(s * 0.55, s * 0.15); c.lineTo(-s * 0.05, s * 0.5); c.closePath();
			fs(c, pal.steel, pal.steelOut, 1.7);
		}
	}
	function fxDraw(ctx, list, dark) {
		var c = ctx, i, p, u, a, pal = palette(dark != null ? dark : API.isDark), sc, k, j;
		FX_DARK = !!pal.dark;
		for (i = 0; i < list.length; i++) {
			p = list[i];
			if (p.delay > 0) continue;
			u = p.age / p.life;
			a = 1;
			c.save();
			switch (p.kind) {
			case 'heart':
				a = u > 0.7 ? (1 - u) / 0.3 : 1;
				sc = (u < 0.2 ? easeOutBack(u / 0.2) : 1) * p.size;
				c.globalAlpha = a;
				c.translate(p.x + sin(p.age * 5 + p.seed) * p.sway, p.y); c.rotate(p.rot + sin(p.age * 4 + p.seed) * 0.15);
				c.beginPath(); heartPath(c, 0, 0, sc * 0.82);
				c.fillStyle = (p.col === '#ff5d8f') ? '#ff5d8f' : p.col; c.fill(); c.strokeStyle = pal.ink; c.lineWidth = 1.7; c.lineJoin = 'round'; c.stroke();
				c.beginPath(); ell(c, -sc * 0.38, -sc * 0.36, sc * 0.2, sc * 0.13); c.fillStyle = 'rgba(255,255,255,0.85)'; c.fill();
				break;
			case 'star':
				a = u > 0.6 ? (1 - u) / 0.4 : 1;
				c.globalAlpha = a; c.translate(p.x, p.y);
				c.beginPath(); starPath(c, 0, 0, p.size * (1 - u * 0.3), p.rot, 0.5); fs(c, p.col, pal.ink, 1.6);
				break;
			case 'spark':
				a = 1 - u; c.globalAlpha = a;
				var vl = sqrt(p.vx * p.vx + p.vy * p.vy) || 1, tl = min(16, vl * 0.05 + 2);
				c.lineCap = 'round';
				c.beginPath(); c.moveTo(p.x, p.y); c.lineTo(p.x - p.vx / vl * tl, p.y - p.vy / vl * tl);
				c.strokeStyle = p.col; c.lineWidth = p.size * 1.8; c.stroke();
				c.strokeStyle = p.col2; c.lineWidth = p.size * 0.8; c.stroke();
				break;
			case 'dust':
				a = (1 - u) * 0.8; c.globalAlpha = a;
				var r = p.size + p.grow * p.age;
				c.translate(p.x, p.y);
				c.fillStyle = p.col;
				c.beginPath(); c.arc(0, 0, r, 0, TAU); c.arc(-r * 0.7, r * 0.18, r * 0.66, 0, TAU); c.arc(r * 0.72, r * 0.2, r * 0.62, 0, TAU); c.fill();
				c.fillStyle = 'rgba(255,255,255,0.28)'; c.beginPath(); c.arc(-r * 0.22, -r * 0.26, r * 0.5, 0, TAU); c.fill();
				break;
			case 'debris':
				a = u > 0.78 ? (1 - u) / 0.22 : 1;
				if (p.wr != null) { var ed = min(p.x - p.wl, p.wr - p.x); if (ed < 14) a *= 0.55 + 0.45 * clamp(ed / 14, 0, 1); }   /* 贴边时略淡, 与反弹配合 */
				c.globalAlpha = a;
				c.translate(p.x, p.y); c.rotate(p.rot);
				drawDebris(c, p, pal, a);
				break;
			case 'ring':
				k = 1 - pow(1 - u, 3);
				var rr = lerp(p.r0, p.r1, k);
				c.globalAlpha = (1 - u) * 0.95;
				c.translate(p.x, p.y);
				c.beginPath(); c.arc(0, 0, rr, 0, TAU);
				c.strokeStyle = p.col; c.lineWidth = max(0.6, p.size * (1 - u)); c.stroke();
				c.beginPath(); c.arc(0, 0, rr * 0.72, 0, TAU);
				c.strokeStyle = pal.dark ? 'rgba(255,255,255,0.45)' : 'rgba(255,255,255,0.8)'; c.lineWidth = max(0.4, p.size * 0.45 * (1 - u)); c.stroke();
				break;
			case 'text':
				var pop = u < 0.2 ? easeOutBack(u / 0.2) : 1;
				a = u > 0.72 ? (1 - u) / 0.28 : 1;
				c.globalAlpha = a;
				c.translate(p.x, p.y); c.rotate(p.rot); c.scale(pop * (1 + (u < 0.2 ? 0.25 * (1 - u / 0.2) : 0)), pop);
				c.font = 'italic 900 ' + p.size + 'px ' + FONT;
				c.textAlign = 'center'; c.textBaseline = 'middle'; c.lineJoin = 'round';
				c.lineWidth = p.size * 0.26; c.strokeStyle = pal.ink; c.strokeText(p.text, 0, 1);
				c.lineWidth = p.size * 0.12; c.strokeStyle = '#ffffff'; c.strokeText(p.text, 0, 0);
				c.fillStyle = p.col; c.fillText(p.text, 0, 0);
				break;
			case 'confetti':
				a = u > 0.8 ? (1 - u) / 0.2 : 1; c.globalAlpha = a;
				c.translate(p.x + sin(p.age * 4 + p.seed) * p.sway, p.y); c.rotate(p.rot);
				c.scale(1, cos(p.age * 9 + p.seed));
				c.fillStyle = p.col; c.fillRect(-p.size * 0.6, -p.size * 0.35 * p.w - 1, p.size * 1.2, p.size * 0.7 * p.w + 2);
				c.strokeStyle = 'rgba(60,30,20,0.55)'; c.lineWidth = 0.8; c.strokeRect(-p.size * 0.6, -p.size * 0.35 * p.w - 1, p.size * 1.2, p.size * 0.7 * p.w + 2);
				break;
			case 'poof':
				k = 1 - pow(1 - u, 2);
				a = u > 0.5 ? (1 - u) / 0.5 : 1; c.globalAlpha = a * 0.95;
				c.translate(p.x, p.y);
				for (j = 0; j < 7; j++) {
					var ja = j * TAU / 7 + p.seed, jr = p.size * (0.4 + 0.9 * k), px = cos(ja) * jr, py = sin(ja) * jr * 0.8, pr = p.size * (0.45 + 0.35 * k);
					c.beginPath(); c.arc(px, py, pr, 0, TAU); c.fillStyle = pal.dark ? '#f3f1ee' : '#ffffff'; c.fill();
					c.strokeStyle = 'rgba(80,60,60,0.38)'; c.lineWidth = 1.5; c.stroke();
				}
				c.beginPath(); c.arc(0, 0, p.size * (0.7 + 0.4 * k), 0, TAU); c.fillStyle = '#ffffff'; c.fill();
				for (j = 0; j < 4; j++) {
					var ga = j * 1.7 + p.seed, gr = p.size * (1.1 + 1.3 * k);
					c.beginPath(); glintPath(c, cos(ga) * gr, sin(ga) * gr, 4.2 * (1 - u)); c.fillStyle = '#ffe97a'; c.fill();
				}
				break;
			case 'sweat':
				a = 1 - u * u; c.globalAlpha = a;
				c.translate(p.x, p.y); c.rotate(atan2(p.vy, p.vx) + PI / 2);
				c.beginPath(); c.moveTo(0, -p.size * 1.5); c.bezierCurveTo(p.size, -p.size * 0.2, p.size * 1.1, p.size * 0.5, p.size * 1.1, p.size * 0.7); c.bezierCurveTo(p.size * 1.1, p.size * 1.8, -p.size * 1.1, p.size * 1.8, -p.size * 1.1, p.size * 0.7); c.bezierCurveTo(-p.size * 1.1, p.size * 0.5, -p.size, -p.size * 0.2, 0, -p.size * 1.5);
				fs(c, p.col, 'rgba(50,100,150,0.9)', 1.3);
				break;
			case 'note':
				a = u > 0.7 ? (1 - u) / 0.3 : 1; c.globalAlpha = a;
				c.translate(p.x + sin(p.age * 4 + p.seed) * p.sway, p.y); c.rotate(sin(p.age * 3 + p.seed) * 0.25);
				sc = (u < 0.15 ? easeOutBack(u / 0.15) : 1) * p.size / 8;
				c.scale(sc, sc);
				c.beginPath(); ell(c, 0, 4, 4.4, 3.2); c.fillStyle = p.col; c.fill(); c.strokeStyle = pal.ink; c.lineWidth = 1.6; c.stroke();
				c.beginPath(); c.moveTo(3.6, 3.6); c.lineTo(3.6, -9); c.quadraticCurveTo(7, -7, 7.6, -3.4);
				c.strokeStyle = pal.ink; c.lineWidth = 2.4; c.stroke(); c.strokeStyle = p.col; c.lineWidth = 1.1; c.stroke();
				break;
			case 'glint':
				a = sin(u * PI); c.globalAlpha = a;
				c.translate(p.x, p.y); c.rotate(p.rot * p.age);
				c.beginPath(); glintPath(c, 0, 0, p.size * (0.6 + 0.6 * a)); c.fillStyle = p.col; c.fill();
				c.strokeStyle = 'rgba(255,170,60,0.8)'; c.lineWidth = 0.9; c.stroke();
				break;
			default:
				c.globalAlpha = 1 - u; c.beginPath(); c.arc(p.x, p.y, p.size, 0, TAU); c.fillStyle = p.col; c.fill();
			}
			c.restore();
		}
	}

	/* =====================================================================
	 *  锤子（光标 / 触屏图标）
	 * ===================================================================== */
	function hammerInfo() {
		/* 64×64 视窗；锤头在左上，锤柄指向右下；返回 svg 与热点（锤击面中心） */
		var rotA = -42, cx = 32, cy = 32, ra = rotA * PI / 180;
		var hx = 9.5, hy = 17.5; /* 锤击面在未旋转坐标里的中心 */
		var dx = hx - cx, dy = hy - cy;
		var X = cx + dx * cos(ra) - dy * sin(ra), Y = cy + dx * sin(ra) + dy * cos(ra);
		return { rot: rotA, hotX: X, hotY: Y };
	}
	function hammerSVG(size) {
		size = size || 32;
		var inf = hammerInfo(), id = 'mh' + Math.round(size);
		return '<svg xmlns=\'http://www.w3.org/2000/svg\' width=\'' + size + '\' height=\'' + size + '\' viewBox=\'0 0 64 64\'>' +
			'<defs>' +
			'<linearGradient id=\'' + id + 's\' x1=\'0\' y1=\'0\' x2=\'0\' y2=\'1\'><stop offset=\'0\' stop-color=\'#f7fbff\'/><stop offset=\'.28\' stop-color=\'#c3d2e6\'/><stop offset=\'.62\' stop-color=\'#7f93b0\'/><stop offset=\'1\' stop-color=\'#4d5f7c\'/></linearGradient>' +
			'<linearGradient id=\'' + id + 'w\' x1=\'0\' y1=\'0\' x2=\'1\' y2=\'0\'><stop offset=\'0\' stop-color=\'#f0bf7a\'/><stop offset=\'.45\' stop-color=\'#d1924a\'/><stop offset=\'1\' stop-color=\'#8d5724\'/></linearGradient>' +
			'<linearGradient id=\'' + id + 'g\' x1=\'0\' y1=\'0\' x2=\'0\' y2=\'1\'><stop offset=\'0\' stop-color=\'#fff3b0\'/><stop offset=\'.5\' stop-color=\'#ffc93a\'/><stop offset=\'1\' stop-color=\'#c27a0c\'/></linearGradient>' +
			'<linearGradient id=\'' + id + 'r\' x1=\'0\' y1=\'0\' x2=\'0\' y2=\'1\'><stop offset=\'0\' stop-color=\'#ff8a7a\'/><stop offset=\'1\' stop-color=\'#d63b3b\'/></linearGradient>' +
			'</defs>' +
			/* 投影 */
			'<g transform=\'rotate(' + inf.rot + ' 32 32)\'>' +
			'<ellipse cx=\'36\' cy=\'61\' rx=\'9\' ry=\'2\' fill=\'#000\' opacity=\'.18\'/>' +
			/* 锤柄 */
			'<path d=\'M29.2 24 L34.8 24 L35.6 57.6 Q32 61.4 28.4 57.6 Z\' fill=\'url(#' + id + 'w)\' stroke=\'#4a2a1f\' stroke-width=\'2.1\' stroke-linejoin=\'round\'/>' +
			'<path d=\'M31 26 L31.4 56\' stroke=\'#fff\' stroke-opacity=\'.5\' stroke-width=\'1.3\' stroke-linecap=\'round\'/>' +
			'<path d=\'M33.2 30 L33.4 40 M32.6 46 L33 54\' stroke=\'#7a4a1c\' stroke-opacity=\'.55\' stroke-width=\'.9\' stroke-linecap=\'round\'/>' +
			/* 握把红带 */
			'<path d=\'M28.8 43 L35.2 43 L35.4 46.4 L28.7 46.4 Z M28.6 49.2 L35.4 49.2 L35.5 52.6 L28.5 52.6 Z\' fill=\'url(#' + id + 'r)\' stroke=\'#4a2a1f\' stroke-width=\'1.6\' stroke-linejoin=\'round\'/>' +
			/* 锤头：圆柱锤，左面为击打面 */
			'<rect x=\'8.5\' y=\'10\' width=\'47\' height=\'15\' rx=\'4.2\' fill=\'url(#' + id + 's)\' stroke=\'#2f3e55\' stroke-width=\'2.3\'/>' +
			'<rect x=\'8.5\' y=\'10\' width=\'8.4\' height=\'15\' rx=\'3.8\' fill=\'url(#' + id + 'g)\' stroke=\'#5a3207\' stroke-width=\'2\'/>' +
			'<rect x=\'47.6\' y=\'10\' width=\'7.9\' height=\'15\' rx=\'3.8\' fill=\'url(#' + id + 'g)\' stroke=\'#5a3207\' stroke-width=\'2\'/>' +
			'<path d=\'M20 13.3 H44\' stroke=\'#fff\' stroke-opacity=\'.85\' stroke-width=\'1.9\' stroke-linecap=\'round\'/>' +
			'<path d=\'M20 22 H44\' stroke=\'#2f3e55\' stroke-opacity=\'.35\' stroke-width=\'1.6\' stroke-linecap=\'round\'/>' +
			'<circle cx=\'12.4\' cy=\'14.2\' r=\'1.1\' fill=\'#fff\' fill-opacity=\'.85\'/>' +
			/* 锤头星标 */
			'<path d=\'M32 13.2 l1.5 3.1 3.4.4 -2.5 2.3 .7 3.4 -3.1 -1.7 -3.1 1.7 .7 -3.4 -2.5 -2.3 3.4 -.4z\' fill=\'#ffd93d\' stroke=\'#7a4a0c\' stroke-width=\'.9\' stroke-linejoin=\'round\'/>' +
			'</g>' +
			/* 小火花 */
			'<path d=\'M5 6 l1.2 2.6 2.6 1.2 -2.6 1.2 -1.2 2.6 -1.2 -2.6 -2.6 -1.2 2.6 -1.2z\' fill=\'#fff6b0\' stroke=\'#e09a10\' stroke-width=\'.8\' stroke-linejoin=\'round\'/>' +
			'</svg>';
	}
	function hammerCursor(size) {
		size = size || 32;
		var inf = hammerInfo(), k = size / 64;
		var hx = Math.round(inf.hotX * k), hy = Math.round(inf.hotY * k);
		return 'url("data:image/svg+xml,' + encodeURIComponent(hammerSVG(size)) + '") ' + hx + ' ' + hy + ', pointer';
	}

	/* =====================================================================
	 *  导出
	 * ===================================================================== */
	API.palette = palette;
	API.setDark = function (b) { API.isDark = !!b; FX_DARK = !!b; };
	API.drawCatV1 = drawCat;
	API.catAnchorsV1 = catAnchors;
	API.CAT_POSES_V1 = CAT_POSES;
	var V2 = root.MwrtCatV2, PV2 = root.MwrtPlushV2;
	API.drawCat = V2 ? function (ctx, o) { try { return V2.draw(ctx, o); } catch (e) { return drawCat(ctx, o); } } : drawCat;
	API.catAnchors = V2 ? function (o, ctx) { try { return V2.anchors(o, ctx); } catch (e) { return catAnchors(o); } } : catAnchors;
	API.CAT_POSES = V2 ? V2.poses : CAT_POSES;
	API.catBounds = V2 ? V2.bounds : null;
	API.drawCage = drawCage;
	API.drawCageBack = drawCageBack;
	API.drawCageFront = drawCageFront;
	API.cageStage = cageStage;
	API.cageRect = cageRect;
	API.cageFloor = cageFloor;
	API.cagePoint = cagePoint;
	API.barPoint = barPoint;
	API.barX = barX;
	API.CAGE = CAGE;
	API.drawPlushV1 = drawPlush;
	API.PLUSH_POSES_V1 = PLUSH_POSES;
	API.drawPlush = PV2 ? function (ctx, o) { try { return PV2.draw(ctx, o); } catch (e) { return drawPlush(ctx, o); } } : drawPlush;
	API.PLUSH_POSES = PV2 ? PV2.poses : PLUSH_POSES;
	API.plushBounds = PV2 ? PV2.bounds : null;
	API.plushHopLift = PV2 ? PV2.hopLift : null;
	API.PLUSH_SIZE = PV2 ? { w: PV2.WIDTH, h: PV2.HEIGHT } : { w: 72, h: 100 };
	API.fx = { spawn: fxSpawn, burst: fxBurst, step: fxStep, draw: fxDraw, clear: fxClear, stats: fxStats };
	API.hammerCursor = hammerCursor;
	API.hammerSVG = hammerSVG;
	API.version = 3;

	root.MwrtCatArt = API;
	if (typeof module !== 'undefined' && module.exports) module.exports = API;
})(typeof window !== 'undefined' ? window : this);
