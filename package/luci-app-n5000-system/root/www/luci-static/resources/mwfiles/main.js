/* Mwrt 文件管理浮窗
   由 boot.js 首次点击时加载, 也可由 admin/mwfiles 落地页加载。后端: admin/services/mwfiles/*。
   只展示后端真实支持的能力: 进度来自真实字节/项数, 预览只用浏览器原生能力, 不可信内容只以文本或附件方式处理。
   界面全部用 DOM/textContent 构造或经 esc() 转义, 文件名等不可信文本不会作为 HTML 执行。 */
(function() {
	'use strict';
	if (window.MwrtFiles) return;

	var cs = document.currentScript;
	var SRC = (cs && cs.src) || '';
	var BASE = SRC ? SRC.replace(/[?#].*$/, '').replace(/[^\/]*$/, '') : (((window.L && L.env && L.env.resource) || '/luci-static/resources') + '/mwfiles/');
	var VER = (SRC.match(/[?&]v=([^&]*)/) || [0, '1'])[1];
	var COARSE = !!(window.matchMedia && matchMedia('(hover: none)').matches);

	/* ---------- 图标(本地 SVG, 无 emoji) ---------- */
	var IP = {
		folder: 'M3 7a2 2 0 0 1 2-2h4l2 2.5h8a2 2 0 0 1 2 2V17a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z',
		file: 'M6 3h8l5 5v11a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z M14 3v5h5',
		image: 'M4 5h16a1 1 0 0 1 1 1v12a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1z M8.5 10.5h.01 M21 16l-5-5-8 8',
		video: 'M3 6.5A1.5 1.5 0 0 1 4.5 5h9A1.5 1.5 0 0 1 15 6.5v11a1.5 1.5 0 0 1-1.5 1.5h-9A1.5 1.5 0 0 1 3 17.5z M15 10.5l6-3.5v10l-6-3.5',
		audio: 'M9 18V6l10-2v12 M9 18a3 3 0 1 1-6 0 3 3 0 0 1 6 0z M19 16a3 3 0 1 1-6 0 3 3 0 0 1 6 0z',
		doc: 'M6 3h8l5 5v11a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z M14 3v5h5 M8 13h8 M8 17h6',
		archive: 'M4 4h16v4H4z M5 8v11a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V8 M10 12h4',
		code: 'M6 3h8l5 5v11a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z M14 3v5h5 M10 12l-2 2 2 2 M14 12l2 2-2 2',
		back: 'M15 5l-7 7 7 7', forward: 'M9 5l7 7-7 7', up: 'M12 19V5 M6 11l6-6 6 6',
		refresh: 'M20 12a8 8 0 1 1-3-6.2 M20 4v5h-5',
		search: 'M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14z M20 20l-4-4',
		close: 'M6 6l12 12 M18 6L6 18',
		list: 'M8 6h12 M8 12h12 M8 18h12 M4 6h.01 M4 12h.01 M4 18h.01',
		grid: 'M4 4h6v6H4z M14 4h6v6h-6z M4 14h6v6H4z M14 14h6v6h-6z',
		upload: 'M12 16V4 M7 9l5-5 5 5 M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3',
		download: 'M12 4v12 M7 11l5 5 5-5 M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3',
		newfolder: 'M3 7a2 2 0 0 1 2-2h4l2 2.5h8a2 2 0 0 1 2 2V17a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z M12 11v5 M9.5 13.5h5',
		newfile: 'M6 3h8l5 5v11a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z M14 3v5h5 M12 12v6 M9 15h6',
		edit: 'M4 20h4L19 9l-4-4L4 16z M13.5 6.5l4 4',
		copy: 'M9 9h10a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H9a1 1 0 0 1-1-1V10a1 1 0 0 1 1-1z M5 15H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v1',
		cut: 'M6 9a3 3 0 1 0 0-.01 M6 15a3 3 0 1 0 0-.01 M8.5 8L20 18 M8.5 16L20 6',
		paste: 'M9 4h6v3H9z M7 5H6a1 1 0 0 0-1 1v14a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V6a1 1 0 0 0-1-1h-1',
		trash: 'M4 7h16 M9 7V4h6v3 M6 7l1 13h10l1-13 M10 11v5 M14 11v5',
		info: 'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M12 11v6 M12 7.5h.01',
		star: 'M12 3l2.7 5.6 6.1.9-4.4 4.3 1 6.1L12 17l-5.4 2.9 1-6.1L3.2 9.5l6.1-.9z',
		clock: 'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M12 7v5l3 2',
		hdd: 'M3 14h18v5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z M3 14l2.5-8h13L21 14 M7 17h.01 M11 17h4',
		usb: 'M12 3.2v14.4 M9.6 6l2.4-3 2.4 3 M12 17.8a1.8 1.8 0 1 0 0 3.6 1.8 1.8 0 0 0 0-3.6z M12 14.4L7.4 11.6V9.6 M7.4 6.6a1.4 1.4 0 1 0 0 2.8 1.4 1.4 0 0 0 0-2.8z M12 11.8l4.6-2.8V7.8 M15.3 5.2h2.6v2.6h-2.6z',
		plus: 'M12 5v14 M5 12h14',
		network: 'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M3 12h18 M12 3c3 3 3 15 0 18 M12 3c-3 3-3 15 0 18',
		share: 'M18 8a3 3 0 1 0 0-.01 M6 15a3 3 0 1 0 0-.01 M18 20a3 3 0 1 0 0-.01 M8.7 13.5l6.6 4 M15.3 6.5l-6.6 4',
		more: 'M5 12h.01 M12 12h.01 M19 12h.01',
		menu: 'M4 7h16 M4 12h16 M4 17h16',
		chev: 'M9 6l6 6-6 6',
		check: 'M5 12.5l4.5 4.5L19 7.5',
		sortup: 'M6 14l6-6 6 6', sortdown: 'M6 10l6 6 6-6',
		warn: 'M12 4l9 16H3z M12 10v5 M12 17.5h.01',
		lock: 'M6 11h12v9H6z M8.5 11V8a3.5 3.5 0 0 1 7 0v3',
		zoomin: 'M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14z M20 20l-4-4 M11 8v6 M8 11h6',
		zoomout: 'M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14z M20 20l-4-4 M8 11h6',
		rotcw: 'M20 12a8 8 0 1 1-3-6.2 M20 4v5h-5', rotccw: 'M4 12a8 8 0 1 0 3-6.2 M4 4v5h5',
		external: 'M14 4h6v6 M20 4l-9 9 M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5',
		gear: 'M4 7h9 M17 7h3 M4 17h3 M11 17h9 M15 4.5v5 M9 14.5v5',
		tasks: 'M7 4v14 M3 14l4 4 4-4 M17 20V6 M13 10l4-4 4 4',
		plug: 'M9 3v5 M15 3v5 M7 8h10v4a5 5 0 0 1-10 0z M12 17v4',
		link: 'M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1 M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1',
		okc: 'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M8 12.5l3 3 5-6',
		badc: 'M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M9 9l6 6 M15 9l-6 6',
		cloudup: 'M7 18a4 4 0 0 1-.6-7.96A5.5 5.5 0 0 1 17 9a4.5 4.5 0 0 1 0 9 M12 17v-6 M9 13.5l3-3 3 3',
		wrap: 'M4 6h16 M4 12h12a3 3 0 0 1 0 6h-4 M14 15.5l-2.5 2.5 2.5 2.5 M4 18h4'
	};
	var FILLED = { folder: 1, file: 1, image: 1, video: 1, audio: 1, doc: 1, archive: 1, code: 1, hdd: 1 };
	var SOLID = { starfill: 'star' };
	var SW = { more: 3.4 };
	function ic(n, cls) {
		var solid = SOLID[n], p = IP[solid || n] || IP.file;
		var fill = solid ? 'currentColor' : (FILLED[n] ? 'currentColor' : 'none');
		return '<span class="mf-i' + (cls ? ' ' + cls : '') + '"><svg viewBox="0 0 24 24" fill="' + fill + '"' + (FILLED[n] ? ' fill-opacity=".14"' : '') +
			' stroke="currentColor" stroke-width="' + (SW[n] || 1.7) + '" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="' + p + '"/></svg></span>';
	}

	/* ---------- 小工具 ---------- */
	function esc(s) {
		return String(s == null ? '' : s).replace(/[&<>"']/g, function(c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]; });
	}
	function fmtSize(n) {
		n = Number(n) || 0;
		if (n < 1024) return n + ' B';
		var u = ['KB', 'MB', 'GB', 'TB'], i = -1;
		do { n /= 1024; i++; } while (n >= 1024 && i < 3);
		var s = n.toFixed(n >= 100 ? 0 : n >= 10 ? 1 : 2);
		if (s.indexOf('.') >= 0) s = s.replace(/0+$/, '').replace(/\.$/, '');
		return s + ' ' + u[i];
	}
	function p2(n) { return n < 10 ? '0' + n : '' + n; }
	function fmtTime(s) {
		if (!s) return '—';
		var d = new Date(s * 1000);
		return d.getFullYear() + '-' + p2(d.getMonth() + 1) + '-' + p2(d.getDate()) + ' ' + p2(d.getHours()) + ':' + p2(d.getMinutes());
	}
	function fmtDur(s) {
		s = Math.max(0, Math.round(s));
		if (s < 60) return s + ' 秒';
		if (s < 3600) return Math.floor(s / 60) + ' 分 ' + (s % 60) + ' 秒';
		return Math.floor(s / 3600) + ' 小时 ' + Math.floor(s % 3600 / 60) + ' 分';
	}
	function div(cls, html) { var d = document.createElement('div'); if (cls) d.className = cls; if (html != null) d.innerHTML = html; return d; }
	function joinp(a, b) { return a ? a + '/' + b : b; }
	function parentOf(p) { var i = p.lastIndexOf('/'); return i < 0 ? '' : p.slice(0, i); }
	function baseOf(p) { var i = p.lastIndexOf('/'); return i < 0 ? p : p.slice(i + 1); }
	function extOf(n) { var m = /\.([^.\/]+)$/.exec(n); return m ? m[1].toLowerCase() : ''; }
	function token() { return (window.L && L.env && L.env.token) || ''; }
	function bytesLen(s) { return unescape(encodeURIComponent(s)).length; }

	var TYPE_LABEL = { dir: '文件夹', image: '图片', video: '视频', audio: '音频', doc: '文档', archive: '压缩包', code: '代码/文本', other: '文件' };
	function typeLabel(e) {
		if (e.o) return '特殊文件';
		var t = TYPE_LABEL[e.ty] || '文件';
		if (e.ty === 'other') { var x = extOf(e.n); if (x) t = x.toUpperCase() + ' 文件'; }
		return t + (e.l ? '(快捷方式)' : '');
	}

	/* ---------- 偏好(localStorage, 失败不影响使用) ---------- */
	var PK = 'mwrt.files.v1';
	var P = (function() { try { return JSON.parse(localStorage.getItem(PK)) || {}; } catch (e) { return {}; } })();
	P.view = P.view === 'grid' ? 'grid' : 'list';
	P.sort = P.sort || 'name'; P.dir = P.dir === 'desc' ? 'desc' : 'asc';
	P.fav = Array.isArray(P.fav) ? P.fav : []; P.rec = Array.isArray(P.rec) ? P.rec : [];
	if (P.thumbs == null) P.thumbs = true;
	function savePrefs() { try { localStorage.setItem(PK, JSON.stringify(P)); } catch (e) {} }

	/* ---------- 网络 ---------- */
	function fnurl(fn) { return (window.L && L.url) ? L.url('admin/services/mwfiles/' + fn) : '/cgi-bin/luci/admin/services/mwfiles/' + fn; }
	function params(o) {
		var u = new URLSearchParams();
		for (var k in (o || {})) { var v = o[k]; if (v == null || v === false) continue; u.append(k, v === true ? '1' : v); }
		return u;
	}
	function parseResp(r) {
		return r.text().then(function(t) {
			try { return JSON.parse(t); } catch (e) {
				var auth = r.status === 401 || r.status === 403;
				return { ok: false, err: auth ? 'auth' : 'bad', msg: auth ? '没有权限, 或登录已过期, 请刷新页面后重试' : '服务器返回了无法识别的内容(登录可能已过期, 请刷新页面)' };
			}
		});
	}
	function api(fn, data, post, ctx) {
		var o = { credentials: 'same-origin', cache: 'no-store' }, url = fnurl(fn);
		if (post) { var b = params(data); b.append('token', token()); o.method = 'POST'; o.body = b; }
		else { var q = params(data).toString(); if (q) url += (url.indexOf('?') < 0 ? '?' : '&') + q; }
		// root 密码只放在请求头里(Authorization: Mwfiles <base64>), 不进 URL 与请求体
		if (ctx && ctx.pw) o.headers = { 'Authorization': pwHeader(ctx.pw) };
		var p = fetch(url, o).then(parseResp, function() { return { ok: false, err: 'net', msg: '无法连接到路由器, 请检查网络' }; });
		return ctx ? p.then(function(r) { return guardRetry(r, ctx, function() { return api(fn, data, post, ctx); }); }) : p;
	}
	function dlUrl(root, path, mode) { return fnurl('dl') + '?' + params({ root: root, path: path, mode: mode }).toString(); }

	/* ---------- 文件类型 / 预览类别 ---------- */
	var IMG_OK = { jpg: 1, jpeg: 1, png: 1, gif: 1, webp: 1, bmp: 1, ico: 1, avif: 1, svg: 1 };
	var VID_OK = { mp4: 1, m4v: 1, webm: 1, ogv: 1, mov: 1 };
	var AUD_OK = { mp3: 1, m4a: 1, aac: 1, wav: 1, ogg: 1, oga: 1, opus: 1, flac: 1 };
	var TXT_OK = { txt: 1, log: 1, md: 1, markdown: 1, csv: 1, conf: 1, cfg: 1, ini: 1, json: 1, yaml: 1, yml: 1, toml: 1, xml: 1, sh: 1, lua: 1, js: 1, css: 1, c: 1, h: 1, cpp: 1, hpp: 1, py: 1, go: 1, rs: 1, java: 1, php: 1, sql: 1, pl: 1, rb: 1, uc: 1, html: 1, htm: 1, ts: 1, list: 1, rules: 1, service: 1, env: 1, properties: 1 };
	function viewKind(e) {
		if (e.d || e.o || e.b) return 'none';
		var x = extOf(e.n);
		if (IMG_OK[x]) return 'image';
		if (VID_OK[x]) return 'video';
		if (AUD_OK[x]) return 'audio';
		if (x === 'pdf') return 'pdf';
		if (x === 'md' || x === 'markdown') return 'md';
		if (TXT_OK[x]) return 'text';
		if (!x) return 'text';
		return 'none';
	}
	var ARCH_RE = /\.(zip|tar|tar\.gz|tgz|tar\.bz2|tbz2?|tar\.xz|txz|gz)$/i;
	function isArchive(e) { return !e.d && !e.o && ARCH_RE.test(e.n); }

	/* ---------- 状态 ---------- */
	var S = {
		built: false, open: false, ready: false,
		roots: [], arch: {}, net: {}, netl: null, netOk: false, authorized: [], maxMb: 0, rootsErr: null,
		root: null, path: '', mode: 'dir', q: '', type: 'all',
		items: [], total: 0, dirs: 0, files: 0, meta: {}, truncated: false, shallow: false,
		sel: {}, nsel: 0, anchor: -1, cur: -1, selectName: null,
		hist: [], hpos: -1, seq: 0, loading: false, err: null,
		clip: null, tasks: [], tseq: 0, upBusy: false, upPolicy: null, tasksOpen: false, tasksAutoShown: false,
		cols: 1, rowH: 36, rng: null, W: 'l', side: false
	};
	var el = {};
	var restoreFocus = null;

	function $r(sel) { return el.root.querySelector(sel); }
	function $a(sel) { return el.root.querySelectorAll(sel); }
	function relOf(e) { return e.p !== undefined ? joinp(e.p, e.n) : joinp(S.path, e.n); }
	function rootObj(id) { for (var i = 0; i < S.roots.length; i++) if (S.roots[i].id === id) return S.roots[i]; return null; }
	function curRoot() { return rootObj(S.root); }
	function canWrite() { var r = curRoot(); return !!r && !r.ro && r.state !== 'error'; }

	/* ---------- 提示 ---------- */
	function toast(msg, kind, ms) {
		if (!el.toasts) return;
		var t = div('mf-toast' + (kind === 'err' ? ' err' : ''));
		t.setAttribute('role', kind === 'err' ? 'alert' : 'status');
		t.innerHTML = ic(kind === 'err' ? 'warn' : kind === 'warn' ? 'info' : 'okc') + '<span>' + esc(msg) + '</span><button class="mf-btn" aria-label="关闭提示">' + ic('close') + '</button>';
		function rm() { if (t.parentNode) t.parentNode.removeChild(t); }
		t.querySelector('button').onclick = rm;
		el.toasts.appendChild(t);
		while (el.toasts.children.length > 4) el.toasts.removeChild(el.toasts.firstChild);
		setTimeout(rm, ms || (kind === 'err' ? 7000 : 3200));
	}
	function fail(r, what) {
		if (r && r.cancelled) return;
		var m = (r && r.msg) || '操作失败';
		toast((what ? what + ': ' : '') + m, 'err');
	}

	/* ---------- 对话框 ---------- */
	var dlgs = [];
	function dlg(o) {
		return new Promise(function(res) {
			var layer = div('mf-layer');
			var box = div('mf-dlg' + (o.wide ? ' wide' : ''));
			var tid = 'mfd' + (++S.tseq);
			box.setAttribute('role', 'dialog'); box.setAttribute('aria-modal', 'true'); box.setAttribute('aria-labelledby', tid); box.tabIndex = -1;
			box.innerHTML = '<div class="mf-dlg-h"><span id="' + tid + '">' + esc(o.title) + '</span></div><div class="mf-dlg-b"></div><div class="mf-dlg-f"></div>';
			var bd = box.querySelector('.mf-dlg-b'), ft = box.querySelector('.mf-dlg-f');
			if (o.body && o.body.nodeType) bd.appendChild(o.body); else bd.innerHTML = o.body || '';
			var prev = el.root.activeElement;
			var done = false;
			function close(v) {
				if (done) return;
				done = true;
				var i = dlgs.indexOf(h); if (i >= 0) dlgs.splice(i, 1);
				if (layer.parentNode) layer.parentNode.removeChild(layer);
				if (prev && prev.focus && prev.isConnected) { try { prev.focus(); } catch (e) {} }
				res(v);
			}
			var h = { close: close, box: box, layer: layer, primary: null };
			(o.btns || [{ t: '好', v: true, primary: 1 }]).forEach(function(b) {
				var bt = document.createElement('button');
				bt.type = 'button'; bt.className = 'mf-pill' + (b.primary ? ' primary' : '') + (b.danger ? ' danger' : '');
				bt.textContent = b.t;
				if (b.primary) h.primary = bt;
				bt.onclick = function() {
					if (o.onbtn) { var keep = o.onbtn(b.v, box, close, bt); if (keep === false) return; }
					close(b.v);
				};
				ft.appendChild(bt);
			});
			layer.addEventListener('mousedown', function(e) { if (e.target === layer && !o.modal) close(null); });
			layer.appendChild(box);
			el.win.appendChild(layer);
			dlgs.push(h);
			if (o.init) o.init(box, close);
			var f = box.querySelector('input:not([type=checkbox]):not([type=hidden]),select,textarea') || h.primary || box;
			setTimeout(function() { try { f.focus(); if (f.select && f.tagName === 'INPUT' && o.selectAll !== false) f.select(); } catch (e) {} }, 0);
		});
	}
	function askConfirm(title, html, okText, danger) {
		return dlg({ title: title, body: html, btns: [{ t: '取消', v: false }, { t: okText || '确定', v: true, primary: !danger, danger: !!danger }] });
	}
	function askText(title, label, value, o) {
		o = o || {};
		var body = div('', '<div class="mf-fld"><label for="mfin">' + esc(label) + '</label><input class="mf-in" id="mfin" autocomplete="off" spellcheck="false"><div class="mf-muted" id="mfer" role="alert" style="min-height:18px;color:var(--danger)"></div></div>');
		var inp = body.querySelector('input'), er = body.querySelector('#mfer');
		inp.value = value || '';
		return dlg({
			title: title, body: body, btns: [{ t: '取消', v: null }, { t: o.ok || '确定', v: 'ok', primary: 1 }], selectAll: false,
			init: function(box) {
				var v = inp.value, dot = o.noExt ? -1 : v.lastIndexOf('.');
				setTimeout(function() { inp.focus(); inp.setSelectionRange(0, dot > 0 ? dot : v.length); }, 0);
			},
			onbtn: function(v, box, close) {
				if (v !== 'ok') return true;
				var s = inp.value.trim();
				var bad = o.validate ? o.validate(s) : nameErr(s);
				if (bad) { er.textContent = bad; inp.focus(); return false; }
				close(s);
				return false;
			}
		});
	}
	function nameErr(s) {
		if (!s) return '名称不能为空';
		if (s === '.' || s === '..') return '名称不能是 . 或 ..';
		if (/[\/\0]/.test(s)) return '名称不能包含 / ';
		if (bytesLen(s) > 255) return '名称过长(最多 255 字节)';
		return '';
	}

	/* ---------- 菜单 ---------- */
	var menuEl = null;
	function closeMenu() {
		if (!menuEl) return;
		var m = menuEl; menuEl = null;
		if (m.parentNode) m.parentNode.removeChild(m);
		if (m._ret && m._ret.isConnected) { try { m._ret.focus(); } catch (e) {} }
	}
	// items: {t, i, k, a, danger, dis, sep, h, chk}
	function menu(x, y, items, anchorEl) {
		closeMenu();
		var m = div('mf-menu'); m.setAttribute('role', 'menu');
		var html = '';
		items.forEach(function(it, n) {
			if (it.sep) { html += '<div class="mf-msep" role="separator"></div>'; return; }
			if (it.h) { html += '<div class="mf-mh">' + esc(it.h) + '</div>'; return; }
			html += '<button type="button" class="mf-mi' + (it.danger ? ' danger' : '') + '" role="menuitem" data-n="' + n + '"' + (it.dis ? ' aria-disabled="true"' : '') + '>' +
				(it.chk != null ? (it.chk ? ic('check') : '<span class="mf-i"></span>') : it.i ? ic(it.i) : '') + '<span>' + esc(it.t) + '</span>' + (it.k ? '<span class="mf-k">' + esc(it.k) + '</span>' : '') + '</button>';
		});
		m.innerHTML = html;
		m._ret = anchorEl || el.root.activeElement;
		m.addEventListener('click', function(e) {
			var b = e.target.closest('.mf-mi'); if (!b || b.getAttribute('aria-disabled') === 'true') return;
			var it = items[+b.getAttribute('data-n')];
			closeMenu();
			if (it && it.a) it.a();
		});
		el.win.appendChild(m);
		var wr = el.win.getBoundingClientRect(), mw = m.offsetWidth, mh = m.offsetHeight;
		var L0 = Math.min(Math.max(4, x - wr.left), wr.width - mw - 4), T0 = y - wr.top;
		if (T0 + mh > wr.height - 4) T0 = Math.max(4, wr.height - mh - 4);
		m.style.left = L0 + 'px'; m.style.top = T0 + 'px';
		menuEl = m;
		var first = m.querySelector('.mf-mi:not([aria-disabled])');
		if (first) first.focus();
		return m;
	}
	function menuAt(btn, items) {
		var r = btn.getBoundingClientRect();
		return menu(r.left, r.bottom + 4, items, btn);
	}
	function menuKey(e) {
		var all = [].slice.call(menuEl.querySelectorAll('.mf-mi:not([aria-disabled])'));
		var i = all.indexOf(el.root.activeElement);
		if (e.key === 'ArrowDown') i = (i + 1) % all.length;
		else if (e.key === 'ArrowUp') i = (i - 1 + all.length) % all.length;
		else if (e.key === 'Home') i = 0;
		else if (e.key === 'End') i = all.length - 1;
		else if (e.key === 'Escape' || e.key === 'Tab') { closeMenu(); e.preventDefault(); return; }
		else return;
		e.preventDefault();
		if (all[i]) all[i].focus();
	}

	/* ---------- 侧栏 ---------- */
	function navItem(o) {
		var cur = S.root === o.root && (o.path == null ? S.path === '' : S.path === o.path);
		return '<button type="button" class="mf-nav-item' + (o.off ? ' off' : '') + '" data-a="' + (o.a || 'nav') + '" data-root="' + esc(o.root || '') + '" data-path="' + esc(o.path || '') + '"' +
			(o.sid ? ' data-sid="' + esc(o.sid) + '"' : '') + (o.file ? ' data-file="1"' : '') + (cur ? ' aria-current="true"' : '') + '>' +
			ic(o.i, o.icls) + '<span class="mf-nm"><b class="mf-ell">' + esc(o.name) + '</b>' + (o.sub ? '<small class="mf-ell">' + esc(o.sub) + '</small>' : '') + (o.use ? meterHtml(o.use) : '') + (o.cap ? '<small class="mf-cap">' + esc(o.cap) + '</small>' : '') + '</span>' +
			(o.tags || '') + '</button>';
	}
	// 外接存储的接口标识: USB(含协商速率) / SD 卡 / NVMe
	function busInfo(r) {
		if (r.bus === 'usb') {
			var sp = Number(r.speed) || 0;
			var gen = sp >= 5000 ? 'USB 3.0' : sp >= 480 ? 'USB 2.0' : sp >= 12 ? 'USB 1.1' : 'USB';
			return { t: gen, tip: '通过 USB 接口连接的存储' + (sp ? '(协商速率 ' + sp + ' Mbps)' : '') };
		}
		if (r.bus === 'emmc') return { t: 'eMMC', tip: '路由器板载 eMMC 存储' };
		if (r.bus === 'sd') return { t: 'SD 卡', tip: '通过 SD 卡槽连接的存储' };
		if (r.bus === 'nvme') return { t: 'NVMe', tip: '通过 NVMe(M.2) 连接的存储' };
		return null;
	}
	function renderSide() {
		if (!el.side) return;
		var h = '<h3><span>存储位置</span>' + (S.fmtOk ? '<button type="button" class="mf-btn" data-a="format" title="格式化外接存储(内部存储不可格式化)">格式化</button>' : '') + '</h3>';
		var locals = S.roots.filter(function(r) { return r.kind !== 'remote'; });
		if (S.rootsErr) h += '<div class="mf-empty-s">' + esc(S.rootsErr.msg || '无法读取存储位置') + '</div>';
		else if (!locals.length) h += '<div class="mf-empty-s">没有可用的存储位置</div>';
		locals.forEach(function(r) {
			var sub = [], tags = '';
			if (r.fstype) sub.push(r.fstype);
			var bus = busInfo(r);
			if (bus) tags += '<span class="mf-tag bus" title="' + esc(bus.tip) + '">' + esc(bus.t) + '</span>';
			if (r.ro) tags += '<span class="mf-tag warn">只读</span>';
			if (r.state === 'error') tags += '<span class="mf-tag bad">无法访问</span>';
			if (r.internal && r.kind !== 'system') tags += '<span class="mf-tag own" title="固件所在的闪存, 改动需要 root 密码">内部存储</span>';
			else if (r.sysdisk) tags += '<span class="mf-tag" title="与系统在同一块存储上, 不可格式化">系统盘</span>';
			h += navItem({ i: bus && r.bus === 'usb' ? 'usb' : 'hdd', icls: bus ? 'bus' : '', name: r.label, sub: sub.join(' · '), cap: capText(r), root: r.id, tags: tags, use: usageOf(r) });
		});

		// 网络位置: 已保存的共享(含未连接) + 其它已挂载的网络卷
		h += '<h3><span>网络位置</span></h3>';
		if (S.netOk) {
			h += '<div class="mf-side-acts">' +
				'<button type="button" class="mf-act primary" data-a="sharenew" title="把本机的一个文件夹共享给局域网里的电脑(SMB)">' + ic('plus') + '新建共享</button>' +
				'<button type="button" class="mf-act" data-a="smbadd" title="把别的电脑/NAS 的共享文件夹挂载进来(SMB/CIFS)">' + ic('link') + '挂载共享</button>' +
				'<button type="button" class="mf-act wide" data-a="shares" title="查看、修改或取消已经创建的共享">' + ic('share') + '共享管理</button></div>';
		}
		var used = {}, nets = 0;
		(S.netl || []).forEach(function(s) {
			var r = s.root && rootObj(s.root);
			if (r) used[r.id] = 1;
			nets++;
			if (s.mounted && r) {
				var tg = (r.state === 'error' ? '<span class="mf-tag bad">已断开</span>' : '') + (r.ro ? '<span class="mf-tag warn">只读</span>' : '');
				h += navItem({ i: 'network', name: s.name, sub: s.host + ' / ' + s.share, cap: capText(r), use: usageOf(r), root: r.id, sid: s.id, a: 'nav', tags: tg });
			} else {
				h += navItem({ i: 'network', name: s.name, sub: s.host + ' / ' + s.share, a: 'netoff', sid: s.id, off: 1, tags: '<span class="mf-tag">未连接</span>' });
			}
		});
		S.roots.filter(function(r) { return r.kind === 'remote' && !used[r.id]; }).forEach(function(r) {
			nets++;
			h += navItem({ i: 'network', name: r.label, sub: r.fstype || '网络', cap: capText(r), use: usageOf(r), root: r.id, tags: r.state === 'error' ? '<span class="mf-tag bad">已断开</span>' : '' });
		});
		if (!nets) h += '<div class="mf-empty-s">' + (S.netOk ? '还没有网络共享' : '没有已挂载的网络共享') + '</div>';

		h += '<h3>收藏夹</h3>';
		if (!P.fav.length) h += '<div class="mf-empty-s">右键文件夹可添加到收藏夹</div>';
		P.fav.forEach(function(f) { h += navItem({ i: 'star', name: f.n || baseOf(f.p) || '(根目录)', root: f.r, path: f.p }); });
		h += '<h3><span>最近</span>' + (P.rec.length ? '<button type="button" class="mf-btn" data-a="recclear">清除</button>' : '') + '</h3>';
		if (!P.rec.length) h += '<div class="mf-empty-s">暂无记录</div>';
		P.rec.slice(0, 6).forEach(function(f) {
			h += navItem({ i: f.d ? 'folder' : (f.t || 'file'), name: f.n, sub: f.d ? '文件夹' : '文件', root: f.r, path: f.p, a: f.d ? 'nav' : 'navfile', file: !f.d });
		});
		h += '<div class="mf-side-foot"><button type="button" class="mf-about" data-a="about" title="版权声明与使用须知">' + ic('info') + '<span>版权 · 关于</span></button></div>';
		el.side.innerHTML = h;
	}
	function pushRec(root, path, name, isDir, ty) {
		if (!root) return;
		P.rec = P.rec.filter(function(x) { return !(x.r === root && x.p === path); });
		P.rec.unshift({ r: root, p: path, n: name, d: isDir ? 1 : 0, t: ty });
		P.rec = P.rec.slice(0, 20);
		savePrefs(); renderSide();
	}
	function isFav(root, path) { return P.fav.some(function(f) { return f.r === root && f.p === path; }); }
	function toggleFav(root, path, name) {
		if (isFav(root, path)) P.fav = P.fav.filter(function(f) { return !(f.r === root && f.p === path); });
		else P.fav.push({ r: root, p: path, n: name });
		savePrefs(); renderSide();
	}

	/* ---------- 加载 ---------- */
	function loadRoots() {
		return api('roots').then(function(r) {
			if (!r.ok) { S.rootsErr = r; S.roots = []; renderSide(); return r; }
			S.rootsErr = null; S.roots = r.roots || []; S.arch = r.archive || {}; S.net = r.net || {}; S.authorized = r.authorized || []; S.maxMb = r.max_upload_mb || 0;
			if (S.fmtOk == null) api('formatinfo').then(function(f) { S.fmtOk = !!f.ok; renderSide(); });
			loadOwnTitles();
			return api('netlist').then(function(n) {
				S.netOk = !!n.ok; S.netl = n.ok ? n.items : null;
				if (n.ok && n.caps) S.net = n.caps;
				renderSide();
				return r;
			});
		});
	}
	function reloadNet() {
		return api('netlist').then(function(n) { S.netOk = !!n.ok; S.netl = n.ok ? n.items : null; if (n.ok && n.caps) S.net = n.caps; renderSide(); });
	}
	var ldTimer = 0;
	function showLoading(b) {
		clearTimeout(ldTimer);
		if (!b) { el.ld.hidden = true; return; }
		ldTimer = setTimeout(function() { el.ld.hidden = false; }, 160);
	}
	function sortCmp(a, b) {
		if ((a.d || 0) !== (b.d || 0)) return (a.d || 0) > (b.d || 0) ? -1 : 1;
		var k = P.sort, r;
		if (k === 'size') r = a.s - b.s; else if (k === 'time') r = a.m - b.m; else if (k === 'type') r = a.ty < b.ty ? -1 : a.ty > b.ty ? 1 : 0;
		else r = 0;
		if (!r) r = a.n.toLowerCase() < b.n.toLowerCase() ? -1 : a.n.toLowerCase() > b.n.toLowerCase() ? 1 : 0;
		return P.dir === 'desc' ? -r : r;
	}
	function load(append) {
		var seq = ++S.seq, off = append ? S.items.length : 0, req;
		S.loading = true; showLoading(true);
		var ty = S.type === 'all' ? null : S.type;
		if (S.mode === 'search') req = api('search', { root: S.root, path: S.path, q: S.q, type: ty, hidden: P.hidden });
		else req = api('list', { root: S.root, path: S.path, sort: P.sort, dir: P.dir, type: ty, q: S.q || null, offset: off, limit: 500, hidden: P.hidden });
		return req.then(function(r) {
			if (seq !== S.seq) return;
			S.loading = false; showLoading(false);
			if (!r.ok) {
				S.err = r;
				if (!append) { S.items = []; S.total = 0; }
				renderAll();
				return;
			}
			S.err = null;
			if (S.mode === 'search') {
				S.items = (r.items || []).slice().sort(sortCmp);
				S.total = r.total; S.truncated = !!r.truncated; S.shallow = !!r.shallow;
			} else {
				S.items = append ? S.items.concat(r.items || []) : (r.items || []);
				S.total = r.total; S.dirs = r.dirs; S.files = r.files; S.truncated = !!r.capped;
				S.meta = { free: r.free, size: r.size, ro: r.ro, kind: r.kind, fstype: r.fstype, internal: r.internal, owner: r.owner };
			}
			if (!append) { S.rng = null; if (!S.keepScroll) el.scroll.scrollTop = 0; S.keepScroll = false; }
			if (S.selectName != null) {
				for (var i = 0; i < S.items.length; i++) if (S.items[i].n === S.selectName) {
					S.sel = {}; S.sel[relOf(S.items[i])] = 1; S.cur = i; S.anchor = i; setTimeout(function(ix) { return function() { ensureVisible(ix); }; }(i), 0); break;
				}
				S.selectName = null;
			}
			recount();
			renderAll();
		});
	}
	function refresh(keepSel) {
		if (!S.root) return Promise.resolve();
		var keep = keepSel ? S.sel : {};
		S.keepScroll = true;
		var p = load(false);
		return p.then(function() { if (keepSel) { var n = {}; S.items.forEach(function(e) { var k = relOf(e); if (keep[k]) n[k] = 1; }); S.sel = n; recount(); renderAll(); } });
	}
	function recount() { var n = 0; for (var k in S.sel) n++; S.nsel = n; }

	/* ---------- 导航 ---------- */
	function pushHist() {
		var h = S.hist[S.hpos];
		if (h && h.r === S.root && h.p === S.path) return;
		S.hist = S.hist.slice(0, S.hpos + 1);
		S.hist.push({ r: S.root, p: S.path });
		if (S.hist.length > 60) S.hist.shift();
		S.hpos = S.hist.length - 1;
	}
	function go(root, path, o) {
		o = o || {};
		S.root = root; S.path = path || ''; S.mode = 'dir'; S.keepScroll = false;
		if (!o.keepq) { S.q = ''; el.q.value = ''; }
		S.sel = {}; S.nsel = 0; S.anchor = -1; S.cur = -1; S.items = []; S.total = 0; S.err = null; S.selectName = o.select != null ? o.select : null;
		if (!o.nohist) pushHist();
		P.last = { r: root, p: S.path }; savePrefs();
		if (S.path) pushRec(root, S.path, baseOf(S.path), true);
		S.side = false; el.side.classList.remove('open'); el.sscrim.classList.remove('on');
		renderSide(); renderChrome();
		return load(false);
	}
	function goBack() { if (S.hpos > 0) { S.hpos--; var h = S.hist[S.hpos]; go(h.r, h.p, { nohist: true }); } }
	function goFwd() { if (S.hpos < S.hist.length - 1) { S.hpos++; var h = S.hist[S.hpos]; go(h.r, h.p, { nohist: true }); } }
	function goUp() { if (!S.path) return; go(S.root, parentOf(S.path), { select: baseOf(S.path) }); }

	/* ---------- 顶栏 / 工具栏 ---------- */
	function disable(b, on, why) {
		if (!b) return;
		if (on) b.setAttribute('aria-disabled', 'true'); else b.removeAttribute('aria-disabled');
		if (why !== undefined) b.title = why || b.getAttribute('data-title') || '';
	}
	function renderCrumb() {
		var r = curRoot();
		var h = '<span class="mf-crumb-in">';
		h += '<button type="button" class="mf-crumb-b' + (!S.path && S.mode === 'dir' ? ' last' : '') + '" data-a="crumb" data-path="">' + esc(r ? r.label : (S.root || '—')) + '</button>';
		var segs = S.path ? S.path.split('/') : [], acc = '';
		segs.forEach(function(sg, i) {
			acc = joinp(acc, sg);
			h += ic('chev', 'mf-chev') + '<button type="button" class="mf-crumb-b' + (i === segs.length - 1 && S.mode === 'dir' ? ' last' : '') + '" data-a="crumb" data-path="' + esc(acc) + '">' + esc(sg) + '</button>';
		});
		if (S.mode === 'search') h += ic('chev', 'mf-chev') + '<span class="mf-crumb-b last">搜索“' + esc(S.q) + '”</span>';
		h += '</span>';
		el.crumb.innerHTML = h;
		var ci = el.crumb.firstChild; ci.scrollLeft = ci.scrollWidth;
		el.where.textContent = (r ? r.label : '') + (S.path ? ' / ' + S.path : '');
	}
	function editCrumb() {
		if (!S.root) return;
		var inp = document.createElement('input');
		inp.className = 'mf-in'; inp.style.cssText = 'border:0;height:26px;'; inp.value = '/' + S.path; inp.setAttribute('aria-label', '当前位置(相对于卷根目录)'); inp.spellcheck = false;
		el.crumb.innerHTML = ''; el.crumb.appendChild(inp);
		inp.focus(); inp.select();
		var done = false;
		function end(ok) {
			if (done) return; done = true;
			var v = inp.value.trim().replace(/^\/+|\/+$/g, '').replace(/\/{2,}/g, '/');
			if (ok && v !== S.path) go(S.root, v); else renderCrumb();
		}
		inp.onkeydown = function(e) { if (e.key === 'Enter') { e.preventDefault(); end(true); } else if (e.key === 'Escape') { e.stopPropagation(); end(false); } };
		inp.onblur = function() { end(false); };
	}
	function renderChrome() {
		renderCrumb();
		disable(el.back, S.hpos <= 0); disable(el.fwd, S.hpos >= S.hist.length - 1); disable(el.upb, !S.path);
		el.vList.setAttribute('aria-pressed', P.view === 'list'); el.vGrid.setAttribute('aria-pressed', P.view === 'grid');
		el.qx.hidden = !el.q.value;
		el.filter.value = S.type;
		el.head.style.display = P.view === 'list' ? '' : 'none';
		updateHead();
		updateActs();
	}
	function updateHead() {
		var cols = [['name', '名称'], ['type', '类型', 'type'], ['size', '大小', 'size r'], ['time', '修改时间']];
		var h = '<span></span>';
		cols.forEach(function(c) {
			var on = P.sort === c[0];
			h += '<button type="button" class="mf-th mf-th-' + (c[0]) + (c[2] && c[2].indexOf('r') >= 0 ? ' r' : '') + '" data-a="th" data-k="' + c[0] + '" aria-sort="' + (on ? (P.dir === 'desc' ? 'descending' : 'ascending') : 'none') + '">' + c[1] + (on ? ic(P.dir === 'desc' ? 'sortdown' : 'sortup') : '') + '</button>';
		});
		h += '<span class="mf-th-stat">状态</span>';
		el.head.innerHTML = h;
	}
	var ACTS = [
		['newfolder', 'newfolder', '新建文件夹'], ['upload', 'upload', '上传'], '|',
		['download', 'download', '下载'], ['rename', 'edit', '重命名'], ['copy', 'copy', '复制'], ['cut', 'cut', '剪切'], ['paste', 'paste', '粘贴'], ['delete', 'trash', '删除'], ['more', 'more', '', '更多操作']
	];
	function buildActs() {
		var h = '';
		ACTS.forEach(function(a) {
			if (a === '|') { h += '<span class="mf-sep"></span>'; return; }
			h += '<button type="button" class="mf-btn' + (a[0] === 'delete' ? ' danger' : '') + '" data-a="' + a[0] + '" data-title="' + esc(a[3] || a[2]) + '" title="' + esc(a[3] || a[2]) + '" aria-label="' + esc(a[3] || a[2]) + '">' + ic(a[1]) + (a[2] ? '<span class="mf-act-l">' + a[2] + '</span>' : '') + '</button>';
		});
		h += '<span class="mf-grow"></span>';
		h += '<button type="button" class="mf-btn" data-a="sort" title="排序方式" aria-label="排序方式">' + ic('sortdown') + '<span class="mf-act-l">排序</span></button>';
		h += '<select class="mf-in mf-filter" data-r="filter" aria-label="按类型筛选"><option value="all">全部类型</option><option value="dir">文件夹</option><option value="image">图片</option><option value="video">视频</option><option value="audio">音频</option><option value="doc">文档</option><option value="archive">压缩包</option><option value="code">代码/文本</option><option value="other">其它</option></select>';
		h += '<button type="button" class="mf-btn" data-a="info" title="显示/隐藏信息栏" aria-label="信息栏">' + ic('info') + '</button>';
		el.acts.innerHTML = h;
		el.filter = $r('[data-r=filter]');
	}
	function updateActs() {
		if (!el.acts) return;
		var n = S.nsel, w = canWrite(), dirMode = S.mode === 'dir', one = n === 1, ro = !w ? '此位置为只读或不可访问' : '';
		var items = n ? selList() : [];
		function A(k) { return el.acts.querySelector('[data-a=' + k + ']'); }
		disable(A('newfolder'), !w || !dirMode, !dirMode ? '请先退出搜索' : ro);
		disable(A('upload'), !w || !dirMode, !dirMode ? '请先退出搜索' : ro);
		disable(A('download'), !n || items.some(function(e) { return e.d || e.o; }), items.some(function(e) { return e.d; }) ? '文件夹请先压缩后下载' : '');
		disable(A('rename'), !one || !w, ro);
		disable(A('copy'), !n);
		disable(A('cut'), !n || !w, ro);
		disable(A('paste'), !S.clip || !w || !dirMode, !S.clip ? '' : ro);
		disable(A('delete'), !n || !w, ro);
		disable(A('more'), false);
		A('info').setAttribute('aria-pressed', !!P.info);
		el.info.classList.toggle('on', !!P.info);
	}

	/* ---------- 列表渲染(虚拟滚动) ---------- */
	function selList() {
		var out = [];
		for (var i = 0; i < S.items.length; i++) if (S.sel[relOf(S.items[i])]) out.push(S.items[i]);
		return out;
	}
	function geom() {
		var w = el.scroll.clientWidth;
		if (P.view === 'grid') { S.cols = Math.max(1, Math.floor((w - 12) / 116)); S.rowH = 120; }
		else { S.cols = 1; S.rowH = S.W === 'xs' ? 52 : 36; }
	}
	function iconFor(e) { return e.d ? 'folder' : (e.o ? 'file' : e.ty && IP[e.ty] ? e.ty : 'file'); }
	function thumbOk(e) {
		return P.thumbs && P.view === 'grid' && !e.d && !e.l && e.ty === 'image' && IMG_OK[extOf(e.n)] && e.s <= 1572864 && e.s > 0 && extOf(e.n) !== 'svg';
	}
	function flagsOf(e) {
		var f = [];
		if (e.b) f.push('损坏的快捷方式'); else if (e.l) f.push('快捷方式');
		if (e.o) f.push('特殊文件');
		if (e.bad) f.push('名称编码异常');
		return f.join('、');
	}
	function rowHtml(e, i, y) {
		var k = relOf(e), sel = !!S.sel[k], tyc = 't-' + (e.d ? 'dir' : (e.ty || 'other'));
		var sizeTx = e.d ? '—' : fmtSize(e.s);
		var chk = '<button type="button" class="mf-chk" role="checkbox" aria-checked="' + sel + '" aria-label="选择 ' + esc(e.n) + '" tabindex="-1" data-a="chk">' + ic('check') + '</button>';
		var drag = e.bad ? '' : ' draggable="true"';
		if (P.view === 'grid') {
			var c = i % S.cols, r = Math.floor(i / S.cols), pad = Math.max(0, (el.scroll.clientWidth - S.cols * 116) / 2);
			var ph = thumbOk(e) ? '<img loading="lazy" decoding="async" alt="" src="' + esc(dlUrl(S.root, k, 'view')) + '">' : ic(iconFor(e), tyc);
			return '<div class="mf-cell' + (i === S.cur ? ' cur' : '') + '" role="option" aria-selected="' + sel + '" data-i="' + i + '" data-d="' + (e.d ? 1 : 0) + '" style="left:' + Math.round(pad + c * 116) + 'px;top:' + (r * S.rowH + 2) + 'px"' + drag + ' title="' + esc(e.n) + '">' +
				chk + '<div class="mf-ph">' + ph + '</div><div class="mf-t">' + esc(e.n) + '</div>' + ownTag(e) + '</div>';
		}
		return '<div class="mf-row' + (i === S.cur ? ' cur' : '') + '" role="option" aria-selected="' + sel + '" data-i="' + i + '" data-d="' + (e.d ? 1 : 0) + '" style="top:' + y + 'px"' + drag + '>' + chk +
			'<div class="mf-name">' + ic(iconFor(e), tyc) + '<span class="mf-tt"><span class="mf-t" title="' + esc(e.n) + '">' + esc(e.n) + '</span><span class="mf-sub">' + esc((e.d ? '文件夹' : sizeTx) + ' · ' + fmtTime(e.m)) + '</span></span>' +
			(e.p !== undefined && e.p !== '' ? '<span class="mf-loc mf-ell">' + esc(e.p) + '</span>' : '') + ownTag(e) + '</div>' +
			'<div class="mf-col mf-col-type">' + esc(typeLabel(e)) + '</div><div class="mf-col mf-col-size">' + sizeTx + '</div><div class="mf-col">' + fmtTime(e.m) + '</div>' +
			'<div class="mf-col mf-col-stat">' + esc(flagsOf(e)) + '</div>' +
			'<button type="button" class="mf-btn mf-more" data-a="rowmore" aria-label="更多操作" tabindex="-1">' + ic('more') + '</button></div>';
	}
	function render(force) {
		if (!el.scroll) return;
		var n = S.items.length, rows = P.view === 'grid' ? Math.ceil(n / S.cols) : n;
		var st = el.scroll.scrollTop, hh = el.scroll.clientHeight;
		var a = Math.max(0, Math.floor(st / S.rowH) - 5), b = Math.min(rows, Math.ceil((st + hh) / S.rowH) + 5);
		var key = a + ':' + b + ':' + S.cols + ':' + P.view;
		if (!force && S.rng === key) return;
		S.rng = key;
		el.sp.style.height = (rows * S.rowH + 6) + 'px';
		var h = '';
		if (P.view === 'grid') { for (var i = a * S.cols; i < Math.min(n, b * S.cols); i++) h += rowHtml(S.items[i], i, 0); }
		else { for (var j = a; j < b; j++) h += rowHtml(S.items[j], j, j * S.rowH); }
		el.sp.innerHTML = h;
		el.scroll.classList.toggle('multi', S.nsel > 0);
		// 接近底部时继续加载下一批
		if (S.mode === 'dir' && !S.loading && !S.err && S.items.length < S.total && st + hh > rows * S.rowH - 400) load(true);
	}
	// 选择变化只改属性, 不重建行元素(否则双击的第二下会落在新节点上, 缩略图也会闪)
	function syncSel() {
		var ch = el.sp.children;
		for (var q = 0; q < ch.length; q++) {
			var r = ch[q], i = +r.getAttribute('data-i'), e = S.items[i];
			if (!e) continue;
			var sel = !!S.sel[relOf(e)], c = r.querySelector('.mf-chk');
			r.setAttribute('aria-selected', sel); r.classList.toggle('cur', i === S.cur);
			if (c) c.setAttribute('aria-checked', sel);
		}
		el.scroll.classList.toggle('multi', S.nsel > 0);
	}
	function ensureVisible(i) {
		if (i < 0) return;
		var row = P.view === 'grid' ? Math.floor(i / S.cols) : i, top = row * S.rowH, st = el.scroll.scrollTop, hh = el.scroll.clientHeight;
		if (top < st) el.scroll.scrollTop = top;
		else if (top + S.rowH > st + hh) el.scroll.scrollTop = top + S.rowH - hh + 4;
		render(false); syncSel();
	}
	function renderEmpty() {
		var e = el.empty, h = '';
		if (S.loading && !S.items.length) { e.hidden = true; return; }
		if (!S.root) {
			var why = S.rootsErr ? S.rootsErr.msg : '';
			h = ic('hdd') + '<b>没有可用的存储位置</b><p>' + esc(why || '文件管理只能访问管理员授权的目录和已挂载的存储卷。请插入 U 盘/硬盘(挂载到 /mnt 或 /media 下), 或连接网络共享; 授权目录在 /etc/config/mwfiles 的 root 列表里配置。') + '</p>';
			if (S.netOk) h += '<button type="button" class="mf-pill primary" data-a="smbadd">连接网络共享</button>';
		} else if (S.err) {
			h = ic('warn') + '<b>无法打开此位置</b><p>' + esc(S.err.msg || '读取失败') + '</p><div><button type="button" class="mf-pill" data-a="refresh">重试</button> ' +
				(S.path ? '<button type="button" class="mf-pill" data-a="up">返回上一级</button>' : '') + (S.err.err === 'share_down' && S.netOk ? ' <button type="button" class="mf-pill" data-a="netdiag">诊断</button>' : '') + '</div>';
			e.className = 'mf-empty err';
		} else if (!S.items.length) {
			if (S.mode === 'search') h = ic('search') + '<b>没有找到匹配项</b><p>' + (S.shallow ? '网络共享上只搜索当前文件夹。' : '已搜索“' + esc(S.q) + '”所在文件夹及其子文件夹。') + '</p>';
			else if (S.q || S.type !== 'all') h = ic('search') + '<b>没有符合条件的项目</b><p>换个关键字或类型再试试。</p>';
			else h = ic('folder') + '<b>此文件夹是空的</b>' + (canWrite() ? '<p>把文件拖到这里, 或点击下面的按钮上传。</p><button type="button" class="mf-pill primary" data-a="upload">上传文件</button>' : '<p>这个位置是只读的。</p>');
		}
		if (!S.err) e.className = 'mf-empty';
		e.hidden = !h; e.innerHTML = h;
	}
	function updateStatus() {
		if (!el.status) return;
		var l = [], r = curRoot();
		if (S.err || !S.root) l.push(S.err ? '不可用' : '');
		else if (S.mode === 'search') l.push(S.items.length + ' 项搜索结果' + (S.truncated ? '(结果过多, 已截断)' : ''));
		else {
			l.push(S.total + ' 项' + (S.items.length < S.total ? '(已加载 ' + S.items.length + ')' : ''));
			if (S.q && S.dirs + S.files !== 0) l[0] += ' · 按名称过滤';
		}
		if (S.nsel) {
			var sz = 0, items = selList();
			items.forEach(function(e) { if (!e.d) sz += e.s; });
			l.push('已选 ' + S.nsel + ' 项' + (sz ? ' · ' + fmtSize(sz) : ''));
		}
		var v = '';
		if (r) {
			var rem = r.kind === 'remote';
			v = '<span class="mf-vol">' + ic(rem ? 'network' : 'hdd') + esc(r.label) + ' · ' + (rem ? '网络共享' : '本地') + (r.fstype ? ' · ' + esc(r.fstype) : '') + '</span>';
			if (S.meta.internal) v += '<span class="warn">内部存储 · 改动需 root 密码</span>';
			if (S.err && S.err.err === 'share_down') v += '<span class="bad">共享已断开</span>';
			else if (S.meta.ro || r.ro) v += '<span class="warn">只读</span>';
			else if (S.err) v += '<span class="bad">不可访问</span>';
			else v += '<span>在线</span>';
			if (S.meta.free != null && S.meta.size) v += '<span>可用 ' + fmtSize(S.meta.free) + ' / 共 ' + fmtSize(S.meta.size) + '</span>';
		}
		el.status.innerHTML = '<span class="mf-grow mf-ell">' + esc(l.join('  ·  ')) + (S.q && S.mode === 'dir' ? ' · 按 Enter 搜索子文件夹' : '') + '</span>' + v;
	}
	function updateBanner() {
		if (!el.banner) return;
		var h = internalBanner();
		el.banner.hidden = !h; el.banner.innerHTML = h;
	}
	function renderAll() {
		updateBanner(); geom(); render(true); renderEmpty(); updateStatus(); updateActs(); updateInfo();
		el.sp.style.display = (S.items.length ? '' : 'none');
	}

	/* ---------- 选择 ---------- */
	function setSel(keys, cur) {
		S.sel = {}; keys.forEach(function(k) { S.sel[k] = 1; }); recount();
		if (cur != null) S.cur = cur;
		syncSel(); updateStatus(); updateActs(); updateInfo();
	}
	function selectOnly(i) { setSel([relOf(S.items[i])], i); S.anchor = i; }
	function toggleSel(i) {
		var k = relOf(S.items[i]);
		if (S.sel[k]) delete S.sel[k]; else S.sel[k] = 1;
		recount(); S.cur = i; S.anchor = i; syncSel(); updateStatus(); updateActs(); updateInfo();
	}
	function rangeSel(i) {
		var a = S.anchor < 0 ? i : S.anchor, lo = Math.min(a, i), hi = Math.max(a, i), keys = [];
		for (var j = lo; j <= hi; j++) keys.push(relOf(S.items[j]));
		setSel(keys, i);
	}
	function selectAll() { setSel(S.items.map(relOf), S.cur); }
	function clearSel() { if (S.nsel) setSel([], S.cur); }
	function moveCur(d, ev) {
		if (!S.items.length) return;
		var i = S.cur < 0 ? (d > 0 ? 0 : S.items.length - 1) : Math.max(0, Math.min(S.items.length - 1, S.cur + d));
		if (ev.shiftKey) { S.cur = i; rangeSel(i); }
		else if (ev.ctrlKey || ev.metaKey) { S.cur = i; syncSel(); }
		else selectOnly(i);
		ensureVisible(i);
		if (S.mode === 'dir' && i > S.items.length - 30 && S.items.length < S.total && !S.loading) load(true);
	}

	/* ---------- 信息栏 ---------- */
	var infoSeq = 0, statCache = {};
	function detailHtml(s) {
		var rows = [['类型', s.type === 'dir' ? '文件夹' : (TYPE_LABEL[s.ty] || '文件')]];
		if (s.type === 'lnk') rows[0][1] = '快捷方式' + (s.link ? ' → ' + s.link : '') + (s.link_ok === false ? '(指向授权范围之外, 已禁止访问)' : '');
		if (s.type !== 'dir') rows.push(['大小', fmtSize(s.size) + ' (' + s.size + ' 字节)']);
		rows.push(['位置', '/' + parentOf(s.rel || '')]);
		rows.push(['修改时间', fmtTime(s.mtime)]);
		if (s.ctime) rows.push(['状态变更', fmtTime(s.ctime)]);
		if (s.mode) rows.push(['权限', s.mode]);
		rows.push(['所在卷', s.volume + ' · ' + (s.kind === 'remote' ? '网络共享' : '本地') + (s.fstype ? ' · ' + s.fstype : '') + (s.ro ? ' · 只读' : '')]);
		if (s.dev && s.kind === 'remote') rows.push(['共享来源', s.dev]);
		if (s.internal) {
			var ow = s.owner;
			rows.push(['存储', '内部存储(固件所在的闪存), 改动需要 root 密码']);
			rows.push(['所属组件', ow && ow.pkg ? ((ow.meta && ow.meta.title ? ow.meta.title + ' · ' : '') + ow.pkg + (ow.meta && ow.meta.version ? ' ' + ow.meta.version : '') + (ow.meta && ow.meta.core ? ' · 系统核心' : '')) : (ow && ow.multi ? '包含多个软件包的文件' : '不属于任何软件包')]);
		}
		if (s.shared) rows.push(['共享状态', '已通过 ' + (s.shared.server === 'ksmbd' ? 'ksmbd' : 'Samba') + ' 共享为“' + s.shared.name + '”' + (s.shared.exact ? '' : '(上级文件夹已共享)') + (s.shared.ro ? ', 只读' : '')]);
		return '<dl class="mf-dl">' + rows.map(function(r) { return '<dt>' + esc(r[0]) + '</dt><dd>' + esc(r[1]) + '</dd>'; }).join('') + '</dl>';
	}
	function updateInfo() {
		if (!P.info || !el.info) return;
		var seq = ++infoSeq, items = selList(), r = curRoot();
		var h = '<div class="mf-info-h"><span>信息</span><button type="button" class="mf-btn" data-a="info" aria-label="关闭信息栏">' + ic('close') + '</button></div>';
		if (S.nsel === 0) {
			h += '<div class="mf-info-pv">' + ic('folder', 't-dir') + '</div><div class="mf-info-nm">' + esc(S.path ? baseOf(S.path) : (r ? r.label : '')) + '</div><div class="mf-info-ty">' + (S.mode === 'search' ? '搜索结果' : '当前文件夹') + '</div>';
			var d = [['项目', S.mode === 'search' ? S.items.length + ' 项' : S.total + ' 项(' + S.dirs + ' 个文件夹, ' + S.files + ' 个文件)']];
			if (r) { d.push(['所在卷', r.label]); d.push(['类型', (r.kind === 'remote' ? '网络共享' : '本地') + (r.fstype ? ' · ' + r.fstype : '')]); }
			if (S.meta.free != null) d.push(['可用空间', fmtSize(S.meta.free) + ' / ' + fmtSize(S.meta.size)]);
			h += '<dl class="mf-dl">' + d.map(function(x) { return '<dt>' + esc(x[0]) + '</dt><dd>' + esc(x[1]) + '</dd>'; }).join('') + '</dl>';
			el.info.innerHTML = h; return;
		}
		if (S.nsel > 1) {
			var sz = 0, nd = 0; items.forEach(function(e) { if (e.d) nd++; else sz += e.s; });
			h += '<div class="mf-info-pv">' + ic('copy') + '</div><div class="mf-info-nm">已选 ' + S.nsel + ' 项</div><div class="mf-info-ty">' + nd + ' 个文件夹, ' + (S.nsel - nd) + ' 个文件</div><dl class="mf-dl"><dt>文件合计</dt><dd>' + fmtSize(sz) + '</dd></dl>';
			el.info.innerHTML = h; return;
		}
		var e = items[0]; if (!e) { el.info.innerHTML = h; return; }
		var k = relOf(e), tyc = 't-' + (e.d ? 'dir' : e.ty || 'other');
		var pv = (e.ty === 'image' && IMG_OK[extOf(e.n)] && !e.d && e.s <= 3145728) ? '<img alt="" src="' + esc(dlUrl(S.root, k, 'view')) + '">' : ic(iconFor(e), tyc);
		var kind = viewKind(e);
		h += '<div class="mf-info-pv' + (kind !== 'none' || e.d ? ' click' : '') + '" data-a="infopv">' + pv + '</div><div class="mf-info-nm">' + esc(e.n) + '</div><div class="mf-info-ty">' + esc(typeLabel(e)) + '</div><div data-r="det"><div class="mf-muted" style="padding:4px 14px">读取属性…</div></div>';
		h += '<div class="mf-info-b">' + (e.d ? '<button type="button" class="mf-pill" data-a="dirsize">计算大小</button>' : '') + '</div>';
		el.info.innerHTML = h;
		var ck = S.root + '\n' + k;
		function show(s) { if (seq !== infoSeq) return; var d = el.info.querySelector('[data-r=det]'); if (d) d.innerHTML = s.ok ? detailHtml(s) : '<div class="mf-muted" style="padding:4px 14px">' + esc(s.msg || '无法读取属性') + '</div>'; }
		if (statCache[ck] && Date.now() - statCache[ck].t < 5000) show(statCache[ck].s);
		else api('stat', { root: S.root, path: k }).then(function(s) { statCache[ck] = { t: Date.now(), s: s }; show(s); });
	}

	/* ---------- 常用 ---------- */
	function selPaths() { return selList().map(relOf); }
	var rootsTimer = 0;
	function rootsSoon() {
		clearTimeout(rootsTimer);
		rootsTimer = setTimeout(function() { loadRoots().then(function() { if (S.root) { var r = curRoot(); if (r && S.meta) { S.meta.free = r.free; S.meta.size = r.total; updateStatus(); } } }); }, 1200);
	}
	function afterChange(selectName) {
		if (selectName != null) S.selectName = selectName;
		rootsSoon();
		return refresh(false);
	}
	function sameLoc(root, path) { return S.open && S.mode === 'dir' && S.root === root && S.path === path; }
	var OPNAME = { cp: '复制', mv: '移动', rm: '删除', pack: '压缩', unpack: '解压' };

	/* ---------- 传输任务面板 ---------- */
	var tasksDirty = 0;
	function addTask(t) {
		t.id = 't' + (++S.tseq); t.t0 = Date.now(); t.state = t.state || 'running';
		S.tasks.push(t);
		if (S.tasks.length > 40) { for (var i = 0; i < S.tasks.length && S.tasks.length > 40; i++) if (S.tasks[i].state !== 'running' && S.tasks[i].state !== 'queued') { S.tasks.splice(i, 1); i--; } }
		if (!S.tasksOpen && !t.quiet) { S.tasksOpen = true; }
		tasksChanged();
		return t;
	}
	function tasksChanged() {
		if (tasksDirty) return;
		tasksDirty = requestAnimationFrame(function() { tasksDirty = 0; renderTasks(); });
	}
	function activeTasks() { return S.tasks.filter(function(t) { return t.state === 'running' || t.state === 'queued' || t.state === 'writing'; }); }
	function renderTasks() {
		if (!el.tasks) return;
		var act = activeTasks().length;
		el.tbadge.hidden = !act; el.tbadge.textContent = act;
		el.tbtn.setAttribute('aria-pressed', S.tasksOpen ? 'true' : 'false');
		el.tbtn.title = act ? '传输任务(' + act + ' 项进行中)' : '传输任务';
		el.tasks.hidden = !S.tasksOpen;
		if (!S.tasksOpen) return;
		var h = '<div class="mf-tasks-h"><span>传输任务</span><span><button type="button" class="mf-btn" data-a="tclear">清除已完成</button><button type="button" class="mf-btn" data-a="tclose" aria-label="关闭任务面板">' + ic('close') + '</button></span></div><div class="mf-tasks-l">';
		if (!S.tasks.length) h += '<div class="mf-tasks-e">暂无任务</div>';
		S.tasks.slice().reverse().forEach(function(t) {
			var run = t.state === 'running' || t.state === 'queued' || t.state === 'writing';
			var cls = 'mf-task' + (t.state === 'done' ? ' done' : (t.state === 'failed' ? ' fail' : ''));
			var pct = t.pct, ind = run && (pct == null);
			if (!run) pct = t.state === 'done' ? 100 : (t.pct || 0);
			var stTx = { queued: '等待中', running: '进行中', writing: '正在写入…', done: '已完成', failed: '失败', cancelled: '已取消', partial: '部分完成' }[t.state] || t.state;
			h += '<div class="' + cls + '" data-id="' + t.id + '"><div class="mf-task-t">' + ic(t.icon || 'tasks') + '<span class="mf-ell" title="' + esc(t.title) + '">' + esc(t.title) + '</span>' +
				(run && t.cancel ? '<button type="button" class="mf-btn" data-a="tcancel" aria-label="取消任务">取消</button>' : '') + '</div>' +
				'<div class="mf-prog' + (ind ? ' ind' : '') + '" role="progressbar" aria-label="' + esc(t.title) + '"' + (ind ? '' : ' aria-valuemin="0" aria-valuemax="100" aria-valuenow="' + Math.round(pct) + '"') + '><i style="' + (ind ? '' : 'width:' + Math.max(0, Math.min(100, pct)) + '%') + '"></i></div>' +
				'<div class="mf-task-m' + (t.state === 'failed' ? ' bad' : '') + '"><span>' + esc(stTx + (t.msg ? ' · ' + t.msg : '')) + '</span><span>' + esc(t.right || '') + '</span></div>' +
				(t.fails && t.fails.length ? '<div class="mf-task-m bad"><span>' + esc(t.fails.slice(0, 3).join('; ') + (t.fails.length > 3 ? ' 等 ' + t.fails.length + ' 个失败' : '')) + '</span></div>' : '') + '</div>';
		});
		h += '</div>';
		el.tasks.innerHTML = h;
	}
	function tasksClick(e) {
		var b = e.target.closest('[data-a]'); if (!b) return;
		var a = b.getAttribute('data-a'), card = b.closest('.mf-task'), t = null;
		if (card) S.tasks.forEach(function(x) { if (x.id === card.getAttribute('data-id')) t = x; });
		if (a === 'tclose') { S.tasksOpen = false; renderTasks(); }
		else if (a === 'tclear') { S.tasks = S.tasks.filter(function(x) { return x.state === 'running' || x.state === 'queued' || x.state === 'writing'; }); renderTasks(); }
		else if (a === 'tcancel' && t && t.cancel) t.cancel();
	}

	/* ---------- 后台任务(复制/移动/删除/压缩/解压)的轮询 ---------- */
	function jobMsg(op, r) {
		var s = OPNAME[op] || '操作';
		if (r.state === 'cancelled') return s + '已取消';
		if (r.state === 'failed') return s + '失败' + (r.msg ? ': ' + r.msg : '');
		if (r.fails > 0) return s + '完成, 但有 ' + r.fails + ' 项失败' + (r.msg ? ': ' + r.msg : '');
		return s + '完成';
	}
	// res: 后端返回的任务结果(state=done|failed|cancelled|running); o: { op, title, onDone(res) }
	function trackJob(res, o) {
		var op = res.op || o.op;
		function finish(r) {
			var bad = r.state === 'failed' || r.fails > 0;
			var m = jobMsg(op, r) + (o.dest || r.dest ? ' → ' + (r.dest || o.dest) : '');
			if (r.skipped) m += '(跳过 ' + r.skipped + ' 项)';
			toast(m, r.state === 'failed' ? 'err' : (bad ? 'warn' : null), bad ? 8000 : 3600);
			if (o.onDone) o.onDone(r);
			else afterChange();
		}
		if (res.state !== 'running') { finish(res); return Promise.resolve(res); }
		var jid = res.id || res.job;
		var t = addTask({ title: (OPNAME[op] || '处理') + ' ' + o.title, icon: op === 'rm' ? 'trash' : (op === 'pack' || op === 'unpack' ? 'archive' : 'copy'), state: 'running', pct: null, jobId: jid,
			cancel: function() { api('jobcancel', { id: jid }, true).then(function(r) { if (!r.ok) fail(r, '取消'); }); t.msg = '正在取消…'; tasksChanged(); } });
		return new Promise(function(resolve) {
			function tick() {
				api('job', { id: jid }).then(function(r) {
					if (!r.ok) {
						if (r.err === 'net') { t.msg = '连接中断, 重试中…'; tasksChanged(); return setTimeout(tick, 2000); }
						t.state = 'failed'; t.msg = r.msg; tasksChanged(); toast(r.msg, 'err'); return resolve(r);
					}
					if (r.state === 'running') {
						t.pct = null; t.msg = '';
						if (r.bytes != null && r.total) { t.pct = Math.min(100, r.bytes * 100 / r.total); t.right = fmtSize(r.bytes) + ' / ' + fmtSize(r.total); }
						else if (r.items > 1 && r.done != null) { t.pct = Math.min(100, r.done * 100 / r.items); t.right = r.done + ' / ' + r.items + ' 项'; }
						else if (r.bytes != null) t.right = fmtSize(r.bytes);
						else t.right = '';
						if (r.elapsed != null) t.msg = '已用 ' + fmtDur(r.elapsed);
						tasksChanged(); return setTimeout(tick, 1000);
					}
					t.state = r.state === 'done' ? (r.fails > 0 ? 'partial' : 'done') : r.state;
					t.pct = r.state === 'done' ? 100 : t.pct; t.msg = r.state === 'failed' && r.msg ? r.msg : (r.dest ? '→ ' + r.dest : ''); t.right = r.elapsed != null ? fmtDur(r.elapsed) : '';
					t.cancel = null; tasksChanged();
					finish(r); resolve(r);
				});
			}
			setTimeout(tick, 800);
		});
	}
	function jobResult(r, o) {
		if (!r.ok) { fail(r, o.label); return Promise.resolve(r); }
		return trackJob(r, o);
	}

	/* ---------- 新建 / 重命名 / 删除 ---------- */
	function doNew(file) {
		if (!canWrite() || S.mode !== 'dir') return;
		askText(file ? '新建文本文件' : '新建文件夹', '名称', file ? '未命名.txt' : '未命名文件夹', { ok: '创建', noExt: !file }).then(function(name) {
			if (!name) return;
			api(file ? 'newfile' : 'mkdir', { root: S.root, path: S.path, name: name }, true, { verb: '在内部存储新建', what: '“' + name + '”' }).then(function(r) {
				if (!r.ok) return fail(r, '创建失败');
				toast((file ? '已创建文件 ' : '已创建文件夹 ') + r.name);
				afterChange(r.name);
			});
		});
	}
	function doRename(e) {
		if (!e || !canWrite()) return;
		askText('重命名', '新名称', e.n, { ok: '重命名', noExt: e.d }).then(function(name) {
			if (!name || name === e.n) return;
			api('rename', { root: S.root, path: relOf(e), name: name }, true, { verb: '重命名', what: '“' + e.n + '” → “' + name + '”' }).then(function(r) {
				if (!r.ok) return fail(r, '重命名失败');
				toast('已重命名为 ' + (r.name || name));
				if (S.mode === 'search') { S.mode = 'dir'; S.q = ''; el.q.value = ''; renderChrome(); }
				afterChange(r.name || name);
			});
		});
	}
	function listNames(items, max) {
		var h = '<ul class="mf-list-sm">';
		items.slice(0, max || 6).forEach(function(e) { h += '<li>' + ic(iconFor(e), 't-' + (e.d ? 'dir' : e.ty || 'other')) + '<span class="mf-ell">' + esc(e.n) + '</span></li>'; });
		if (items.length > (max || 6)) h += '<li class="mf-muted">… 以及另外 ' + (items.length - (max || 6)) + ' 项</li>';
		return h + '</ul>';
	}
	function doDelete(items) {
		if (!items.length || !canWrite()) return;
		var nd = items.filter(function(e) { return e.d; }).length;
		var what = items.length === 1 ? '“' + items[0].n + '”' : items.length + ' 个项目';
		var paths = items.map(relOf);
		function run(ctx) {
			api('delete', { root: S.root, paths: JSON.stringify(paths) }, true, ctx).then(function(r) {
				jobResult(r, { op: 'rm', label: '删除失败', title: items.length === 1 ? items[0].n : items.length + ' 项' });
			});
		}
		// 内部存储: 由「后果 + root 密码」弹窗兼作确认; 其它位置走普通确认
		gateInternal('delete', { root: S.root, paths: paths }, { verb: '删除', what: what, ok: '确认删除' }).then(function(g) {
			if (!g) return;
			if (g.internal) return run(g.ctx);
			askConfirm('删除 ' + what + '?',
				(items.length > 1 ? listNames(items) : '') + '<p>此操作<b>不可恢复</b>(没有回收站)' + (nd ? ', 文件夹内的全部内容也会一并删除' : '') + '。</p>', '删除', true).then(function(ok) {
				if (ok) run(null);
			});
		});
	}

	/* ---------- 复制 / 剪切 / 粘贴 / 移动 ---------- */
	function setClip(op, items) {
		if (!items.length) return;
		if (op === 'cut' && !canWrite()) return toast('此位置是只读的, 不能剪切', 'err');
		S.clip = { op: op, root: S.root, paths: items.map(relOf), n: items.length, first: items[0].n };
		updateActs();
		toast((op === 'cut' ? '已剪切 ' : '已复制 ') + (items.length === 1 ? '“' + items[0].n + '”' : items.length + ' 项') + ', 到目标文件夹后粘贴');
	}
	function conflictDlg(list, op) {
		var hasDir = list.some(function(c) { return c.d; });
		var body = '<p>目标文件夹里已有同名项目:</p><ul class="mf-list-sm">' + list.slice(0, 8).map(function(c) { return '<li>' + ic(c.d ? 'folder' : 'file') + '<span class="mf-ell">' + esc(c.n) + '</span></li>'; }).join('') + (list.length > 8 ? '<li class="mf-muted">… 以及另外 ' + (list.length - 8) + ' 项</li>' : '') + '</ul>';
		if (hasDir) body += '<div class="mf-note warn">' + ic('warn') + '<span>文件夹不支持直接覆盖(不会合并内容), 可选择「保留两者」自动改名。</span></div>';
		var btns = [{ t: '取消', v: null }, { t: '跳过', v: 'skip' }, { t: '保留两者', v: 'rename', primary: !hasDir }];
		if (!hasDir) btns.push({ t: '替换', v: 'overwrite', danger: true });
		return dlg({ title: op === 'mv' ? '移动时发现同名项目' : '复制时发现同名项目', body: body, btns: btns, modal: true });
	}
	function transfer(op, srcRoot, paths, dRoot, dPath, label) {
		var fn = op === 'cp' ? 'copy' : 'move';
		// 从内部存储复制出去只是读取; 移动(会删源)和写入内部存储才需要密码。服务端判定, 这里只在被要求时弹窗
		var ctx = { verb: op === 'cp' ? '复制' : '移动', what: label ? '“' + label + '”' : '' };
		function call(conflict) { return api(fn, { root: srcRoot, paths: JSON.stringify(paths), droot: dRoot, dpath: dPath, conflict: conflict }, true, ctx); }
		return call('ask').then(function(r) {
			if (!r.ok) { fail(r, (OPNAME[op]) + '失败'); return r; }
			if (r.conflicts) {
				return conflictDlg(r.conflicts, op).then(function(c) {
					if (!c) return null;
					return call(c).then(function(r2) { return r2.ok ? trackJob(r2, { op: op, title: label }) : (fail(r2, OPNAME[op] + '失败'), r2); });
				});
			}
			return trackJob(r, { op: op, title: label });
		});
	}
	function doPaste() {
		var c = S.clip;
		if (!c || !canWrite() || S.mode !== 'dir') return;
		var op = c.op === 'cut' ? 'mv' : 'cp';
		transfer(op, c.root, c.paths, S.root, S.path, c.n === 1 ? c.first : c.n + ' 项').then(function(r) {
			if (r && r.ok && op === 'mv') { S.clip = null; updateActs(); }
		});
	}
	// 拖到文件夹 / 侧栏 / 面包屑上
	var dragInfo = null;
	function moveInto(items, dRoot, dPath, copy) {
		var paths = items.map(relOf);
		if (!copy && dRoot === S.root && dPath === S.path) return;
		transfer(copy ? 'cp' : 'mv', S.root, paths, dRoot, dPath, items.length === 1 ? items[0].n : items.length + ' 项');
	}

	/* ---------- 下载 ---------- */
	function doDownload(items) {
		items = items.filter(function(e) { return !e.d && !e.o; });
		if (!items.length) return toast('请先选择文件(文件夹需要先压缩)', 'warn');
		if (items.length > 1) toast('将依次下载 ' + items.length + ' 个文件; 浏览器可能询问是否允许多个下载。');
		items.forEach(function(e, i) {
			setTimeout(function() {
				var a = document.createElement('a');
				a.href = dlUrl(S.root, relOf(e), 'default'); a.download = e.n; a.rel = 'noopener'; a.style.display = 'none';
				el.root.appendChild(a); a.click(); setTimeout(function() { if (a.parentNode) a.parentNode.removeChild(a); }, 1000);
			}, i * 450);
		});
	}

	/* ---------- 压缩 / 解压 ---------- */
	function doCompress(items) {
		if (!items.length || !canWrite()) return;
		var parents = {};
		items.forEach(function(e) { parents[parentOf(relOf(e))] = 1; });
		var pl = Object.keys(parents);
		if (pl.length > 1) return toast('搜索结果里的项目不在同一文件夹, 请先进入目标文件夹再压缩', 'warn');
		var a = S.arch || {}, canTgz = a.tar && a.gzip, canZip = !!a.zip;
		if (!canTgz && !canZip) return dlg({ title: '无法压缩', body: '<div class="mf-note bad">' + ic('warn') + '<span>固件缺少压缩工具(tar + gzip 或 zip), 无法压缩。可在软件包里安装 tar、gzip 后重试。</span></div>', btns: [{ t: '好', v: 1, primary: 1 }] });
		var def = items.length === 1 ? items[0].n.replace(/\.[^.]+$/, '') || items[0].n : (S.path ? baseOf(S.path) : '归档');
		var body = div('', '<div class="mf-fld"><label for="mfcn">压缩包名称</label><input class="mf-in" id="mfcn" autocomplete="off" spellcheck="false"></div>' +
			'<div class="mf-fld"><label for="mfcf">格式</label><select class="mf-in" id="mfcf">' + (canTgz ? '<option value="tgz">.tar.gz</option>' : '') + '<option value="zip"' + (canZip ? '' : ' disabled') + '>.zip' + (canZip ? '' : '(固件未安装 zip 工具)') + '</option></select></div>' +
			'<div class="mf-muted" id="mfce" role="alert" style="color:var(--danger);min-height:18px"></div>');
		var inp = body.querySelector('#mfcn'), sel = body.querySelector('#mfcf'), er = body.querySelector('#mfce');
		inp.value = def; if (!canTgz) sel.value = 'zip';
		dlg({ title: '压缩 ' + (items.length === 1 ? '“' + items[0].n + '”' : items.length + ' 个项目'), body: body, btns: [{ t: '取消', v: null }, { t: '压缩', v: 'ok', primary: 1 }], selectAll: false,
			onbtn: function(v, box, close) {
				if (v !== 'ok') return true;
				var bad = nameErr(inp.value.trim()); if (bad) { er.textContent = bad; return false; }
				close({ name: inp.value.trim(), fmt: sel.value }); return false;
			} }).then(function(o) {
			if (!o) return;
			api('compress', { root: S.root, paths: JSON.stringify(items.map(relOf)), dpath: pl[0], name: o.name, fmt: o.fmt }, true, { verb: '在内部存储生成压缩包', what: '“' + o.name + '”' }).then(function(r) {
				jobResult(r, { op: 'pack', label: '压缩失败', title: items.length === 1 ? items[0].n : items.length + ' 项', dest: r.dest });
			});
		});
	}
	function extractMissing(e) {
		var a = S.arch || {}, n = e.n.toLowerCase();
		if (/\.zip$/.test(n)) return a.unzip ? '' : '固件未安装 unzip, 无法解压 .zip';
		if (/\.(tar\.bz2|tbz2?)$/.test(n)) return a.tar && a.bzip2 ? '' : '固件缺少 tar/bzip2, 无法解压';
		if (/\.(tar\.xz|txz)$/.test(n)) return a.tar && a.xz ? '' : '固件缺少 tar/xz, 无法解压';
		if (/\.(tar\.gz|tgz)$/.test(n)) return a.tar && a.gzip ? '' : '固件缺少 tar/gzip, 无法解压';
		if (/\.gz$/.test(n)) return a.gzip ? '' : '固件缺少 gzip, 无法解压';
		if (/\.tar$/.test(n)) return a.tar ? '' : '固件缺少 tar, 无法解压';
		return '不支持的压缩格式';
	}
	function doExtract(e) {
		if (!e || !canWrite()) return;
		var miss = extractMissing(e);
		if (miss) return toast(miss, 'err');
		api('extract', { root: S.root, path: relOf(e), dpath: parentOf(relOf(e)) }, true, { verb: '解压到内部存储', what: '“' + e.n + '”' }).then(function(r) {
			jobResult(r, { op: 'unpack', label: '解压失败', title: e.n, dest: r.dest, onDone: function(res) {
				if (res.state === 'done' && res.fails === 0 && res.dest && sameLoc(S.root, parentOf(relOf(e)))) afterChange(res.dest); else afterChange();
			} });
		});
	}

	/* ---------- 属性 ---------- */
	function showProps(e) {
		var rel = e ? relOf(e) : S.path;
		var body = div('', '<div class="mf-muted">读取中…</div>');
		dlg({ title: '属性', body: body, wide: false, btns: [{ t: '关闭', v: 1, primary: 1 }] });
		api('stat', { root: S.root, path: rel }).then(function(s) {
			if (!s.ok) { body.innerHTML = '<div class="mf-note bad">' + ic('warn') + '<span>' + esc(s.msg || '无法读取属性') + '</span></div>'; return; }
			body.innerHTML = '<div style="display:flex;align-items:center;gap:10px;margin:0 0 6px">' + ic(s.type === 'dir' ? 'folder' : (s.ty && IP[s.ty] ? s.ty : 'file'), 't-' + (s.type === 'dir' ? 'dir' : s.ty || 'other')) + '<b style="color:var(--ink);word-break:break-all">' + esc(s.name || (curRoot() || {}).label || '') + '</b></div>' +
				detailHtml(s) + (s.type === 'dir' ? '<div style="padding:0 14px 6px"><button type="button" class="mf-pill" data-r="ds">计算文件夹大小</button> <span class="mf-muted" data-r="dsr"></span></div>' : '');
			var b = body.querySelector('[data-r=ds]');
			if (b) b.onclick = function() { dirSizeInto(rel, b, body.querySelector('[data-r=dsr]')); };
		});
	}
	function dirSizeInto(rel, btn, out) {
		if (btn) btn.disabled = true;
		out.textContent = '计算中…';
		api('dirsize', { root: S.root, path: rel }).then(function(r) {
			if (btn) btn.disabled = false;
			if (!r.ok) { out.textContent = r.msg || '计算失败'; return; }
			out.textContent = fmtSize(r.bytes) + '(' + r.files + ' 个文件)' + (r.timeout ? ', 文件太多, 仅统计了一部分' : '');
		});
	}

	/* ---------- 上传 ---------- */
	var upq = [], upRunning = false;
	function hasActiveUploads() { return S.tasks.some(function(t) { return t.kind === 'up' && (t.state === 'running' || t.state === 'queued' || t.state === 'writing'); }); }
	function pickFiles(folder) {
		if (!canWrite() || S.mode !== 'dir') return toast(S.mode !== 'dir' ? '请先退出搜索' : '此位置是只读的, 不能上传', 'err');
		var inp = document.createElement('input');
		inp.type = 'file'; inp.multiple = true; inp.style.display = 'none';
		if (folder) { inp.setAttribute('webkitdirectory', ''); inp.setAttribute('directory', ''); }
		inp.onchange = function() {
			var fl = [].slice.call(inp.files || []);
			if (inp.parentNode) inp.parentNode.removeChild(inp);
			if (!fl.length) return;
			uploadItems(fl.map(function(f) { return { file: f, dir: folder && f.webkitRelativePath ? parentOf(f.webkitRelativePath) : '' }; }), S.root, S.path);
		};
		el.root.appendChild(inp); inp.click();
	}
	function uploadItems(items, root, path) {
		if (!items.length) return;
		// 上传到内部存储前先讲清后果并取得 root 密码(别等大文件传完才被拒绝)
		gateInternal('write', { root: root, paths: [], dpath: path }, { verb: '上传到', what: '内部存储的 /' + path, ok: '确认并上传' }).then(function(g) {
			if (g) queueUpload(items, root, path, g.internal ? g.ctx : null);
		});
	}
	function queueUpload(items, root, path, ctx) {
		var files = items.filter(function(i) { return i.file; });
		var total = files.reduce(function(a, i) { return a + i.file.size; }, 0);
		var title = files.length === 1 ? files[0].file.name : (files.length ? files.length + ' 个文件' : '空文件夹');
		var t = addTask({ kind: 'up', icon: 'upload', title: '上传 ' + title, state: 'queued', pct: 0, total: total, done: 0, items: items, root: root, path: path, fails: [], okN: 0, skipN: 0, msg: '等待中', cancelled: false, xhr: null, dirs: {}, ctx: ctx });
		t.cancel = function() {
			t.cancelled = true;
			if (t.xhr) { try { t.xhr.abort(); } catch (e) {} }
			if (t.state === 'queued') { t.state = 'cancelled'; t.msg = '已取消'; t.cancel = null; tasksChanged(); }
		};
		upq.push(t); pumpUploads();
	}
	function pumpUploads() {
		if (upRunning) return;
		var t = upq.shift();
		if (!t) return;
		if (t.cancelled) return pumpUploads();
		upRunning = true;
		runUpload(t).then(function() { upRunning = false; pumpUploads(); }, function() { upRunning = false; pumpUploads(); });
	}
	function ensureDirs(t, rel) {
		var segs = rel ? rel.split('/') : [], acc = t.path, chain = Promise.resolve({ ok: true });
		segs.forEach(function(sg) {
			var parent = acc; acc = joinp(acc, sg); var full = acc;
			chain = chain.then(function(r) {
				if (!r.ok || t.dirs[full]) return r;
				return api('mkdir', { root: t.root, path: parent, name: sg }, true, t.ctx).then(function(m) {
					if (m.ok || m.err === 'exists') { t.dirs[full] = 1; return { ok: true }; }
					return m;
				});
			});
		});
		return chain;
	}
	function askUpConflict(name, t, prefix) {
		if (S.upPolicy) return Promise.resolve(S.upPolicy);
		if (!S.open) return Promise.resolve('rename');
		if (t) { t.state = 'running'; t.msg = prefix + ' · 等待你选择同名处理方式'; t.right = ''; tasksChanged(); }
		var body = div('', '<p>目标文件夹里已有名为“' + esc(name) + '”的文件。要怎么处理?</p><label class="mf-chkl"><input type="checkbox" id="mfall"><span>对后面遇到的同名文件也这样处理</span></label>');
		var all = body.querySelector('#mfall'), val = null;
		return dlg({ title: '同名文件', body: body, modal: true, btns: [{ t: '跳过', v: 'skip' }, { t: '保留两者', v: 'rename', primary: 1 }, { t: '覆盖', v: 'overwrite', danger: true }],
			onbtn: function(v) { val = v; return true; } }).then(function(v) {
			v = v || 'skip';
			if (all.checked) S.upPolicy = v;
			return v;
		});
	}
	function xhrUpload(t, it, name, conflict, onprog) {
		return new Promise(function(resolve) {
			var xhr = new XMLHttpRequest(), f = it.file;
			t.xhr = xhr;
			var q = params({ token: token(), root: t.root, path: joinp(t.path, it.dir || '').replace(/\/$/, ''), name: name, size: f.size, conflict: conflict, mtime: f.lastModified ? Math.floor(f.lastModified / 1000) : null }).toString();
			xhr.open('POST', fnurl('upload') + '?' + q);
			xhr.setRequestHeader('Content-Type', 'application/octet-stream');
			if (t.ctx && t.ctx.pw) xhr.setRequestHeader('Authorization', pwHeader(t.ctx.pw));
			xhr.upload.onprogress = function(ev) { if (ev.lengthComputable) onprog(ev.loaded, ev.total); };
			xhr.upload.onload = function() { onprog(f.size, f.size, true); };
			xhr.onload = function() {
				t.xhr = null;
				var j; try { j = JSON.parse(xhr.responseText); } catch (e) { j = { ok: false, err: xhr.status === 403 || xhr.status === 401 ? 'auth' : 'bad', msg: xhr.status === 403 || xhr.status === 401 ? '没有权限, 或登录已过期, 请刷新页面后重试' : '服务器返回了无法识别的内容' }; }
				resolve(j);
			};
			xhr.onerror = function() { t.xhr = null; resolve({ ok: false, err: 'net', msg: '网络中断, 上传未完成' }); };
			xhr.ontimeout = xhr.onerror;
			xhr.onabort = function() { t.xhr = null; resolve({ ok: false, err: 'abort', msg: '已取消' }); };
			xhr.send(f);
		});
	}
	function runUpload(t) {
		t.state = 'running'; t.msg = ''; tasksChanged();
		var idx = 0, done = 0, lastT = Date.now(), lastB = 0, speed = 0, nfile = t.items.filter(function(i) { return i.file; }).length, fno = 0;
		function fin() {
			t.xhr = null; t.cancel = null; t.ctx = null;
			var summary;
			if (t.cancelled) { t.state = 'cancelled'; summary = '上传已取消(已完成 ' + t.okN + ' 个)'; }
			else if (t.fails.length && !t.okN) { t.state = 'failed'; summary = '上传失败'; }
			else if (t.fails.length) { t.state = 'partial'; summary = '上传完成 ' + t.okN + ' 个, ' + t.fails.length + ' 个失败'; }
			else { t.state = 'done'; t.pct = 100; summary = '上传完成' + (t.okN ? ' ' + t.okN + ' 个文件' : '') + (t.skipN ? '(跳过 ' + t.skipN + ' 个)' : ''); }
			t.msg = ''; t.right = ''; t.items = null;
			tasksChanged();
			toast(summary, t.state === 'failed' ? 'err' : (t.fails.length ? 'warn' : null), t.fails.length ? 8000 : 3600);
			if (!hasActiveUploads()) S.upPolicy = null;
			rootsSoon();
			if (sameLoc(t.root, t.path)) refresh(true);
		}
		function next() {
			if (t.cancelled || idx >= t.items.length) return Promise.resolve(fin());
			var it = t.items[idx++];
			return ensureDirs(t, it.dir || '').then(function(d) {
				if (!d.ok) { t.fails.push((it.file ? it.file.name : it.dir) + ': ' + (d.msg || '无法创建文件夹')); return next(); }
				if (!it.file) return next();
				fno++;
				var f = it.file, dpath = joinp(t.path, it.dir || '').replace(/\/$/, '');
				var prefix = '第 ' + fno + '/' + nfile + ' 个: ' + f.name;
				t.msg = prefix; tasksChanged();
				var small = f.size < 262144 && S.upPolicy !== 'skip';
				function send(name, conflict) {
					var base = done; lastT = Date.now(); lastB = base;
					return xhrUpload(t, it, name, conflict, function(loaded, tot, full) {
						var cur = base + loaded, now = Date.now();
						if (now - lastT >= 600) { speed = speed ? speed * 0.6 + (cur - lastB) / ((now - lastT) / 1000) * 0.4 : (cur - lastB) / ((now - lastT) / 1000); lastT = now; lastB = cur; }
						t.pct = t.total ? Math.min(99.5, cur * 100 / t.total) : null;
						t.right = fmtSize(cur) + ' / ' + fmtSize(t.total) + (speed > 0 ? ' · ' + fmtSize(speed) + '/s' : '');
						if (full) { t.state = 'writing'; t.msg = prefix + ' · 等待写入完成'; } else { t.state = 'running'; t.msg = prefix; }
						tasksChanged();
					}).then(function(r) {
						t.state = 'running';
						if (r.ok) { t.okN++; done += f.size; return next(); }
						if (r.err === 'abort') return next();
						if (r.err === 'exists' && small) {
							return askUpConflict(f.name, t, prefix).then(function(c) {
								if (c === 'skip') { t.skipN++; done += f.size; return next(); }
								return send(f.name, c);
							});
						}
						t.fails.push(f.name + ': ' + (r.msg || '上传失败'));
						done += f.size;
						if (r.err === 'net' || r.err === 'auth' || r.err === 'token') { t.cancelled = true; }
						return next();
					});
				}
				if (small) return send(f.name, S.upPolicy || 'fail');
				return api('upcheck', { root: t.root, path: dpath, name: f.name, size: f.size }).then(function(c) {
					if (!c.ok) { t.fails.push(f.name + ': ' + (c.msg || '无法上传')); done += f.size; if (c.err === 'net' || c.err === 'auth') t.cancelled = true; return next(); }
					if (c.exists) {
						if (c.is_dir) { t.fails.push(f.name + ': 已存在同名文件夹'); done += f.size; return next(); }
						return askUpConflict(f.name, t, prefix).then(function(p) {
							if (p === 'skip') { t.skipN++; done += f.size; return next(); }
							return send(f.name, p);
						});
					}
					return send(f.name, 'fail');
				});
			});
		}
		return next();
	}

	/* ---------- 拖入文件 / 文件夹 ---------- */
	function readAll(reader) {
		return new Promise(function(res) {
			var all = [];
			(function more() { reader.readEntries(function(b) { if (!b.length) return res(all); all = all.concat([].slice.call(b)); more(); }, function() { res(all); }); })();
		});
	}
	function walk(entry, prefix, out, cap) {
		if (out.length > cap) return Promise.resolve();
		if (entry.isFile) return new Promise(function(res) { entry.file(function(f) { out.push({ file: f, dir: prefix }); res(); }, function() { res(); }); });
		var here = joinp(prefix, entry.name);
		return readAll(entry.createReader()).then(function(kids) {
			if (!kids.length) { out.push({ dir: here }); return; }
			return kids.reduce(function(p, k) { return p.then(function() { return walk(k, here, out, cap); }); }, Promise.resolve());
		});
	}
	function dropFiles(dt, root, path) {
		var ents = [], plain = [];
		var its = dt.items ? [].slice.call(dt.items) : [];
		its.forEach(function(i) {
			if (i.kind !== 'file') return;
			var en = i.webkitGetAsEntry ? i.webkitGetAsEntry() : null;
			if (en) ents.push(en); else { var f = i.getAsFile(); if (f) plain.push({ file: f, dir: '' }); }
		});
		if (!its.length && dt.files) [].slice.call(dt.files).forEach(function(f) { plain.push({ file: f, dir: '' }); });
		var out = plain.slice(), CAP = 5000;
		return ents.reduce(function(p, en) { return p.then(function() { return walk(en, '', out, CAP); }); }, Promise.resolve()).then(function() {
			if (out.length > CAP) { out = out.slice(0, CAP); toast('一次最多上传 ' + CAP + ' 个项目, 超出部分已忽略', 'warn'); }
			uploadItems(out, root, path);
		});
	}
	/* =====================================================================
	   内部存储守卫 / 占用条 / 格式化 / SMB 共享管理
	   内部存储(固件所在闪存)的改动由服务端强制要求 root 密码; 这里只负责把后果讲清楚并收集密码。
	   密码只放在内存里的 ctx.pw, 经 Authorization: Mwfiles <base64> 请求头发给服务端, 不进 URL、不写存储、不写控制台。
	   ===================================================================== */

	/* ---------- 占用条 ---------- */
	// 正常蓝 / ≥75% 橙 / ≥90% 红; 同时给出文字, 不只靠颜色
	function usageOf(r) {
		if (!r || !r.total) return null;
		var pct = Math.max(0, Math.min(100, Math.round((r.total - (r.free || 0)) * 100 / r.total)));
		var lv = pct >= 90 ? 'crit' : pct >= 75 ? 'warn' : 'ok';
		return { pct: pct, lv: lv, txt: pct + '%', word: lv === 'crit' ? '空间紧张' : lv === 'warn' ? '空间偏少' : '' };
	}
	// 容量说明: 共 X · 可用 Y (总容量未知时只给可用)
	function capText(r) {
		if (!r) return '';
		var t = [];
		if (r.total) t.push('共 ' + fmtSize(r.total));
		if (r.free != null) t.push('可用 ' + fmtSize(r.free));
		return t.join(' · ');
	}
	function meterHtml(u) {
		if (!u) return '';
		return '<span class="mf-use ' + u.lv + '"><span class="mf-meter" role="img" aria-label="已用 ' + u.pct + '%' + (u.word ? ', ' + u.word : '') + '"><i style="width:' + u.pct + '%"></i></span><em>' + u.txt + '</em></span>';
	}

	/* ---------- 密码确认 ---------- */
	function pwHeader(pw) { return 'Mwfiles ' + btoa(unescape(encodeURIComponent(pw))); }
	var LV_TXT = { info: '提示', warn: '有风险', crit: '高风险' };
	function consequenceHtml(g) {
		var h = '';
		if (g.owners && g.owners.length) {
			h += '<div class="mf-g-sec">涉及的软件包 / 组件</div><ul class="mf-own">';
			g.owners.slice(0, 6).forEach(function(o) {
				h += '<li><b>' + esc(o.title || o.pkg) + '</b> <code>' + esc(o.pkg) + (o.version ? ' ' + esc(o.version) : '') + '</code>' +
					(o.core ? ' <span class="mf-tag bad">系统核心</span>' : '') + (o.desc ? '<small>' + esc(o.desc) + '</small>' : '') + '</li>';
			});
			if (g.owners.length > 6) h += '<li class="mf-muted">… 以及另外 ' + (g.owners.length - 6) + ' 个</li>';
			h += '</ul>';
		}
		if (g.text && g.text.length) {
			h += '<div class="mf-g-sec">可能的后果</div><ul class="mf-cons">';
			g.text.forEach(function(t) { h += '<li>' + esc(t) + '</li>'; });
			h += '</ul>';
		}
		return h;
	}
	// g: { level, owners, text, count }; o: { verb, what, ok, err }; 返回 Promise<密码 | null>
	function pwPrompt(g, o) {
		o = o || {};
		g = g || {};
		var lv = g.level || 'info';
		var body = div('mf-guard lv-' + lv,
			'<div class="mf-g-h">' + ic(lv === 'info' ? 'lock' : 'warn') + '<span>这是设备的<b>内部存储</b>(固件所在的闪存), 改动需要 root 密码确认</span><em class="mf-lv">' + LV_TXT[lv] + '</em></div>' +
			(o.what ? '<p class="mf-g-w">' + esc(o.verb || '修改') + ' ' + esc(o.what) + '</p>' : '') +
			consequenceHtml(g) +
			'<label class="mf-chkl"><input type="checkbox" id="mfgk"><span>我已了解上述后果</span></label>' +
			'<div class="mf-fld"><label for="mfgp">root 密码</label><input class="mf-in" id="mfgp" type="password" autocomplete="off" spellcheck="false"></div>' +
			'<div class="mf-muted" id="mfge" role="alert" style="min-height:18px;color:var(--danger)"></div>');
		var pw = body.querySelector('#mfgp'), ck = body.querySelector('#mfgk'), er = body.querySelector('#mfge');
		if (o.err) er.textContent = o.err;
		var busy = false;
		return dlg({
			title: o.title || '需要 root 密码确认', body: body, modal: true, wide: true, selectAll: false,
			btns: [{ t: '取消', v: null }, { t: o.ok || '确认并继续', v: 'ok', primary: lv === 'info', danger: lv !== 'info' }],
			init: function() { setTimeout(function() { ck.focus(); }, 0); },
			onbtn: function(v, box, close, bt) {
				if (v !== 'ok') return true;
				if (busy) return false;
				if (!ck.checked) { er.textContent = '请先勾选「我已了解上述后果」'; ck.focus(); return false; }
				if (!pw.value) { er.textContent = '请输入 root 密码'; pw.focus(); return false; }
				busy = true; bt.disabled = true; er.textContent = '';
				var probe = { pw: pw.value, noprompt: true };
				api('checkpw', {}, true, probe).then(function(r) {
					busy = false; bt.disabled = false;
					if (r.ok) { close(probe.pw); return; }
					er.textContent = r.msg || '密码校验失败';
					pw.value = ''; pw.focus();
				});
				return false;
			}
		});
	}
	// 写请求被服务端要求密码时的统一处理: 弹窗取得密码 → 记到本次操作的 ctx → 重试
	function guardRetry(r, ctx, retry) {
		if (!r || r.ok || !ctx || ctx.noprompt || (r.err !== 'need_root_pw' && r.err !== 'bad_pw')) return r;
		if (ctx.asking) return ctx.asking.then(function() { return retry(); });
		ctx.pw = null;
		var info = r.info || {};
		ctx.asking = pwPrompt(info, { verb: ctx.verb, what: ctx.what, err: r.err === 'bad_pw' ? r.msg : '' }).then(function(pw) {
			ctx.asking = null;
			if (pw == null) { ctx.cancelled = true; return null; }
			ctx.pw = pw;
			return pw;
		});
		return ctx.asking.then(function(pw) {
			if (pw == null) return { ok: false, err: 'cancel', msg: '已取消', cancelled: true };
			return retry();
		});
	}
	// 事先询问: 这批路径是不是内部存储。是 → 一个弹窗里讲清后果并收密码(顺带充当「确认」); 返回 Promise<ctx | null>(null=用户取消)
	function gateInternal(op, q, o) {
		o = o || {};
		var ctx = { verb: o.verb, what: o.what };
		return api('guardinfo', { op: op, root: q.root, paths: JSON.stringify(q.paths || []), droot: q.droot, dpath: q.dpath }).then(function(g) {
			if (!g.ok) { fail(g, '检查失败'); return null; }
			if (!g.internal) return { ctx: ctx, internal: false };
			return pwPrompt(g, { verb: o.verb, what: o.what, ok: o.ok }).then(function(pw) {
				if (pw == null) return null;
				ctx.pw = pw;
				return { ctx: ctx, internal: true };
			});
		});
	}

	/* ---------- 内部存储提示(文件夹横幅 / 条目归属) ---------- */
	function ownerName(m) { return m.title || m.pkg || m.name || ''; }
	function internalBanner() {
		var r = curRoot();
		if (!r || S.mode !== 'dir') return '';
		var m = S.meta || {};
		if (!m.internal) return '';
		var o = m.owner, txt;
		if (o && o.pkg) txt = '此文件夹属于软件包「' + esc(o.meta && o.meta.title ? o.meta.title + '(' + o.pkg + ')' : o.pkg) + '」' + (o.meta && o.meta.core ? ', 是系统核心组件' : '');
		else if (o && o.multi) txt = '此文件夹里有多个软件包的文件';
		else txt = '这里是内部存储(固件所在的闪存)';
		return '<div class="mf-banner' + (o && o.meta && o.meta.core ? ' crit' : '') + '">' + ic('lock') + '<span>' + txt + '。删除、重命名、移动或写入需要输入 root 密码, 并可能影响系统或插件运行。</span></div>';
	}
	function ownTag(e) {
		if (!e || !e.ow || e.ow === '-') return '';
		if (e.ow === '*') return '<span class="mf-tag own" title="包含多个软件包的文件">多个组件</span>';
		var z = OWN_TITLE[e.ow];
		return '<span class="mf-tag own" title="属于软件包 ' + esc(e.ow) + '">' + esc(z || e.ow) + '</span>';
	}
	var OWN_TITLE = {};   // 软件包 → 中文名(由 pkg-zh.json 按需填充, 取不到就显示包名)
	var ownLoad = null;
	function loadOwnTitles() {
		if (ownLoad) return ownLoad;
		ownLoad = fetch(((window.L && L.env && L.env.resource) || '/luci-static/resources') + '/pkg-zh.json', { credentials: 'same-origin' }).then(function(r) { return r.ok ? r.json() : null; }).then(function(d) {
			if (d && d.p) for (var k in d.p) if (d.p[k] && typeof d.p[k][0] === 'string') OWN_TITLE[k] = d.p[k][0];
			if (S.open) render(true);
		}, function() {});
		return ownLoad;
	}

	/* ---------- 格式化 ---------- */
	function fmtDev(d) {
		var h = '<span class="mf-fmt-n">' + esc(d.name) + '</span>';
		var sub = [fmtSize(d.size)];
		if (d.model) sub.push(d.model);
		if (d.fstype) sub.push(d.fstype.toUpperCase());
		if (d.label) sub.push('「' + d.label + '」');
		return h + '<small class="mf-ell">' + esc(sub.join(' · ')) + '</small>';
	}
	function formatDialog(pre) {
		var body = div('', '<div class="mf-muted">读取存储设备…</div>');
		var closer;
		dlg({ title: '格式化存储设备', body: body, wide: true, modal: true, btns: [{ t: '关闭', v: 1 }], onbtn: function() { return !body.__busy; }, init: function(box, close) { closer = close; } });
		api('formatinfo').then(function(fi) {
			if (!fi.ok) { body.innerHTML = '<div class="mf-note bad">' + ic('warn') + '<span>' + esc(fi.msg || '无法读取存储设备') + '</span></div>'; return; }
			var disks = fi.disks || [], fss = fi.fs || [];
			var h = '<div class="mf-note warn">' + ic('warn') + '<span>格式化会<b>清除所选分区上的全部文件, 无法恢复</b>。只能格式化外接 / 辅助存储; 承载系统的内部存储(固件、/overlay 所在的闪存或 SD 卡)不可格式化。文件管理不会创建或修改分区表。</span></div>';
			if (!disks.length) { body.innerHTML = h + '<div class="mf-empty-s">没有找到可格式化的存储设备(请先插入 U 盘或 SD 卡)</div>'; return; }
			h += '<div class="mf-g-sec">1. 选择要格式化的分区</div><div class="mf-fmt-l" role="radiogroup" aria-label="选择分区">';
			disks.forEach(function(d, i) {
				var off = d.system;
				h += '<label class="mf-fmt-i' + (off ? ' off' : '') + '"><input type="radio" name="mfdev" value="' + esc(d.dev) + '"' + (off ? ' disabled' : '') + '>' + ic('hdd') + '<span class="mf-nm">' + fmtDev(d) +
					(d.mounts && d.mounts.length ? '<small class="mf-ell">已挂载: ' + esc(d.mounts.join('、')) + '(格式化前会先卸载)</small>' : '') + '</span>' +
					(off ? '<span class="mf-tag bad" title="' + esc(d.reason || '') + '">系统盘 · 不可格式化</span>' : '<span class="mf-tag">' + (d.kind === 'sd' ? 'SD 卡' : d.kind === 'nvme' ? 'NVMe' : 'USB') + '</span>') + '</label>';
			});
			h += '</div><div class="mf-g-sec">2. 文件系统</div><div class="mf-fmt-fs" role="radiogroup" aria-label="文件系统">';
			var firstOk = null;
			fss.forEach(function(f) {
				if (f.ok && !firstOk) firstOk = f.id;
				h += '<label class="mf-fmt-i fs' + (f.ok ? '' : ' off') + '"><input type="radio" name="mffs" value="' + esc(f.id) + '"' + (f.ok ? '' : ' disabled') + (f.ok && firstOk === f.id ? ' checked' : '') + '><span class="mf-nm"><b>' + esc(f.name) + '</b><small>' + esc(f.ok ? f.note : '不可用: ' + (f.why || '缺少工具')) + '</small></span></label>';
			});
			h += '</div><div class="mf-fld"><label for="mffl">卷标(可留空)</label><input class="mf-in" id="mffl" autocomplete="off" spellcheck="false"></div>' +
				'<div class="mf-g-sec">3. 确认</div>' +
				'<div class="mf-fld"><label for="mffc">输入所选设备名以确认(如 <code id="mffn">sda1</code>)</label><input class="mf-in" id="mffc" autocomplete="off" spellcheck="false"></div>' +
				'<div class="mf-fld"><label for="mffp">root 密码</label><input class="mf-in" id="mffp" type="password" autocomplete="off" spellcheck="false"></div>' +
				'<div class="mf-muted" id="mffe" role="alert" style="min-height:18px;color:var(--danger)"></div>' +
				'<div style="display:flex;gap:8px;align-items:center"><button type="button" class="mf-pill danger" data-r="go">格式化</button><span class="mf-muted" id="mffs"></span></div>';
			body.innerHTML = h;
			var er = body.querySelector('#mffe'), st = body.querySelector('#mffs'), gobtn = body.querySelector('[data-r=go]');
			function chosen() { var x = body.querySelector('input[name=mfdev]:checked'); return x ? x.value : ''; }
			function devOf(v) { for (var i = 0; i < disks.length; i++) if (disks[i].dev === v) return disks[i]; return null; }
			function chosenFs() { var x = body.querySelector('input[name=mffs]:checked'); return x ? x.value : ''; }
			body.addEventListener('change', function() { var d = devOf(chosen()); body.querySelector('#mffn').textContent = d ? d.name : 'sda1'; });
			if (pre) { var px = body.querySelector('input[name=mfdev][value="' + pre.replace(/"/g, '') + '"]'); if (px && !px.disabled) { px.checked = true; body.querySelector('#mffn').textContent = devOf(pre).name; } }
			gobtn.onclick = function() {
				er.textContent = '';
				var d = devOf(chosen()), fsid = chosenFs();
				if (!d) return (er.textContent = '请先选择要格式化的分区');
				if (!fsid) return (er.textContent = '请选择文件系统');
				if (body.querySelector('#mffc').value.trim() !== d.name) return (er.textContent = '确认文字不匹配, 请输入「' + d.name + '」');
				var pw = body.querySelector('#mffp').value;
				if (!pw) return (er.textContent = '请输入 root 密码');
				body.__busy = true; gobtn.disabled = true;
				body.querySelectorAll('input').forEach(function(x) { x.disabled = true; });
				st.textContent = '正在格式化 ' + d.name + ' …(大容量存储可能需要几分钟, 请勿拔出)';
				api('format', { dev: d.dev, fs: fsid, label: body.querySelector('#mffl').value.trim(), confirm: d.name }, true, { pw: pw, noprompt: true }).then(function(r) {
					return r.ok && r.state === 'running' ? pollMkfs(r, st) : r;
				}).then(function(r) {
					body.__busy = false;
					if (!r.ok || r.state === 'failed') {
						gobtn.disabled = false; st.textContent = '';
						body.querySelectorAll('input').forEach(function(x) { x.disabled = false; });
						devOf(chosen()); body.querySelectorAll('input[name=mfdev]').forEach(function(x) { var dd = devOf(x.value); if (dd && dd.system) x.disabled = true; });
						body.querySelectorAll('.mf-fmt-fs .off input').forEach(function(x) { x.disabled = true; });
						er.textContent = r.msg || '格式化失败';
						if (r.err === 'bad_pw' || r.err === 'pw_locked') body.querySelector('#mffp').value = '';
						return;
					}
					if (closer) closer(1);
					loadRoots().then(function() {
						if (r.code === 'nomount' || !r.dest) { toast('已格式化 ' + d.name + ', 但没能自动挂载: ' + (r.msg || '请拔出重新插入'), 'warn', 9000); return; }
						toast('已格式化 ' + d.name + ' 为 ' + fsid.toUpperCase());
						var nr = null; S.roots.forEach(function(x) { if (x.path === r.dest) nr = x; });
						if (nr) go(nr.id, '');
					});
				});
			};
		});
	}
	function pollMkfs(r, st) {
		return new Promise(function(resolve) {
			var jid = r.id || r.job;
			(function tick() {
				api('job', { id: jid }).then(function(x) {
					if (!x.ok && x.err === 'net') return setTimeout(tick, 2000);
					if (!x.ok) return resolve(x);
					if (x.state === 'running') { if (x.elapsed != null) st.textContent = '正在格式化…已用 ' + fmtDur(x.elapsed) + '(请勿拔出)'; return setTimeout(tick, 1500); }
					resolve({ ok: true, state: x.state, code: x.code, msg: x.msg, dest: x.dest });
				});
			})();
		});
	}

	/* ---------- SMB 共享管理 ---------- */
	function shareState(sl) {
		var c = sl.caps || {};
		if (!c.share) return { bad: true, html: '<div class="mf-note warn">' + ic('warn') + '<span>固件没有安装 SMB 服务端, 无法共享文件夹。请先在「软件包」里安装 <b>ksmbd-server</b>(推荐, 更省内存)或 samba4-server。安装后这里会自动出现共享功能。</span></div>' };
		var s = sl.state || {};
		var h = '<div class="mf-share-svc"><span class="mf-dot ' + (s.running ? 'on' : 'off') + '"></span><b>' + (c.share_server === 'ksmbd' ? 'ksmbd' : 'Samba') + ' 服务' + (s.running ? '运行中' : '未运行') + '</b>' +
			'<span class="mf-muted">' + (s.enabled ? '开机自动启动' : '未设为开机启动') + '</span><span class="mf-grow"></span>';
		h += s.running ? '<button type="button" class="mf-pill" data-r="svc" data-v="restart">重启</button><button type="button" class="mf-pill" data-r="svc" data-v="stop">停止</button>' : '<button type="button" class="mf-pill primary" data-r="svc" data-v="start">启动</button>';
		h += '<button type="button" class="mf-pill" data-r="svc" data-v="' + (s.enabled ? 'disable' : 'enable') + '">' + (s.enabled ? '取消开机启动' : '开机启动') + '</button></div>';
		return { bad: false, html: h };
	}
	function shareRowHtml(x) {
		var meta = [x.ro ? '只读' : '可读写', x.guest ? '允许匿名访问' : (x.users && x.users.length ? '账户: ' + x.users.join('、') : '需要账户')];
		if (x.comment) meta.push(x.comment);
		return '<div class="mf-share-row' + (x.exists ? '' : ' gone') + '">' + ic('share') + '<div class="mf-nm"><b class="mf-ell">' + esc(x.name) + '</b><small class="mf-ell" title="' + esc(x.path || '') + '">' + esc(x.path || '') + (x.exists ? '' : '(文件夹已不存在)') + '</small><small>' + esc(meta.join(' · ')) + '</small></div>' +
			(x.ours ? '<button type="button" class="mf-pill" data-r="edit" data-id="' + esc(x.id) + '">修改</button><button type="button" class="mf-pill danger" data-r="del" data-id="' + esc(x.id) + '">取消共享</button>' : '<span class="mf-tag" title="不是由文件管理创建的, 请到「SMB 服务」页面修改">外部配置</span>') + '</div>';
	}
	// 共享参数表单(新建 / 修改共用)。返回 Promise<{ name, ro, guest, users, newuser, newpass, comment } | null>
	function shareForm(o, sl) {
		var acc = sl.accounts || [];
		var body = div('', '<div class="mf-fld"><label for="shn">共享名称</label><input class="mf-in" id="shn" autocomplete="off" spellcheck="false"></div>' +
			'<div class="mf-fld"><label for="shc">说明(可选)</label><input class="mf-in" id="shc" autocomplete="off" spellcheck="false"></div>' +
			'<label class="mf-chkl"><input type="checkbox" id="shr"><span>只读(访问者不能修改文件)</span></label>' +
			'<label class="mf-chkl"><input type="checkbox" id="shg"><span>允许匿名访问(无需账户)<small>局域网内任何人都可以访问这个文件夹, 仅在确认安全时勾选。</small></span></label>' +
			'<div class="mf-fld" id="shuw"><label>允许访问的账户</label><div id="shul" class="mf-share-users"></div>' +
			'<div class="mf-muted">' + (acc.length ? '勾选已有账户, 或在下面新建一个; 都不勾选 = 服务端账户库里的任何账户都能登录。' : '服务端还没有任何账户, 请在下面创建一个。') + '</div></div>' +
			'<div class="mf-fld" id="shnw"><label>新建账户(电脑访问时输入的用户名和密码)</label>' +
			'<input class="mf-in" id="shnu" autocomplete="off" spellcheck="false" maxlength="32" placeholder="用户名(字母、数字、_ . -)">' +
			'<input class="mf-in" id="shnp" type="password" autocomplete="new-password" maxlength="128" placeholder="密码" style="margin-top:6px">' +
			'<input class="mf-in" id="shnq" type="password" autocomplete="new-password" maxlength="128" placeholder="再输一次密码" style="margin-top:6px">' +
			'<div class="mf-muted" id="shnh">不填 = 不新建。不勾「允许匿名访问」时, 电脑必须输入正确的用户名和密码才能打开这个共享。</div></div>' +
			'<div class="mf-muted" id="she" role="alert" style="min-height:18px;color:var(--danger)"></div>');
		var inp = body.querySelector('#shn'), cm = body.querySelector('#shc'), ro = body.querySelector('#shr'), gs = body.querySelector('#shg'), ul = body.querySelector('#shul'), er = body.querySelector('#she'), uw = body.querySelector('#shuw'), nw = body.querySelector('#shnw'), nu = body.querySelector('#shnu'), np = body.querySelector('#shnp'), nq = body.querySelector('#shnq'), nh = body.querySelector('#shnh');
		inp.value = o.name || ''; cm.value = o.comment || ''; ro.checked = !!o.ro; gs.checked = !!o.guest;
		var have = {}; (o.users || []).forEach(function(u) { have[u] = 1; });
		var all = acc.slice(); (o.users || []).forEach(function(u) { if (all.indexOf(u) < 0) all.push(u); });
		all.forEach(function(u) {
			var l = document.createElement('label'); l.className = 'mf-chkl';
			var c = document.createElement('input'); c.type = 'checkbox'; c.value = u; c.checked = !!have[u];
			var s = document.createElement('span'); s.textContent = u;
			l.appendChild(c); l.appendChild(s); ul.appendChild(l);
		});
		if (!all.length) uw.hidden = false;
		function sync() {
			ul.style.opacity = gs.checked ? .45 : 1; nw.style.opacity = gs.checked ? .45 : 1;
			nu.disabled = np.disabled = nq.disabled = gs.checked;
		}
		gs.onchange = sync; sync();
		nu.oninput = function() {
			var v = nu.value.trim();
			nh.textContent = (v && acc.indexOf(v) >= 0) ? '账户“' + v + '”已经存在: 填写密码会重置它的密码(用这个账户的其它共享也会受影响); 不想改密码就在上面勾选它、这里留空。' : '不填 = 不新建。不勾「允许匿名访问」时, 电脑必须输入正确的用户名和密码才能打开这个共享。';
		};
		return dlg({
			title: o.title, body: body, wide: false, btns: [{ t: '取消', v: null }, { t: o.ok || '保存', v: 'ok', primary: 1 }], selectAll: false,
			onbtn: function(v, box, close) {
				if (v !== 'ok') return true;
				var n = inp.value.trim();
				if (!n) { er.textContent = '请填写共享名称'; return false; }
				if (/[\/\\:*?"<>|,=]/.test(n) || /[\x00-\x1f]/.test(n)) { er.textContent = '共享名称不能包含 / \\ : * ? " < > | , = 等字符'; return false; }
				var users = []; ul.querySelectorAll('input:checked').forEach(function(c) { users.push(c.value); });
				var nuv = '', npv = '';
				if (!gs.checked) {
					nuv = nu.value.trim(); npv = np.value;
					if (nuv || npv || nq.value) {
						if (!nuv) { er.textContent = '请填写新账户的用户名'; return false; }
						if (!/^[A-Za-z0-9_.\-]{1,32}$/.test(nuv)) { er.textContent = '用户名只能含字母、数字、下划线、点和短横线, 最长 32 个字符'; return false; }
						if (!npv) { er.textContent = '请填写密码'; return false; }
						if (npv.length > 128 || /[\x00-\x1f]/.test(npv)) { er.textContent = '密码不能含控制字符, 最长 128 个字符'; return false; }
						if (npv !== nq.value) { er.textContent = '两次输入的密码不一致'; return false; }
					} else if (!users.length && !acc.length) {
						er.textContent = '还没有任何账户, 电脑将无法登录。请创建一个账户, 或勾选「允许匿名访问」'; return false;
					}
				}
				close({ name: n, ro: ro.checked, guest: gs.checked, users: gs.checked ? [] : users, comment: cm.value.trim(), newuser: nuv, newpass: npv });
				return false;
			}
		});
	}
	// 选一个要共享的文件夹(只在授权的本地存储里挑)
	function pickFolder(startRoot, startPath) {
		var roots = S.roots.filter(function(r) { return r.kind !== 'remote' && r.state !== 'error'; });
		var cur = { root: startRoot || (roots[0] && roots[0].id), path: startPath || '' };
		var body = div('mf-pick', '');
		var done;
		function draw() {
			var h = '<div class="mf-fld"><label for="pkr">存储位置</label><select class="mf-in" id="pkr">' + roots.map(function(r) { return '<option value="' + esc(r.id) + '"' + (r.id === cur.root ? ' selected' : '') + '>' + esc(r.label) + (r.internal ? '(内部存储)' : '') + '</option>'; }).join('') + '</select></div>' +
				'<div class="mf-pick-p"><button type="button" class="mf-btn" data-r="up" ' + (cur.path ? '' : 'disabled') + ' aria-label="上一级">' + ic('sortup') + '</button><span class="mf-ell">/' + esc(cur.path) + '</span></div><div class="mf-pick-l" id="pkl"><div class="mf-muted" style="padding:8px">读取中…</div></div>';
			body.innerHTML = h;
			body.querySelector('#pkr').onchange = function() { cur.root = this.value; cur.path = ''; draw(); };
			body.querySelector('[data-r=up]').onclick = function() { cur.path = parentOf(cur.path); draw(); };
			api('list', { root: cur.root, path: cur.path, type: 'dir', limit: 500 }).then(function(r) {
				var l = body.querySelector('#pkl'); if (!l) return;
				if (!r.ok) { l.innerHTML = '<div class="mf-note bad" style="margin:8px">' + ic('warn') + '<span>' + esc(r.msg || '无法读取') + '</span></div>'; return; }
				var ds = (r.items || []).filter(function(e) { return e.d; });
				l.innerHTML = ds.length ? '' : '<div class="mf-muted" style="padding:8px">这里没有子文件夹</div>';
				ds.forEach(function(e) {
					var b = document.createElement('button'); b.type = 'button'; b.className = 'mf-pick-i';
					b.innerHTML = ic('folder', 't-dir') + '<span class="mf-ell"></span>'; b.querySelector('span.mf-ell').textContent = e.n;
					b.onclick = function() { cur.path = joinp(cur.path, e.n); draw(); };
					l.appendChild(b);
				});
			});
		}
		draw();
		return dlg({ title: '选择要共享的文件夹', body: body, btns: [{ t: '取消', v: null }, { t: '选择当前文件夹', v: 'ok', primary: 1 }], selectAll: false,
			onbtn: function(v, box, close) { if (v !== 'ok') return true; close({ root: cur.root, path: cur.path }); return false; } });
	}
	function aboutDialog() {
		var body = div('mf-ab',
			'<div class="mf-ab-t">Mwrt 文件管理</div>' +
			'<p class="mf-muted">路由器自带的网页文件管理器, 无需安装任何客户端。</p>' +
			'<h4>主要功能</h4><ul>' +
			'<li>浏览、上传、下载、复制、移动、重命名、删除, 以及图片/视频/音频/文本的在线预览</li>' +
			'<li>识别 USB、SD 卡、NVMe 等外接存储, 并支持格式化</li>' +
			'<li>SMB 网络共享: 把本机文件夹共享给电脑, 或挂载别的电脑/NAS 的共享文件夹</li></ul>' +
			'<h4>版权声明</h4>' +
			'<p>© 2026 Mwrt 保留所有权利。本文件管理器(包括界面设计、文案、图标、程序代码及后台接口)受著作权法及相关法律保护。</p>' +
			'<div class="mf-ab-warn"><b>使用须知</b>' +
			'未经书面许可, 禁止对本软件进行反编译、反汇编、逆向工程, 或以其它方式提取其源代码、算法与接口协议; ' +
			'禁止复制、修改、转载、分发、出售本软件的全部或任何部分, 也不得将其用于其它产品或服务。违反者将依法追究责任。</div>' +
			'<p class="mf-muted" style="margin-top:10px">固件中所包含的第三方开源组件, 仍分别遵循其各自的开源许可证。</p>');
		return dlg({ title: '版权 · 关于', body: body, btns: [{ t: '我知道了', v: 1, primary: 1 }] });
	}
	function shareManager(autoNew) {
		var body = div('', '<div class="mf-muted">读取共享状态…</div>');
		var closer;
		dlg({ title: '共享管理(SMB)', body: body, wide: true, btns: [{ t: '关闭', v: 1, primary: 1 }], init: function(box, close) { closer = close; } });
		function load() {
			return api('sharelist').then(function(sl) {
				if (!sl.ok) { body.innerHTML = '<div class="mf-note bad">' + ic('warn') + '<span>' + esc(sl.msg || '无法读取共享') + '</span></div>'; return; }
				var st = shareState(sl);
				var h = st.html;
				if (!st.bad) {
					h += '<div class="mf-g-sec" style="display:flex;align-items:center"><span>共享的文件夹</span><span class="mf-grow"></span><button type="button" class="mf-pill primary" data-r="new">新建共享</button></div>';
					var items = sl.items || [];
					h += items.length ? items.map(shareRowHtml).join('') : '<div class="mf-empty-s">还没有共享的文件夹</div>';
					h += '<div class="mf-note">' + ic('info') + '<span>共享通过设备上的 ' + (sl.caps.share_server === 'ksmbd' ? 'ksmbd' : 'Samba') + ' 服务提供, 在电脑上用 <code>\\\\路由器地址\\共享名</code> 访问。文件管理只创建和修改自己建的共享, 账户与密码请在「SMB 服务」页面管理。</span></div>';
				}
				body.innerHTML = h;
				body.__sl = sl;
			});
		}
		load().then(function() { if (autoNew) { var nb = body.querySelector('[data-r="new"]'); if (nb) nb.click(); } });
		body.onclick = function(ev) {
			var b = ev.target.closest('[data-r]'); if (!b || !body.__sl) return;
			var sl = body.__sl, act = b.getAttribute('data-r'), id = b.getAttribute('data-id');
			if (act === 'svc') {
				b.disabled = true;
				api('sharesvc', { action: b.getAttribute('data-v') }, true).then(function(r) { if (!r.ok) fail(r, '操作失败'); else toast('已完成'); load(); });
			} else if (act === 'new') {
				pickFolder().then(function(pf) {
					if (!pf) return;
					var nm = pf.path ? baseOf(pf.path) : ((rootObj(pf.root) || {}).label || 'share');
					shareForm({ title: '新建共享', name: nm.replace(/[\/\\:*?"<>|,=]/g, '_'), ok: '创建共享' }, sl).then(function(f) {
						if (!f) return;
						var verb = '共享', ctx = { verb: verb, what: '“' + nm + '”' };
						gateInternal('share', { root: pf.root, paths: [pf.path] }, { verb: verb, what: '“' + nm + '”', ok: '确认并共享' }).then(function(g) {
							if (!g) return;
							api('shareadd', { root: pf.root, path: pf.path, name: f.name, ro: f.ro, guest: f.guest, users: JSON.stringify(f.users), newuser: f.newuser, newpass: f.newpass, comment: f.comment }, true, g.ctx).then(function(r) {
								if (!r.ok) { fail(r, '创建失败'); return; }
								toast('已共享为“' + r.name + '”'); load();
							});
						});
					});
				});
			} else if (act === 'edit') {
				var x = null; (sl.items || []).forEach(function(i) { if (i.id === id) x = i; });
				if (!x) return;
				shareForm({ title: '修改共享“' + x.name + '”', name: x.name, ro: x.ro, guest: x.guest, users: x.users, comment: x.comment }, sl).then(function(f) {
					if (!f) return;
					api('shareedit', { id: id, name: f.name, ro: f.ro, guest: f.guest, users: JSON.stringify(f.users), newuser: f.newuser, newpass: f.newpass, comment: f.comment }, true, { verb: '共享', what: '“' + x.name + '”' }).then(function(r) {
						if (!r.ok) { fail(r, '修改失败'); return; }
						toast('已保存'); load();
					});
				});
			} else if (act === 'del') {
				var y = null; (sl.items || []).forEach(function(i) { if (i.id === id) y = i; });
				if (!y) return;
				askConfirm('取消共享“' + y.name + '”?', '局域网里的设备将无法再通过这个共享名访问; 文件夹里的文件不会被删除。', '取消共享', true).then(function(ok) {
					if (!ok) return;
					api('shareremove', { id: id }, true).then(function(r) { if (!r.ok) fail(r, '操作失败'); else toast('已取消共享'); load(); });
				});
			}
		};
	}

	/* ================= 预览器 ================= */
	var V = null;           // 当前预览: { e, list, i, kind, cleanup, el... }
	var rangeProbe = null;  // null 未测 / true / false
	function probeRange(url) {
		if (rangeProbe !== null) return Promise.resolve(rangeProbe);
		var ac = window.AbortController ? new AbortController() : null;
		return fetch(url, { credentials: 'same-origin', headers: { Range: 'bytes=0-0' }, signal: ac ? ac.signal : undefined }).then(function(r) {
			rangeProbe = r.status === 206;
			if (ac) ac.abort();
			return rangeProbe;
		}, function() { return null; });
	}
	function vbtn(a, icon, label, extra) {
		return '<button type="button" class="mf-btn" data-v="' + a + '" title="' + esc(label) + '" aria-label="' + esc(label) + '"' + (extra || '') + '>' + ic(icon) + '</button>';
	}
	function openViewer(e) {
		var kind = viewKind(e);
		// 只有图片/音视频才在同类文件之间翻页; 文本、PDF 等一次只看一个
		var paged = kind === 'image' || kind === 'video' || kind === 'audio';
		var list = paged ? S.items.filter(function(x) { return !x.d && viewKind(x) === kind; }) : [e];
		var i = list.indexOf(e);
		if (i < 0) { list = [e]; i = 0; }
		if (!V) {
			var v = div('mf-viewer'); v.setAttribute('role', 'dialog'); v.setAttribute('aria-modal', 'true'); v.setAttribute('aria-label', '预览'); v.tabIndex = -1;
			v.innerHTML = '<div class="mf-v-bar"></div><div class="mf-v-body"></div>';
			el.win.appendChild(v);
			V = { el: v, bar: v.firstChild, body: v.lastChild, prev: el.root.activeElement };
			v.addEventListener('click', viewerClick);
		}
		V.list = list; V.i = i;
		showViewerItem();
		pushRec(S.root, relOf(e), e.n, false, e.ty);
	}
	function closeViewer() {
		if (!V) return;
		if (V.cleanup) V.cleanup();
		var back = V.prev;
		if (V.el.parentNode) V.el.parentNode.removeChild(V.el);
		V = null;
		if (back && back.isConnected) { try { back.focus(); } catch (e) {} } else el.scroll.focus();
	}
	function viewerClick(ev) {
		var b = ev.target.closest('[data-v]'); if (!b || !V) return;
		var a = b.getAttribute('data-v');
		if (a === 'close') closeViewer();
		else if (a === 'prev') viewerStep(-1);
		else if (a === 'next') viewerStep(1);
		else if (a === 'dl') doDownload([V.e]);
		else if (a === 'tab') window.open(dlUrl(S.root, relOf(V.e), 'view'), '_blank', 'noopener');
		else if (V.act) V.act(a, b);
	}
	function viewerStep(d) {
		if (!V || V.list.length < 2) return;
		V.i = (V.i + d + V.list.length) % V.list.length;
		showViewerItem();
	}
	function showViewerItem() {
		if (V.cleanup) { V.cleanup(); V.cleanup = null; }
		V.act = null; V.keys = null;
		var e = V.e = V.list[V.i], kind = viewKind(e), rel = relOf(e), url = dlUrl(S.root, rel, 'view');
		var n = V.list.length, tools = '';
		V.kind = kind;
		if (kind === 'image') tools = vbtn('zout', 'zoomout', '缩小') + '<span class="mf-v-ix" data-r="zp">100%</span>' + vbtn('zin', 'zoomin', '放大') + vbtn('fit', 'external', '适应窗口 / 实际大小', ' style="display:none"') + vbtn('rccw', 'rotccw', '向左旋转') + vbtn('rcw', 'rotcw', '向右旋转') + '<button type="button" class="mf-btn" data-v="fit">适应</button><span class="mf-sep"></span>';
		else if (kind === 'text') tools = vbtn('wrap', 'wrap', '自动换行') + '<span class="mf-sep"></span>';
		else if (kind === 'md') tools = '<button type="button" class="mf-btn" data-v="src" aria-pressed="false">源码</button><span class="mf-sep"></span>';
		V.bar.innerHTML = '<span class="mf-v-nm mf-ell" title="' + esc(e.n) + '">' + esc(e.n) + '</span>' + (n > 1 ? '<span class="mf-v-ix">' + (V.i + 1) + ' / ' + n + '</span>' : '') + tools +
			vbtn('dl', 'download', '下载') + (kind === 'pdf' || kind === 'image' || kind === 'video' || kind === 'audio' ? vbtn('tab', 'external', '在新标签页打开') : '') + vbtn('close', 'close', '关闭预览(Esc)');
		V.body.innerHTML = '';
		if (n > 1) {
			V.body.insertAdjacentHTML('beforeend', '<button type="button" class="mf-v-nav l" data-v="prev" aria-label="上一个(←)">' + ic('back') + '</button><button type="button" class="mf-v-nav r" data-v="next" aria-label="下一个(→)">' + ic('forward') + '</button>');
		}
		var host = div('mf-v-stage'); host.style.touchAction = kind === 'image' ? 'none' : 'auto';
		V.body.insertBefore(host, V.body.firstChild);
		if (kind === 'image') vImage(host, e, url);
		else if (kind === 'video' || kind === 'audio') vMedia(host, e, url, kind);
		else if (kind === 'text') vText(host, e, false);
		else if (kind === 'md') vText(host, e, true);
		else if (kind === 'pdf') vFallback(host, e, { ic: 'doc', t: 'PDF 文档', p: '为了安全和轻量, 文件管理不内置 PDF 阅读器。可以在新标签页用浏览器自带的阅读器打开, 或直接下载。', tab: true });
		else vFallback(host, e, { ic: iconFor(e), t: e.b ? '快捷方式已损坏' : (e.o ? '特殊文件' : '此类型暂不支持预览'), p: e.b ? '它指向的位置不存在, 或超出了授权范围。' : (e.o ? '设备文件、管道等特殊文件不能预览或下载。' : typeLabel(e) + ' · ' + fmtSize(e.s) + '。可以下载到本机后用对应软件打开。'), text: !e.b && !e.o });
		V.el.focus();
	}
	function vFallback(host, e, o) {
		if (!V || V.e !== e) return;
		host.style.display = 'flex'; host.style.alignItems = 'center'; host.style.justifyContent = 'center';
		var f = div('mf-v-fallback', ic(o.ic) + '<b>' + esc(o.t) + '</b><div>' + esc(o.p || '') + '</div><div style="margin-top:10px"></div>');
		var bt = f.lastChild;
		if (!e.b && !e.o) bt.insertAdjacentHTML('beforeend', '<button type="button" class="mf-pill primary" data-v="dl">下载</button>');
		if (o.tab) bt.insertAdjacentHTML('beforeend', '<button type="button" class="mf-pill" data-v="tab">在新标签页打开</button>');
		if (o.text) bt.insertAdjacentHTML('beforeend', '<button type="button" class="mf-pill" data-v="astext">尝试以文本打开</button>');
		if (isArchive(e) && canWrite()) bt.insertAdjacentHTML('beforeend', '<button type="button" class="mf-pill" data-v="extract">解压到当前文件夹</button>');
		host.appendChild(f);
		V.act = function(a) {
			if (a === 'astext') { host.innerHTML = ''; host.style.display = ''; V.kind = 'text'; vText(host, e, false); }
			else if (a === 'extract') { closeViewer(); doExtract(e); }
		};
	}
	function vNote(msg) {
		var n = div('mf-v-note', esc(msg)); V.body.appendChild(n); return n;
	}

	/* --- 图片: 缩放 / 平移 / 旋转 / 捏合 --- */
	function vImage(host, e, url) {
		var img = document.createElement('img');
		img.className = 'mf-v-img'; img.alt = e.n; img.draggable = false;
		var st = { s: 1, r: 0, cx: 0, cy: 0, w: 0, h: 0, fit: true };
		var zp = V.bar.querySelector('[data-r=zp]');
		function box() { var sw = host.clientWidth, sh = host.clientHeight, q = Math.abs(st.r) % 180 === 90; return { sw: sw, sh: sh, w: q ? st.h : st.w, h: q ? st.w : st.h }; }
		function apply() {
			img.style.transform = 'translate(' + st.cx + 'px,' + st.cy + 'px) rotate(' + st.r + 'deg) scale(' + st.s + ') translate(' + (-st.w / 2) + 'px,' + (-st.h / 2) + 'px)';
			if (zp) zp.textContent = Math.round(st.s * 100) + '%';
		}
		function fit() { var b = box(); st.s = Math.min(1, (b.sw - 24) / b.w, (b.sh - 24) / b.h); st.cx = b.sw / 2; st.cy = b.sh / 2; st.fit = true; apply(); }
		function actual() { var b = box(); st.s = 1; st.cx = b.sw / 2; st.cy = b.sh / 2; st.fit = false; apply(); }
		function zoom(k, px, py) {
			var b = box(); if (px == null) { px = b.sw / 2; py = b.sh / 2; }
			var ns = Math.max(0.05, Math.min(16, st.s * k)); k = ns / st.s;
			st.cx = px - (px - st.cx) * k; st.cy = py - (py - st.cy) * k; st.s = ns; st.fit = false; apply();
		}
		function rot(d) { st.r = (st.r + d) % 360; fit(); }
		img.onload = function() {
			st.w = img.naturalWidth || 1; st.h = img.naturalHeight || 1;
			img.style.width = st.w + 'px'; img.style.height = st.h + 'px'; fit();
			if (/\.svg$/i.test(e.n)) vNote('SVG 以图片方式显示, 不会执行其中的脚本');
		};
		img.onerror = function() { host.innerHTML = ''; host.style.display = 'flex'; host.style.alignItems = 'center'; host.style.justifyContent = 'center'; vFallback(host, e, { ic: 'image', t: '无法显示这张图片', p: '文件可能已损坏, 或浏览器不支持这种图片格式。' }); };
		img.src = url;
		host.appendChild(img);
		var ptrs = {}, last = null, pinch = 0, moved = false;
		function onwheel(ev) { ev.preventDefault(); var r = host.getBoundingClientRect(); zoom(ev.deltaY < 0 ? 1.15 : 1 / 1.15, ev.clientX - r.left, ev.clientY - r.top); }
		function pd(ev) {
			host.setPointerCapture && host.setPointerCapture(ev.pointerId);
			ptrs[ev.pointerId] = { x: ev.clientX, y: ev.clientY }; moved = false;
			var ids = Object.keys(ptrs);
			if (ids.length === 2) { var a = ptrs[ids[0]], b = ptrs[ids[1]]; pinch = Math.hypot(a.x - b.x, a.y - b.y); }
			last = { x: ev.clientX, y: ev.clientY }; img.classList.add('drag');
		}
		function pm(ev) {
			if (!ptrs[ev.pointerId]) return;
			ptrs[ev.pointerId] = { x: ev.clientX, y: ev.clientY };
			var ids = Object.keys(ptrs);
			if (ids.length >= 2) {
				var a = ptrs[ids[0]], b = ptrs[ids[1]], d = Math.hypot(a.x - b.x, a.y - b.y), r = host.getBoundingClientRect();
				if (pinch) zoom(d / pinch, (a.x + b.x) / 2 - r.left, (a.y + b.y) / 2 - r.top);
				pinch = d; moved = true; return;
			}
			if (last) { var dx = ev.clientX - last.x, dy = ev.clientY - last.y; if (Math.abs(dx) + Math.abs(dy) > 2) moved = true; st.cx += dx; st.cy += dy; st.fit = false; last = { x: ev.clientX, y: ev.clientY }; apply(); }
		}
		function pu(ev) { delete ptrs[ev.pointerId]; pinch = 0; last = Object.keys(ptrs).length ? last : null; img.classList.remove('drag'); }
		function dbl() { if (st.fit && st.s < 1) actual(); else if (st.fit) zoom(2); else fit(); }
		host.addEventListener('wheel', onwheel, { passive: false });
		host.addEventListener('pointerdown', pd); host.addEventListener('pointermove', pm); host.addEventListener('pointerup', pu); host.addEventListener('pointercancel', pu);
		host.addEventListener('dblclick', dbl);
		var ro = window.ResizeObserver ? new ResizeObserver(function() { if (st.w && st.fit) fit(); else if (st.w) { var b = box(); st.cx = Math.min(Math.max(st.cx, 0), b.sw); st.cy = Math.min(Math.max(st.cy, 0), b.sh); apply(); } }) : null;
		if (ro) ro.observe(host);
		V.cleanup = function() { if (ro) ro.disconnect(); img.onerror = null; img.onload = null; img.removeAttribute('src'); };
		V.act = function(a) {
			if (a === 'zin') zoom(1.25); else if (a === 'zout') zoom(1 / 1.25); else if (a === 'fit') { if (st.fit && st.s < 1) actual(); else fit(); }
			else if (a === 'rcw') rot(90); else if (a === 'rccw') rot(-90);
		};
		V.keys = function(ev) {
			if (ev.key === '+' || ev.key === '=') { zoom(1.25); return true; }
			if (ev.key === '-') { zoom(1 / 1.25); return true; }
			if (ev.key === '0') { fit(); return true; }
			if (ev.key === '1') { actual(); return true; }
			if (ev.key === 'r' || ev.key === 'R') { rot(ev.shiftKey ? -90 : 90); return true; }
			return false;
		};
	}

	/* --- 视频 / 音频: 只用浏览器原生播放器 --- */
	function vMedia(host, e, url, kind) {
		host.style.display = 'flex'; host.style.alignItems = 'center'; host.style.justifyContent = 'center'; host.style.flexDirection = 'column'; host.style.gap = '16px';
		var m = document.createElement(kind);
		m.controls = true; m.preload = 'metadata'; m.className = kind === 'audio' ? 'mf-v-audio' : 'mf-v-media'; m.setAttribute('aria-label', e.n);
		if (kind === 'video') m.playsInline = true;
		if (kind === 'audio') host.insertAdjacentHTML('beforeend', '<div class="mf-v-fallback" style="padding:0">' + ic('audio') + '<b>' + esc(e.n) + '</b></div>');
		m.src = url; host.appendChild(m);
		var note = null;
		m.onerror = function() {
			if (!V || V.e !== e) return;
			if (note && note.parentNode) note.parentNode.removeChild(note);
			note = vNote('浏览器无法播放这个文件(编码或封装格式不受支持)。可以下载后用本地播放器打开。');
		};
		probeRange(url).then(function(ok) {
			if (ok === false && V && V.e === e && !note) note = vNote('提示: 此设备的 Web 服务不支持分段读取, 播放前需要缓冲, 拖动进度条可能不生效, 大文件请优先下载。');
		});
		V.cleanup = function() { try { m.pause(); m.removeAttribute('src'); m.load(); } catch (x) {} };
		V.keys = function(ev) {
			if (ev.key === ' ') { if (m.paused) m.play(); else m.pause(); return true; }
			return false;
		};
		setTimeout(function() { try { m.focus(); } catch (x) {} }, 0);
	}

	/* --- 文本: 只读, 分段加载 --- */
	function vText(host, e, md) {
		host.style.overflow = 'hidden';
		var wrap = div('mf-v-text'); host.appendChild(wrap);
		var pre = document.createElement('pre'); pre.setAttribute('tabindex', '0');
		var info = div('mf-more-t', '读取中…');
		var st = { off: 0, eof: false, busy: false, size: e.s, bad: false, raw: '', showSrc: !md };
		var mdBox = md ? div('mf-v-md') : null;
		if (md) wrap.appendChild(mdBox);
		wrap.appendChild(pre); wrap.appendChild(info);
		pre.style.display = md ? 'none' : '';
		var cancelled = false;
		function status() {
			info.innerHTML = '';
			if (md && st.eof) { info.style.display = 'none'; return; }
			var t = document.createElement('span');
			t.textContent = (st.eof ? '已全部加载' : '已加载 ' + fmtSize(st.off) + ' / ' + fmtSize(st.size)) + (st.bad ? ' · 含无法识别的字符, 可能不是 UTF-8 文本' : '') + ' · 只读';
			info.appendChild(t);
			if (!st.eof) { var b = document.createElement('button'); b.type = 'button'; b.className = 'mf-pill'; b.textContent = '加载更多'; b.onclick = more; info.appendChild(b); }
		}
		function more() {
			if (st.busy || st.eof || cancelled) return;
			if (md && st.off >= 1048576) { st.eof = true; status(); return; }
			st.busy = true;
			api('read', { root: S.root, path: relOf(e), offset: st.off, limit: 131072 }).then(function(r) {
				st.busy = false;
				if (cancelled) return;
				if (!r.ok) { info.textContent = r.msg || '读取失败'; return; }
				if (r.binary) { wrap.remove(); host.style.display = 'flex'; host.style.alignItems = 'center'; host.style.justifyContent = 'center'; vFallback(host, e, { ic: 'file', t: '看起来是二进制文件', p: '无法以文本方式显示。可以下载后用合适的软件打开。' }); return; }
				pre.appendChild(document.createTextNode(r.text)); st.raw += r.text;
				st.off = r.next; st.eof = !!r.eof; st.size = r.size; if (r.bad) st.bad = true;
				if (md && (st.eof || st.off >= 1048576)) renderMd();
				status();
				if (!st.eof && wrap.scrollHeight <= wrap.clientHeight + 40) more();
			});
		}
		function renderMd() {
			mdBox.innerHTML = '';
			try { renderMarkdown(mdBox, st.raw); } catch (x) { mdBox.textContent = st.raw; }
			if (!st.eof) mdBox.appendChild(div('mf-note', ic('info') + '<span>文件较大, 仅渲染了前 1 MB。</span>'));
		}
		wrap.addEventListener('scroll', function() { if (!st.eof && wrap.scrollTop + wrap.clientHeight > wrap.scrollHeight - 300 && (!md)) more(); });
		V.cleanup = function() { cancelled = true; };
		V.act = function(a, b) {
			if (a === 'wrap') { wrap.classList.toggle('wrap'); b.setAttribute('aria-pressed', wrap.classList.contains('wrap')); }
			else if (a === 'src') { st.showSrc = !st.showSrc; b.setAttribute('aria-pressed', st.showSrc); pre.style.display = st.showSrc ? '' : 'none'; mdBox.style.display = st.showSrc ? 'none' : ''; }
		};
		if (md) { mdBox.style.display = ''; }
		more();
		if (!md) pre.focus();
		if (md) pre.style.display = 'none';
	}

	/* --- Markdown: 自带的安全渲染器(只用 DOM API 和 textContent, 不产生任何 HTML 字符串) --- */
	function mdInline(parent, s) {
		var re = /(`+)([\s\S]*?[^`])\1(?!`)|!\[([^\]]*)\]\(((?:[^()\s]|\([^()\s]*\))*)[^)]*\)|\[([^\]]+)\]\(((?:[^()\s]|\([^()\s]*\))+)[^)]*\)|\*\*([^*]+)\*\*|__([^_]+)__|\*([^*\s][^*]*)\*|(?:^|\b)_([^_\s][^_]*)_(?=\b|$)|~~([^~]+)~~/g;
		var pos = 0, m;
		function text(t) { if (t) parent.appendChild(document.createTextNode(t)); }
		while ((m = re.exec(s))) {
			text(s.slice(pos, m.index)); pos = m.index + m[0].length;
			var n;
			if (m[2] != null) { n = document.createElement('code'); n.textContent = m[2]; parent.appendChild(n); }
			else if (m[4] != null || m[3] != null && m[0].charAt(0) === '!') { n = document.createElement('span'); n.className = 'mf-muted'; n.textContent = '[图片' + (m[3] ? ': ' + m[3] : '') + '(为安全不加载外部图片)]'; parent.appendChild(n); }
			else if (m[6] != null) {
				var u = m[6];
				if (/^(https?:\/\/|mailto:)/i.test(u)) { n = document.createElement('a'); n.href = u; n.target = '_blank'; n.rel = 'noopener noreferrer nofollow'; n.textContent = m[5]; parent.appendChild(n); }
				else text(m[5]);
			}
			else if (m[7] != null || m[8] != null) { n = document.createElement('strong'); n.textContent = m[7] != null ? m[7] : m[8]; parent.appendChild(n); }
			else if (m[9] != null || m[10] != null) { n = document.createElement('em'); n.textContent = m[9] != null ? m[9] : m[10]; parent.appendChild(n); }
			else if (m[11] != null) { n = document.createElement('del'); n.textContent = m[11]; parent.appendChild(n); }
			else text(m[0]);
		}
		text(s.slice(pos));
	}
	function renderMarkdown(box, src) {
		var lines = src.replace(/\r\n?/g, '\n').split('\n'), i = 0;
		function el_(tag, p) { var n = document.createElement(tag); (p || box).appendChild(n); return n; }
		while (i < lines.length) {
			var ln = lines[i], m;
			if (/^\s*$/.test(ln)) { i++; continue; }
			if ((m = /^(```|~~~)\s*(\S*)/.exec(ln))) {
				var fence = m[1], buf = []; i++;
				while (i < lines.length && lines[i].indexOf(fence) !== 0) buf.push(lines[i++]);
				i++; var pre = el_('pre'), code = el_('code', pre); code.textContent = buf.join('\n'); continue;
			}
			if ((m = /^(#{1,6})\s+(.*?)\s*#*\s*$/.exec(ln))) { mdInline(el_('h' + m[1].length), m[2]); i++; continue; }
			if (/^\s*([-*_])(\s*\1){2,}\s*$/.test(ln)) { el_('hr'); i++; continue; }
			if (/^\s*>/.test(ln)) {
				var q = []; while (i < lines.length && /^\s*>/.test(lines[i])) q.push(lines[i++].replace(/^\s*> ?/, ''));
				renderMarkdown(el_('blockquote'), q.join('\n')); continue;
			}
			if (/^\s*([-*+]|\d+[.)])\s+/.test(ln)) {
				var ord = /^\s*\d+[.)]/.test(ln), list = el_(ord ? 'ol' : 'ul');
				while (i < lines.length && /^\s*([-*+]|\d+[.)])\s+/.test(lines[i])) {
					var li = el_('li', list), t = lines[i].replace(/^\s*([-*+]|\d+[.)])\s+/, '');
					var tk = /^\[( |x|X)\]\s+(.*)$/.exec(t);
					if (tk) { var cb = el_('input', li); cb.type = 'checkbox'; cb.disabled = true; cb.checked = tk[1] !== ' '; cb.style.marginRight = '6px'; t = tk[2]; }
					mdInline(li, t); i++;
				}
				continue;
			}
			if (/\|/.test(ln) && i + 1 < lines.length && /^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$/.test(lines[i + 1])) {
				var cells = function(s) { return s.replace(/^\s*\|/, '').replace(/\|\s*$/, '').split('|').map(function(x) { return x.trim(); }); };
				var tb = el_('table'), hd = el_('tr', el_('thead', tb));
				cells(ln).forEach(function(c) { mdInline(el_('th', hd), c); });
				i += 2; var bd = el_('tbody', tb);
				while (i < lines.length && /\|/.test(lines[i]) && !/^\s*$/.test(lines[i])) { var tr = el_('tr', bd); cells(lines[i]).forEach(function(c) { mdInline(el_('td', tr), c); }); i++; }
				continue;
			}
			var para = [ln]; i++;
			while (i < lines.length && !/^\s*$/.test(lines[i]) && !/^(#{1,6}\s|```|~~~|\s*>|\s*([-*+]|\d+[.)])\s)/.test(lines[i])) para.push(lines[i++]);
			mdInline(el_('p'), para.join(' '));
		}
	}
	function viewerKey(ev) {
		if (ev.key === 'Escape') { closeViewer(); return true; }
		if (ev.key === 'ArrowLeft') { viewerStep(-1); return true; }
		if (ev.key === 'ArrowRight') { viewerStep(1); return true; }
		if (ev.key === 'Tab') { return false; }
		if (V.keys && V.keys(ev)) return true;
		return false;
	}

	/* ================= 打开项目 ================= */
	function openItem(e) {
		if (!e) return;
		if (e.d) { go(S.root, relOf(e)); return; }
		if (e.o) return toast('特殊文件不能打开', 'warn');
		if (e.b) return toast('这个快捷方式已损坏, 或指向授权范围之外', 'err');
		openViewer(e);
	}

	/* ================= 网络位置对话框 ================= */
	function noDepNote(miss) {
		return '<div class="mf-note warn">' + ic('warn') + '<span>固件缺少 SMB 挂载组件: <b>' + esc(miss.join('、')) + '</b>。请在「软件包」里安装后再试(安装 kmod-fs-cifs 后需要重启一次才能加载内核模块)。</span></div>';
	}
	function smbDialog(pre) {
		pre = pre || {};
		var caps = S.net || {}, miss = caps.mount_missing || [];
		var body = div('', (caps.mount ? '' : noDepNote(miss)) +
			'<div class="mf-row2"><div class="mf-fld"><label for="sh">服务器地址(主机名或 IP)</label><input class="mf-in" id="sh" autocomplete="off" spellcheck="false" placeholder="例如 192.168.1.10"></div>' +
			'<div class="mf-fld"><label for="ss">共享名(对方共享出来的名字)</label><input class="mf-in" id="ss" autocomplete="off" spellcheck="false" placeholder="例如 D 或 share"></div></div>' +
			'<div class="mf-muted" style="margin:-2px 0 8px">共享名不是文件夹名, 也不是电脑名。Windows: 在资源管理器地址栏输入 \\\\对方地址 回车, 里面列出的文件夹名就是共享名; 或右键共享文件夹 → 属性 → 共享, 看「网络路径」的最后一段。NAS/手机: 到它的 SMB 或共享设置里查看。</div>' +
			'<label class="mf-chkl"><input type="checkbox" id="sg"><span>匿名访问(不需要用户名和密码)</span></label>' +
			'<div class="mf-row2" id="scr"><div class="mf-fld"><label for="su">用户名</label><input class="mf-in" id="su" autocomplete="off" spellcheck="false" autocapitalize="off"></div>' +
			'<div class="mf-fld"><label for="sp">密码</label><input class="mf-in" id="sp" type="password" autocomplete="new-password" spellcheck="false"></div></div>' +
			'<div class="mf-row2"><div class="mf-fld"><label for="sn">显示名称(可选)</label><input class="mf-in" id="sn" autocomplete="off"></div>' +
			'<div class="mf-fld"><label for="sv">SMB 协议版本</label><select class="mf-in" id="sv"><option value="">自动</option><option value="3.1.1">3.1.1</option><option value="3.0">3.0</option><option value="2.1">2.1</option><option value="2.0">2.0</option><option value="1.0">1.0(不安全, 仅旧设备)</option></select></div></div>' +
			'<label class="mf-chkl" id="slr"><input type="checkbox" id="sr"><span>只读挂载</span></label>' +
			'<label class="mf-chkl" id="slm"><input type="checkbox" id="sm"><span>记住凭据并在开机时自动连接<small>凭据保存在路由器 /etc/mwfiles/ 下的受保护文件里(仅 root 可读), 不会显示或写入日志。不勾选则只在本次连接时使用, 用完即删。</small></span></label>' +
			'<div class="mf-muted" id="se" role="alert" style="color:var(--danger);min-height:18px"></div>');
		var g = function(id) { return body.querySelector('#' + id); };
		var guest = g('sg'), cr = g('scr');
		guest.onchange = function() { cr.style.opacity = guest.checked ? .4 : 1; g('su').disabled = g('sp').disabled = guest.checked; g('slm').style.display = guest.checked ? 'none' : ''; };
		if (pre.host) g('sh').value = pre.host; if (pre.share) g('ss').value = pre.share; if (pre.name) g('sn').value = pre.name; if (pre.vers) g('sv').value = pre.vers;
		var diagBtn = { t: '诊断', v: 'diag' };
		return dlg({ title: pre.title || '连接网络共享(SMB/CIFS)', body: body, wide: true, selectAll: false, btns: [{ t: '取消', v: null }, diagBtn, { t: '连接', v: 'go', primary: 1 }],
			onbtn: function(v, box, close, btn) {
				var er = g('se');
				if (v === 'diag') { var h = g('sh').value.trim(); closeAndDiag(h); return false; }
				if (v !== 'go') return true;
				if (!caps.mount) { er.textContent = '缺少 SMB 挂载组件, 无法连接'; return false; }
				var host = g('sh').value.trim(), share = g('ss').value.trim();
				if (!host || !share) { er.textContent = '请填写服务器地址和共享名(共享名的查法见上方说明)'; return false; }
				if (!guest.checked && !g('su').value.trim()) { er.textContent = '请填写用户名, 或勾选「匿名访问」'; return false; }
				var data = { host: host, share: share, name: g('sn').value.trim(), vers: g('sv').value, ro: g('sr').checked, guest: guest.checked, remember: g('sm').checked, auto: g('sm').checked, user: guest.checked ? null : g('su').value, pass: guest.checked ? null : g('sp').value, domain: null };
				var pw = g('sp'); er.textContent = ''; btn.disabled = true; btn.textContent = '连接中…';
				api('smbmount', data, true).then(function(r) {
					data.pass = null; data.user = null; pw.value = '';
					btn.disabled = false; btn.textContent = '连接';
					if (!r.ok) { er.textContent = r.msg || '连接失败'; return; }
					close(r);
				});
				return false;
			} }).then(function(r) {
			if (r && r.ok) { toast('已连接 ' + (r.label || '网络共享')); loadRoots().then(function() { var rt = null; S.roots.forEach(function(x) { if (x.id === r.root || x.path === r.root) rt = x; }); if (rt) go(rt.id, ''); }); }
			else if (r && r.diag) netDiag(r.diag);
			return r;
		});
		function closeAndDiag(host) {
			var top = dlgs[dlgs.length - 1]; if (top) top.close({ diag: host });
		}
	}
	function netDiag(host) {
		var body = div('', '<div class="mf-muted">诊断中, 可能需要几秒…</div>');
		dlg({ title: '网络共享诊断', body: body, btns: [{ t: '关闭', v: 1, primary: 1 }] });
		api('netdiag', { host: host || null }).then(function(r) {
			if (!r.ok) { body.innerHTML = '<div class="mf-note bad">' + ic('warn') + '<span>' + esc(r.msg || '诊断失败') + '</span></div>'; return; }
			var h = '<ul class="mf-diag">';
			(r.items || []).forEach(function(i) { h += '<li class="' + (i.ok ? 'ok' : 'no') + '">' + ic(i.ok ? 'okc' : 'badc') + '<span><b style="font-weight:600">' + esc(i.t) + '</b> — ' + (i.ok ? '正常' : '异常') + (i.h ? '<small>' + esc(i.h) + '</small>' : '') + '</span></li>'; });
			(r.mounts || []).forEach(function(m) { h += '<li class="' + (m.ok ? 'ok' : 'no') + '">' + ic(m.ok ? 'okc' : 'badc') + '<span><b style="font-weight:600">已挂载共享「' + esc(m.name) + '」</b> — ' + (m.ok ? '可访问' : '无响应') + (m.ok ? '' : '<small>共享可能已断线, 可在侧栏右键选择「重新连接」或强制卸载</small>') + '</span></li>'; });
			if (!host) h += '<li class="ok">' + ic('info') + '<span class="mf-muted">要检查某台服务器, 请在「连接网络共享」里填写地址后点「诊断」。</span></li>';
			body.innerHTML = h + '</ul>';
		});
	}
	function netConnectSaved(sid) {
		var s = null; (S.netl || []).forEach(function(x) { if (x.id === sid) s = x; });
		if (!s) return;
		if (!S.net.mount) return dlg({ title: '无法连接', body: noDepNote(S.net.mount_missing || []), btns: [{ t: '好', v: 1, primary: 1 }] });
		toast('正在连接 ' + s.name + ' …');
		api('smbremount', { id: sid }, true).then(function(r) {
			if (r.ok) { loadRoots().then(function() { var m = null; (S.netl || []).forEach(function(x) { if (x.id === sid) m = x; }); if (m && m.root) go(m.root, ''); }); return; }
			if (r.err === 'no_cred') { smbDialog({ host: s.host, share: s.share, name: s.name, vers: s.vers, title: '重新连接「' + s.name + '」' }); return; }
			fail(r, '连接失败');
		});
	}
	function netItemMenu(btn, sid, x, y) {
		var s = null; (S.netl || []).forEach(function(v) { if (v.id === sid) s = v; });
		if (!s) return;
		var items = [];
		if (s.mounted) { items.push({ t: '断开连接', i: 'plug', a: function() { netUmount(s, false); } }); items.push({ t: '强制断开(共享无响应时)', i: 'warn', a: function() { netUmount(s, true); } }); }
		else items.push({ t: '连接', i: 'plug', a: function() { netConnectSaved(sid); } });
		items.push({ t: '诊断', i: 'info', a: function() { netDiag(s.host); } });
		items.push({ sep: 1 });
		items.push({ t: '删除此条记录', i: 'trash', danger: true, a: function() {
			askConfirm('删除“' + s.name + '”?', '会断开连接并删除已保存的地址和凭据, 不会删除共享里的任何文件。', '删除', true).then(function(ok) {
				if (!ok) return;
				api('smbforget', { id: sid }, true).then(function(r) { if (!r.ok) return fail(r, '删除失败'); if (S.root && !rootObj(S.root)) {} loadRoots().then(function() { if (!rootObj(S.root)) pickDefault(); }); });
			});
		} });
		menu(x, y, items, btn);
	}
	function netUmount(s, lazy) {
		api('smbumount', { id: s.id, lazy: lazy }, true).then(function(r) {
			if (!r.ok) return fail(r, '断开失败');
			toast('已断开 ' + s.name);
			loadRoots().then(function() { if (!rootObj(S.root)) pickDefault(); });
		});
	}
	// 文件夹右键「共享」: 直接弹出共享参数表单; 已经由文件管理共享过的则转到共享管理
	function shareDialog(e) {
		var rel = e ? relOf(e) : S.path, nm = e ? e.n : (S.path ? baseOf(S.path) : (curRoot() || {}).label || 'share');
		Promise.all([api('stat', { root: S.root, path: rel }), api('sharelist')]).then(function(rs) {
			var st = rs[0], sl = rs[1];
			if (!st.ok) return fail(st, '无法共享');
			if (st.type !== 'dir') return toast('只能共享文件夹', 'warn');
			if (!sl.ok) return fail(sl, '无法读取共享');
			var ss = shareState(sl);
			if (ss.bad) return dlg({ title: '共享此文件夹', body: ss.html, btns: [{ t: '好', v: 1, primary: 1 }] });
			var real = joinp(st.volume_path || '', rel), mine = null;
			(sl.items || []).forEach(function(x) { if (x.path === real) mine = x; });
			if (mine) { toast('此文件夹已共享为“' + mine.name + '”, 可在共享管理里修改'); return shareManager(); }
			if (st.shared) return dlg({ title: '共享此文件夹', body: '<div class="mf-note warn">' + ic('warn') + '<span>此文件夹' + (st.shared.exact ? '' : '(或其上级)') + '已被共享为“' + esc(st.shared.name) + '”(不是由文件管理创建的), 请到「SMB 服务」页面修改。</span></div>', btns: [{ t: '好', v: 1, primary: 1 }] });
			shareForm({ title: '共享文件夹“' + nm + '”', name: nm.replace(/[\/\\:*?"<>|,=]/g, '_'), ok: '创建共享' }, sl).then(function(f) {
				if (!f) return;
				var what = '“' + nm + '”';
				gateInternal('share', { root: S.root, paths: [rel] }, { verb: '共享', what: what, ok: '确认并共享' }).then(function(g) {
					if (!g) return;
					api('shareadd', { root: S.root, path: rel, name: f.name, ro: f.ro, guest: f.guest, users: JSON.stringify(f.users), newuser: f.newuser, newpass: f.newpass, comment: f.comment }, true, g.ctx).then(function(r) {
						if (!r.ok) return fail(r, '创建失败');
						toast('已共享为“' + r.name + '”'); refresh(true);
					});
				});
			});
		});
	}
	function capsDialog() {
		var a = S.arch || {}, n = S.net || {};
		function row(k, v, ok) { return '<tr><td>' + esc(k) + '</td><td>' + (ok === undefined ? '' : ic(ok ? 'okc' : 'badc')) + ' ' + esc(v) + '</td></tr>'; }
		var h = '<table class="mf-cap-t">';
		h += row('授权目录', S.authorized && S.authorized.length ? S.authorized.join('、') : '(仅自动发现的外置存储与网络共享)');
		h += row('压缩 .tar.gz', a.tar && a.gzip ? '可用' : '缺少 tar / gzip', !!(a.tar && a.gzip));
		h += row('压缩 .zip', a.zip ? '可用' : '固件未安装 zip', !!a.zip);
		h += row('解压 zip / tar / gz', [a.unzip ? 'zip' : '', a.tar ? 'tar' : '', a.gzip ? 'gz' : '', a.bzip2 ? 'bz2' : '', a.xz ? 'xz' : ''].filter(Boolean).join(' ') || '缺少相关工具', !!(a.unzip || a.tar || a.gzip));
		h += row('挂载网络共享', n.mount ? '可用(mount.cifs + 内核 CIFS)' : '缺少: ' + ((n.mount_missing || []).join('、') || '—'), !!n.mount);
		h += row('共享本地文件夹', n.share ? '可用(' + (n.share_server || '') + ')' : '未安装 ksmbd-server / samba4-server', !!n.share);
		h += row('上传大小上限', S.maxMb ? S.maxMb + ' MB' : '不限制(以剩余空间为准)');
		h += row('视频拖动进度', rangeProbe === null ? '播放视频时检测' : rangeProbe ? '支持' : '此设备 Web 服务不支持分段读取', rangeProbe === null ? undefined : rangeProbe);
		h += '</table><p class="mf-muted" style="margin-top:10px">收藏夹与最近访问只保存在当前浏览器里。不会自动分区或格式化任何磁盘。</p>';
		var cbh = '<label class="mf-chkl" style="margin-top:8px"><input type="checkbox" id="chh"' + (P.hidden ? ' checked' : '') + '><span>显示隐藏文件(以 . 开头)</span></label><label class="mf-chkl"><input type="checkbox" id="cht"' + (P.thumbs ? ' checked' : '') + '><span>网格视图中显示图片缩略图(仅 1.5 MB 以内)</span></label>';
		var body = div('', h + cbh);
		body.querySelector('#chh').onchange = function(ev) { P.hidden = ev.target.checked; savePrefs(); refresh(false); };
		body.querySelector('#cht').onchange = function(ev) { P.thumbs = ev.target.checked; savePrefs(); S.rng = null; render(true); };
		dlg({ title: '存储与能力', body: body, wide: true, btns: [{ t: '关闭', v: 1, primary: 1 }] });
	}

	/* ================= 菜单 ================= */
	function itemMenuList(items) {
		var one = items.length === 1 ? items[0] : null, w = canWrite(), arch = one && isArchive(one) && w, out = [];
		if (one) out.push({ t: one.d ? '打开' : '预览 / 打开', i: one.d ? 'folder' : 'external', k: 'Enter', a: function() { openItem(one); } });
		if (items.some(function(x) { return !x.d && !x.o; })) out.push({ t: '下载', i: 'download', a: function() { doDownload(items); } });
		if (arch) { var miss = extractMissing(one); out.push({ t: '解压到当前文件夹' + (miss ? '(' + miss.replace(/^固件/, '') + ')' : ''), i: 'archive', dis: !!miss, a: function() { doExtract(one); } }); }
		out.push({ sep: 1 });
		out.push({ t: '复制', i: 'copy', k: 'Ctrl+C', a: function() { setClip('copy', items); } });
		out.push({ t: '剪切', i: 'cut', k: 'Ctrl+X', dis: !w, a: function() { setClip('cut', items); } });
		if (one && one.d) out.push({ t: '粘贴到此文件夹', i: 'paste', dis: !S.clip || !w, a: function() { pasteInto(relOf(one)); } });
		out.push({ sep: 1 });
		if (one) out.push({ t: '重命名', i: 'edit', k: 'F2', dis: !w, a: function() { doRename(one); } });
		out.push({ t: '压缩…', i: 'archive', dis: !w, a: function() { doCompress(items); } });
		if (one && one.d) {
			var fav = isFav(S.root, relOf(one));
			out.push({ t: fav ? '从收藏夹移除' : '添加到收藏夹', i: 'star', a: function() { toggleFav(S.root, relOf(one), one.n); } });
			if (S.net && S.net.share && (curRoot() || {}).kind !== 'remote') out.push({ t: '共享此文件夹…', i: 'share', a: function() { shareDialog(one); } });
		}
		if (one) out.push({ t: '属性', i: 'info', a: function() { showProps(one); } });
		out.push({ sep: 1 });
		out.push({ t: '删除', i: 'trash', k: 'Del', danger: true, dis: !w, a: function() { doDelete(items); } });
		return out;
	}
	function pasteInto(path) {
		var c = S.clip; if (!c) return;
		transfer(c.op === 'cut' ? 'mv' : 'cp', c.root, c.paths, S.root, path, c.n === 1 ? c.first : c.n + ' 项').then(function(r) { if (r && r.ok && c.op === 'cut') { S.clip = null; updateActs(); } });
	}
	function blankMenu(x, y, anchor) {
		var w = canWrite() && S.mode === 'dir';
		menu(x, y, [
			{ t: '新建文件夹', i: 'newfolder', dis: !w, a: function() { doNew(false); } },
			{ t: '新建文本文件', i: 'newfile', dis: !w, a: function() { doNew(true); } },
			{ t: '上传文件…', i: 'upload', dis: !w, a: function() { pickFiles(false); } },
			{ t: '上传文件夹…', i: 'upload', dis: !w, a: function() { pickFiles(true); } },
			{ t: '粘贴', i: 'paste', k: 'Ctrl+V', dis: !w || !S.clip, a: doPaste },
			{ sep: 1 },
			{ t: '全选', k: 'Ctrl+A', a: selectAll },
			{ t: '刷新', i: 'refresh', k: 'F5', a: function() { refresh(false); rootsSoon(); } },
			{ t: '当前文件夹属性', i: 'info', a: function() { showProps(null); } }
		], anchor);
	}
	function sortMenu(btn) {
		var ks = [['name', '名称'], ['type', '类型'], ['size', '大小'], ['time', '修改时间']], items = [{ h: '排序方式' }];
		ks.forEach(function(k) { items.push({ t: k[1], chk: P.sort === k[0], a: function() { setSort(k[0], P.sort === k[0] ? P.dir : (k[0] === 'time' || k[0] === 'size' ? 'desc' : 'asc')); } }); });
		items.push({ sep: 1 }); items.push({ t: '升序', chk: P.dir === 'asc', a: function() { setSort(P.sort, 'asc'); } }); items.push({ t: '降序', chk: P.dir === 'desc', a: function() { setSort(P.sort, 'desc'); } });
		menuAt(btn, items);
	}
	function setSort(k, d) { P.sort = k; P.dir = d; savePrefs(); updateHead(); if (S.mode === 'search') { S.items.sort(sortCmp); renderAll(); } else load(false); }
	function moreMenu(btn) {
		var items = selList(), w = canWrite(), one = items.length === 1 ? items[0] : null;
		var m = [];
		if (one) m.push({ t: '预览 / 打开', i: 'external', a: function() { openItem(one); } });
		if (one && isArchive(one)) { var miss = extractMissing(one); m.push({ t: '解压' + (miss ? '(' + miss.replace(/^固件/, '') + ')' : ''), i: 'archive', dis: !!miss || !w, a: function() { doExtract(one); } }); }
		m.push({ t: '压缩…', i: 'archive', dis: !items.length || !w, a: function() { doCompress(items); } });
		if (one) m.push({ t: '属性', i: 'info', a: function() { showProps(one); } });
		if (one && one.d && S.net && S.net.share && (curRoot() || {}).kind !== 'remote') m.push({ t: '共享此文件夹…', i: 'share', a: function() { shareDialog(one); } });
		if (one && one.d) { var fav = isFav(S.root, relOf(one)); m.push({ t: fav ? '从收藏夹移除' : '添加到收藏夹', i: 'star', a: function() { toggleFav(S.root, relOf(one), one.n); } }); }
		m.push({ sep: 1 });
		m.push({ t: '新建文本文件', i: 'newfile', dis: !w || S.mode !== 'dir', a: function() { doNew(true); } });
		m.push({ t: '全选', k: 'Ctrl+A', a: selectAll });
		m.push({ t: '当前文件夹属性', i: 'info', a: function() { showProps(null); } });
		m.push({ sep: 1 });
		m.push({ t: '显示隐藏文件', chk: !!P.hidden, a: function() { P.hidden = !P.hidden; savePrefs(); refresh(false); } });
		m.push({ t: '图片缩略图', chk: !!P.thumbs, a: function() { P.thumbs = !P.thumbs; savePrefs(); S.rng = null; render(true); } });
		m.push({ t: '存储与能力…', i: 'gear', a: capsDialog });
		menuAt(btn, m);
	}
	function uploadMenu(btn) {
		var w = canWrite() && S.mode === 'dir';
		menuAt(btn, [{ t: '上传文件…', i: 'upload', dis: !w, a: function() { pickFiles(false); } }, { t: '上传文件夹…', i: 'upload', dis: !w, a: function() { pickFiles(true); } }]);
	}

	/* ================= DOM 骨架 ================= */
	var host = null, dragTimer = 0;
	function build() {
		host = document.createElement('div');
		host.id = 'mwfiles-host';
		host.style.cssText = 'position:fixed;inset:0;z-index:100001;display:none';
		var sr = host.attachShadow({ mode: 'open' });
		var link = document.createElement('link');
		link.rel = 'stylesheet'; link.href = BASE + 'main.css?v=' + encodeURIComponent(VER);
		var rt = document.createElement('div');
		rt.className = 'mf-root'; rt.style.visibility = 'hidden';
		var shown = function() { rt.style.visibility = ''; };
		link.onload = shown; link.onerror = shown; setTimeout(shown, 2500);
		sr.appendChild(link); sr.appendChild(rt);
		rt.innerHTML =
			'<div class="mf-scrim"></div>' +
			'<div class="mf-win" role="dialog" aria-modal="true" aria-label="文件管理" tabindex="-1">' +
			'<div class="mf-title"><button type="button" class="mf-btn mf-menu-btn" data-a="side" aria-label="显示位置侧栏">' + ic('menu') + '</button>' +
			'<h2>' + ic('folder') + '文件管理</h2><span class="mf-where mf-ell"></span><span class="mf-grow"></span>' +
			'<button type="button" class="mf-btn" data-a="tasks" title="传输任务" aria-label="传输任务">' + ic('tasks') + '<span class="mf-badge" hidden></span></button>' +
			'<button type="button" class="mf-btn" data-a="caps" title="存储与能力" aria-label="存储与能力">' + ic('gear') + '</button>' +
			'<button type="button" class="mf-btn" data-a="close" title="关闭(Esc)" aria-label="关闭文件管理">' + ic('close') + '</button></div>' +
			'<div class="mf-body"><div class="mf-side-scrim" data-a="sidex"></div><nav class="mf-side" aria-label="位置"></nav>' +
			'<div class="mf-main"><div class="mf-bar">' +
			'<button type="button" class="mf-btn" data-a="back" title="后退(Alt+←)" aria-label="后退">' + ic('back') + '</button>' +
			'<button type="button" class="mf-btn" data-a="fwd" title="前进(Alt+→)" aria-label="前进">' + ic('forward') + '</button>' +
			'<button type="button" class="mf-btn" data-a="up" title="上一级(Alt+↑ / Backspace)" aria-label="上一级">' + ic('up') + '</button>' +
			'<button type="button" class="mf-btn" data-a="refresh" title="刷新(F5)" aria-label="刷新">' + ic('refresh') + '</button>' +
			'<div class="mf-crumb" role="navigation" aria-label="当前位置" data-a="crumbbar"></div>' +
			'<div class="mf-search"><span class="mf-i">' + ic('search').replace(/^<span class="mf-i">|<\/span>$/g, '') + '</span><input class="mf-in" type="search" data-r="q" placeholder="搜索名称" aria-label="搜索文件名(回车搜索子文件夹)" autocomplete="off" spellcheck="false"><button type="button" class="mf-btn mf-x" data-a="qx" aria-label="清除搜索" hidden>' + ic('close') + '</button></div>' +
			'<div class="mf-seg" role="group" aria-label="视图"><button type="button" class="mf-btn" data-a="vlist" title="列表视图" aria-label="列表视图">' + ic('list') + '</button><button type="button" class="mf-btn" data-a="vgrid" title="图标视图" aria-label="图标视图">' + ic('grid') + '</button></div></div>' +
			'<div class="mf-acts" role="toolbar" aria-label="操作"></div><div class="mf-head" role="presentation"></div>' +
			'<div class="mf-lw"><div class="mf-banner-w" hidden></div><div class="mf-scroll" tabindex="0" role="listbox" aria-multiselectable="true" aria-label="文件列表"><div class="mf-sp"></div></div><div class="mf-empty" hidden></div><div class="mf-loading" hidden><i></i></div>' +
			'<div class="mf-drop">' + ic('cloudup') + '<span data-r="dropt">松开即可上传到当前文件夹</span></div></div>' +
			'<div class="mf-status" role="status"></div></div><aside class="mf-info" aria-label="信息"></aside></div>' +
			'<div class="mf-toasts" aria-live="polite"></div><div class="mf-tasks" hidden role="region" aria-label="传输任务"></div></div>';
		el.root = sr; el.rt = rt; el.host = host;
		el.win = rt.querySelector('.mf-win'); el.body = rt.querySelector('.mf-body'); el.side = rt.querySelector('.mf-side'); el.sscrim = rt.querySelector('.mf-side-scrim');
		el.crumb = rt.querySelector('.mf-crumb'); el.where = rt.querySelector('.mf-where'); el.q = rt.querySelector('[data-r=q]'); el.qx = rt.querySelector('[data-a=qx]');
		el.back = rt.querySelector('[data-a=back]'); el.fwd = rt.querySelector('[data-a=fwd]'); el.upb = rt.querySelector('[data-a=up]');
		el.vList = rt.querySelector('[data-a=vlist]'); el.vGrid = rt.querySelector('[data-a=vgrid]');
		el.acts = rt.querySelector('.mf-acts'); el.head = rt.querySelector('.mf-head'); el.scroll = rt.querySelector('.mf-scroll'); el.banner = rt.querySelector('.mf-banner-w'); el.sp = rt.querySelector('.mf-sp');
		el.empty = rt.querySelector('.mf-empty'); el.ld = rt.querySelector('.mf-loading'); el.drop = rt.querySelector('.mf-drop'); el.dropt = rt.querySelector('[data-r=dropt]');
		el.status = rt.querySelector('.mf-status'); el.info = rt.querySelector('.mf-info'); el.toasts = rt.querySelector('.mf-toasts'); el.tasks = rt.querySelector('.mf-tasks');
		el.tbtn = rt.querySelector('[data-a=tasks]'); el.tbadge = el.tbtn.querySelector('.mf-badge');
		buildActs();
		document.body.appendChild(host);
		syncDark();
		if (window.MutationObserver) new MutationObserver(syncDark).observe(document.documentElement, { attributes: true, attributeFilter: ['data-darkmode', 'class', 'data-theme'] });
		wire();
		if (window.ResizeObserver) new ResizeObserver(onResize).observe(el.body);
		S.built = true;
	}
	function syncDark() {
		var d = document.documentElement, dark = d.getAttribute('data-darkmode') === 'true' || d.classList.contains('dark') || d.getAttribute('data-theme') === 'dark';
		host.classList.toggle('dark', dark);
	}
	function onResize() {
		if (!S.open) return;
		var w = el.body.clientWidth - (P.info && el.info.offsetWidth && getComputedStyle(el.info).position !== 'absolute' ? el.info.offsetWidth : 0) - (getComputedStyle(el.side).position === 'absolute' ? 0 : el.side.offsetWidth);
		var k = w < 480 ? 'xs' : w < 640 ? 's' : w < 800 ? 'm' : 'l';
		if (el.body.getAttribute('data-w') !== k) { el.body.setAttribute('data-w', k); S.W = k; }
		geom(); render(true);
	}

	/* ================= 事件 ================= */
	function itemAt(t) {
		var r = t.closest && t.closest('.mf-row,.mf-cell');
		if (!r) return null;
		return { n: +r.getAttribute('data-i'), el: r };
	}
	function dropTarget(ev) {
		var t = ev.target; if (!t || !t.closest) return null;
		var row = t.closest('.mf-row,.mf-cell');
		if (row && row.getAttribute('data-d') === '1') { var it = S.items[+row.getAttribute('data-i')]; if (it && !(S.sel[relOf(it)] && dragInfo)) return { root: S.root, path: relOf(it), el: row }; }
		var nav = t.closest('.mf-nav-item[data-root]');
		if (nav && nav.getAttribute('data-root') && nav.getAttribute('data-a') !== 'netoff') return { root: nav.getAttribute('data-root'), path: nav.getAttribute('data-path') || '', el: nav };
		var cb = t.closest('.mf-crumb-b[data-path]');
		if (cb) return { root: S.root, path: cb.getAttribute('data-path') || '', el: cb };
		if (t.closest('.mf-lw')) return { root: S.root, path: S.path, el: null, here: true };
		return null;
	}
	function writableRoot(id) { var r = rootObj(id); return !!r && !r.ro && r.state !== 'error'; }
	var dtEl = null;
	function setDT(x) { if (dtEl === x) return; if (dtEl) dtEl.classList.remove('dt'); dtEl = x; if (x) x.classList.add('dt'); }
	function hasFiles(ev) { var t = ev.dataTransfer && ev.dataTransfer.types; return !!t && ([].indexOf.call(t, 'Files') >= 0 || (t.contains && t.contains('Files'))); }
	function hideDrop() { el.drop.classList.remove('on'); setDT(null); }

	function wire() {
		var rt = el.rt, lpT = 0;
		// ---- 点击 ----
		rt.addEventListener('click', function(ev) {
			var t = ev.target;
			if (menuEl && !menuEl.contains(t)) closeMenu();
			if (S.tasksOpen && !el.tasks.contains(t) && !t.closest('[data-a=tasks]') && !t.closest('.mf-dlg') && S.tasks.every(function(x) { return x.state !== 'running' && x.state !== 'queued' && x.state !== 'writing'; })) { S.tasksOpen = false; renderTasks(); }
			var b = t.closest('[data-a]');
			if (b && b.closest('.mf-tasks')) return tasksClick(ev);
			if (b && b.getAttribute('aria-disabled') === 'true') return;
			var it = itemAt(t);
			if (b && b.getAttribute('data-a') === 'chk' && it) { toggleSel(it.n); return; }
			if (b && b.getAttribute('data-a') === 'rowmore' && it) {
				var e = S.items[it.n]; if (!S.sel[relOf(e)]) selectOnly(it.n);
				menuAt(b, itemMenuList(selList())); return;
			}
			if (b && b.getAttribute('data-a')) return action(b, ev);
			if (it) {
				if (Date.now() - (S.lp || 0) < 600) return;
				var e2 = S.items[it.n];
				if (COARSE) { if (S.nsel > 0) toggleSel(it.n); else openItem(e2); return; }
				if (ev.ctrlKey || ev.metaKey) toggleSel(it.n);
				else if (ev.shiftKey) rangeSel(it.n);
				else selectOnly(it.n);
				el.scroll.focus({ preventScroll: true });
				return;
			}
			if (t.closest('.mf-scroll') && !t.closest('.mf-row,.mf-cell')) { clearSel(); el.scroll.focus({ preventScroll: true }); }
		});
		rt.addEventListener('dblclick', function(ev) {
			if (COARSE) return;
			var it = itemAt(ev.target); if (!it || ev.target.closest('.mf-chk,.mf-more')) return;
			ev.preventDefault();
			openItem(S.items[it.n]);
		});
		rt.addEventListener('contextmenu', function(ev) {
			var tag = ev.target.tagName;
			if (tag === 'INPUT' || tag === 'TEXTAREA' || ev.target.closest('.mf-viewer,.mf-dlg')) return;
			var it = itemAt(ev.target), side = ev.target.closest('.mf-nav-item');
			if (COARSE) { ev.preventDefault(); return; }
			if (it) {
				ev.preventDefault();
				var e = S.items[it.n]; if (!S.sel[relOf(e)]) selectOnly(it.n);
				menu(ev.clientX, ev.clientY, itemMenuList(selList()));
			} else if (side) {
				ev.preventDefault();
				var sid = side.getAttribute('data-sid'), root = side.getAttribute('data-root'), path = side.getAttribute('data-path') || '';
				if (sid) return netItemMenu(side, sid, ev.clientX, ev.clientY);
				if (side.getAttribute('data-file')) return;
				if (path) menu(ev.clientX, ev.clientY, [{ t: '打开', a: function() { go(root, path); } }, { t: isFav(root, path) ? '从收藏夹移除' : '添加到收藏夹', i: 'star', a: function() { toggleFav(root, path, baseOf(path)); } }]);
				else if (root) {
					var rr = rootObj(root), mi = [{ t: '打开', a: function() { go(root, ''); } }, { t: '属性', i: 'info', a: function() { go(root, '').then(function() { showProps(null); }); } }];
					if (rr && rr.kind !== 'remote') {
						mi.push({ sep: 1 });
						if (rr.internal || rr.sysdisk) mi.push({ t: '格式化(系统存储不可格式化)', i: 'lock', dis: true, a: function() { toast(rr.internal ? '内部存储承载着固件和系统, 不能格式化' : '这是系统所在的存储卡, 不能格式化', 'warn'); } });
						else if (S.fmtOk && (rr.dev || '').indexOf('/dev/') === 0) mi.push({ t: '格式化…', i: 'trash', danger: true, a: function() { formatDialog(rr.dev); } });
					}
					menu(ev.clientX, ev.clientY, mi);
				}
			} else if (ev.target.closest('.mf-scroll,.mf-empty')) {
				ev.preventDefault(); clearSel(); blankMenu(ev.clientX, ev.clientY);
			}
		});
		// ---- 触摸长按 = 进入多选 ----
		rt.addEventListener('touchstart', function(ev) {
			var it = itemAt(ev.target); if (!it || ev.target.closest('.mf-chk,.mf-more')) return;
			clearTimeout(lpT);
			lpT = setTimeout(function() { S.lp = Date.now(); toggleSel(it.n); if (navigator.vibrate) { try { navigator.vibrate(12); } catch (x) {} } }, 480);
		}, { passive: true });
		['touchmove', 'touchend', 'touchcancel'].forEach(function(n) { rt.addEventListener(n, function() { clearTimeout(lpT); }, { passive: true }); });
		// ---- 搜索 ----
		var qT = 0;
		el.q.addEventListener('input', function() {
			el.qx.hidden = !el.q.value;
			clearTimeout(qT);
			qT = setTimeout(function() { S.q = el.q.value.trim(); S.mode = 'dir'; S.sel = {}; S.nsel = 0; renderChrome(); load(false); }, 260);
		});
		el.q.addEventListener('keydown', function(ev) {
			if (ev.key === 'Enter') { ev.preventDefault(); clearTimeout(qT); var v = el.q.value.trim(); if (!v) return; S.q = v; S.mode = 'search'; S.sel = {}; S.nsel = 0; renderChrome(); load(false); el.scroll.focus(); }
			else if (ev.key === 'Escape') { ev.stopPropagation(); clearSearch(); el.scroll.focus(); }
			else if (ev.key === 'ArrowDown') { ev.preventDefault(); el.scroll.focus(); if (S.cur < 0 && S.items.length) selectOnly(0); }
		});
		el.filter.addEventListener('change', function() { S.type = el.filter.value; load(false); });
		el.crumb.addEventListener('dblclick', function(ev) { if (!ev.target.closest('.mf-crumb-b')) editCrumb(); });
		// ---- 滚动 / 键盘 ----
		el.scroll.addEventListener('scroll', function() { render(false); }, { passive: true });
		el.win.addEventListener('keydown', onKey);
		el.tasks.addEventListener('click', tasksClick);
		// ---- 拖放 ----
		rt.addEventListener('dragstart', function(ev) {
			var it = itemAt(ev.target); if (!it) return;
			var e = S.items[it.n]; if (!S.sel[relOf(e)]) selectOnly(it.n);
			var items = selList();
			if (S.mode === 'search' || !canWrite()) { dragInfo = { items: items, copyOnly: true }; } else dragInfo = { items: items };
			try { ev.dataTransfer.setData('text/plain', items.map(function(x) { return x.n; }).join('\n')); ev.dataTransfer.effectAllowed = 'copyMove'; } catch (x) {}
		});
		rt.addEventListener('dragend', function() { dragInfo = null; hideDrop(); });
		rt.addEventListener('dragover', function(ev) {
			var tg = dropTarget(ev);
			if (dragInfo) {
				if (!tg || tg.here || !writableRoot(tg.root)) { setDT(null); ev.dataTransfer.dropEffect = 'none'; return; }
				ev.preventDefault(); setDT(tg.el);
				ev.dataTransfer.dropEffect = (ev.ctrlKey || ev.altKey || dragInfo.copyOnly) ? 'copy' : 'move';
				return;
			}
			if (hasFiles(ev)) {
				ev.preventDefault();
				var ok = tg && writableRoot(tg.root) && (tg.here ? S.mode === 'dir' : true);
				ev.dataTransfer.dropEffect = ok ? 'copy' : 'none';
				if (ok && !tg.here) { setDT(tg.el); el.drop.classList.remove('on'); }
				else if (ok) { setDT(null); el.dropt.textContent = '松开即可上传到“' + ((S.path ? baseOf(S.path) : (curRoot() || {}).label) || '') + '”'; el.drop.classList.add('on'); }
				else { setDT(null); el.drop.classList.remove('on'); }
				clearTimeout(dragTimer); dragTimer = setTimeout(hideDrop, 700);
			}
		});
		rt.addEventListener('dragleave', function(ev) { if (!ev.relatedTarget || !el.win.contains(ev.relatedTarget)) { clearTimeout(dragTimer); dragTimer = setTimeout(hideDrop, 150); } });
		rt.addEventListener('drop', function(ev) {
			var tg = dropTarget(ev), di = dragInfo;
			hideDrop(); dragInfo = null;
			if (di) {
				ev.preventDefault();
				if (!tg || tg.here || !writableRoot(tg.root)) return;
				moveInto(di.items, tg.root, tg.path, ev.ctrlKey || ev.altKey || di.copyOnly);
				return;
			}
			if (hasFiles(ev)) {
				ev.preventDefault();
				if (!tg || !writableRoot(tg.root) || (tg.here && S.mode !== 'dir')) return toast(S.mode !== 'dir' ? '请先退出搜索再上传' : '此位置是只读的, 不能上传', 'err');
				dropFiles(ev.dataTransfer, tg.root, tg.path);
			}
		});
		// 防止在窗口外的拖放把页面导航走
		host.addEventListener('dragover', function(ev) { if (hasFiles(ev)) ev.preventDefault(); });
		host.addEventListener('drop', function(ev) { if (hasFiles(ev)) ev.preventDefault(); });
	}
	function clearSearch() {
		el.q.value = ''; el.qx.hidden = true; S.q = ''; S.mode = 'dir'; S.sel = {}; S.nsel = 0; renderChrome(); load(false);
	}
	function action(b, ev) {
		var a = b.getAttribute('data-a'), items = selList();
		switch (a) {
			case 'close': closeWin(); break;
			case 'side': S.side = !S.side; el.side.classList.toggle('open', S.side); el.sscrim.classList.toggle('on', S.side); break;
			case 'sidex': S.side = false; el.side.classList.remove('open'); el.sscrim.classList.remove('on'); break;
			case 'tasks': S.tasksOpen = !S.tasksOpen; renderTasks(); break;
			case 'caps': capsDialog(); break;
			case 'back': goBack(); break;
			case 'fwd': goFwd(); break;
			case 'up': goUp(); break;
			case 'refresh': refresh(false); rootsSoon(); break;
			case 'vlist': case 'vgrid': P.view = a === 'vgrid' ? 'grid' : 'list'; savePrefs(); S.rng = null; renderChrome(); geom(); render(true); break;
			case 'qx': clearSearch(); el.q.focus(); break;
			case 'nav': { var r = b.getAttribute('data-root'); if (r && rootObj(r)) go(r, b.getAttribute('data-path') || ''); break; }
			case 'navfile': { var rr = b.getAttribute('data-root'), pp = b.getAttribute('data-path') || ''; if (rr && rootObj(rr)) go(rr, parentOf(pp), { select: baseOf(pp) }); break; }
			case 'netoff': netConnectSaved(b.getAttribute('data-sid')); break;
			case 'smbadd': smbDialog(); break;
			case 'shares': shareManager(); break;
			case 'sharenew': shareManager(true); break;
			case 'about': aboutDialog(); break;
			case 'format': formatDialog(); break;
			case 'netdiag': netDiag(null); break;
			case 'recclear': P.rec = []; savePrefs(); renderSide(); break;
			case 'crumb': go(S.root, b.getAttribute('data-path') || '', { keepq: false }); break;
			case 'th': { var k = b.getAttribute('data-k'); setSort(k, P.sort === k ? (P.dir === 'asc' ? 'desc' : 'asc') : 'asc'); break; }
			case 'newfolder': doNew(false); break;
			case 'upload': { var w = canWrite() && S.mode === 'dir'; if (b.closest('.mf-empty')) pickFiles(false); else uploadMenu(b); break; }
			case 'download': doDownload(items); break;
			case 'rename': doRename(items[0]); break;
			case 'copy': setClip('copy', items); break;
			case 'cut': setClip('cut', items); break;
			case 'paste': doPaste(); break;
			case 'delete': doDelete(items); break;
			case 'more': moreMenu(b); break;
			case 'sort': sortMenu(b); break;
			case 'info': P.info = !P.info; savePrefs(); updateActs(); updateInfo(); onResize(); break;
			case 'infopv': { var e0 = items[0]; if (e0) openItem(e0); break; }
			case 'dirsize': { var e1 = items[0]; if (e1) { var o = div('mf-muted'); o.style.padding = '0 14px 8px'; b.parentNode.appendChild(o); dirSizeInto(relOf(e1), b, o); } break; }
		}
	}

	/* ---------- 键盘 ---------- */
	function dlgTrap(ev, h) {
		var f = [].slice.call(h.box.querySelectorAll('button:not([disabled]),input:not([disabled]),select,textarea,[tabindex="0"]')).filter(function(x) { return x.offsetParent !== null || x === el.root.activeElement; });
		if (!f.length) return;
		var i = f.indexOf(el.root.activeElement);
		if (ev.shiftKey && (i <= 0)) { ev.preventDefault(); f[f.length - 1].focus(); }
		else if (!ev.shiftKey && (i === f.length - 1)) { ev.preventDefault(); f[0].focus(); }
	}
	function onKey(ev) {
		ev.stopPropagation();
		var tag = ev.target.tagName, typing = tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT';
		if (menuEl) return menuKey(ev);
		if (dlgs.length) {
			var h = dlgs[dlgs.length - 1];
			if (ev.key === 'Escape') { ev.preventDefault(); h.close(null); }
			else if (ev.key === 'Enter' && tag !== 'BUTTON' && tag !== 'TEXTAREA' && tag !== 'SELECT' && h.primary) { ev.preventDefault(); h.primary.click(); }
			else if (ev.key === 'Tab') dlgTrap(ev, h);
			return;
		}
		if (V) {
			if (typing) return;
			if (ev.key === 'Tab') return;
			if (viewerKey(ev)) ev.preventDefault();
			return;
		}
		if (ev.key === 'Escape') {
			ev.preventDefault();
			if (S.tasksOpen && el.tasks.contains(ev.target)) { S.tasksOpen = false; renderTasks(); el.scroll.focus(); return; }
			if (typing) { if (ev.target === el.q) return; ev.target.blur(); return; }
			if (S.side) { action(el.sscrim, ev); return; }
			if (S.nsel) { clearSel(); return; }
			closeWin();
			return;
		}
		if (typing) return;
		var inList = el.scroll.contains(ev.target) || ev.target === el.scroll || ev.target === el.win || ev.target === document.body;
		var mod = ev.ctrlKey || ev.metaKey, k = ev.key;
		if (mod && (k === 'a' || k === 'A')) { if (!inList) return; ev.preventDefault(); selectAll(); return; }
		if (mod && (k === 'c' || k === 'C')) { if (!inList) return; ev.preventDefault(); setClip('copy', selList()); return; }
		if (mod && (k === 'x' || k === 'X')) { if (!inList) return; ev.preventDefault(); setClip('cut', selList()); return; }
		if (mod && (k === 'v' || k === 'V')) { if (!inList) return; ev.preventDefault(); doPaste(); return; }
		if (mod && (k === 'f' || k === 'F')) { ev.preventDefault(); el.q.focus(); el.q.select(); return; }
		if (k === 'F5') { ev.preventDefault(); refresh(false); rootsSoon(); return; }
		if (ev.altKey && k === 'ArrowLeft') { ev.preventDefault(); goBack(); return; }
		if (ev.altKey && k === 'ArrowRight') { ev.preventDefault(); goFwd(); return; }
		if (ev.altKey && k === 'ArrowUp') { ev.preventDefault(); goUp(); return; }
		if (!inList) return;
		var cols = S.cols || 1;
		if (k === 'ArrowDown') { ev.preventDefault(); moveCur(P.view === 'grid' ? cols : 1, ev); }
		else if (k === 'ArrowUp') { ev.preventDefault(); moveCur(P.view === 'grid' ? -cols : -1, ev); }
		else if (k === 'ArrowRight' && P.view === 'grid') { ev.preventDefault(); moveCur(1, ev); }
		else if (k === 'ArrowLeft' && P.view === 'grid') { ev.preventDefault(); moveCur(-1, ev); }
		else if (k === 'PageDown') { ev.preventDefault(); moveCur(Math.max(1, Math.floor(el.scroll.clientHeight / S.rowH)) * cols, ev); }
		else if (k === 'PageUp') { ev.preventDefault(); moveCur(-Math.max(1, Math.floor(el.scroll.clientHeight / S.rowH)) * cols, ev); }
		else if (k === 'Home') { ev.preventDefault(); if (S.items.length) { S.cur = -1; moveCur(0, ev); selectOnly(0); ensureVisible(0); } }
		else if (k === 'End') { ev.preventDefault(); if (S.items.length) { selectOnly(S.items.length - 1); ensureVisible(S.items.length - 1); } }
		else if (k === ' ') { ev.preventDefault(); if (S.cur >= 0) toggleSel(S.cur); }
		else if (k === 'Enter') { ev.preventDefault(); var e = S.items[S.cur]; if (e && S.nsel <= 1) openItem(e); else if (S.nsel > 1) selList().filter(function(x) { return x.d; }).slice(0, 1).forEach(openItem); }
		else if (k === 'Backspace') { ev.preventDefault(); goUp(); }
		else if (k === 'F2') { ev.preventDefault(); if (S.nsel === 1) doRename(selList()[0]); }
		else if (k === 'Delete') { ev.preventDefault(); if (S.nsel) doDelete(selList()); }
		else if (k === 'ContextMenu' || (ev.shiftKey && k === 'F10')) {
			ev.preventDefault();
			var r = el.scroll.getBoundingClientRect();
			if (S.nsel) menu(r.left + 40, r.top + 40, itemMenuList(selList())); else blankMenu(r.left + 40, r.top + 40);
		}
	}

	/* ================= 打开 / 关闭 ================= */
	function pickDefault() {
		var want = P.last && rootObj(P.last.r) ? P.last : null;
		var r = want ? rootObj(want.r) : (S.roots.filter(function(x) { return x.kind !== 'remote' && x.state !== 'error'; })[0] || S.roots[0]);
		if (!r) { S.root = null; S.path = ''; S.items = []; S.err = null; renderChrome(); renderAll(); return Promise.resolve(); }
		return go(r.id, want ? want.p : '', { nohist: false });
	}
	function hostFocus() { if (el.win) el.win.focus({ preventScroll: true }); }
	var docKey = null;
	function openWin(opt) {
		opt = opt || {};
		if (!S.built) build();
		var first = !S.ready;
		host.style.display = '';
		S.open = true;
		document.documentElement.setAttribute('data-mwfiles-shown', '1');
		if (!docKey) {
			docKey = function(ev) {
				if (!S.open) return;
				if (ev.key === 'Escape' && document.activeElement !== host) { ev.preventDefault(); ev.stopPropagation(); closeWinTop(); }
				else if (ev.key === 'Tab' && document.activeElement !== host) { ev.preventDefault(); hostFocus(); }
			};
			document.addEventListener('keydown', docKey, true);
			window.addEventListener('beforeunload', function(ev) { if (hasActiveUploads()) { ev.preventDefault(); ev.returnValue = '还有文件正在上传, 离开页面会中断上传。'; return ev.returnValue; } });
		}
		onResize();
		renderChrome(); renderTasks();
		if (first) {
			S.ready = true;
			el.info.classList.toggle('on', !!P.info);
			loadRoots().then(function() { return pickDefault(); }).then(function() { setTimeout(function() { onResize(); el.scroll.focus({ preventScroll: true }); }, 0); });
		} else {
			loadRoots().then(function() { if (!rootObj(S.root)) pickDefault(); else refresh(true); });
			el.scroll.focus({ preventScroll: true });
		}
		if (opt.path != null && opt.root) go(opt.root, opt.path);
	}
	function closeWinTop() {
		if (menuEl) return closeMenu();
		if (dlgs.length) return dlgs[dlgs.length - 1].close(null);
		if (V) return closeViewer();
		if (S.nsel) return clearSel();
		closeWin();
	}
	function reallyClose() {
		closeMenu(); dlgs.slice().forEach(function(h) { h.close(null); }); if (V) closeViewer();
		S.open = false; host.style.display = 'none';
		document.documentElement.removeAttribute('data-mwfiles-shown');
		if (opener && opener.isConnected) { try { opener.focus(); } catch (e) {} }
	}
	var opener = null;
	function closeWin() {
		if (!S.open) return;
		if (hasActiveUploads()) {
			dlg({ title: '还有文件正在上传', body: '<p>关闭窗口后, 上传可以在后台继续(请不要刷新或离开此页面)。</p>', modal: true,
				btns: [{ t: '取消', v: 'stay' }, { t: '中断上传并关闭', v: 'abort', danger: true }, { t: '后台继续', v: 'bg', primary: 1 }] }).then(function(v) {
				if (v === 'bg') reallyClose();
				else if (v === 'abort') { S.tasks.forEach(function(t) { if (t.kind === 'up' && t.cancel) t.cancel(); }); setTimeout(reallyClose, 50); }
			});
			return;
		}
		reallyClose();
	}

	window.MwrtFiles = {
		open: function(o) { opener = document.activeElement; openWin(o); },
		close: closeWin,
		version: VER
	};
})();
