/* mwrt-topo.js —— 网络拓扑全屏覆盖层 (s90 / B1)
   入口: window.MwrtTopo = {open(), close(), isOpen()}。首页「网络拓扑」磁贴点击时才懒加载本文件。
   数据: GET admin/modem/topo_data (JSON, 见 _s90/CONTRACT.md), 打开时拉一次, 之后每 3 秒一次; 页面隐藏 / 覆盖层关闭立即停, 在途回包用代数作废。
   美术: window.MwrtTopoArt (mwrt-topo-art.js, B2 线程)。没有它或缺工厂时, 用本文件 FB 区里同名同签名的备用图元顶上。
   ES5 风格, 无依赖; 类名前缀 .mt-, id 前缀 mt-。所有异常在入口处吞掉, 不影响首页。 */
(function () {
'use strict';
if (window.MwrtTopo && window.MwrtTopo.__mt) return;

var NS = 'http://www.w3.org/2000/svg';
var POLL_MS = 3000;

/* ======================================================================
   一、通用小工具
   ====================================================================== */
function $(id) { return document.getElementById(id); }
function esc(s) {
  return String(s == null ? '' : s).replace(/[&<>"']/g, function (c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
  });
}
function num(v) { return (typeof v === 'number' && isFinite(v)) ? v : null; }
function has(v) { return v !== null && v !== undefined && v !== ''; }
function dash(v, u) { return has(v) ? (u ? v + ' ' + u : String(v)) : '—'; }
function clamp01(x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
function closest(el, cls) {
  while (el && el.nodeType === 1) {
    var c = el.getAttribute && el.getAttribute('class');
    if (c && (' ' + c + ' ').indexOf(' ' + cls + ' ') >= 0) return el;
    el = el.parentNode;
  }
  return null;
}
function fmtRate(bps) {
  var b = num(bps);
  if (b == null) return '—';
  if (b >= 1e9) return (b / 1e9).toFixed(2) + ' Gbps';
  if (b >= 1e6) return (b / 1e6).toFixed(b >= 1e7 ? 1 : 2) + ' Mbps';
  if (b >= 1e3) return Math.round(b / 1e3) + ' Kbps';
  return Math.round(b) + ' bps';
}
function fmtBytes(n) {
  n = num(n);
  if (n == null) return '—';
  var u = ['B', 'KB', 'MB', 'GB', 'TB'], i = 0;
  while (n >= 1024 && i < u.length - 1) { n /= 1024; i++; }
  return (i ? n.toFixed(n >= 100 ? 0 : (n >= 10 ? 1 : 2)) : Math.round(n)) + ' ' + u[i];
}
function fmtDur(s) {
  s = num(s);
  if (s == null) return '—';
  s = Math.floor(s);
  var d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
  if (d) return d + ' 天 ' + h + ' 小时';
  if (h) return h + ' 小时 ' + m + ' 分';
  if (m) return m + ' 分 ' + (s % 60) + ' 秒';
  return s + ' 秒';
}
function fmtMbps(m) { m = num(m); return m == null ? '—' : (m >= 1000 ? (m / 1000).toFixed(m % 1000 ? 2 : 0) + ' Gbps' : Math.round(m) + ' Mbps'); }
/* 去掉双向覆盖/隔离控制符(如 U+202E 会让整段文字视觉反序), 来自 DHCP 主机名、SSID 等不可信字符串 */
function scrub(v) {
  if (typeof v === 'string') return v.replace(/[\u200e\u200f\u061c\u202a-\u202e\u2066-\u2069]/g, '');
  if (v && typeof v === 'object') {
    for (var k in v) if (Object.prototype.hasOwnProperty.call(v, k)) v[k] = scrub(v[k]);
  }
  return v;
}
/* CSS 属性选择器里的字符串值转义(值来自端口名、MAC 等) */
function qa(v) { return String(v == null ? '' : v).replace(/[\x00-\x1f]/g, '').replace(/(["\\])/g, '\\$1'); }
function cut(s, n) {   /* 汉字按 2 宽截断 */
  s = String(s == null ? '' : s);
  var w = 0, o = '';
  for (var i = 0; i < s.length; i++) {
    var c = s.charCodeAt(i) > 255 ? 2 : 1;
    if (w + c > n) return o + '…';
    w += c; o += s.charAt(i);
  }
  return o;
}

/* ---- 信号质量: 0..1 (越大越好) ---- */
function qRsrp(v) { return v == null ? null : clamp01((v + 130) / 55); }
function qRsrq(v) { return v == null ? null : clamp01((v + 20) / 15); }
function qSinr(v) { return v == null ? null : clamp01((v + 5) / 30); }
function qRssi(v) { return v == null ? null : clamp01((v + 95) / 50); }
function qSnr(v) { return v == null ? null : clamp01(v / 40); }
function tone(q) { return q == null ? 'na' : (q >= 0.62 ? 'good' : (q >= 0.34 ? 'mid' : 'bad')); }
function barsOf(q) { return q == null ? 0 : (q >= 0.8 ? 4 : (q >= 0.55 ? 3 : (q >= 0.3 ? 2 : 1))); }

/* ---- 运营商 ---- */
var CARR = {
  cmcc: { id: 'cmcc', name: '中国移动', color: '#19b6a0' },
  cu: { id: 'cu', name: '中国联通', color: '#ef4444' },
  ct: { id: 'ct', name: '中国电信', color: '#3b82f6' },
  cbn: { id: 'cbn', name: '中国广电', color: '#f59e0b' },
  other: { id: 'other', name: '运营商', color: '#8b9ac0' }
};
function carrierOf(cell) {
  if (!cell) return CARR.other;
  var p = String(cell.plmn || ''), n = String(cell.operator || '');
  if (/移动|CMCC|MOBILE/i.test(n) || /^460(00|02|04|07|08)$/.test(p)) return CARR.cmcc;
  if (/联通|UNICOM/i.test(n) || /^460(01|06|09)$/.test(p)) return CARR.cu;
  if (/电信|TELECOM|CHINANET/i.test(n) || /^460(03|05|11)$/.test(p)) return CARR.ct;
  if (/广电|BROADCAST|CBN/i.test(n) || /^46015$/.test(p)) return CARR.cbn;
  return CARR.other;
}

/* ---- 机型 ---- */
var MODELS = {
  'c5800-688': 'NRadio C5800-688', n5000: 'NRadio N5000', e87n: 'EdgePi E87N', max: 'C2000 MAX', ultra: 'C2000 Ultra', h5000m: 'H5000M'
};
function modelPng(m) { m = String(m || '').toLowerCase(); return MODELS[m] ? m : 'n5000'; }
function modelName(m) { m = String(m || '').toLowerCase(); return MODELS[m] || (m ? m.toUpperCase() : '本设备'); }

/* ---- 终端图标粗分 ---- */
function devKind(c) {
  var s = ((c.name || '') + ' ' + (c.vendor || '')).toLowerCase();
  if (/(^|[^a-z])tv([^a-z]|$)|电视|bravia|roku|chromecast|appletv|apple-tv|firetv/.test(s)) return 'tv';
  if (/printer|打印|laserjet|epson|canon-|brother/.test(s)) return 'printer';
  if (/camera|cam[-_ ]|摄像|ipc|hikvision|dahua|ezviz|萤石|imou/.test(s)) return 'camera';
  if (/nintendo|switch|xbox|playstation|ps[345]|steamdeck|steam-deck|游戏机/.test(s)) return 'console';
  if (/nas|synology|qnap|diskstation|群晖|威联通/.test(s)) return 'nas';
  if (/echo|alexa|homepod|xiaoai|小爱|sonos|speaker|音箱|天猫精灵|小度/.test(s)) return 'speaker';
  if (/ipad|tablet|[-_ ]pad|平板|matepad|mipad|tab[-_ ]?[a-z0-9]/.test(s)) return 'tablet';
  if (/macbook|laptop|notebook|thinkpad|book|legion|xps|surface|笔记本|zenbook|latitude/.test(s)) return 'laptop';
  if (/desktop|[-_ ]pc|^pc|workstation|imac|server|win(dows)?[-_ ]?\d|主机|台式|mini[-_ ]?pc|mac-?mini|orangepi|raspberry|linux/.test(s)) return 'pc';
  if (/iphone|android|phone|redmi|xiaomi|mi[-_ ]?\d|honor|荣耀|oppo|vivo|oneplus|pixel|galaxy|huawei|mate|nova|realme|iqoo|meizu|手机|samsung/.test(s)) return 'phone';
  if (/esp|tuya|涂鸦|iot|plug|bulb|sensor|lamp|shelly|zigbee|gateway|智能|yeelight|aqara|midea|美的|gree|格力|haier|海尔|robot|扫地|roborock|ecovacs|espressif/.test(s)) return 'iot';
  return c.type === 'wifi' ? 'phone' : 'unknown';
}

/* ---- 在线设备排序(稳定: 按 IP 数值, 再按 MAC) ---- */
function ipKey(ip) {
  var m = String(ip || '').match(/^(\d+)\.(\d+)\.(\d+)\.(\d+)$/);
  return m ? ((+m[1]) * 16777216 + (+m[2]) * 65536 + (+m[3]) * 256 + (+m[4])) : 1e12;
}
function sortClients(a) {
  return a.slice().sort(function (x, y) {
    var d = ipKey(x.ip) - ipKey(y.ip);
    if (d) return d;
    return String(x.mac) < String(y.mac) ? -1 : 1;
  });
}
function clientTitle(c) { return c.name || c.vendor || (c.type === 'wifi' ? 'WiFi 设备' : '有线设备'); }

/* 出口类型 */
var KINDS = {
  cell: { t: '蜂窝网络', c: '#2dd4bf' },
  wifi: { t: 'WiFi 桥接', c: '#60a5fa' },
  wired: { t: '有线宽带', c: '#34d399' },
  usbnic: { t: 'USB 网卡', c: '#a78bfa' },
  tether: { t: '手机共享', c: '#fbbf24' },
  none: { t: '未连接', c: '#f87171' }
};
function isPrivIp(ip) { return /^(10\.|192\.168\.|172\.(1[6-9]|2\d|3[01])\.|100\.(6[4-9]|[7-9]\d|1[01]\d|12[0-7])\.)/.test(String(ip || '')); }
function usbSpeedText(v) {
  v = String(v == null ? '' : v);
  if (!v) return '—';
  var n = parseFloat(v);
  if (n >= 10000) return 'USB 3.2 · 10 Gbps';
  if (n >= 5000) return 'USB 3.0 · 5 Gbps';
  if (n >= 480) return 'USB 2.0 · 480 Mbps';
  if (n >= 12) return 'USB 1.1 · ' + n + ' Mbps';
  return v;
}
function ratText(cell) { return cell && cell.rat ? cell.rat : ''; }

/* ======================================================================
   二、备用图元 FB (与 window.MwrtTopoArt 同名同签名; 检测到真的就优先用真的)
   工厂统一返回 {svg:'<g>…</g>', w, h, anchors:{名:{x,y}}}, 坐标系为自己的 0..w × 0..h。
   ====================================================================== */
var FB = (function () {
  var C = { cmcc: '#19b6a0', cu: '#ef4444', ct: '#3b82f6', cbn: '#f59e0b', other: '#8b9ac0' };
  function R(w, h, svg, anchors) { return { svg: '<g>' + svg + '</g>', w: w, h: h, anchors: anchors || {} }; }
  function f1(n) { return Math.round(n * 10) / 10; }

  var css =
    '.ta-b{fill:url(#ta-gb);stroke:var(--ta-line);stroke-width:1.6;stroke-linejoin:round}' +
    '.ta-b2{fill:var(--ta-b2);stroke:var(--ta-line);stroke-width:1.4;stroke-linejoin:round}' +
    '.ta-ln{fill:none;stroke:var(--ta-line);stroke-width:1.6;stroke-linecap:round;stroke-linejoin:round}' +
    '.ta-ik{fill:var(--ta-ink)}.ta-dm{fill:var(--ta-dim)}' +
    '.ta-sk{fill:none;stroke:var(--ta-ink);stroke-width:2;stroke-linecap:round;stroke-linejoin:round}' +
    '.ta-led{animation:ta-blink 1.6s ease-in-out infinite}' +
    '.ta-flow{fill:none;stroke-linecap:round;stroke-dasharray:2 14;animation:ta-flow 1.2s linear infinite}' +
    '.ta-flow.r{animation-direction:reverse}.ta-flow.s{stroke-dasharray:2 14;animation-duration:.8s}' +
    '.ta-wv{fill:none;stroke-linecap:round;animation:ta-wave 1.8s ease-in-out infinite}' +
    '.ta-spin{transform-box:fill-box;transform-origin:50% 50%;animation:ta-spin 6s linear infinite}' +
    '@keyframes ta-flow{to{stroke-dashoffset:-16}}' +
    '@keyframes ta-blink{0%,100%{opacity:1}50%{opacity:.25}}' +
    '@keyframes ta-wave{0%{opacity:0}30%{opacity:1}100%{opacity:0}}' +
    '@keyframes ta-spin{to{transform:rotate(360deg)}}';

  function defs() {
    return '<linearGradient id="ta-gb" x1="0" y1="0" x2="0" y2="1"><stop offset="0" style="stop-color:var(--ta-b1)"/><stop offset="1" style="stop-color:var(--ta-b2)"/></linearGradient>' +
      '<radialGradient id="ta-gl"><stop offset="0" style="stop-color:var(--ta-glow);stop-opacity:.55"/><stop offset="1" style="stop-color:var(--ta-glow);stop-opacity:0"/></radialGradient>' +
      '<filter id="ta-sh" x="-20%" y="-20%" width="140%" height="150%"><feDropShadow dx="0" dy="5" stdDeviation="5" flood-color="#000" flood-opacity=".28"/></filter>';
  }

  function router(model, o) {
    var m = String(model || '').toLowerCase();
    if (!/^(n5000|e87n|max|ultra|h5000m)$/.test(m)) m = 'n5000';
    var w = 240, ih = m === 'max' ? 160 : 186;
    var h = ih + 14;
    return R(w, h,
      '<ellipse cx="120" cy="' + (ih - 2) + '" rx="108" ry="14" fill="url(#ta-gl)"/>' +
      '<ellipse cx="120" cy="' + (ih - 2) + '" rx="76" ry="7" fill="#000" opacity=".22"/>' +
      '<image href="/luci-static/router3d/' + m + '.png" x="0" y="0" width="' + w + '" height="' + ih + '" preserveAspectRatio="xMidYMid meet"/>',
      { wan: { x: 56, y: ih - 24 }, lan: { x: 120, y: ih - 4 }, usb: { x: 196, y: ih * 0.62 }, ant: { x: 120, y: 14 }, top: { x: 120, y: 0 }, bottom: { x: 120, y: ih } });
  }

  function module5g(o) {
    /* 板子: USB 金手指在左(朝本设备), U.FL 射频线在右(朝基站) */
    var t = esc((o && o.model) || '5G 模组');
    var s = '<g>';
    s += '<g filter="url(#ta-sh)"><rect x="14" y="14" width="118" height="62" rx="7" class="ta-b"/>' +
      '<rect x="22" y="22" width="60" height="46" rx="4" class="ta-b2"/>' +
      '<path d="M30 30h44M30 38h44M30 46h30" class="ta-ln" opacity=".5"/>' +
      '<rect x="88" y="26" width="30" height="30" rx="3" class="ta-b2"/><circle cx="96" cy="34" r="1.8" class="ta-ik"/></g>';
    for (var i = 0; i < 9; i++) s += '<rect x="8" y="' + (20 + i * 5.4) + '" width="7" height="3.4" rx="1" fill="#e5b84a"/>';
    s += '<circle cx="128" cy="22" r="3.2" fill="#e5b84a"/><circle cx="128" cy="36" r="3.2" fill="#e5b84a"/>' +
      '<path d="M128 22C140 22 142 14 150 14M128 36C142 36 144 36 150 36" class="ta-ln"/></g>' +
      '<text x="52" y="72" text-anchor="middle" font-size="7.5" font-weight="700" class="ta-ik" font-family="ui-monospace,Menlo,Consolas,monospace">' + t.slice(0, 12) + '</text>' +
      '<circle cx="122" cy="68" r="2.4" fill="#34d399" class="ta-led"/>';
    return R(156, 92, s, { usb: { x: 8, y: 46 }, rf: { x: 150, y: 25 } });
  }

  function tower(o) {
    var col = C[(o && o.carrier) || 'other'] || C.other;
    var s = '<ellipse cx="55" cy="160" rx="46" ry="8" fill="url(#ta-gl)"/>' +
      '<path d="M55 30L22 156M55 30L88 156M38 92H72M30 122H80M46 62H64" class="ta-ln" stroke-width="2"/>' +
      '<path d="M46 62L72 92L30 122L80 156M64 62L38 92L80 122L30 156" class="ta-ln" opacity=".5"/>' +
      '<rect x="38" y="150" width="34" height="16" rx="3" class="ta-b"/>' +
      '<g fill="' + col + '" stroke="var(--ta-line)" stroke-width="1.2"><rect x="30" y="14" width="10" height="26" rx="3" transform="rotate(-14 35 27)"/><rect x="50" y="10" width="10" height="26" rx="3"/><rect x="70" y="14" width="10" height="26" rx="3" transform="rotate(14 75 27)"/></g>' +
      '<g class="ta-wv" stroke="' + col + '" stroke-width="2"><path d="M88 14q8 10 0 20"/></g><g class="ta-wv" stroke="' + col + '" stroke-width="2" style="animation-delay:.5s"><path d="M96 8q12 16 0 32"/></g>';
    return R(110, 170, s, { air: { x: 55, y: 24 }, back: { x: 55, y: 164 } });
  }

  function cloud(o) {
    var s = '<ellipse cx="82" cy="92" rx="62" ry="6" fill="url(#ta-gl)"/>' +
      '<path d="M40 82C18 82 8 66 16 52C22 40 36 38 44 42C48 22 68 12 88 18C102 8 128 14 132 36C150 36 160 54 150 68C144 78 134 82 124 82Z" class="ta-b" filter="url(#ta-sh)"/>' +
      '<g class="ta-ln" opacity=".75"><circle cx="86" cy="52" r="20"/><ellipse cx="86" cy="52" rx="9" ry="20"/><path d="M66 52h40M70 42h32M70 62h32"/></g>';
    return R(164, 100, s, { 'in': { x: 82, y: 82 }, left: { x: 10, y: 56 }, right: { x: 156, y: 56 } });
  }

  function antenna(x, y, rot) { return '<rect x="' + (x - 2.5) + '" y="' + (y - 34) + '" width="5" height="34" rx="2.5" class="ta-b2" transform="rotate(' + rot + ' ' + x + ' ' + y + ')"/>'; }
  function upRouter(o) {
    var s = '<ellipse cx="70" cy="96" rx="58" ry="6" fill="url(#ta-gl)"/>' + antenna(26, 40, -14) + antenna(114, 40, 14) + antenna(44, 40, -4) + antenna(96, 40, 4) +
      '<rect x="10" y="40" width="120" height="44" rx="10" class="ta-b" filter="url(#ta-sh)"/>' +
      '<circle cx="28" cy="62" r="3" fill="#34d399" class="ta-led"/><circle cx="40" cy="62" r="3" fill="#60a5fa" class="ta-led" style="animation-delay:.4s"/><circle cx="52" cy="62" r="3" fill="#fbbf24" class="ta-led" style="animation-delay:.8s"/>' +
      '<rect x="88" y="56" width="30" height="12" rx="3" class="ta-b2"/>';
    return R(140, 100, s, { ant: { x: 70, y: 6 }, lan: { x: 70, y: 84 }, left: { x: 10, y: 62 }, right: { x: 130, y: 62 } });
  }
  function onu(o) {
    var s = '<ellipse cx="70" cy="84" rx="56" ry="6" fill="url(#ta-gl)"/>' +
      '<rect x="14" y="20" width="112" height="58" rx="10" class="ta-b" filter="url(#ta-sh)"/>' +
      '<path d="M26 34h46" class="ta-ln" opacity=".5"/>' +
      '<circle cx="30" cy="56" r="3" fill="#34d399" class="ta-led"/><circle cx="42" cy="56" r="3" fill="#34d399" class="ta-led" style="animation-delay:.3s"/><circle cx="54" cy="56" r="3" fill="#fbbf24" class="ta-led" style="animation-delay:.6s"/><circle cx="66" cy="56" r="3" fill="#60a5fa"/>' +
      '<rect x="88" y="48" width="26" height="14" rx="3" class="ta-b2"/><path d="M92 48v-8M101 48v-8M110 48v-8" class="ta-ln"/>' +
      '<path d="M104 20c0-10 8-14 8-14" class="ta-ln" stroke-dasharray="3 3"/>';
    return R(140, 92, s, { lan: { x: 101, y: 78 }, fiber: { x: 104, y: 8 }, left: { x: 14, y: 50 }, right: { x: 126, y: 50 } });
  }
  function usbNic(o) {
    var s = '<ellipse cx="80" cy="72" rx="60" ry="5" fill="url(#ta-gl)"/>' +
      '<rect x="4" y="26" width="34" height="22" rx="3" fill="#cfd5e2" stroke="var(--ta-line)" stroke-width="1.4"/><rect x="10" y="32" width="22" height="10" rx="1.5" fill="#6d7790"/>' +
      '<rect x="38" y="20" width="76" height="34" rx="9" class="ta-b" filter="url(#ta-sh)"/>' +
      '<circle cx="54" cy="37" r="3" fill="#34d399" class="ta-led"/><path d="M64 30h34M64 37h28M64 44h20" class="ta-ln" opacity=".45"/>' +
      '<rect x="114" y="24" width="34" height="26" rx="4" class="ta-b2"/><rect x="120" y="30" width="22" height="14" rx="2" fill="#0d1220"/><path d="M123 44v4M127 44v4M131 44v4M135 44v4M139 44v4" stroke="#e5b84a" stroke-width="1.6"/>';
    return R(156, 80, s, { usb: { x: 4, y: 37 }, lan: { x: 148, y: 37 } });
  }
  function phone(kind) {
    var ip = kind === 'iphone';
    var s = '<ellipse cx="38" cy="132" rx="30" ry="5" fill="url(#ta-gl)"/>' +
      '<rect x="12" y="10" width="52" height="112" rx="' + (ip ? 11 : 8) + '" class="ta-b" filter="url(#ta-sh)"/>' +
      '<rect x="17" y="18" width="42" height="94" rx="' + (ip ? 7 : 4) + '" fill="#0b1226" opacity=".92"/>' +
      (ip ? '<rect x="28" y="21" width="20" height="6" rx="3" fill="#000"/>' : '<circle cx="38" cy="23" r="2" fill="#000"/>') +
      '<g class="ta-sk" opacity=".8" stroke="#5eead4"><path d="M26 56q12-12 24 0M30 62q8-7 16 0"/><circle cx="38" cy="68" r="1.6" fill="#5eead4"/></g>' +
      '<rect x="24" y="84" width="28" height="4" rx="2" fill="#5eead4" opacity=".5"/><rect x="24" y="92" width="20" height="4" rx="2" fill="#5eead4" opacity=".3"/>' +
      '<rect x="28" y="116" width="20" height="6" rx="2" class="ta-b2"/>';
    return R(76, 138, s, { usb: { x: 38, y: 122 }, ant: { x: 38, y: 10 }, left: { x: 12, y: 66 }, right: { x: 64, y: 66 } });
  }

  var G = {
    pc: '<rect x="3" y="4" width="22" height="14" rx="2.5"/><path d="M14 18v5M8 24h12"/>',
    laptop: '<rect x="5" y="6" width="18" height="12" rx="2"/><path d="M2 22h24l-2-4H4z"/>',
    phone: '<rect x="8" y="2" width="12" height="24" rx="3.5"/><path d="M12.5 22.5h3"/>',
    tablet: '<rect x="4" y="4" width="20" height="20" rx="3.5"/><path d="M12.5 21h3"/>',
    tv: '<rect x="2" y="5" width="24" height="15" rx="2.5"/><path d="M9 24h10M14 20v4"/>',
    iot: '<rect x="8" y="8" width="12" height="12" rx="2.5"/><path d="M11 8V4M17 8V4M11 24v-4M17 24v-4M8 11H4M8 17H4M24 11h-4M24 17h-4"/>',
    printer: '<rect x="3" y="10" width="22" height="10" rx="2.5"/><path d="M8 10V4h12v6M8 17h12v7H8z"/>',
    console: '<rect x="2" y="8" width="24" height="13" rx="6.5"/><path d="M8 14.5h5M10.5 12v5"/><circle cx="18" cy="13" r="1"/><circle cx="21" cy="16" r="1"/>',
    camera: '<rect x="3" y="8" width="22" height="14" rx="3"/><circle cx="14" cy="15" r="4"/><path d="M9 8l2-3h6l2 3"/>',
    nas: '<rect x="4" y="3" width="20" height="10" rx="2.5"/><rect x="4" y="15" width="20" height="10" rx="2.5"/><path d="M8 8h.1M8 20h.1M12 8h8M12 20h8"/>',
    speaker: '<rect x="7" y="2" width="14" height="24" rx="6"/><circle cx="14" cy="17" r="3.4"/><path d="M14 7h.1"/>',
    unknown: '<circle cx="14" cy="14" r="10"/><path d="M11 11.5a3 3 0 1 1 4.2 2.7c-.8.5-1.2 1-1.2 2M14 20h.1"/>'
  };
  function client(kind, o) {
    var g = G[kind] || G.unknown;
    return R(56, 56,
      '<rect x="2" y="2" width="52" height="52" rx="15" class="ta-b" filter="url(#ta-sh)"/>' +
      '<g transform="translate(14 14)" class="ta-sk">' + g + '</g>', { top: { x: 28, y: 2 }, bottom: { x: 28, y: 54 }, ant: { x: 28, y: 2 } });
  }

  /* ---- 链路 ---- */
  function pathOf(a, b, vert) {
    var dx = b.x - a.x, dy = b.y - a.y;
    if (vert == null) vert = Math.abs(dy) > Math.abs(dx);
    return vert
      ? 'M' + f1(a.x) + ' ' + f1(a.y) + 'C' + f1(a.x) + ' ' + f1(a.y + dy / 2) + ' ' + f1(b.x) + ' ' + f1(b.y - dy / 2) + ' ' + f1(b.x) + ' ' + f1(b.y)
      : 'M' + f1(a.x) + ' ' + f1(a.y) + 'C' + f1(a.x + dx / 2) + ' ' + f1(a.y) + ' ' + f1(b.x - dx / 2) + ' ' + f1(b.y) + ' ' + f1(b.x) + ' ' + f1(b.y);
  }
  function flows(d, col, dir, w) {
    var s = '';
    if (dir !== 'rev') s += '<path d="' + d + '" class="ta-flow" stroke="' + col + '" stroke-width="' + w + '"/>';
    if (dir === 'rev' || dir === 'both') s += '<path d="' + d + '" class="ta-flow r" stroke="#fff" stroke-opacity=".85" stroke-width="' + (w * 0.8) + '" style="stroke-dashoffset:7"/>';
    return s;
  }
  function chev(a, b, col, n, delay0, size) {
    var ang = Math.atan2(b.y - a.y, b.x - a.x) * 180 / Math.PI, s = '';
    for (var i = 1; i <= n; i++) {
      var t = i / (n + 1), x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t;
      s += '<path class="ta-wv" d="M-4 -' + size + 'Q6 0 -4 ' + size + '" transform="translate(' + f1(x) + ' ' + f1(y) + ') rotate(' + f1(ang) + ')" stroke="' + col + '" stroke-width="2.2" style="animation-delay:' + (delay0 + i * 0.28) + 's"/>';
    }
    return s;
  }
  function link(type, a, b, o) {
    o = o || {};
    var col = o.color || '#4cc2ff', dir = o.dir || 'both', q = o.q == null ? 0.7 : o.q;
    var d = pathOf(a, b, o.vert), s = '';
    if (type === 'dashed') {
      return '<g class="ta-link"><path d="' + d + '" fill="none" stroke="#7b8499" stroke-width="2.2" stroke-dasharray="6 8" stroke-linecap="round" opacity=".7"/></g>';
    }
    if (type === 'rf') {
      s = '<path d="' + d + '" fill="none" stroke="' + col + '" stroke-opacity=".35" stroke-width="1.6" stroke-dasharray="3 7" stroke-linecap="round"/>' +
        chev(a, b, col, 3, 0, 9) + '<circle cx="' + f1(a.x) + '" cy="' + f1(a.y) + '" r="3.2" fill="' + col + '"/><circle cx="' + f1(b.x) + '" cy="' + f1(b.y) + '" r="3.2" fill="' + col + '"/>';
      return '<g class="ta-link">' + s + '</g>';
    }
    if (type === 'wifi') {
      var w = 1.4 + 1.8 * q;
      s = '<path d="' + d + '" fill="none" stroke="' + col + '" stroke-opacity="' + f1(0.25 + 0.4 * q) + '" stroke-width="' + f1(w) + '" stroke-dasharray="' + (q > 0.5 ? '4 6' : '2 9') + '" stroke-linecap="round"/>' +
        flows(d, col, dir, f1(w + 0.8)) + chev(a, b, col, 3, 0, 10);
      return '<g class="ta-link">' + s + '</g>';
    }
    /* cable / usb */
    var ca = Math.atan2(b.y - a.y, b.x - a.x) * 180 / Math.PI;
    s = '<path d="' + d + '" fill="none" stroke="var(--ta-line)" stroke-width="' + (type === 'usb' ? 6 : 5) + '" stroke-linecap="round"/>' +
      '<path d="' + d + '" fill="none" stroke="' + col + '" stroke-opacity=".55" stroke-width="2.6" stroke-linecap="round"/>' +
      flows(d, col === '#fff' ? '#4cc2ff' : col, dir, 3);
    if (type === 'usb') {
      s += '<rect x="-8" y="-5" width="14" height="10" rx="2.5" fill="#cfd5e2" stroke="var(--ta-line)" stroke-width="1.2" transform="translate(' + f1(a.x) + ' ' + f1(a.y) + ') rotate(' + f1(ca) + ')"/>' +
        '<rect x="-6" y="-5" width="14" height="10" rx="2.5" fill="#cfd5e2" stroke="var(--ta-line)" stroke-width="1.2" transform="translate(' + f1(b.x) + ' ' + f1(b.y) + ') rotate(' + f1(ca) + ')"/>';
    }
    return '<g class="ta-link">' + s + '</g>';
  }

  function signalBars(q) {
    var n = barsOf(q), s = '';
    for (var i = 0; i < 4; i++) s += '<rect x="' + (i * 5) + '" y="' + (12 - i * 3.4) + '" width="3.4" height="' + (3 + i * 3.4) + '" rx="1" fill="' + (i < n ? 'currentColor' : 'currentColor') + '" opacity="' + (i < n ? 1 : 0.22) + '"/>';
    return '<g>' + s + '</g>';
  }
  function meter(v, mn, mx, o) {
    var p = clamp01((v - mn) / (mx - mn)) * 100;
    return '<g><rect width="100" height="6" rx="3" fill="currentColor" opacity=".18"/><rect width="' + f1(p) + '" height="6" rx="3" fill="currentColor"/></g>';
  }
  function badge(text, tn) {
    return '<g><rect width="' + (String(text).length * 8 + 14) + '" height="18" rx="9" fill="currentColor" opacity=".18"/><text x="9" y="13" font-size="10" fill="currentColor">' + esc(text) + '</text></g>';
  }

  return {
    __fb: 1, css: css, defs: defs, router: router, module5g: module5g, tower: tower, cloud: cloud, upRouter: upRouter,
    onu: onu, usbNic: usbNic, phone: phone, client: client, link: link, signalBars: signalBars, meter: meter, badge: badge
  };
})();

/* 选用美术库: 真的有就用真的, 缺哪个工厂就用备用图元补哪个; 任何一个工厂抛异常也回退到备用 */
function pickArt() {
  var T = window.MwrtTopoArt, A = {}, k, names = ['router', 'module5g', 'tower', 'cloud', 'upRouter', 'onu', 'usbNic', 'phone', 'client', 'link', 'signalBars', 'meter', 'badge'];
  A.real = !!(T && typeof T.router === 'function');
  A.css = (A.real && T.css ? String(T.css) : '') + FB.css;
  A.defs = function () {
    var s = '';
    try { if (A.real && T.defs) s += T.defs(); } catch (e) { }
    return s + FB.defs();
  };
  names.forEach(function (n) {
    A[n] = function () {
      var r = null;
      if (A.real && typeof T[n] === 'function') {
        try { r = T[n].apply(T, arguments); } catch (e) { r = null; }
        if (r && n !== 'link' && n !== 'signalBars' && n !== 'meter' && n !== 'badge' && (!r.svg || !(r.w > 0) || !(r.h > 0))) r = null;
      }
      if (r == null) r = FB[n].apply(FB, arguments);
      return r;
    };
  });
  return A;
}

/* ======================================================================
   三、样式 (深色玻璃拟态为主, .mt-l = 浅色皮肤; .mt-mob = 手机纵向布局)
   ====================================================================== */
var CSS = [
'.mt-root{--mt-bg1:#0a1226;--mt-bg2:#05080f;--mt-ink:#e9eefb;--mt-mut:#8f9bb8;--mt-glass:rgba(255,255,255,.055);--mt-glass2:rgba(14,22,44,.72);--mt-bd:rgba(255,255,255,.10);--mt-bd2:rgba(255,255,255,.18);--mt-acc:#4cc2ff;--mt-acc2:#7c5cfc;--mt-good:#34d399;--mt-mid:#fbbf24;--mt-bad:#f87171;--mt-shadow:0 20px 60px rgba(0,0,0,.5);--mt-grid:rgba(120,160,255,.055);--mt-th:68px;',
'  --ta-ink:#dbe6ff;--ta-line:#5a6c97;--ta-b1:#3a4a76;--ta-b2:#1d2744;--ta-glow:#4cc2ff;--ta-dim:#8f9bb8;--ta-acc:#4cc2ff;',
'  position:fixed;left:var(--mwrt-ov-l,0px);top:var(--mwrt-ov-t,0px);right:var(--mwrt-ov-r,0px);bottom:var(--mwrt-ov-b,0px);z-index:2147483000;overflow:hidden;color:var(--mt-ink);font:14px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei","Noto Sans SC",Roboto,sans-serif;-webkit-font-smoothing:antialiased;-webkit-tap-highlight-color:transparent;opacity:0;transition:opacity .22s ease}',
'.mt-root.mt-on{opacity:1}',
'.mt-root.mt-l{--mt-bg1:#f4f8ff;--mt-bg2:#dce6f6;--mt-ink:#15203a;--mt-mut:#5d6b8a;--mt-glass:rgba(255,255,255,.62);--mt-glass2:rgba(255,255,255,.88);--mt-bd:rgba(30,50,100,.12);--mt-bd2:rgba(30,50,100,.22);--mt-acc:#1f7fe6;--mt-acc2:#6a4de0;--mt-good:#12a46f;--mt-mid:#d98a07;--mt-bad:#e5484d;--mt-shadow:0 20px 50px rgba(40,70,130,.22);--mt-grid:rgba(40,80,160,.07);',
'  --ta-ink:#1c2a4d;--ta-line:#7d8db3;--ta-b1:#ffffff;--ta-b2:#d3ddf0;--ta-glow:#3b8fe8;--ta-dim:#5d6b8a;--ta-acc:#1f7fe6}',
'.mt-root *{box-sizing:border-box}',
'.mt-root button{font:inherit;color:inherit;cursor:pointer}',
'.mt-root :focus-visible{outline:2px solid var(--mt-acc);outline-offset:2px}',
'.mt-bg{position:absolute;left:0;top:0;right:0;bottom:0;background:radial-gradient(900px 520px at 78% 8%,rgba(124,92,252,.20),transparent 62%),radial-gradient(800px 520px at 12% 92%,rgba(76,194,255,.16),transparent 60%),linear-gradient(160deg,var(--mt-bg1),var(--mt-bg2))}',
'.mt-bg::after{content:"";position:absolute;left:0;top:0;right:0;bottom:0;background-image:linear-gradient(var(--mt-grid) 1px,transparent 1px),linear-gradient(90deg,var(--mt-grid) 1px,transparent 1px);background-size:44px 44px;-webkit-mask-image:radial-gradient(ellipse at 50% 45%,#000 30%,transparent 78%);mask-image:radial-gradient(ellipse at 50% 45%,#000 30%,transparent 78%)}',
'.mt-l .mt-bg{background:radial-gradient(900px 520px at 78% 8%,rgba(106,77,224,.13),transparent 62%),radial-gradient(800px 520px at 12% 92%,rgba(31,127,230,.14),transparent 60%),linear-gradient(160deg,var(--mt-bg1),var(--mt-bg2))}',
/* 顶部状态条 */
'.mt-top{position:absolute;left:0;top:0;right:0;min-height:68px;padding:calc(14px + env(safe-area-inset-top,0px)) 78px 8px 26px;display:flex;align-items:center;gap:22px;z-index:3}',
'.mt-ttl{display:flex;align-items:center;gap:12px;flex:none;min-width:0}',
'.mt-logo{width:36px;height:36px;border-radius:10px;display:flex;align-items:center;justify-content:center;color:var(--mt-acc);background:var(--mt-glass);border:1px solid var(--mt-bd2);box-shadow:inset 0 1px 0 rgba(255,255,255,.07);flex:none}',
'.mt-logo svg{width:20px;height:20px}',
'#mt-root .mt-ttl h1,#mt-root .mt-pt h2,#mt-root.mt-root h1,#mt-root.mt-root h2{color:var(--mt-ink)!important;background:none!important;text-shadow:none!important}',
'#mt-root.mt-root .mt-ttl h1,#mt-root.mt-root .mt-pt h2,#mt-root.mt-root h1,#mt-root.mt-root h2{border:0!important;border-bottom:0 none transparent!important;border-bottom-width:0!important;box-shadow:none!important;outline:0;padding-bottom:0!important;background-image:none!important}',
'#mt-root.mt-root h1::before,#mt-root.mt-root h1::after,#mt-root.mt-root h2::before,#mt-root.mt-root h2::after{content:none!important;display:none!important}',
'.mt-ttl h1{margin:0;font-size:18px;font-weight:650;letter-spacing:.5px;line-height:1.2;color:var(--mt-ink)!important}',
'.mt-ttl p{margin:2px 0 0;font-size:12px;color:var(--mt-mut);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:34vw}',
'.mt-chips{display:flex;align-items:center;gap:10px;flex:1 1 auto;min-width:0;flex-wrap:wrap}',
'.mt-strip{display:inline-flex;align-items:stretch;height:34px;border-radius:10px;background:var(--mt-glass);border:1px solid var(--mt-bd);-webkit-backdrop-filter:blur(14px);backdrop-filter:blur(14px);box-shadow:inset 0 1px 0 rgba(255,255,255,.05);overflow:hidden;max-width:100%}',
'.mt-chip{display:inline-flex;align-items:center;gap:7px;padding:0 14px;font-size:12.5px;white-space:nowrap}',
'.mt-strip .mt-chip+.mt-chip{border-left:1px solid var(--mt-bd)}',
'.mt-chip b{font-weight:600;font-variant-numeric:tabular-nums;color:var(--mt-ink)}',
'.mt-k{color:var(--mt-mut);font-size:12px}',
'.mt-eg b{color:var(--mt-k,var(--mt-ink))}',
'.mt-ar{font-size:12px;font-weight:700;line-height:1}.mt-ar.d{color:var(--mt-good)}.mt-ar.u{color:var(--mt-acc)}',
'.mt-rt b+.mt-ar{margin-left:6px}',
'.mt-x{position:absolute;right:20px;top:calc(15px + env(safe-area-inset-top,0px));z-index:6;width:42px;height:42px;border-radius:50%;border:1px solid var(--mt-bd2);background:var(--mt-glass);-webkit-backdrop-filter:blur(14px);backdrop-filter:blur(14px);display:flex;align-items:center;justify-content:center;padding:0;transition:transform .25s,background .2s,border-color .2s}',
'.mt-x:hover{transform:rotate(90deg);background:rgba(248,113,113,.2);border-color:var(--mt-bad)}',
'.mt-x svg{width:18px;height:18px;stroke:currentColor;stroke-width:2.2;stroke-linecap:round;fill:none}',
/* 舞台 */
'.mt-stage{position:absolute;left:0;top:var(--mt-th);right:0;bottom:0;z-index:1;transition:right .38s cubic-bezier(.2,.8,.2,1),bottom .3s ease}',
'.mt-svg{position:absolute;left:0;top:0;width:100%;height:100%;display:block;overflow:visible;user-select:none;-webkit-user-select:none}',
'.mt-svg text{font-family:inherit;pointer-events:none}',
'.mt-lb{font-size:15px;font-weight:600;fill:var(--mt-ink);text-anchor:middle}',
'.mt-ls{font-size:12px;fill:var(--mt-mut);text-anchor:middle;font-variant-numeric:tabular-nums}',
'.mt-mob .mt-lb,.mt-mob .mt-ls{text-anchor:start}',
'.mt-mob .mt-cl .mt-lb,.mt-mob .mt-cl .mt-ls{text-anchor:middle}',
'.mt-cl .mt-lb{font-size:13px;font-weight:600}.mt-cl .mt-ls{font-size:12.5px}',
'.mt-svg text{unicode-bidi:isolate}',
'.mt-node[data-id="router"] .mt-lb,.mt-node[data-id="router"] .mt-ls{paint-order:stroke;stroke:var(--mt-bg1);stroke-width:4px;stroke-linejoin:round}',
'.mt-cl .mt-lb,.mt-cl .mt-ls{paint-order:stroke;stroke:var(--mt-bg1);stroke-width:5px;stroke-linejoin:round}',
'.mt-node{cursor:pointer;outline:none}',
'.mt-hit{fill:transparent}',
'.mt-hl{fill:var(--mt-acc);fill-opacity:0;stroke:var(--mt-acc);stroke-width:1;stroke-dasharray:4 4;stroke-opacity:0;transition:fill-opacity .2s,stroke-opacity .2s}',
'.mt-node:hover .mt-hl,.mt-node:focus-visible .mt-hl{fill-opacity:.10;stroke-opacity:.35}',
'.mt-l .mt-node:hover .mt-hl,.mt-l .mt-node:focus-visible .mt-hl{fill-opacity:.05;stroke-opacity:.6}',
'.mt-selring{fill:none;stroke:var(--mt-acc);stroke-width:2;stroke-dasharray:5 5;opacity:0;transition:opacity .2s}',
'.mt-node.sel .mt-selring{opacity:1;animation:mt-dash 1.4s linear infinite}',
'.mt-node.sel .mt-hl{fill-opacity:.14;stroke-opacity:.5}',
'.mt-l .mt-node.sel .mt-hl{fill-opacity:.07;stroke-opacity:.7}',
'.mt-off .mt-art{filter:grayscale(1) brightness(.8);opacity:.5}',
'.mt-off .mt-lb,.mt-off .mt-ls{opacity:.6}',
'.mt-pill rect{fill:var(--mt-glass2);stroke:var(--mt-bd2);stroke-width:1}',
'.mt-pill text{font-size:11.5px;font-weight:600;fill:var(--mt-ink);text-anchor:middle;font-variant-numeric:tabular-nums}',
'.mt-pill.bad rect{stroke:var(--mt-bad)}.mt-pill.bad text{fill:var(--mt-bad)}',
'.mt-hub rect{fill:var(--mt-glass2);stroke:var(--mt-bd2)}',
'.mt-hub text{font-size:12.5px;font-weight:600;fill:var(--mt-ink);text-anchor:start}',
'.mt-hub .mt-ic{fill:none;stroke:var(--mt-acc);stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}',
'.mt-tr{fill:none;stroke-linecap:round}',
'.mt-tr.w{stroke:var(--mt-acc);stroke-opacity:.5;stroke-width:3.4}',
'.mt-tr.hl{stroke:#fff;stroke-opacity:.38;stroke-width:1.1}',
'.mt-tr.f{stroke:var(--mt-acc2);stroke-opacity:.6;stroke-width:2.4;stroke-dasharray:.1 7}',
'.mt-flow{fill:none;stroke-linecap:round;stroke-dasharray:2 12;animation:mt-flow 1.4s linear infinite}',
'.mt-flow.w{stroke:var(--mt-acc)}.mt-flow.f{stroke:var(--mt-acc2);animation-duration:2s}',
'.mt-ring{fill:none;stroke:var(--mt-acc2);stroke-width:1.6;transform-box:fill-box;transform-origin:50% 50%;animation:mt-ring 2.8s ease-out infinite;opacity:0}',
'.mt-bar{fill:var(--mt-mut);opacity:.28}.mt-bar.on{opacity:1}',
'.mt-bq1 .mt-bar.on{fill:var(--mt-bad)}.mt-bq2 .mt-bar.on{fill:var(--mt-mid)}.mt-bq3 .mt-bar.on,.mt-bq4 .mt-bar.on{fill:var(--mt-good)}',
'.mt-more circle{fill:var(--mt-glass2);stroke:var(--mt-bd2);stroke-width:1.5}',
'.mt-more text.mt-mn{font-size:15px;font-weight:700;fill:var(--mt-acc);text-anchor:middle}',
'.mt-empty text{font-size:13px;fill:var(--mt-mut);text-anchor:middle}',
'.mt-warn{fill:var(--mt-bad);font-size:12.5px;font-weight:700;text-anchor:middle;animation:mt-blink 1.3s ease-in-out infinite}',
'.mt-fresh{animation:mt-fade .35s ease both}',
'.mt-enter .mt-node,.mt-enter .mt-link,.mt-enter .mt-hub{animation:mt-in .55s cubic-bezier(.2,.8,.2,1) both;animation-delay:calc(var(--i,0)*70ms)}',
/* 加载 / 错误 */
'.mt-load{position:absolute;left:0;top:0;right:0;bottom:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;color:var(--mt-mut);font-size:13px;z-index:2;transition:opacity .3s}',
'.mt-load.hid{opacity:0;pointer-events:none}',
'.mt-spin{width:34px;height:34px;border-radius:50%;border:3px solid var(--mt-bd2);border-top-color:var(--mt-acc);animation:mt-spin .9s linear infinite}',
'.mt-load button{height:34px;padding:0 18px;border-radius:17px;border:1px solid var(--mt-bd2);background:var(--mt-glass);}',
'.mt-load button:hover{border-color:var(--mt-acc)}',
'.mt-stale{display:none;color:var(--mt-mid)}.mt-st .mt-stale{display:inline-flex}',
/* 详情面板 */
'.mt-panel{position:absolute;z-index:5;right:20px;top:calc(var(--mt-th) + 6px);bottom:20px;width:372px;max-width:calc(100% - 40px);display:flex;flex-direction:column;border-radius:20px;background:var(--mt-glass2);border:1px solid var(--mt-bd2);box-shadow:var(--mt-shadow);-webkit-backdrop-filter:blur(22px) saturate(1.3);backdrop-filter:blur(22px) saturate(1.3);transform:translateX(30px);opacity:0;visibility:hidden;pointer-events:none;transition:transform .38s cubic-bezier(.2,.8,.2,1),opacity .25s,visibility 0s linear .38s;overflow:hidden}',
'.mt-panel.on{transform:none;opacity:1;visibility:visible;pointer-events:auto;transition-delay:0s}',
'.mt-ph{display:flex;align-items:center;gap:14px;padding:18px 18px 14px;border-bottom:1px solid var(--mt-bd);flex:none;position:relative}',
'.mt-pi{width:68px;height:68px;flex:none;border-radius:16px;background:var(--mt-glass);border:1px solid var(--mt-bd);display:flex;align-items:center;justify-content:center;overflow:hidden}',
'.mt-pi svg{width:100%;height:100%;display:block;overflow:visible;padding:6px}',
'.mt-pt{min-width:0;flex:1;padding-right:26px}',
'.mt-pt h2{margin:0;font-size:16.5px;font-weight:650;line-height:1.25;word-break:break-all;overflow-wrap:anywhere;unicode-bidi:isolate;color:var(--mt-ink)!important}',
'.mt-pt p{margin:3px 0 0;font-size:12.5px;color:var(--mt-mut);word-break:break-all;overflow-wrap:anywhere;unicode-bidi:isolate}',
'.mt-pc{position:absolute;right:12px;top:12px;width:30px;height:30px;border-radius:50%;border:1px solid var(--mt-bd);background:var(--mt-glass);padding:0;display:flex;align-items:center;justify-content:center}',
'.mt-pc:hover{border-color:var(--mt-bd2);background:rgba(128,140,170,.2)}',
'.mt-pc::after{content:"";position:absolute;left:-6px;top:-6px;right:-6px;bottom:-6px}',
'.mt-pc svg{width:13px;height:13px;stroke:currentColor;stroke-width:2.4;stroke-linecap:round;fill:none}',
'.mt-pb{overflow-y:auto;overflow-x:hidden;padding:6px 18px 20px;flex:1 1 auto;-webkit-overflow-scrolling:touch;overscroll-behavior:contain}',
'.mt-pb::-webkit-scrollbar{width:6px}.mt-pb::-webkit-scrollbar-thumb{background:var(--mt-bd2);border-radius:3px}',
'.mt-sh{display:flex;align-items:center;gap:10px;margin:16px 0 4px;font-size:11px;font-weight:700;letter-spacing:1.4px;color:var(--mt-mut)}',
'.mt-sh::after{content:"";flex:1;height:1px;background:var(--mt-bd)}',
'.mt-r{display:flex;align-items:baseline;justify-content:space-between;gap:16px;padding:7px 0;border-bottom:1px dashed var(--mt-bd)}',
'.mt-r:last-child{border-bottom:0}',
'.mt-r>span{color:var(--mt-mut);font-size:12.5px;flex:none}',
'.mt-r>b{font-weight:600;font-size:13px;text-align:right;word-break:break-all;overflow-wrap:anywhere;unicode-bidi:isolate;max-width:100%;font-variant-numeric:tabular-nums;min-width:0}',
'.mt-rm{padding:8px 0 9px;border-bottom:1px dashed var(--mt-bd)}',
'.mt-rm .mt-rt{display:flex;align-items:baseline;justify-content:space-between;gap:12px}',
'.mt-rm .mt-rt span{color:var(--mt-mut);font-size:12.5px}.mt-rm .mt-rt b{font-size:14px;font-weight:650;font-variant-numeric:tabular-nums}',
'.mt-mt{position:relative;height:6px;border-radius:3px;background:rgba(128,140,170,.22);margin-top:7px;overflow:visible}',
'.mt-mt i{display:block;height:100%;border-radius:3px;width:0;background:var(--mt-mc,var(--mt-acc));transition:width .6s cubic-bezier(.2,.8,.2,1),background .3s;box-shadow:0 0 10px color-mix(in srgb,var(--mt-mc,#4cc2ff) 60%,transparent)}',
'.mt-mt.good{--mt-mc:var(--mt-good)}.mt-mt.mid{--mt-mc:var(--mt-mid)}.mt-mt.bad{--mt-mc:var(--mt-bad)}',
'.mt-mscale{display:flex;justify-content:space-between;font-size:10.5px;color:var(--mt-mut);opacity:.75;margin-top:3px;font-variant-numeric:tabular-nums}',
'.mt-tg{display:inline-flex;align-items:center;gap:5px;height:20px;padding:0 9px;border-radius:10px;font-size:11.5px;font-weight:600;background:rgba(128,140,170,.2);margin-left:6px;vertical-align:1px}',
'.mt-tg.good{background:rgba(52,211,153,.18);color:var(--mt-good)}.mt-tg.mid{background:rgba(251,191,36,.18);color:var(--mt-mid)}.mt-tg.bad{background:rgba(248,113,113,.18);color:var(--mt-bad)}',
'.mt-chipr{display:flex;flex-wrap:wrap;gap:6px;justify-content:flex-end}',
'.mt-chipr em{font-style:normal;font-size:11.5px;font-weight:600;padding:2px 9px;border-radius:10px;background:var(--mt-glass);border:1px solid var(--mt-bd)}',
'.mt-li{display:flex;align-items:center;gap:12px;width:100%;padding:9px 6px;border:0;border-bottom:1px solid var(--mt-bd);background:transparent;text-align:left;border-radius:10px}',
'.mt-li:hover{background:rgba(128,140,170,.14)}',
'.mt-li svg{width:34px;height:34px;flex:none}',
'.mt-li div{min-width:0;flex:1}.mt-li strong{display:block;font-size:13.5px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}',
'.mt-li small{display:block;font-size:11.5px;color:var(--mt-mut);font-variant-numeric:tabular-nums}',
'.mt-li em{font-style:normal;font-size:11px;color:var(--mt-mut);flex:none}',
'.mt-lvl{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px;background:var(--mt-mut)}',
/* 手机纵向 */
'.mt-mob{--mt-th:104px}',
'.mt-mob .mt-top{padding:calc(12px + env(safe-area-inset-top,0px)) 64px 8px 16px;flex-wrap:wrap;gap:8px 10px;background:linear-gradient(var(--mt-bg1) 78%,transparent);-webkit-backdrop-filter:blur(14px);backdrop-filter:blur(14px)}',
'.mt-mob .mt-ttl p{max-width:56vw}',
'.mt-mob .mt-chips{flex:1 0 100%;flex-wrap:wrap;gap:8px;margin-right:-48px;padding-bottom:2px}',
'.mt-mob .mt-strip{height:30px;flex-wrap:nowrap;overflow-x:auto;scrollbar-width:none;-webkit-mask-image:linear-gradient(90deg,#000 86%,transparent);mask-image:linear-gradient(90deg,#000 86%,transparent)}.mt-mob .mt-strip::-webkit-scrollbar{display:none}',
'.mt-mob .mt-chip{padding:0 10px;font-size:12px;flex:none}',
'.mt-mob .mt-x{right:12px;top:calc(11px + env(safe-area-inset-top,0px));width:38px;height:38px}',
'.mt-mob.mt-pan .mt-x{opacity:0;visibility:hidden;pointer-events:none}',
'.mt-mob .mt-pc{width:40px;height:40px;right:10px;top:10px}',
'.mt-mob .mt-stage{overflow-y:auto;overflow-x:hidden;-webkit-overflow-scrolling:touch;transition:none}',
'.mt-mob .mt-svg{position:relative;height:auto}',
'.mt-mob .mt-panel{left:0;right:0;top:auto;bottom:0;width:auto;max-width:none;max-height:68%;height:auto;border-radius:22px 22px 0 0;transform:translateY(105%);border-bottom:0}',
'.mt-mob .mt-panel.on{transform:none}',
'.mt-mob .mt-panel::before{content:"";position:absolute;left:50%;top:7px;width:38px;height:4px;margin-left:-19px;border-radius:2px;background:var(--mt-bd2)}',
'.mt-mob .mt-ph{padding:20px 16px 12px}',
'.mt-mob .mt-pb{padding:4px 16px calc(18px + env(safe-area-inset-bottom,0px))}',
'.mt-mob .mt-pi{width:56px;height:56px}',
/* 动画 */
'@keyframes mt-pulse{0%{opacity:.55;transform:scale(1)}70%,100%{opacity:0;transform:scale(2.7)}}',

'@keyframes mt-in{from{opacity:0;transform:translateY(12px)}to{opacity:1;transform:none}}',
'@keyframes mt-fade{from{opacity:0}to{opacity:1}}',
'@keyframes mt-flow{to{stroke-dashoffset:-14}}',
'@keyframes mt-dash{to{stroke-dashoffset:-20}}',
'@keyframes mt-spin{to{transform:rotate(360deg)}}',
'@keyframes mt-blink{0%,100%{opacity:1}50%{opacity:.3}}',
'@keyframes mt-ring{0%{opacity:.7;transform:scale(.4)}100%{opacity:0;transform:scale(1.5)}}',
'.mt-pause *,.mt-pause *::before,.mt-pause *::after{animation-play-state:paused!important}',
'.mt-mscale{font-size:11.5px}.mt-sh{font-size:12px}.mt-chipr em{font-size:12px}.mt-pill text{font-size:12.5px}.mt-tg{font-size:12px}.mt-li small,.mt-li em{font-size:12px}'
].join('\n');

/* ======================================================================
   四、场景: 数据 → 布局模型 → SVG 字符串
   结构键(skey)不变时只刷新绑定了 data-b / data-bq 的文字与信号格, 不重建 DOM(不闪烁)。
   ====================================================================== */
var PC_W = 1280, MOB_W = 420, MAX_SHOW = 12, PITCH = 100;
var HGT = { cloud: 96, tower: 150, module: 124, uprouter: 112, onu: 96, ghost: 112, nic: 90, phone: 140, router: 176 };

function chainOf(D) {
  var e = D.egress || {}, k = KINDS[e.kind] ? e.kind : 'none';
  var up = (e.proto === 'pppoe' || (e.gw && !isPrivIp(e.gw))) ? 'onu' : 'uprouter';
  if (k === 'cell') return ['cloud', 'tower', 'module'];
  if (k === 'wifi') return ['cloud', 'uprouter'];
  if (k === 'wired') return ['cloud', up];
  if (k === 'usbnic') return ['cloud', up, 'nic'];
  if (k === 'tether') return ['cloud', 'tower', 'phone'];
  return ['cloud', 'ghost'];
}
function nodeId(t) { return (t === 'uprouter' || t === 'onu' || t === 'ghost') ? 'up' : t; }

function artFor(A, t, D) {
  var cell = D.cell, tf = D.usb && D.usb.tether;
  switch (t) {
    case 'cloud': return A.cloud({});
    case 'tower': return A.tower({ carrier: (D.egress && D.egress.kind === 'cell') ? carrierOf(cell).id : 'other' });
    case 'module': return A.module5g({ model: cell && cell.module && cell.module.model || '5G 模组' });
    case 'uprouter': case 'ghost': return A.upRouter({});
    case 'onu': return A.onu({});
    case 'nic': return A.usbNic({});
    case 'phone': return A.phone(tf && tf.kind || 'unknown');
    case 'router': return A.router(modelPng(D.model), {});
  }
  return A.cloud({});
}

/* 下挂设备: 分组 + 折叠 */
function groupsOf(D) {
  var cs = sortClients(D.clients || []), w = [], f = [], i;
  for (i = 0; i < cs.length; i++) (cs[i].type === 'wifi' ? f : w).push(cs[i]);
  /* r2: 终端顺序只由 IP/MAC 决定, 不再按实时 RSSI 排序(RSSI 抖动会让名次互换, 触发整图重建、重播入场动画并丢焦点) */
  var sw = w.length, sf = f.length, total = sw + sf;
  if (total > MAX_SHOW) {
    var keep = MAX_SHOW - 1;
    while (sw + sf > keep) { if (sw >= sf) sw--; else sf--; }
  }
  var hw = w.length - sw, hf = f.length - sf;
  return { w: w, f: f, sw: sw, sf: sf, hw: hw, hf: hf, moreIn: (hw + hf) ? (hf > hw ? 'f' : 'w') : '', hidden: hw + hf };
}

/* 上游 WiFi 信号 0..4 档, 带迟滞: 在两档边界来回抖动时不重建 */
function qBucket(v) {
  if (S.qb != null && Math.abs(v - S.qb) < 0.8) return S.qb;
  S.qb = Math.round(v);
  return S.qb;
}
/* 结构键: 变了才重建 DOM */
function structKey(D, mobile, vh) {
  var e = D.egress || {}, c = D.cell, u = D.uplink_wifi, g = groupsOf(D), k = KINDS[e.kind] ? e.kind : 'none';
  var parts = [mobile ? 'm' : 'p', mobile ? 0 : Math.round(vh / 20), k, modelPng(D.model), chainOf(D).join('')];
  if (k === 'cell') parts.push(carrierOf(c).id, c ? 1 : 0);
  if (k === 'tether') parts.push(D.usb && D.usb.tether && D.usb.tether.kind);
  if (k === 'wifi') parts.push(u ? u.state : 'x', qBucket(clamp01(qRssi(u && num(u.rssi))) * 4));
  parts.push(g.sw, g.sf, g.moreIn, g.hidden);
  for (var i = 0; i < g.w.length && i < g.sw; i++) parts.push(g.w[i].mac, devKind(g.w[i]));
  for (i = 0; i < g.f.length && i < g.sf; i++) parts.push(g.f[i].mac, devKind(g.f[i]));
  return parts.join('|');
}

/* ---- 布局 ---- */
function layout(D, A, mobile, vh) {
  var chain = chainOf(D), g = groupsOf(D), e = D.egress || {}, kind = KINDS[e.kind] ? e.kind : 'none';
  var sc = { mobile: mobile, nodes: [], links: [], under: '', over: '', hubs: [], trunks: [], kind: kind, g: g };
  var VW = mobile ? MOB_W : PC_W, VH, i;
  var cs = carrierOf(D.cell);
  var lineCol = kind === 'cell' ? cs.color : KINDS[kind].c;

  function mkNode(id, type, art, cx, cy, h) {
    var k = h / art.h, w = art.w * k;
    var n = { id: id, type: type, art: art, k: k, w: w, h: h, x: cx - w / 2, y: cy - h / 2, cx: cx, cy: cy, off: false };
    n.at = function (name, dx, dy) {
      var a = art.anchors && art.anchors[name];
      return a ? { x: n.x + a.x * k, y: n.y + a.y * k } : { x: cx + (dx || 0), y: cy + (dy || 0) };
    };
    n.left = function () { return { x: n.x + 4, y: n.cy }; };
    n.right = function () { return { x: n.x + n.w - 4, y: n.cy }; };
    n.top = function () { return { x: n.cx, y: n.y + 4 }; };
    n.bottom = function () { return { x: n.cx, y: n.y + n.h - 4 }; };
    sc.nodes.push(n);
    return n;
  }

  /* 链路: 由 chain[i] → chain[i+1] 依次连到路由器 */
  /* PC 横向: 本设备在左, 互联网在右; 手机纵向: 互联网在上, 本设备在下 */
  var order = mobile ? chain.concat(['router']) : ['router'].concat(chain.slice().reverse()), nodes = [], by = {};
  var cy, cx;
  var routerX = 230;
  if (!mobile) {
    var dy = Math.max(0, (vh - 640) * 0.35), chainY = 150 + dy, x0 = 230, x1 = 1190;
    VH = vh;
    var step = (x1 - x0) / (order.length - 1);
    for (i = 0; i < order.length; i++) {
      var t = order[i], id = nodeId(t);
      var nd = mkNode(id, t, artFor(A, t, D), x0 + step * i, chainY, HGT[t]);
      nd.lblY = chainY + 108; nd.lblX = nd.cx; nd.box = { x: nd.cx - Math.max(nd.w / 2 + 10, 90), y: nd.y - 8, w: Math.max(nd.w + 20, 180), h: nd.lblY + 28 - nd.y + 8 };
      nodes.push(nd); by[t] = nd;
    }
    sc.chainY = chainY;
    sc.hubY = 345 + dy * 2.2;
    sc.rowY0 = sc.hubY + 86;
    sc.rowPitch = 112;
  } else {
    var y = 30;
    for (i = 0; i < order.length; i++) {
      var tt = order[i], hh = Math.round(HGT[tt] * 0.74);
      var art = artFor(A, tt, D);
      var nm = mkNode(nodeId(tt), tt, art, 96, y + hh / 2, hh);
      nm.lblX = 190; nm.lblY = nm.cy - 2;
      nm.box = { x: 8, y: nm.y - 8, w: 404, h: Math.max(nm.h, 44) + 16 };
      nodes.push(nm); by[tt] = nm;
      y += hh + 80;
    }
    sc.hubY0 = y - 80 + 54;
  }
  var router = by.router;
  router.isRouter = true;
  if (kind === 'none') { by.cloud.off = true; if (by.ghost) by.ghost.off = true; }

  /* 链路定义 */
  function L(typ, a, b, o) { sc.links.push({ type: typ, a: a, b: b, o: o || {} }); }
  function pt(n, side, rule) { return mobile ? (side === 'a' ? n.bottom() : n.top()) : (side === 'a' ? n.right() : n.left()); }
  function pill(a, b, key, bad) {
    var m = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 + (mobile ? 0 : 38) };
    sc.links[sc.links.length - 1].pill = { key: key, x: m.x, y: m.y, bad: !!bad };
  }
  var lnCol = lineCol;
  var q = 0.7;
  if (kind === 'wifi' && D.uplink_wifi) q = qRssi(num(D.uplink_wifi.rssi)); if (q == null) q = 0.5;
  var dirAll = 'both';
  for (i = 0; i < order.length - 1; i++) {
    /* near = 离本设备近的一端, far = 远端 */
    var nearT = mobile ? order[i + 1] : order[i], farT = mobile ? order[i] : order[i + 1];
    var nN = by[nearT], fN = by[farT];
    var pa, pb, typ = 'cable', key = '', col = lnCol, opt = { dir: dirAll };
    if (mobile) { pa = fN.bottom(); pb = nN.top(); } else { pa = nN.right(); pb = fN.left(); }
    var isR = nearT === 'router';
    if (kind === 'none') { typ = 'dashed'; key = isR ? 'bad' : ''; }
    else if (isR && (farT === 'module' || farT === 'nic' || farT === 'phone')) {
      typ = 'usb'; key = 'p:usb';
      if (!mobile && farT === 'module') pb = fN.at('usb', 0, 0);
    }
    else if (isR && farT === 'uprouter' && kind === 'wifi') {
      typ = 'wifi'; key = 'p:rf'; opt.q = q;
      if (mobile) { pa = { x: fN.cx, y: fN.y + fN.h - 6 }; pb = { x: nN.cx, y: nN.y + 6 }; }
      else { pa = nN.at('ant', 0, -nN.h / 2); pb = fN.at('ant', 0, -fN.h / 2); }
    }
    else if (farT === 'tower') {
      typ = 'rf'; key = 'p:rf';
      if (!mobile) { pa = nN.at('rf', nN.w / 2, 0); pb = fN.at('air', 0, -fN.h / 2); }
    }
    else if (farT === 'cloud') { typ = 'cable'; key = 'p:bh'; }
    else { typ = 'cable'; key = isR ? 'p:cb' : ''; }
    opt.color = col;
    opt.id = 'l' + i;
    L(typ, pa, pb, opt);
    if (kind === 'none' && isR) { pill(pa, pb, 'p:bad', true); sc.links[sc.links.length - 1].pill.text = '未连接'; }
    else if (key) pill(pa, pb, key, false);
    if (mobile) { var lk = sc.links[sc.links.length - 1]; if (lk.pill) { lk.pill.y = (pa.y + pb.y) / 2; lk.pill.x = 96; } }
  }
  sc.router = router; sc.by = by;

  /* ---- 下挂设备群 ---- */
  var wl = g.w.slice(0, g.sw), fl = g.f.slice(0, g.sf);
  var groups = [];
  if (g.w.length) groups.push({ key: 'w', list: wl.map(function (c) { return { kind: 'c', c: c }; }), cnt: g.w.length });
  if (g.f.length) groups.push({ key: 'f', list: fl.map(function (c) { return { kind: 'c', c: c }; }), cnt: g.f.length });
  groups.forEach(function (gr) { if (g.moreIn === gr.key) gr.list.push({ kind: 'more', n: g.hidden }); });
  var both = groups.length === 2;
  sc.empty = groups.length === 0;

  if (!mobile) {
    groups.forEach(function (gr) {
      var nn = gr.list.length, per = both ? Math.min(5, Math.max(3, Math.ceil(nn / 2))) : 8, cols = Math.min(nn, per);
      gr.cols = cols; gr.per = per; gr.wid = cols * PITCH; gr.ew = Math.max(gr.wid, 150);
    });
    if (both) {
      /* r2: 两群并排成一簇, 以路由器 x 为中心(放不下就整体右移到不出界), 主干缩短到一两百单位 */
      var GAP = 72, tot = groups[0].ew + GAP + groups[1].ew;
      var cc = Math.min(Math.max(routerX, 40 + tot / 2), VW - 40 - tot / 2);
      groups[0].cx = cc - tot / 2 + groups[0].ew / 2;
      groups[1].cx = cc + tot / 2 - groups[1].ew / 2;
    } else {
      groups.forEach(function (gr) { gr.cx = Math.max(gr.wid / 2 + 40, Math.min(VW - 40 - gr.wid / 2, routerX)); });
    }
  } else {
    var yy = sc.hubY0;
    groups.forEach(function (gr) {
      gr.cols = 4; gr.per = 4; gr.cx = 210; gr.hubY = yy;
      var rows = Math.ceil(gr.list.length / 4);
      gr.rowY0 = yy + 80;
      yy = gr.rowY0 + rows * 104 + 6;
    });
    VH = Math.max(yy, sc.hubY0 + 70) + 20;
    if (sc.empty) VH = sc.hubY0 + 70;
  }

  groups.forEach(function (gr) {
    var hubY = mobile ? gr.hubY : sc.hubY;
    gr.hub = { x: mobile ? 104 : gr.cx, y: hubY };
    var rows = Math.ceil(gr.list.length / gr.per);
    gr.items = [];
    gr.list.forEach(function (it, idx) {
      var r = Math.floor(idx / gr.per), cI = idx % gr.per;
      var inRow = Math.min(gr.per, gr.list.length - r * gr.per);
      var pitch = mobile ? 96 : PITCH;
      var cxn = gr.cx + (cI - (inRow - 1) / 2) * pitch;
      /* r3: PC 多行时, 奇数行整行错开半个间距, 让第 2 行以后的分支线从上一行设备之间的空隙穿过, 不再压在上一行图标和文字上 */
      if (!mobile && r > 0) {
        var n1 = Math.min(gr.per, gr.list.length), p1 = ((n1 - 1) / 2) % 1, pr = ((inRow - 1) / 2) % 1;
        var tg = (p1 + (r % 2 ? 0.5 : 0)) % 1, shf = (((tg - pr) % 1) + 1) % 1;
        if (shf > 0.01 && shf < 0.99) cxn += shf * pitch;
      }
      it.row = r;
      var cyn = (mobile ? gr.rowY0 : sc.rowY0) + r * (mobile ? 104 : sc.rowPitch);
      it.cx = cxn; it.cy = cyn;
      gr.items.push(it);
    });
    gr.rows = rows;
  });
  sc.groups = groups;
  sc.VW = VW; sc.VH = mobile ? VH : vh;
  if (!mobile) {
    groups.forEach(function (gr) { sc.VH = Math.max(sc.VH, Math.round(sc.rowY0 + (gr.rows - 1) * sc.rowPitch + 84)); });
    /* r3: 内容在画布里垂直/水平居中, 内容比画布小时略放大(最多 1.15 倍), 1440 宽下不再上重下空 */
    var bx0 = 1e9, bx1 = -1e9, by0 = 1e9, by1 = -1e9;
    sc.nodes.forEach(function (nd) { bx0 = Math.min(bx0, nd.box.x); bx1 = Math.max(bx1, nd.box.x + nd.box.w); by0 = Math.min(by0, nd.box.y); by1 = Math.max(by1, nd.box.y + nd.box.h); });
    groups.forEach(function (gr) {
      gr.items.forEach(function (it) { bx0 = Math.min(bx0, it.cx - 46); bx1 = Math.max(bx1, it.cx + 46); by1 = Math.max(by1, it.cy + 68); });
      bx0 = Math.min(bx0, gr.hub.x - 74); bx1 = Math.max(bx1, gr.hub.x + 74);
      by1 = Math.max(by1, gr.hub.y + 20);
    });
    if (sc.empty) by1 = Math.max(by1, sc.hubY + 90);
    var cw = bx1 - bx0, ch = by1 - by0;
    if (cw > 0 && ch > 0) {
      var fs = Math.max(1, Math.min(1.15, (sc.VH - 56) / ch, (VW - 24) / cw));
      sc.fit = { s: fs, tx: Math.round((VW - cw * fs) / 2 - bx0 * fs), ty: Math.round((sc.VH - ch * fs) / 2 - by0 * fs) };
    }
  }
  return sc;
}

/* ---- 渲染 ---- */
var ICO = {
  lan: '<path class="mt-ic" d="M-6 -3h12v6h-12zM-3 3v4M3 3v4M-6 7h12"/>',
  wifi: '<path class="mt-ic" d="M-8 -2a11 11 0 0 1 16 0M-5 1.5a6.5 6.5 0 0 1 10 0M0 6h.1"/>'
};
function pathBez(a, b, vert) {
  var dx = b.x - a.x, dy = b.y - a.y;
  function r(v) { return Math.round(v * 10) / 10; }
  return vert === false
    ? 'M' + r(a.x) + ' ' + r(a.y) + 'C' + r(a.x + dx / 2) + ' ' + r(a.y) + ' ' + r(b.x - dx / 2) + ' ' + r(b.y) + ' ' + r(b.x) + ' ' + r(b.y)
    : 'M' + r(a.x) + ' ' + r(a.y) + 'C' + r(a.x) + ' ' + r(a.y + dy * 0.55) + ' ' + r(b.x) + ' ' + r(b.y - dy * 0.55) + ' ' + r(b.x) + ' ' + r(b.y);
}
/* 圆角折线: 竖直下行到 busY, 横向到目标 x, 再竖直到目标 */
function pathBus(a, b, busY) {
  function r(v) { return Math.round(v * 10) / 10; }
  var dx = b.x - a.x, rad = 16;
  if (Math.abs(dx) < rad * 2) return pathBez(a, b, true);
  var sg = dx > 0 ? 1 : -1;
  return 'M' + r(a.x) + ' ' + r(a.y) + 'V' + r(busY - rad) + 'Q' + r(a.x) + ' ' + r(busY) + ' ' + r(a.x + sg * rad) + ' ' + r(busY) +
    'H' + r(b.x - sg * rad) + 'Q' + r(b.x) + ' ' + r(busY) + ' ' + r(b.x) + ' ' + r(busY + rad) + 'V' + r(b.y);
}
function renderScene(sc, A, first) {
  var s = '', i, n, idx = 0;
  /* 本机 AP 辐射环 */
  if (sc.router && !sc.empty && sc.g.f.length) {
    var ap = sc.router.at('ant', 0, -sc.router.h / 2);
    s += '<g pointer-events="none"><circle class="mt-ring" cx="' + ap.x + '" cy="' + ap.y + '" r="40"/><circle class="mt-ring" style="animation-delay:.9s" cx="' + ap.x + '" cy="' + ap.y + '" r="40"/><circle class="mt-ring" style="animation-delay:1.8s" cx="' + ap.x + '" cy="' + ap.y + '" r="40"/></g>';
  }
  /* 链路 */
  for (i = 0; i < sc.links.length; i++) {
    var lk = sc.links[i], frag = '';
    try { var r = A.link(lk.type, lk.a, lk.b, lk.o); frag = (r && r.svg) ? r.svg : String(r || ''); } catch (e) { frag = ''; }
    s += '<g class="mt-link" style="--i:' + (i + 1) + '">' + frag + '</g>';
  }
  /* 下挂设备: 干线 + 分支 */
  var rt = sc.router, animBr = (sc.g.sw + sc.g.sf) <= 8;
  sc.groups.forEach(function (gr) {
    var hub = gr.hub, wired = gr.key === 'w';
    var src = wired ? rt.at('lan', 0, rt.h / 2) : rt.at('ant', 0, -rt.h / 2);
    var d, hubIn;
    if (!sc.mobile) {
      hubIn = { x: hub.x, y: hub.y - 18 };
      /* 从机身底部两侧下行(避开居中的机身标签), 再沿总线横向到群标题 */
      /* r4: 起点贴在机身地台椭圆的左/右端点(art 的 left/right 锚点)上, 随机型图大小走; 至少离中线 88, 仍在机身标签(约 ±85)之外不穿过 IP。
         旧写法按节点宽度 +8 取 ≥112, 机型图一变就飘到机身外面, 线头悬空对不准 */
      var tip = rt.at(wired ? 'left' : 'right', 0, 0);
      src = { x: rt.cx + (wired ? -1 : 1) * Math.max(88, Math.abs(tip.x - rt.cx)), y: tip.y };
      d = pathBus(src, hubIn, hubIn.y - 30);
    } else {
      if (wired) { src = { x: rt.cx, y: rt.y + rt.h - 6 }; hubIn = { x: hub.x, y: hub.y - 18 }; d = pathBez(src, hubIn, true); }
      else {
        src = { x: rt.x + rt.w - 8, y: rt.y + rt.h - 8 };
        var ex = 404, hy = hub.y;
        d = 'M' + src.x + ' ' + src.y + 'C' + (src.x + 60) + ' ' + src.y + ' ' + ex + ' ' + (src.y + 10) + ' ' + ex + ' ' + (src.y + 60) + 'L' + ex + ' ' + (hy - 20) + 'C' + ex + ' ' + hy + ' ' + (ex - 20) + ' ' + hy + ' ' + (hub.x + 76) + ' ' + hy;
        hubIn = null;
      }
    }
    gr.trunk = d;
    s += '<g class="mt-link" style="--i:' + (sc.nodes.length + 1) + '"><path class="mt-tr ' + (wired ? 'w' : 'f') + '" d="' + d + '"/>' + (wired ? '<path class="mt-tr hl" d="' + d + '"/>' : '') + '<path class="mt-flow ' + (wired ? 'w' : 'f') + '" d="' + d + '" stroke-width="3"/></g>';
    gr.items.forEach(function (it) {
      var p0 = { x: hub.x, y: hub.y + 18 }, p1 = { x: it.cx, y: it.cy - 36 };
      var dd = pathBez(p0, p1, true);
      if (!sc.mobile && it.row > 0) {
        /* 先和第 1 行一样弧线落到第 1 行图标上方, 再沿两台设备之间的空隙笔直下到本行 */
        var yt = sc.rowY0 - 36;
        dd = 'M' + p0.x + ' ' + p0.y + 'C' + p0.x + ' ' + (p0.y + 34) + ' ' + p1.x + ' ' + (yt - 34) + ' ' + p1.x + ' ' + yt + 'V' + p1.y;
      }
      /* r2: 设备多时分支线只画静态线, 不再逐条跑流动动画(stroke-dashoffset 在主线程逐帧重算样式) */
      s += '<g class="mt-link"><path class="mt-tr ' + (wired ? 'w' : 'f') + '" d="' + dd + '"' + (wired ? ' stroke-width="2.4"' : ' stroke-width="1.6"') + '/>' +
        (animBr ? (wired ? '<path class="mt-flow w" d="' + dd + '" stroke-width="2.4" style="animation-delay:-' + ((it.cx % 7) * 0.2).toFixed(1) + 's"/>' : '<path class="mt-flow f" d="' + dd + '" stroke-width="2" style="animation-delay:-' + ((it.cx % 9) * 0.2).toFixed(1) + 's"/>') : '') + '</g>';
    });
  });
  /* 链路上的小标签 */
  sc.links.forEach(function (lk) {
    if (!lk.pill) return;
    s += '<g class="mt-pill' + (lk.pill.bad ? ' bad' : '') + '" data-pill="' + lk.pill.key + '" transform="translate(' + Math.round(lk.pill.x) + ' ' + Math.round(lk.pill.y) + ')"><rect x="-40" y="-11" width="80" height="22" rx="11"/><text y="4" data-b="' + lk.pill.key + '">' + esc(lk.pill.text || '') + '</text></g>';
  });
  /* 节点 */
  function nodeG(id, art, x, y, k, box, lbl, sub, extra, cls, label, noArt) {
    var o = '<g class="mt-node' + (cls ? ' ' + cls : '') + '" data-id="' + esc(id) + '" tabindex="0" role="button" aria-label="' + esc(label) + '" style="--i:' + (idx++) + '">';
    o += '<rect class="mt-hl" x="' + box.x + '" y="' + box.y + '" width="' + box.w + '" height="' + box.h + '" rx="18"/>';
    if (art) o += '<g class="mt-art" aria-hidden="true" transform="translate(' + (Math.round(x * 10) / 10) + ' ' + (Math.round(y * 10) / 10) + ') scale(' + (Math.round(k * 1000) / 1000) + ')">' + art.svg + '</g>';
    o += extra || '';
    o += '<rect class="mt-selring" x="' + box.x + '" y="' + box.y + '" width="' + box.w + '" height="' + box.h + '" rx="18"/>';
    o += '<rect class="mt-hit" x="' + box.x + '" y="' + box.y + '" width="' + box.w + '" height="' + box.h + '"/>';
    o += lbl + sub + '</g>';
    return o;
  }
  sc.nodes.forEach(function (nd) {
    var lb = '<text class="mt-lb" x="' + nd.lblX + '" y="' + nd.lblY + '" data-b="lb:' + nd.id + '"></text>';
    var ls = '<text class="mt-ls" x="' + nd.lblX + '" y="' + (nd.lblY + (sc.mobile ? 20 : 19)) + '" data-b="ls:' + nd.id + '"></text>';
    s += nodeG(nd.id, nd.art, nd.x, nd.y, nd.k, nd.box, lb, ls, '', nd.off ? 'mt-off' : '', nd.id);
  });
  /* 群标题 + 设备节点 */
  sc.groups.forEach(function (gr) {
    var hub = gr.hub, wired = gr.key === 'w';
    s += '<g class="mt-hub" transform="translate(' + Math.round(hub.x) + ' ' + Math.round(hub.y) + ')" style="--i:' + (idx++) + '"><rect x="-70" y="-18" width="140" height="36" rx="18"/><g transform="translate(-50 0)">' + (wired ? ICO.lan : ICO.wifi) + '</g><text x="-32" y="4.5" data-b="hub:' + gr.key + '"></text></g>';
    gr.items.forEach(function (it) {
      var box = { x: it.cx - 46, y: it.cy - 36, w: 92, h: 104 };
      if (it.kind === 'more') {
        var mo = '<g class="mt-more" transform="translate(' + it.cx + ' ' + it.cy + ')"><circle r="25"/><text class="mt-mn" y="5.5" data-b="more:n"></text></g>';
        s += nodeG('more', null, 0, 0, 1, box, '<text class="mt-ls" x="' + it.cx + '" y="' + (it.cy + 44) + '">更多设备</text>', '', mo, 'mt-cl', '更多设备');
        return;
      }
      var c = it.c, kd = devKind(c), art = A.client(kd, { link: c.type === 'wifi' ? 'wifi' : 'wired' }), k = 52 / art.h;
      var ax = it.cx - art.w * k / 2, ay = it.cy - art.h * k / 2;
      var bars = '';
      if (c.type === 'wifi') {
        bars = '<g class="mt-bq0" data-bq="' + esc(c.mac) + '" transform="translate(' + (it.cx + 14) + ' ' + (it.cy - 36) + ')" pointer-events="none"><rect x="-1" y="-1" width="24" height="16" rx="8" fill="var(--mt-glass2)" stroke="var(--mt-bd2)"/>';
        for (var b = 0; b < 4; b++) bars += '<rect class="mt-bar" x="' + (4 + b * 4) + '" y="' + (11 - b * 2.2) + '" width="2.8" height="' + (2.4 + b * 2.2) + '" rx="1"/>';
        bars += '</g>';
      }
      var lb2 = '<text class="mt-lb" x="' + it.cx + '" y="' + (it.cy + 43) + '" data-b="lb:c:' + esc(c.mac) + '"></text>';
      var ls2 = '<text class="mt-ls" x="' + it.cx + '" y="' + (it.cy + 58) + '" data-b="ls:c:' + esc(c.mac) + '"></text>';
      s += nodeG('c:' + c.mac, art, ax, ay, k, box, lb2, ls2, bars, 'mt-cl', clientTitle(c));
    });
  });
  if (sc.empty) {
    var ey = sc.mobile ? sc.hubY0 + 10 : sc.hubY + 60, ex2 = sc.mobile ? 210 : rt.cx;
    s += '<g class="mt-empty" transform="translate(' + ex2 + ' ' + ey + ')"><rect x="-130" y="-26" width="260" height="52" rx="26" fill="none" stroke="var(--mt-bd2)" stroke-dasharray="5 6"/><text y="5">暂无下挂设备</text></g>';
  }
  if (sc.fit && (sc.fit.s !== 1 || sc.fit.tx || sc.fit.ty)) s = '<g class="mt-fit" transform="translate(' + sc.fit.tx + ' ' + sc.fit.ty + ') scale(' + (Math.round(sc.fit.s * 1000) / 1000) + ')">' + s + '</g>';
  return s;
}

/* ---- 文字/信号绑定表 ---- */
function rateSum(e) { return '↓' + fmtRate(e.rx_bps) + ' ↑' + fmtRate(e.tx_bps); }
function wanPort(D) {
  var p = D.ports || [], e = D.egress || {}, i;
  for (i = 0; i < p.length; i++) if (e.dev && p[i].name === e.dev) return p[i];
  for (i = 0; i < p.length; i++) if (p[i].role === 'WAN') return p[i];
  return null;
}
function texts(D) {
  var T = {}, e = D.egress || {}, kind = KINDS[e.kind] ? e.kind : 'none', c = D.cell, u = D.uplink_wifi, usb = D.usb || {}, cnt = D.counts || {}, lan = D.lan || {};
  var cs = carrierOf(c);
  T['lb:cloud'] = '互联网';
  T['ls:cloud'] = kind === 'none' ? '无法访问' : (has(e.ip) ? '出口 ' + e.ip : '已连接');
  T['lb:router'] = '本设备';
  T['ls:router'] = modelName(D.model) + (lan.ip ? ' · ' + lan.ip : '');
  T['lb:tower'] = (c && c.operator ? c.operator : (kind === 'tether' ? '手机蜂窝' : '运营商')) + (kind === 'tether' ? '' : ' 基站');
  T['ls:tower'] = kind === 'tether' ? '移动网络' : ((c ? [ratText(c), c.band].filter(has).join(' · ') : '') || '蜂窝信号');
  T['lb:module'] = c && c.label || '5G 模组';
  T['ls:module'] = (c && c.module && c.module.model) || '蜂窝模组';
  var wb = u && (u.band ? u.band : ''), ssid = u && u.ssid;
  if (kind === 'wifi') {
    T['lb:up'] = ssid || '上游 WiFi';
    T['ls:up'] = u ? [u.band, has(u.channel) ? 'ch' + u.channel : '', num(u.rssi) != null ? u.rssi + ' dBm' : ''].filter(has).join(' · ') : '无连接信息';
  } else if (kind === 'wired' || kind === 'usbnic') {
    var isOnu = chainOf(D)[1] === 'onu';
    T['lb:up'] = isOnu ? '光猫 / 运营商网关' : '上级路由器';
    T['ls:up'] = has(e.gw) ? '网关 ' + e.gw : '—';
  } else {
    T['lb:up'] = '上游网络'; T['ls:up'] = '未连接';
  }
  var nic = usb.nic;
  T['lb:nic'] = 'USB 网卡';
  T['ls:nic'] = nic ? [nic.vendor, nic.speed_mbps ? fmtMbps(nic.speed_mbps) : ''].filter(has).join(' · ') : '—';
  var tf = usb.tether;
  T['lb:phone'] = tf && tf.kind === 'iphone' ? 'iPhone 共享' : (tf && tf.kind === 'android' ? '安卓手机共享' : '手机共享');
  T['ls:phone'] = tf ? [tf.dev, tf.ip].filter(has).join(' · ') : '—';
  /* 链路小标签 */
  T['p:bh'] = kind === 'none' ? '' : rateSum(e);
  var rat = c ? [ratText(c), c.band].filter(has).join(' · ') : '';
  T['p:rf'] = kind === 'cell' ? (rat || '射频') : (kind === 'tether' ? '手机蜂窝' : (u ? [u.band, has(u.channel) ? 'ch' + u.channel : '', num(u.rssi) != null ? u.rssi + ' dBm' : ''].filter(has).join(' · ') : '无线'));
  var ms = c && c.module && c.module.usb_speed;
  T['p:usb'] = kind === 'cell' ? (c && c.module && c.module.link ? c.module.link : ms ? usbSpeedText(ms).replace(/ ·.*$/, '') + (parseFloat(ms) >= 5000 ? ' · 5G' : ' · 480M') : 'USB') : (kind === 'usbnic' ? (nic && nic.speed_mbps ? fmtMbps(nic.speed_mbps) : 'USB') : 'USB 共享');
  var wp = wanPort(D);
  T['p:cb'] = [e.proto ? ({ pppoe: 'PPPoE', dhcp: 'DHCP', static: '静态 IP' }[e.proto] || e.proto) : '', wp && wp.up && wp.speed ? fmtMbps(wp.speed) : ''].filter(has).join(' · ') || '以太网';
  T['p:bad'] = '未连接';
  var g = groupsOf(D);
  T['hub:w'] = '有线 · ' + g.w.length + ' 台';
  T['hub:f'] = '无线 · ' + g.f.length + ' 台';
  T['more:n'] = '+' + g.hidden;
  (D.clients || []).forEach(function (cl) {
    T['lb:c:' + cl.mac] = cut(clientTitle(cl), 14);
    T['ls:c:' + cl.mac] = cl.ip || '—';
  });
  /* 顶部 */
  T['eg'] = KINDS[kind].t;
  T['sub'] = subTitle(D);
  T['ip'] = has(e.ip) ? e.ip : '—';
  T['down'] = fmtRate(e.rx_bps);
  T['up'] = fmtRate(e.tx_bps);
  T['cl'] = '有线 ' + (cnt.wired != null ? cnt.wired : g.w.length) + ' · 无线 ' + (cnt.wifi != null ? cnt.wifi : g.f.length);
  /* r2: 不可信长字符串统一截断(完整值在点开的详情面板里) */
  var LIM = { 'ls:cloud': 34, 'ls:router': 30, 'lb:tower': 26, 'ls:tower': 34, 'ls:module': 30, 'lb:up': 24, 'ls:up': 34, 'ls:nic': 34, 'ls:phone': 34, 'p:rf': 30, 'p:usb': 24, 'p:cb': 24, 'p:bh': 36, 'sub': 44, 'ip': 40 }, lk;
  (D.clients || []).forEach(function (cl) { T['ls:c:' + cl.mac] = cut(cl.ip || '—', 18); });
  for (lk in LIM) if (Object.prototype.hasOwnProperty.call(LIM, lk) && T[lk] != null) T[lk] = cut(T[lk], LIM[lk]);
  return T;
}
function subTitle(D) {
  var e = D.egress || {}, k = KINDS[e.kind] ? e.kind : 'none', c = D.cell, u = D.uplink_wifi, p = [];
  if (k === 'cell') { p.push(c && c.operator || '蜂窝'); if (c && c.rat) p.push(c.rat); }
  else if (k === 'wifi') { p.push(u && u.ssid || 'WiFi 桥接'); if (u && u.band) p.push(u.band); }
  else if (k === 'wired') p.push('WAN 有线接入');
  else if (k === 'usbnic') p.push(D.usb && D.usb.nic && D.usb.nic.dev || 'USB 网卡');
  else if (k === 'tether') p.push(D.usb && D.usb.tether && D.usb.tether.kind === 'iphone' ? 'iPhone 数据线共享' : '手机数据线共享');
  else p.push('当前没有可用的上网出口');
  return p.join(' · ');
}
function bars(D) {
  var B = {};
  (D.clients || []).forEach(function (cl) { if (cl.type === 'wifi') B[cl.mac] = barsOf(qRssi(num(cl.rssi))); });
  return B;
}

/* ======================================================================
   五、详情面板内容 (PC 右侧浮层 / 手机底部抽屉)
   ====================================================================== */
CSS += '\n.mt-pan .mt-stage{right:408px}.mt-mob.mt-pan .mt-stage{right:0}.mt-svg{touch-action:manipulation}' +
  '\n.mt-panel.swap .mt-pb,.mt-panel.swap .mt-ph{animation:mt-fade .3s ease both}' +
  '\n.mt-note{margin:10px 0 0;padding:10px 12px;border-radius:12px;background:var(--mt-glass);border:1px solid var(--mt-bd);font-size:12.5px;color:var(--mt-mut);line-height:1.6}' +
  '\n.mt-back{display:inline-flex;align-items:center;gap:4px;margin:8px 0 0;padding:4px 10px 4px 6px;border:1px solid var(--mt-bd);border-radius:14px;background:var(--mt-glass);font-size:12px;color:var(--mt-mut)}' +
  '\n.mt-back:hover{color:var(--mt-ink);border-color:var(--mt-bd2)}';

var KIND_ZH = { pc: '台式机', laptop: '笔记本', phone: '手机', tablet: '平板', tv: '电视', iot: '智能设备', printer: '打印机', console: '游戏机', camera: '摄像头', nas: '存储设备', speaker: '智能音箱', unknown: '未知设备' };
var DIAL_ZH = { connected: '已拨号', dialing: '拨号中', disconnected: '未拨号' };
var SIM_ZH = { ready: '就绪', absent: '未插卡', pin: '需 PIN 码', puk: '需 PUK 码', error: '异常' };
var PROTO_ZH = { pppoe: 'PPPoE 拨号', dhcp: 'DHCP 自动获取', 'static': '静态 IP' };

function RW(k, l, v) { return { k: k, l: l, v: has(v) ? String(v) : '—' }; }
function MR(k, l, val, unit, mn, mx, qf, lo, hi) {
  var v = num(val);
  return { k: k, l: l, v: v == null ? '—' : v + ' ' + unit, m: { p: v == null ? 0 : clamp01((v - mn) / (mx - mn)), tn: tone(v == null ? null : qf(v)), lo: lo != null ? lo : String(mn), hi: hi != null ? hi : mx + ' ' + unit } };
}
function levelTag(q) { return q == null ? null : (q >= 0.62 ? { t: '信号良好', tn: 'good' } : (q >= 0.34 ? { t: '信号一般', tn: 'mid' } : { t: '信号较差', tn: 'bad' })); }

/* 页面里已有的蜂窝卡片(#mrt-sec)作后备 —— 只读 DOM */
function domCellRows() {
  var out = [], sec = $('mrt-sec');
  if (!sec) return out;
  function tx(el) { return el ? String(el.textContent || '').replace(/\s+/g, ' ').replace(/^[●\s]+/, '').trim() : ''; }
  try {
    var items = sec.querySelectorAll('.cell-item'), i;
    for (i = 0; i < items.length; i++) {
      var l = tx(items[i].querySelector('.ci-label')), v = tx(items[i].querySelector('.ci-value'));
      if (l && v) out.push([l, v]);
    }
    var sims = sec.querySelectorAll('.sim-grid .ir');
    for (i = 0; i < sims.length; i++) {
      var k2 = tx(sims[i].querySelector('.ik')), v2 = tx(sims[i].querySelector('.iv'));
      if (k2 && v2) out.push([k2, v2]);
    }
    var mc = tx(sec.querySelector('.mod-cell-v.lg'));
    if (mc) out.unshift(['模组', mc]);
  } catch (e) { }
  return out;
}

function panelFor(id, D, S) {
  var e = D.egress || {}, kind = KINDS[e.kind] ? e.kind : 'none', c = D.cell, u = D.uplink_wifi, usb = D.usb || {}, lan = D.lan || {};
  var sp = null, secs = [], i;
  var tag = null, title = '', sub = '';
  function ratebits() { return [RW('rx', '下行速率', fmtRate(e.rx_bps)), RW('tx', '上行速率', fmtRate(e.tx_bps))]; }

  if (id === 'cloud') {
    title = '互联网'; sub = '出口 ' + (KINDS[kind].t);
    tag = kind === 'none' ? { t: '离线', tn: 'bad' } : { t: '在线', tn: 'good' };
    secs.push({ h: '出口', rows: [RW('k', '出口类型', KINDS[kind].t), RW('dev', '出口接口', e.dev), RW('ip', '出口 IP', e.ip), RW('gw', '网关', e.gw), RW('pr', '接入协议', PROTO_ZH[e.proto] || e.proto)] });
    secs.push({ h: '实时流量', rows: ratebits().concat([RW('rtt', '往返延迟', num(e.rtt_ms) != null ? e.rtt_ms + ' ms' : null)]) });
  } else if (id === 'router') {
    title = '本设备'; sub = modelName(D.model);
    var cn = D.counts || {};
    secs.push({ h: '局域网', rows: [RW('lip', 'LAN 地址', lan.ip ? lan.ip + (num(lan.mask) != null ? ' / ' + lan.mask : '') : null)] });
    var wr = [];
    wr.push(RW('s24', '2.4G 热点', lan.ssid24));
    wr.push(RW('c24', '2.4G 信道', has(lan.ch24) ? '信道 ' + lan.ch24 + (lan.w24 ? ' · ' + lan.w24 : '') : null));
    wr.push(RW('s5', '5G 热点', lan.ssid5));
    wr.push(RW('c5', '5G 信道', has(lan.ch5) ? '信道 ' + lan.ch5 + (lan.w5 ? ' · ' + lan.w5 : '') : null));
    secs.push({ h: '本机无线', rows: wr });
    var pr = (D.ports || []).map(function (p) { return RW('p:' + p.name, p.name + ' · ' + (p.role || 'LAN'), p.up ? fmtMbps(p.speed) : '未连接'); });
    if (pr.length) secs.push({ h: '网口', rows: pr });
    secs.push({ h: '在线设备', rows: [RW('nw', '有线', cn.wired), RW('nf', '无线', cn.wifi), RW('nt', '合计', cn.total)] });
    tag = { t: kind === 'none' ? '无出口' : KINDS[kind].t, tn: kind === 'none' ? 'bad' : 'good' };
  } else if (id === 'tower') {
    var isCell = kind === 'cell';
    if (isCell) {
      title = (c && c.operator || '运营商') + ' 基站'; sub = c ? [ratText(c), c.band].filter(has).join(' · ') : '蜂窝网络';
      if (c) {
        tag = levelTag(qRsrp(num(c.rsrp)));
        if (S.mob) secs.push({ h: '概览', rows: [RW('rsum', '信号', [num(c.rsrp) != null ? 'RSRP ' + c.rsrp + ' dBm' : '', num(c.sinr) != null ? 'SINR ' + c.sinr + ' dB' : '', tag ? tag.t : ''].filter(has).join(' · '))] });
        secs.push({ h: '基站', rows: [RW('op', '运营商', c.operator ? c.operator + (c.plmn ? '（' + c.plmn + '）' : '') : c.plmn), RW('rat', '网络制式', c.rat), RW('band', '频段', c.band), RW('arfcn', '频点 ARFCN', c.arfcn), RW('pci', '物理小区 PCI', c.pci), RW('cid', '小区 ID', c.cell_id)] });
        if (c.ca && c.ca.length) secs[0].rows.push({ k: 'ca', l: '载波聚合', chips: c.ca.map(String), v: c.ca.length + 'CC' });
        secs.push({ h: '信号质量', rows: [
          MR('rsrp', 'RSRP', c.rsrp, 'dBm', -140, -44, qRsrp, '-140', '-44 dBm'),
          MR('rsrq', 'RSRQ', c.rsrq, 'dB', -20, -3, qRsrq, '-20', '-3 dB'),
          MR('sinr', 'SINR', c.sinr, 'dB', -5, 30, qSinr, '-5', '30 dB')] });
      } else {
        var dr = domCellRows();
        secs.push({ h: '来自页面的蜂窝信息', rows: dr.length ? dr.map(function (p, ix) { return RW('d' + ix, p[0], p[1]); }) : [RW('na', '蜂窝数据', '暂未取到')] });
      }
    } else {
      title = '手机蜂窝'; sub = '手机通过自己的移动网络上网';
      secs.push({ h: '说明', rows: [RW('a', '接入方式', '数据线共享（USB 网络共享）'), RW('b', '手机', usb.tether && usb.tether.kind === 'iphone' ? 'iPhone' : '安卓手机')] });
      tag = { t: '共享中', tn: 'good' };
    }
  } else if (id === 'module') {
    title = c && c.label || '5G 模组'; sub = c && c.module && c.module.model || '蜂窝模组';
    if (c) {
      var dl = c.dial || {}, md = c.module || {}, sm = c.sim || {};
      tag = dl.state ? { t: DIAL_ZH[dl.state] || dl.state, tn: dl.state === 'connected' ? 'good' : (dl.state === 'dialing' ? 'mid' : 'bad') } : null;
      if (S.mob) secs.push({ h: '概览', rows: [RW('sum', '拨号', [DIAL_ZH[dl.state] || dl.state, c.operator, ratText(c), c.band].filter(has).join(' · '))] });
      secs.push({ h: '拨号信息', rows: [RW('st', '拨号状态', DIAL_ZH[dl.state] || dl.state), RW('ip', '拨号 IP', dl.ip), RW('dns', 'DNS', dl.dns), RW('apn', 'APN', dl.apn), RW('up', '在线时长', num(dl.uptime_s) != null ? fmtDur(dl.uptime_s) : null), RW('mtu', 'MTU', dl.mtu), RW('rx', '累计下行', num(dl.rx_bytes) != null ? fmtBytes(dl.rx_bytes) : null), RW('tx', '累计上行', num(dl.tx_bytes) != null ? fmtBytes(dl.tx_bytes) : null)] });
      secs.push({ h: '模组', rows: [RW('mm', '型号', md.model), RW('mv', '厂商', md.vendor), RW('mf', '固件版本', md.fw), RW('us', md.link ? '数据接口' : 'USB 接口', md.link || usbSpeedText(md.usb_speed)), RW('ud', 'USB 设备', md.usb_dev), RW('im', 'IMEI 尾号', md.imei_tail)] });
      secs.push({ h: 'SIM 卡', rows: [RW('sl', '卡槽', sm.slot), RW('ss', '状态', SIM_ZH[sm.state] || sm.state), RW('ic', 'ICCID 尾号', sm.iccid_tail)] });
    } else {
      var dr2 = domCellRows();
      secs.push({ h: '来自页面的模组信息', rows: dr2.length ? dr2.map(function (p, ix) { return RW('d' + ix, p[0], p[1]); }) : [RW('na', '模组数据', '暂未取到')] });
    }
  } else if (id === 'up') {
    if (kind === 'wifi') {
      title = u && u.ssid || '上游 WiFi'; sub = '上游 WiFi · 无线桥接';
      if (u) {
        var qq = qRssi(num(u.rssi));
        tag = u.state === 'up' ? (levelTag(qq) || { t: '已连接', tn: 'good' }) : { t: u.state === 'connecting' ? '连接中' : '已断开', tn: u.state === 'connecting' ? 'mid' : 'bad' };
        secs.push({ h: '连接', rows: [RW('stt', '连接状态', { up: '已连接', down: '已断开', connecting: '连接中' }[u.state] || u.state), RW('ssid', 'SSID', u.ssid), RW('bssid', 'BSSID', u.bssid), RW('sec', '加密方式', u.security), RW('if', '桥接接口', u.iface), RW('ls', '最近活跃', num(u.last_seen_s) != null ? (u.last_seen_s < 2 ? '刚刚' : u.last_seen_s + ' 秒前') : null)] });
        secs.push({ h: '射频', rows: [RW('band', '频段', u.band), RW('ch', '信道', u.channel), RW('wd', '带宽', u.width)] });
        secs.push({ h: '信号', rows: [
          MR('rssi', 'RSSI', u.rssi, 'dBm', -100, -30, qRssi, '-100', '-30 dBm'),
          RW('noise', '底噪', num(u.noise) != null ? u.noise + ' dBm' : null),
          MR('snr', 'SNR', u.snr, 'dB', 0, 40, qSnr, '0', '40 dB'),
          RW('rate', '协商速率', (num(u.tx_rate) != null || num(u.rx_rate) != null) ? '↑ ' + dash(u.tx_rate, 'Mbps') + '　↓ ' + dash(u.rx_rate, 'Mbps') : null)] });
      } else secs.push({ h: '连接', rows: [RW('na', '桥接状态', '暂无上联信息')] });
      secs.push({ h: '本机热点', rows: [RW('a24', '2.4G', lan.ssid24 ? lan.ssid24 + (has(lan.ch24) ? ' · 信道 ' + lan.ch24 : '') : null), RW('a5', '5G', lan.ssid5 ? lan.ssid5 + (has(lan.ch5) ? ' · 信道 ' + lan.ch5 : '') : null)] });
    } else if (kind === 'wired' || kind === 'usbnic') {
      var onu = chainOf(D)[1] === 'onu', wp = wanPort(D);
      title = onu ? '光猫 / 运营商网关' : '上级路由器'; sub = has(e.gw) ? '网关 ' + e.gw : '有线上联';
      tag = { t: '已连接', tn: 'good' };
      secs.push({ h: '上联', rows: [RW('dev', '上联接口', e.dev), RW('spd', '链路速率', wp && wp.up ? fmtMbps(wp.speed) : null), RW('pr', '接入方式', PROTO_ZH[e.proto] || e.proto), RW('gw', '网关', e.gw), RW('ip', '本机 IP', e.ip)] });
      secs.push({ h: '实时流量', rows: ratebits().concat([RW('rtt', '往返延迟', num(e.rtt_ms) != null ? e.rtt_ms + ' ms' : null)]) });
    } else {
      title = '上游网络'; sub = '当前没有可用的上网出口'; tag = { t: '未连接', tn: 'bad' };
      secs.push({ h: '状态', rows: [RW('s', '出口状态', '未连接')] });
      secs.push({ h: '', note: '请检查：网线或光猫是否接好、SIM 卡与模组是否识别、上游 WiFi 的桥接设置是否正确。' });
    }
  } else if (id === 'nic') {
    var nic = usb.nic || {};
    title = 'USB 网卡'; sub = [nic.vendor, nic.dev].filter(has).join(' · ') || 'USB 以太网适配器';
    tag = { t: { wan: '作上网出口', lan: '作局域网口', pending: '识别中' }[nic.role] || '已接入', tn: nic.role === 'pending' ? 'mid' : 'good' };
    secs.push({ h: '设备', rows: [RW('dev', '网卡接口', nic.dev), RW('ven', '厂商 / 型号', nic.vendor), RW('spd', '链路速率', nic.speed_mbps ? fmtMbps(nic.speed_mbps) : null), RW('role', '当前角色', { wan: '上网出口 (WAN)', lan: '局域网口 (LAN)', pending: '等待选择' }[nic.role] || nic.role)] });
    if (kind === 'usbnic') secs.push({ h: '上级网络', rows: [RW('gw', '网关', e.gw), RW('ip', '本机 IP', e.ip), RW('pr', '接入方式', PROTO_ZH[e.proto] || e.proto)].concat(ratebits()) });
  } else if (id === 'phone') {
    var tf = usb.tether || {};
    title = tf.kind === 'iphone' ? 'iPhone' : (tf.kind === 'android' ? '安卓手机' : '手机'); sub = 'USB 数据线网络共享';
    tag = { t: '共享中', tn: 'good' };
    secs.push({ h: '设备', rows: [RW('k', '设备类型', tf.kind === 'iphone' ? 'iPhone（个人热点 USB）' : (tf.kind === 'android' ? '安卓（USB 网络共享）' : '未识别')), RW('dev', '网络接口', tf.dev), RW('ip', '手机分配 IP', tf.ip), RW('gw', '网关', e.gw)] });
    secs.push({ h: '实时流量', rows: ratebits() });
  } else if (id === 'more') {
    var all = sortClients(D.clients || []), ws = [], fs = [];
    all.forEach(function (x) { (x.type === 'wifi' ? fs : ws).push(x); });
    title = '全部在线设备'; sub = '共 ' + all.length + ' 台 · 点击查看详情';
    function itm(x) { return { id: 'c:' + x.mac, kind: devKind(x), type: x.type, tk: 't:' + x.mac, sk: 's:' + x.mac, rk: 'r:' + x.mac, t: clientTitle(x), s: (x.ip || '—') + ' · ' + x.mac, r: x.type === 'wifi' ? (num(x.rssi) != null ? x.rssi + ' dBm' : x.band) : '有线' }; }
    if (ws.length) secs.push({ h: '有线 · ' + ws.length, items: ws.map(itm) });
    if (fs.length) secs.push({ h: '无线 · ' + fs.length, items: fs.map(itm) });
  } else if (id.indexOf('c:') === 0) {
    var mac = id.slice(2), cl = null;
    for (i = 0; i < (D.clients || []).length; i++) if (D.clients[i].mac === mac) cl = D.clients[i];
    if (!cl) return null;
    var wifi = cl.type === 'wifi', kd = devKind(cl);
    title = clientTitle(cl); sub = (KIND_ZH[kd] || '设备') + (cl.vendor && cl.name ? ' · ' + cl.vendor : '');
    tag = wifi ? (levelTag(qRssi(num(cl.rssi))) || { t: 'WiFi', tn: 'good' }) : { t: '有线', tn: 'good' };
    var rows = [RW('ip', 'IP 地址', cl.ip), RW('mac', 'MAC 地址', cl.mac), RW('ct', '连接方式', wifi ? 'WiFi 无线' : '有线以太网'), RW('pt', wifi ? '接入网络' : '接入端口', wifi ? (cl.band + (cl.guest ? ' · 访客网络' : '') + (cl.port ? ' · ' + cl.port : '')) : cl.port), RW('ven', '厂商', cl.vendor), RW('on', '在线时长', num(cl.online_s) != null ? fmtDur(cl.online_s) : null)];
    secs.push({ h: '设备', rows: rows });
    if (wifi) secs.push({ h: '无线信号', rows: [MR('rssi', 'RSSI', cl.rssi, 'dBm', -100, -30, qRssi, '-100', '-30 dBm'), RW('txr', '发送速率', num(cl.tx_rate) != null ? cl.tx_rate + ' Mbps' : null), RW('rxr', '接收速率', num(cl.rx_rate) != null ? cl.rx_rate + ' Mbps' : null)] });
  } else return null;
  /* r2: 手机抽屉高度有限, 信号分区提到基础信息之前(有"概览"摘要时排在它后面) */
  if (S.mob) {
    for (i = 0; i < secs.length; i++) {
      if (secs[i].h === '信号质量' || secs[i].h === '信号' || secs[i].h === '无线信号') {
        var sg = secs.splice(i, 1)[0], at = (secs[0] && secs[0].h === '概览') ? 1 : 0;
        secs.splice(at, 0, sg);
        break;
      }
    }
  }
  return { id: id, title: title, sub: sub, tag: tag, secs: secs };
}

function specSig(sp) {
  var k = [sp.id], bk = sp.back ? 'B' : '';
  sp.secs.forEach(function (s) {
    k.push('#' + s.h + (s.note ? '!' : ''));
    (s.rows || []).forEach(function (r) { k.push((r.m ? 'm' : (r.chips ? 'c' : 'r')) + r.k); });
    (s.items || []).forEach(function (r) { k.push('i' + r.id); });
  });
  return k.join('|') + bk;
}

/* ======================================================================
   六、运行时: DOM / 轮询 / 交互 / 生命周期
   ====================================================================== */
var S = {
  open: false, root: null, svg: null, stage: null, panel: null, load: null,
  data: null, key: '', sel: null, from: '', gen: 0, timer: 0, busy: false, fails: 0,
  mob: false, A: null, sc: null, arts: {}, psig: '', prevFocus: null, prevOverflow: '', closing: 0,
  mo: null, tmEnter: 0, pillNeed: false, qb: null, ro: null, vm: null, vmOrig: ''
};
var ICON_X = '<svg viewBox="0 0 24 24"><path d="M6 6l12 12M18 6L6 18"/></svg>';
var ICON_LOGO = '<svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="5" r="2.4"/><circle cx="5" cy="18" r="2.4"/><circle cx="19" cy="18" r="2.4"/><path d="M12 7.4v4.2M12 11.6L6 16M12 11.6l6 4.4"/></svg>';

function topoUrl() {
  var u = window.MWRT_TOPO_URL;
  if (u) return u;
  var m = location.pathname.match(/^(.*?)\/admin\//);
  return (m ? m[1] : '/cgi-bin/luci') + '/admin/modem/topo_data';
}
function isLight() { var p = $('P'); if (p) return p.classList.contains('L'); return !!document.querySelector('.P.L'); }
function injectCss() {
  var st = $('mt-css');
  if (!st) { st = document.createElement('style'); st.id = 'mt-css'; document.head.appendChild(st); }
  if (st.textContent !== CSS) st.textContent = CSS;
  var ac = $('mt-artcss');
  if (!ac) { ac = document.createElement('style'); ac.id = 'mt-artcss'; document.head.appendChild(ac); }
  var t = S.A.css || '';
  if (ac.textContent !== t) ac.textContent = t;
}
function stageSize() {
  /* s91: 覆盖层不再盖住侧栏/顶栏/底栏, 可用范围以覆盖层自身为准 */
  var r = $('mt-root');
  if (r && r.clientWidth > 0 && r.clientHeight > 0) return { w: r.clientWidth, h: r.clientHeight };
  var w = window.innerWidth || document.documentElement.clientWidth, h = window.innerHeight || document.documentElement.clientHeight;
  var cs = getComputedStyle(document.documentElement);
  w -= (parseFloat(cs.getPropertyValue('--mwrt-ov-l')) || 0) + (parseFloat(cs.getPropertyValue('--mwrt-ov-r')) || 0);
  h -= (parseFloat(cs.getPropertyValue('--mwrt-ov-t')) || 0) + (parseFloat(cs.getPropertyValue('--mwrt-ov-b')) || 0);
  return { w: w, h: h };
}

function buildRoot() {
  var r = document.createElement('div');
  r.className = 'mt-root ta-scope'; r.id = 'mt-root';
  r.setAttribute('role', 'dialog'); r.setAttribute('aria-modal', 'true'); r.setAttribute('aria-label', '网络拓扑');
  r.innerHTML =
    '<div class="mt-bg"></div>' +
    '<button type="button" class="mt-x" aria-label="关闭">' + ICON_X + '</button>' +
    '<div class="mt-top"><div class="mt-ttl"><span class="mt-logo">' + ICON_LOGO + '</span><div><h1>网络拓扑</h1><p data-b="sub">正在读取网络状态…</p></div></div>' +
    '<div class="mt-chips"><div class="mt-strip">' +
    '<span class="mt-chip mt-eg" id="mt-eg"><span class="mt-k">出口</span><b data-b="eg">—</b></span>' +
    '<span class="mt-chip"><span class="mt-k">IP</span><b data-b="ip">—</b></span>' +
    '<span class="mt-chip mt-rt"><span class="mt-ar d">↓</span><b data-b="down">—</b><span class="mt-ar u">↑</span><b data-b="up">—</b></span>' +
    '<span class="mt-chip"><span class="mt-k">在线</span><b data-b="cl">—</b></span>' +
    '</div><span class="mt-chip mt-stale">数据更新失败，重试中…</span>' +
    '</div></div>' +
    '<div class="mt-stage" id="mt-stage"><svg class="mt-svg" id="mt-svg" xmlns="' + NS + '" preserveAspectRatio="xMidYMid meet" aria-label="网络拓扑图"></svg>' +
    '<div class="mt-load"><div class="mt-spin"></div><span>正在读取网络状态…</span></div></div>' +
    '<aside class="mt-panel" id="mt-panel" aria-label="详情"></aside>';
  return r;
}

/* ---- 数据 ---- */
function fetchOnce(done) {
  var x = new XMLHttpRequest(), u = topoUrl();
  x.open('GET', u + (u.indexOf('?') >= 0 ? '&' : '?') + '_=' + Date.now(), true);
  x.timeout = 8000;
  x.setRequestHeader('Accept', 'application/json');
  x.onreadystatechange = function () {
    if (x.readyState !== 4) return;
    var d = null, err = '';
    if (x.status === 200) {
      try { d = JSON.parse(x.responseText); } catch (e) { err = '数据格式错误'; }
      if (d && d.ok === false) { err = d.error || '后端返回失败'; d = null; }
    } else if (x.status === 403 || x.status === 401) err = '登录已过期，请刷新页面重新登录';
    else err = x.status ? '请求失败 (' + x.status + ')' : '网络不通或请求超时';
    done(d, err);
  };
  try { x.send(); } catch (e2) { done(null, '请求发送失败'); }
}
function poll() {
  if (!S.open || S.busy) return;
  clearTimeout(S.timer); S.timer = 0;
  if (document.hidden) return;   /* 页面隐藏: 不拉; 可见时由 visibilitychange 接上 */
  var gen = ++S.gen;
  S.busy = true;
  fetchOnce(function (d, err) {
    S.busy = false;
    if (!S.open || gen !== S.gen) return;   /* 代数作废: 关闭/重开后的旧回包丢掉 */
    if (d) {
      S.fails = 0; S.root.classList.remove('mt-st');
      try { apply(d); } catch (e) { if (window.console) console.error('[MwrtTopo]', e); }
    } else {
      S.fails++;
      if (!S.data) showLoadError(err);
      else if (S.fails >= 2) { S.root.classList.add('mt-st'); syncTh(); }
    }
    if (S.open && !document.hidden) S.timer = setTimeout(poll, POLL_MS);
  });
}
function showLoadError(msg) {
  var l = S.load;
  l.classList.remove('hid');
  l.innerHTML = '<span style="color:var(--mt-bad)">' + esc(msg || '读取失败') + '</span><button type="button" data-retry="1">重试</button>';
}

/* ---- 渲染 ---- */
function vhFor() {
  var s = stageSize();
  return Math.max(620, Math.min(880, Math.round(PC_W * (s.h - 68) / s.w)));
}
/* 顶栏高度实测写入 --mt-th(手机上 chip 换行后高度会变), 舞台与面板据此下移 */
function syncTh() {
  if (!S.root) return;
  var t = S.root.querySelector('.mt-top');
  if (!t) return;
  var h = Math.ceil(t.getBoundingClientRect().height);
  if (h >= 40 && S.root._th !== h) { S.root._th = h; S.root.style.setProperty('--mt-th', h + 'px'); }
}
function apply(D) {
  scrub(D);
  var eg=D.egress||{}, now=Date.now(), prev=S.data&&S.data.egress;
  if(D!==S.data&&prev&&prev.dev===eg.dev&&S.sampleTime&&now>S.sampleTime){
    var dt=(now-S.sampleTime)/1000;
    if(num(eg.rx_bytes)!=null&&num(prev.rx_bytes)!=null&&eg.rx_bytes>=prev.rx_bytes)eg.rx_bps=(eg.rx_bytes-prev.rx_bytes)*8/dt;
    if(num(eg.tx_bytes)!=null&&num(prev.tx_bytes)!=null&&eg.tx_bytes>=prev.tx_bytes)eg.tx_bps=(eg.tx_bytes-prev.tx_bytes)*8/dt;
  }
  if(D!==S.data)S.sampleTime=now;
  S.data = D;
  S.mob = stageSize().w < 760;
  S.root.classList.toggle('mt-mob', S.mob);
  var vh = vhFor(), key = structKey(D, S.mob, vh);
  if (key !== S.key) rebuild(D, vh, !S.key);
  S.key = key;
  refresh(D);
  if (S.sel) refreshPanel(D);
  S.load.classList.add('hid');
  syncTh();
}
function bindArt(root) {
  try { if (window.MwrtTopoArt && window.MwrtTopoArt.bindFallback) window.MwrtTopoArt.bindFallback(root); } catch (e) { }
}
function rebuild(D, vh, first) {
  var A = S.A, sc = layout(D, A, S.mob, vh), svg = S.svg;
  S.sc = sc;
  /* r2: 重建会销毁当前获焦节点, 先记下它的 data-id, 重建后还原 */
  var ae = document.activeElement, fid = (ae && ae !== document.body && svg.contains(ae) && ae.getAttribute) ? ae.getAttribute('data-id') : null;
  var inner = '<defs>' + A.defs() + '</defs>' + renderScene(sc, A, first);
  svg.setAttribute('viewBox', '0 0 ' + sc.VW + ' ' + Math.round(sc.VH));
  if (S.mob) svg.style.height = Math.round(S.stage.clientWidth * sc.VH / sc.VW) + 'px'; else svg.style.height = '';
  svg.innerHTML = inner;
  bindArt(svg);
  if (fid) {
    var nf = svg.querySelector('[data-id="' + qa(fid) + '"]');
    if (nf) { try { nf.focus({ preventScroll: true }); } catch (e1) { try { nf.focus(); } catch (e2) { } } }
  }
  /* 图元登记(详情面板头部的小图要用) */
  S.arts = {};
  sc.nodes.forEach(function (n) { S.arts[n.id] = n.art; });
  sc.groups.forEach(function (gr) { gr.items.forEach(function (it) { if (it.kind === 'c') S.arts['c:' + it.c.mac] = A.client(devKind(it.c), {}); }); });
  if (first) {
    S.root.classList.add('mt-enter');
    clearTimeout(S.tmEnter);
    S.tmEnter = setTimeout(function () { if (S.root) S.root.classList.remove('mt-enter'); }, 1800);
  } else {
    svg.setAttribute('class', 'mt-svg'); void svg.getBoundingClientRect(); svg.setAttribute('class', 'mt-svg mt-fresh');
  }
  S.pillNeed = true;
  markSel();
}
function refresh(D) {
  var T = texts(D), B = bars(D), root = S.root, i;
  var els = root.querySelectorAll('[data-b]');
  for (i = 0; i < els.length; i++) {
    var el = els[i], k = el.getAttribute('data-b'), v = T[k];
    if (v === undefined) continue;
    if (el._v !== v) { el._v = v; el.textContent = v; el._d = 1; }
  }
  /* 链路标签: 文字变了就按文字宽度重排底板, 空文字则隐藏 */
  var pills = S.svg.querySelectorAll('.mt-pill');
  for (i = 0; i < pills.length; i++) {
    var tx = pills[i].querySelector('text');
    if (!tx._d && !S.pillNeed) continue;
    tx._d = 0;
    var s = tx._v == null ? tx.textContent : tx._v;
    pills[i].style.display = s ? '' : 'none';
    if (s) {
      var w = 64;
      try { w = Math.max(64, Math.ceil(tx.getComputedTextLength()) + 26); } catch (e) { w = Math.max(64, String(s).length * 7 + 26); }
      var rc = pills[i].querySelector('rect');
      rc.setAttribute('x', -w / 2); rc.setAttribute('width', w);
    }
  }
  S.pillNeed = false;
  var bq = S.svg.querySelectorAll('[data-bq]');
  for (i = 0; i < bq.length; i++) {
    var n = B[bq[i].getAttribute('data-bq')] || 0;
    if (bq[i]._n !== n) {
      bq[i]._n = n; bq[i].setAttribute('class', 'mt-bq' + n);
      var rs = bq[i].querySelectorAll('.mt-bar');
      for (var j = 0; j < rs.length; j++) rs[j].setAttribute('class', 'mt-bar' + (j < n ? ' on' : ''));
    }
  }
  var e = D.egress || {}, kind = KINDS[e.kind] ? e.kind : 'none', cell = D.cell;
  var egc = kind === 'cell' ? carrierOf(cell).color : KINDS[kind].c;
  $('mt-eg').style.setProperty('--mt-k', egc);
}

/* ---- 选中 / 面板 ---- */
function markSel() {
  var ns = S.svg.querySelectorAll('.mt-node'), i;
  for (i = 0; i < ns.length; i++) {
    var on = ns[i].getAttribute('data-id') === S.sel;
    var c = ns[i].getAttribute('class') || '';
    var has_ = (' ' + c + ' ').indexOf(' sel ') >= 0;
    if (on && !has_) ns[i].setAttribute('class', c + ' sel');
    else if (!on && has_) ns[i].setAttribute('class', c.replace(/\s*\bsel\b/, ''));
  }
}
function select(id, fromList) {
  if (!S.data) return;
  var same = S.sel === id;
  S.from = fromList ? 'more' : '';
  S.sel = id;
  S.psig = '';
  markSel();
  var spec = panelFor(id, S.data, S);
  if (!spec) { unselect(); return; }
  S.root.classList.add('mt-pan');
  S.panel.classList.add('on');
  if (!same || fromList) { S.panel.classList.remove('swap'); void S.panel.offsetWidth; S.panel.classList.add('swap'); }
  refreshPanel(S.data);
  if (S.mob) {
    S.stage.style.paddingBottom = '68vh';
    scrollToSel();
  }
}
function scrollToSel() {
  var nd = S.svg.querySelector('.mt-node.sel');
  if (!nd || !S.stage) return;
  try {
    var bb = nd.getBoundingClientRect(), sr = S.stage.getBoundingClientRect();
    var want = bb.top - sr.top + S.stage.scrollTop - 12;
    S.stage.scrollTop = Math.max(0, want - 4);
  } catch (e) { }
}
function unselect(refocus) {
  var id = S.sel;
  S.sel = null; S.psig = '';
  markSel();
  if (!S.panel) return;
  S.panel.classList.remove('on');
  S.root.classList.remove('mt-pan');
  if (S.mob) S.stage.style.paddingBottom = '';
  if (refocus && id && S.svg) {   /* 面板关闭后把焦点还给原节点(列表里的设备可能不在图上, 退回"更多设备") */
    var n = S.svg.querySelector('[data-id="' + qa(id) + '"]') || S.svg.querySelector('[data-id="more"]');
    if (n) { try { n.focus({ preventScroll: true }); } catch (e) { } }
  }
}
function miniArt(id) {
  var a = S.arts[id];
  if (!a && id === 'more') return '<svg viewBox="0 0 48 48" fill="none" stroke="var(--mt-acc)" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"><rect x="8" y="10" width="22" height="16" rx="3"/><path d="M14 32h10M19 26v6"/><rect x="33" y="18" width="9" height="18" rx="2.5"/></svg>';
  if (!a) return '';
  return '<svg viewBox="0 0 ' + a.w + ' ' + a.h + '" preserveAspectRatio="xMidYMid meet">' + a.svg + '</svg>';
}
function buildPanel(sp) {
  var h = '<div class="mt-ph"><div class="mt-pi">' + miniArt(sp.id) + '</div><div class="mt-pt"><h2><span data-pk="title"></span><span class="mt-tg" data-pk="tag" style="display:none"></span></h2><p data-pk="sub"></p>' +
    (S.from === 'more' ? '<button type="button" class="mt-back" data-back="1">‹ 全部设备</button>' : '') + '</div>' +
    '<button type="button" class="mt-pc" aria-label="关闭详情" data-pclose="1">' + ICON_X + '</button></div><div class="mt-pb">';
  sp.secs.forEach(function (s) {
    if (s.h) h += '<div class="mt-sh">' + esc(s.h) + '</div>';
    if (s.note) h += '<p class="mt-note">' + esc(s.note) + '</p>';
    (s.rows || []).forEach(function (r) {
      if (r.m) {
        h += '<div class="mt-rm"><div class="mt-rt"><span>' + esc(r.l) + '</span><b data-pk="' + esc(r.k) + '"></b></div><div class="mt-mt" data-pm="' + esc(r.k) + '"><i></i></div><div class="mt-mscale"><span>' + esc(r.m.lo) + '</span><span>' + esc(r.m.hi) + '</span></div></div>';
      } else if (r.chips) {
        h += '<div class="mt-r"><span>' + esc(r.l) + '</span><div class="mt-chipr" data-pc="' + esc(r.k) + '"></div></div>';
      } else {
        h += '<div class="mt-r"><span>' + esc(r.l) + '</span><b data-pk="' + esc(r.k) + '"></b></div>';
      }
    });
    (s.items || []).forEach(function (it) {
      var art = S.arts[it.id] || (S.A.client(it.kind, {}));
      h += '<button type="button" class="mt-li" data-li="' + esc(it.id) + '"><svg viewBox="0 0 ' + art.w + ' ' + art.h + '">' + art.svg + '</svg><div><strong data-pk="' + esc(it.tk) + '"></strong><small data-pk="' + esc(it.sk) + '"></small></div><em data-pk="' + esc(it.rk) + '"></em></button>';
    });
  });
  S.panel.innerHTML = h + '</div>';
  bindArt(S.panel);
}
/* 面板文字: 超长截断显示, 完整值放 title; lim 为宽度上限(汉字按 2 计) */
function setPk(k, v, lim) {
  var el = S.panel.querySelector('[data-pk="' + qa(k) + '"]');
  if (el && el._v !== v) {
    el._v = v;
    var d = lim ? cut(v, lim) : v;
    el.textContent = d;
    if (d !== v) el.setAttribute('title', v); else el.removeAttribute('title');
  }
}
function refreshPanel(D) {
  var sp = panelFor(S.sel, D, S);
  if (!sp) { unselect(); return; }
  sp.back = S.from === 'more';
  var sig = specSig(sp);
  if (sig !== S.psig) { buildPanel(sp); S.psig = sig; }
  setPk('title', sp.title, 44); setPk('sub', sp.sub, 60);
  var tg = S.panel.querySelector('[data-pk="tag"]');
  if (tg) {
    if (sp.tag) {
      tg.style.display = ''; if (tg._v !== sp.tag.t) { tg._v = sp.tag.t; tg.textContent = sp.tag.t; }
      var cn = 'mt-tg ' + sp.tag.tn; if (tg.getAttribute('class') !== cn) tg.setAttribute('class', cn);
    } else tg.style.display = 'none';
  }
  sp.secs.forEach(function (s) {
    (s.rows || []).forEach(function (r) {
      if (r.m) {
        setPk(r.k, r.v, 56);
        var mt = S.panel.querySelector('[data-pm="' + qa(r.k) + '"]');
        if (mt) {
          var cls = 'mt-mt ' + r.m.tn; if (mt.getAttribute('class') !== cls) mt.setAttribute('class', cls);
          var bar = mt.firstChild, wv = Math.round(r.m.p * 100) + '%';
          if (bar.style.width !== wv) bar.style.width = wv;
        }
      } else if (r.chips) {
        var ce = S.panel.querySelector('[data-pc="' + qa(r.k) + '"]'), hh = r.chips.map(function (x) { return '<em>' + esc(x) + '</em>'; }).join('');
        if (ce && ce._h !== hh) { ce._h = hh; ce.innerHTML = hh; }
      } else setPk(r.k, r.v, 56);
    });
    (s.items || []).forEach(function (it) { setPk(it.tk, it.t, 32); setPk(it.sk, it.s, 56); setPk(it.rk, it.r, 16); });
  });
}

/* ---- 事件 ---- */
function onClick(ev) {
  var t = ev.target;
  if (closest(t, 'mt-x')) { close(); return; }
  if (t.getAttribute && t.getAttribute('data-retry')) { S.load.innerHTML = '<div class="mt-spin"></div><span>正在读取网络状态…</span>'; S.fails = 0; poll(); return; }
  var pc = closest(t, 'mt-pc');
  if (pc) { unselect(true); return; }
  var bk = closest(t, 'mt-back');
  if (bk) { select('more'); return; }
  var li = closest(t, 'mt-li');
  if (li) { select(li.getAttribute('data-li'), true); return; }
  var nd = closest(t, 'mt-node');
  if (nd) {
    var id = nd.getAttribute('data-id');
    if (S.sel === id && !S.from) unselect(); else select(id);
    return;
  }
  if (closest(t, 'mt-panel') || closest(t, 'mt-top') || closest(t, 'mt-chip')) return;
  if (S.sel) unselect();
}
/* 可见且可聚焦的元素(排除 visibility:hidden 的面板/手机上被隐藏的顶栏关闭钮) */
function focusables() {
  var list = S.root.querySelectorAll('button,[tabindex="0"]'), vis = [], i;
  for (i = 0; i < list.length; i++) {
    var r = list[i].getBoundingClientRect(), cs = null;
    if (!(r.width > 0 && r.height > 0) || list[i].disabled) continue;
    try { cs = window.getComputedStyle(list[i]); } catch (e) { }
    if (cs && (cs.visibility === 'hidden' || cs.display === 'none')) continue;
    vis.push(list[i]);
  }
  return vis;
}
/* r2: 焦点一旦跑到覆盖层之外(比如被销毁的节点让焦点掉到 BODY 之外的元素), 立即拉回, 不等下一次按键 */
function onFocusIn(ev) {
  if (!S.open || !S.root) return;
  var t = ev.target;
  if (t && t.nodeType === 1 && !S.root.contains(t)) {
    var v = focusables();
    if (v.length) v[0].focus();
  }
}
function onKey(ev) {
  if (!S.open) return;
  var k = ev.key || ev.keyCode;
  if (k === 'Escape' || k === 'Esc' || k === 27) {
    ev.preventDefault(); ev.stopPropagation();
    if (S.sel) unselect(true); else close();   /* r2: 先关详情面板, 再关整个浮层 */
    return;
  }
  if ((k === 'Enter' || k === ' ' || k === 13 || k === 32) && ev.target && ev.target.getAttribute && ev.target.getAttribute('class') && closest(ev.target, 'mt-node') === ev.target) {
    ev.preventDefault(); onClick({ target: ev.target }); return;
  }
  if (k === 'Tab' || k === 9) {
    var vis = focusables();
    if (!vis.length) return;
    var first = vis[0], last = vis[vis.length - 1], cur = document.activeElement;
    if (!S.root.contains(cur)) { ev.preventDefault(); first.focus(); }
    else if (ev.shiftKey && cur === first) { ev.preventDefault(); last.focus(); }
    else if (!ev.shiftKey && cur === last) { ev.preventDefault(); first.focus(); }
  }
}
var rzT = 0;
function onResize() {
  clearTimeout(rzT);
  rzT = setTimeout(function () {
    if (!S.open || !S.data) return;
    S.key = '';   /* 视口变了: 强制按新尺寸重排 */
    S.data && apply(S.data);
    if (S.sel) markSel();
  }, 140);
}
function onVis() {
  if (!S.open) return;
  S.root.classList.toggle('mt-pause', document.hidden);
  S.root.classList.toggle('ta-paused', document.hidden);
  if (document.hidden) { clearTimeout(S.timer); S.timer = 0; }
  else if (!S.busy) poll();
}
function applySkin() { if (S.root) { var l = isLight(); S.root.classList.toggle('mt-l', l); S.root.classList.toggle('ta-light', l); } }

/* ---- 生命周期 ---- */
function open() {
  try {
    if (S.open) return;
    if (S.closing) { clearTimeout(S.closing); S.closing = 0; if (S.closingEl && S.closingEl.parentNode) S.closingEl.parentNode.removeChild(S.closingEl); S.closingEl = null; }
    S.A = pickArt();
    injectCss();
    S.root = buildRoot();
    document.body.appendChild(S.root);
    S.svg = $('mt-svg'); S.stage = $('mt-stage'); S.panel = $('mt-panel'); S.load = S.root.querySelector('.mt-load');
    S.data = null; S.key = ''; S.sel = null; S.from = ''; S.psig = ''; S.fails = 0; S.busy = false; S.qb = null;
    S.mob = stageSize().w < 760;
    S.root.classList.toggle('mt-mob', S.mob);
    applySkin();
    S.prevFocus = document.activeElement;
    S.prevOverflow = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    S.open = true;
    S.root.addEventListener('click', onClick);
    document.addEventListener('keydown', onKey, true);
    document.addEventListener('focusin', onFocusIn, true);
    window.addEventListener('resize', onResize);
    window.addEventListener('orientationchange', onResize);
    document.addEventListener('visibilitychange', onVis);
    var p = $('P');
    if (p && window.MutationObserver) { S.mo = new MutationObserver(applySkin); S.mo.observe(p, { attributes: true, attributeFilter: ['class'] }); }
    /* r2: 让 env(safe-area-inset-*) 在刘海屏上生效(关闭时还原) */
    try {
      var vm = document.querySelector('meta[name="viewport"]');
      if (vm) {
        var vc = vm.getAttribute('content') || '';
        if (vc.indexOf('viewport-fit') < 0) { S.vm = vm; S.vmOrig = vc; vm.setAttribute('content', vc + (vc ? ',' : '') + 'viewport-fit=cover'); }
      }
    } catch (e3) { }
    if (window.ResizeObserver) {
      try { S.ro = new ResizeObserver(syncTh); S.ro.observe(S.root.querySelector('.mt-top')); } catch (e4) { S.ro = null; }
    }
    syncTh();
    requestAnimationFrame(function () { if (S.root) S.root.classList.add('mt-on'); });
    var cb = S.root.querySelector('.mt-x'); if (cb) cb.focus();
    poll();
  } catch (e) {
    if (window.console) console.error('[MwrtTopo] open', e);
    try { close(); } catch (e2) { }
  }
}
function close() {
  if (!S.open && !S.root) return;
  S.open = false; S.gen++; S.busy = false;
  clearTimeout(S.timer); S.timer = 0; clearTimeout(S.tmEnter);
  document.removeEventListener('keydown', onKey, true);
  document.removeEventListener('focusin', onFocusIn, true);
  window.removeEventListener('resize', onResize);
  window.removeEventListener('orientationchange', onResize);
  document.removeEventListener('visibilitychange', onVis);
  if (S.mo) { S.mo.disconnect(); S.mo = null; }
  if (S.ro) { S.ro.disconnect(); S.ro = null; }
  if (S.vm) { try { S.vm.setAttribute('content', S.vmOrig); } catch (e5) { } S.vm = null; }
  document.body.style.overflow = S.prevOverflow || '';
  var r = S.root;
  if (r) {
    r.classList.remove('mt-on');
    S.closingEl = r; S.closing = setTimeout(function () { S.closing = 0; S.closingEl = null; if (r.parentNode) r.parentNode.removeChild(r); }, 260);
  }
  S.root = null; S.svg = null; S.stage = null; S.panel = null; S.load = null; S.data = null; S.sc = null; S.arts = {};
  var pf = S.prevFocus; S.prevFocus = null;
  try { if (pf && pf.focus) pf.focus(); } catch (e) { }
}

window.MwrtTopo = {
  __mt: 1, ver: 1,
  open: open, close: close,
  isOpen: function () { return !!S.open; }
};
})();
