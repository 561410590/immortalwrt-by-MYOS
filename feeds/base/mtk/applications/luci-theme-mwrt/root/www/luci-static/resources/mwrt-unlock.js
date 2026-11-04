/* MWRT 1154 cat adoption: local code 88888888, independent of plugin access. */
(function () {
	'use strict';

	if (window.__mwrtUnlock) return;
	window.__mwrtUnlock = true;

	var me = document.currentScript;
	var URL_ = '/cgi-bin/luci/admin/modem/cat_adopt';
	if (!URL_) return;

	var WORD = 'mwrtmwrt';
	var buf = '';
	var overlay, card, badge, title, sub, input, eye, submit, label, status, busy = false, done = false;

	/* 图标全用内联 SVG, 不依赖主题带不带 FontAwesome。都是写死的常量, 不含任何外部数据。 */
	var SVG = {
		shield: '<svg viewBox="0 0 24 24" width="26" height="26" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3l7 3v5c0 4.6-3 8.4-7 10-4-1.6-7-5.4-7-10V6l7-3z"/><circle cx="12" cy="11" r="2.2"/><path d="M12 13.2V16"/></svg>',
		check: '<svg viewBox="0 0 24 24" width="28" height="28" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>',
		lock: '<svg viewBox="0 0 24 24" width="17" height="17" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4.5" y="10.5" width="15" height="10" rx="2.5"/><path d="M8 10.5V7.8a4 4 0 0 1 8 0v2.7"/></svg>',
		eye: '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12z"/><circle cx="12" cy="12" r="2.8"/></svg>',
		eyeOff: '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 3l18 18"/><path d="M10.6 6A9.7 9.7 0 0 1 12 5.5c6 0 9.5 6.5 9.5 6.5a16 16 0 0 1-2.9 3.6M6.4 7.4A16.3 16.3 0 0 0 2.5 12S6 18.5 12 18.5c1.5 0 2.9-.4 4.1-1"/><path d="M9.9 9.9a2.8 2.8 0 0 0 4.2 4.2"/></svg>',
		close: '<svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="M6 6l12 12M18 6L6 18"/></svg>'
	};

	var CSS = [
		'#mwrt-unlock-overlay{display:none;position:fixed;inset:0;z-index:2147483646;align-items:center;justify-content:center;padding:16px;box-sizing:border-box;',
		'background:radial-gradient(120% 90% at 50% 30%,rgba(30,41,74,.55),rgba(5,8,16,.82));-webkit-backdrop-filter:blur(8px) saturate(120%);backdrop-filter:blur(8px) saturate(120%);',
		'font-family:-apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;color:#e7eaf3;opacity:0;transition:opacity .22s ease}',
		'#mwrt-unlock-overlay.mu-on{opacity:1}',
		'#mwrt-unlock-overlay *{box-sizing:border-box}',
		'#mwrt-unlock-overlay .mu-card{position:relative;width:368px;max-width:100%;padding:34px 30px 24px;border-radius:22px;text-align:center;overflow:hidden;',
		'background:linear-gradient(160deg,#1d2333 0%,#141824 55%,#10131c 100%);border:1px solid rgba(255,255,255,.08);',
		'box-shadow:0 30px 80px -20px rgba(0,0,0,.7),0 0 0 1px rgba(0,0,0,.25),inset 0 1px 0 rgba(255,255,255,.06);',
		'transform:translateY(10px) scale(.97);transition:transform .28s cubic-bezier(.2,.9,.3,1.2)}',
		'#mwrt-unlock-overlay.mu-on .mu-card{transform:none}',
		/* 顶部一条品牌色细光带 + 卡片顶部的柔光, 是"高级感"的主要来源, 两样都很淡 */
		'#mwrt-unlock-overlay .mu-card::before{content:"";position:absolute;left:0;right:0;top:0;height:3px;background:linear-gradient(90deg,#6366f1,#22d3ee 55%,#a855f7)}',
		'#mwrt-unlock-overlay .mu-card::after{content:"";position:absolute;left:50%;top:-120px;width:300px;height:220px;margin-left:-150px;pointer-events:none;',
		'background:radial-gradient(closest-side,rgba(99,102,241,.28),transparent);transition:background .3s}',
		'#mwrt-unlock-overlay .mu-card.mu-ok::after{background:radial-gradient(closest-side,rgba(16,185,129,.3),transparent)}',
		'#mwrt-unlock-overlay .mu-x{position:absolute;top:14px;right:14px;z-index:1;width:32px!important;height:32px!important;min-width:0!important;padding:0!important;margin:0!important;',
		'display:inline-flex!important;align-items:center;justify-content:center;border-radius:10px!important;border:0!important;background:transparent!important;',
		'color:#8b93a7!important;cursor:pointer;box-shadow:none!important;transition:background .15s,color .15s}',
		'#mwrt-unlock-overlay .mu-x:hover{background:rgba(255,255,255,.07)!important;color:#fff!important}',
		'#mwrt-unlock-overlay .mu-badge{position:relative;z-index:1;width:60px;height:60px;margin:0 auto 18px;border-radius:18px;display:flex;align-items:center;justify-content:center;color:#fff;',
		'background:linear-gradient(135deg,#6366f1,#06b6d4);box-shadow:0 10px 28px -6px rgba(99,102,241,.65),inset 0 1px 0 rgba(255,255,255,.25);transition:background .3s,box-shadow .3s}',
		'#mwrt-unlock-overlay .mu-ok .mu-badge{background:linear-gradient(135deg,#10b981,#22c55e);box-shadow:0 10px 28px -6px rgba(16,185,129,.65),inset 0 1px 0 rgba(255,255,255,.25)}',
		'#mwrt-unlock-overlay#mwrt-unlock-overlay .mu-title{position:relative;z-index:1;margin:0;padding:0;border:0;background:none;font-size:19px;font-weight:650;letter-spacing:.3px;',
		'color:#f5f7fb!important;-webkit-text-fill-color:#f5f7fb!important;line-height:1.35}',
		'#mwrt-unlock-overlay#mwrt-unlock-overlay .mu-sub{color:#8b93a7!important}',
		'#mwrt-unlock-overlay .mu-sub{position:relative;z-index:1;margin:8px 0 24px;font-size:13px;line-height:1.6;color:#8b93a7}',
		'#mwrt-unlock-overlay .mu-field{position:relative;display:flex;align-items:center;height:48px;border-radius:13px;',
		'background:rgba(255,255,255,.045);border:1px solid rgba(255,255,255,.1);transition:border-color .18s,box-shadow .18s,background .18s}',
		'#mwrt-unlock-overlay .mu-field:focus-within{border-color:rgba(99,102,241,.85);background:rgba(99,102,241,.07);box-shadow:0 0 0 4px rgba(99,102,241,.18)}',
		'#mwrt-unlock-overlay .mu-field.mu-bad{border-color:rgba(244,63,94,.8);box-shadow:0 0 0 4px rgba(244,63,94,.15)}',
		'#mwrt-unlock-overlay .mu-lock{flex:none;width:44px;display:flex;justify-content:center;color:#6b7390}',
		'#mwrt-unlock-overlay .mu-field:focus-within .mu-lock{color:#a5b4fc}',
		'#mwrt-unlock-overlay .mu-input{flex:1;min-width:0;height:46px!important;margin:0!important;padding:0 4px 0 0!important;border:0!important;border-radius:0!important;',
		'background:transparent!important;color:#f5f7fb!important;font-family:inherit!important;font-size:15px!important;font-weight:500!important;line-height:46px!important;letter-spacing:.5px;',
		'box-shadow:none!important;outline:none!important;-webkit-appearance:none;appearance:none;width:auto!important;max-width:none!important}',
		/* 聚焦态: 主题/aurora 对 input:focus 有带 id 提权的 !important 描边和阴影,
		 * 这里用双 id 把特异度抬到它们之上; 聚焦反馈由外层 .mu-field:focus-within 给 */
		'#mwrt-unlock-overlay#mwrt-unlock-overlay .mu-input:focus,#mwrt-unlock-overlay#mwrt-unlock-overlay .mu-input:focus-visible{outline:none!important;border:0!important;box-shadow:none!important;background:transparent!important}',
		'#mwrt-unlock-overlay#mwrt-unlock-overlay .mu-input{-webkit-text-fill-color:#f5f7fb!important;caret-color:#a5b4fc}',
		'#mwrt-unlock-overlay .mu-input::selection{background:rgba(99,102,241,.45);color:#fff;-webkit-text-fill-color:#fff}',
		'#mwrt-unlock-overlay .mu-input:-webkit-autofill{-webkit-box-shadow:0 0 0 40px #1a1f2e inset!important;-webkit-text-fill-color:#f5f7fb!important}',
		'#mwrt-unlock-overlay .mu-input::placeholder{color:#5d6580;-webkit-text-fill-color:#5d6580;letter-spacing:0;opacity:1}',
		'#mwrt-unlock-overlay .mu-input::-ms-reveal{display:none}',
		'#mwrt-unlock-overlay .mu-eye{flex:none;width:40px!important;height:40px!important;min-width:0!important;margin:0 4px 0 0!important;padding:0!important;border:0!important;border-radius:10px!important;',
		'background:transparent!important;color:#6b7390!important;display:inline-flex!important;align-items:center;justify-content:center;cursor:pointer;box-shadow:none!important}',
		'#mwrt-unlock-overlay .mu-eye:hover{color:#c7cbe0!important;background:rgba(255,255,255,.06)!important}',
		'#mwrt-unlock-overlay .mu-st{min-height:20px;margin:10px 2px 14px;font-size:12.5px;line-height:20px;text-align:left;color:#8b93a7}',
		'#mwrt-unlock-overlay .mu-st.mu-err{color:#fb7185}',
		'#mwrt-unlock-overlay .mu-st.mu-good{color:#34d399;text-align:center}',
		'#mwrt-unlock-overlay .mu-go{position:relative;width:100%!important;height:46px!important;margin:0!important;padding:0 16px!important;border:0!important;border-radius:13px!important;cursor:pointer;',
		'display:inline-flex!important;align-items:center;justify-content:center;gap:8px;font-family:inherit!important;font-size:15px!important;font-weight:600!important;line-height:1!important;letter-spacing:1px;color:#fff!important;',
		'background:linear-gradient(135deg,#6366f1 0%,#4f46e5 45%,#0891b2 100%)!important;box-shadow:0 10px 24px -8px rgba(79,70,229,.75),inset 0 1px 0 rgba(255,255,255,.2)!important;',
		'text-shadow:none!important;transition:transform .15s,box-shadow .15s,filter .15s}',
		'#mwrt-unlock-overlay .mu-go:hover:not(:disabled){transform:translateY(-1px);filter:brightness(1.08);box-shadow:0 14px 28px -8px rgba(79,70,229,.85),inset 0 1px 0 rgba(255,255,255,.2)!important}',
		'#mwrt-unlock-overlay .mu-go:active:not(:disabled){transform:translateY(0)}',
		'#mwrt-unlock-overlay .mu-go:disabled{cursor:default;filter:saturate(.75) brightness(.92)}',
		'#mwrt-unlock-overlay .mu-ok .mu-go{background:linear-gradient(135deg,#10b981,#059669)!important;box-shadow:0 10px 24px -8px rgba(16,185,129,.7)!important}',
		'#mwrt-unlock-overlay .mu-spin{display:none;width:16px;height:16px;border-radius:50%;border:2px solid rgba(255,255,255,.35);border-top-color:#fff;animation:mu-rot .7s linear infinite}',
		'#mwrt-unlock-overlay .mu-busy .mu-spin{display:inline-block}',
		'#mwrt-unlock-overlay .mu-x:focus-visible,#mwrt-unlock-overlay .mu-eye:focus-visible,#mwrt-unlock-overlay .mu-go:focus-visible{outline:2px solid #a5b4fc!important;outline-offset:2px}',
		'#mwrt-unlock-overlay .mu-hint{margin-top:16px;font-size:11.5px;color:#5d6580;letter-spacing:.3px}',
		'#mwrt-unlock-overlay .mu-hint kbd{display:inline-block;min-width:20px;padding:1px 6px;margin:0 2px;border-radius:5px;font:600 10.5px/16px ui-monospace,Consolas,monospace;',
		'color:#a3aac0;background:rgba(255,255,255,.06);border:1px solid rgba(255,255,255,.1);border-bottom-width:2px}',
		'#mwrt-unlock-overlay .mu-shake{animation:mu-shake .42s cubic-bezier(.36,.07,.19,.97)}',
		'@keyframes mu-rot{to{transform:rotate(360deg)}}',
		'@keyframes mu-shake{10%,90%{transform:translateX(-1px)}20%,80%{transform:translateX(3px)}30%,50%,70%{transform:translateX(-6px)}40%,60%{transform:translateX(6px)}}',
		'@media (max-width:420px){#mwrt-unlock-overlay .mu-card{padding:30px 20px 20px;border-radius:20px}}',
		'@media (prefers-reduced-motion:reduce){#mwrt-unlock-overlay,#mwrt-unlock-overlay .mu-card{transition:none}#mwrt-unlock-overlay .mu-shake{animation:none}#mwrt-unlock-overlay .mu-spin{animation-duration:1.6s}}'
	].join('');

	function mk(tag, cls, text) {
		var el = document.createElement(tag);
		if (cls) el.className = cls;
		if (text != null) el.textContent = text;
		return el;
	}

	function icon(el, name) { el.innerHTML = SVG[name]; return el; }

	/* 卡片里的状态行。kind: '' 普通 / 'err' 错误(卡片轻摇) / 'good' 成功 */
	function say(msg, kind) {
		status.textContent = msg || '';
		status.className = 'mu-st' + (kind ? ' mu-' + kind : '');
		field.classList.toggle('mu-bad', kind === 'err');
		if (kind === 'err') {
			card.classList.remove('mu-shake');
			void card.offsetWidth;   /* 重新触发动画 */
			card.classList.add('mu-shake');
		}
	}

	var field;
	function build() {
		var st = mk('style');
		st.id = 'mwrt-unlock-style';
		st.textContent = CSS;
		document.head.appendChild(st);

		overlay = mk('div');
		overlay.id = 'mwrt-unlock-overlay';
		overlay.setAttribute('role', 'dialog');
		overlay.setAttribute('aria-modal', 'true');
		overlay.setAttribute('aria-labelledby', 'mwrt-unlock-title');
		overlay.setAttribute('aria-describedby', 'mwrt-unlock-sub');

		card = mk('div', 'mu-card');
		card.addEventListener('animationend', function () { card.classList.remove('mu-shake'); });

		var close = icon(mk('button', 'mu-x'), 'close');
		close.type = 'button';
		close.id = 'mwrt-unlock-close';
		close.setAttribute('aria-label', '关闭');
		close.addEventListener('click', hide);

		badge = icon(mk('div', 'mu-badge'), 'shield');
		/* 标题用 div 不用 h2: aurora 皮有 h2:not(#mwrt-nope){color:var(--text)!important},
		 * 浅色皮下 --text 是深色, 压在深色卡片上标题直接看不见 */
		title = mk('div', 'mu-title', '领养猫咪');
		title.setAttribute('role', 'heading');
		title.setAttribute('aria-level', '2');
		title.id = 'mwrt-unlock-title';
		sub = mk('p', 'mu-sub', '输入 8 个 8，领取你的猫咪');
		sub.id = 'mwrt-unlock-sub';

		field = mk('div', 'mu-field');
		var lock = icon(mk('span', 'mu-lock'), 'lock');
		input = mk('input', 'mu-input');
		input.type = 'password';
		input.id = 'mwrt-unlock-input';
		input.autocomplete = 'off';
		input.spellcheck = false;
		input.placeholder = '请输入授权口令';
		input.setAttribute('aria-label', '领养口令');
		input.setAttribute('aria-describedby', 'mwrt-unlock-toast');
		input.addEventListener('keydown', function (e) {
			if (e.key === 'Enter') { e.preventDefault(); send(); }
		});
		/* 重新打字时把上一次的错误提示收掉, 不让红字一直挂着 */
		input.addEventListener('input', function () { if (!busy && !done && status.textContent) say(''); });

		eye = icon(mk('button', 'mu-eye'), 'eye');
		eye.type = 'button';
		eye.setAttribute('aria-label', '显示口令');
		eye.setAttribute('aria-pressed', 'false');
		eye.addEventListener('click', function () { reveal(input.type === 'password'); input.focus(); });

		field.appendChild(lock);
		field.appendChild(input);
		field.appendChild(eye);

		status = mk('div', 'mu-st');
		status.id = 'mwrt-unlock-toast';
		status.setAttribute('role', 'status');
		status.setAttribute('aria-live', 'polite');

		submit = mk('button', 'mu-go');
		submit.type = 'button';
		submit.id = 'mwrt-unlock-submit';
		label = mk('span', null, '领养猫咪');
		submit.appendChild(mk('span', 'mu-spin'));
		submit.appendChild(label);
		submit.addEventListener('click', send);

		var hint = mk('div', 'mu-hint');
		hint.appendChild(mk('kbd', null, 'Enter'));
		hint.appendChild(document.createTextNode(' 提交　'));
		hint.appendChild(mk('kbd', null, 'Esc'));
		hint.appendChild(document.createTextNode(' 关闭'));

		[ close, badge, title, sub, field, status, submit, hint ].forEach(function (n) { card.appendChild(n); });
		overlay.appendChild(card);
		overlay.addEventListener('mousedown', function (e) { if (e.target === overlay && !busy && !done) hide(); });
		overlay.addEventListener('keydown', trap);
		/* s90r2: Esc 挂 document 捕获阶段。点卡片文字/空白后焦点落到 body, 挂在 overlay 上的监听收不到 Esc */
		document.addEventListener('keydown', escAnywhere, true);
		document.body.appendChild(overlay);
	}

	function reveal(on) {
		input.type = on ? 'text' : 'password';
		icon(eye, on ? 'eyeOff' : 'eye');
		eye.setAttribute('aria-label', on ? '隐藏口令' : '显示口令');
		eye.setAttribute('aria-pressed', on ? 'true' : 'false');
	}

	/* 认证框打开且空闲时, 任何位置按 Esc 都关闭(提交中/已成功不响应) */
	function escAnywhere(e) {
		if ((e.key === 'Escape' || e.key === 'Esc') && overlay && overlay.style.display === 'flex' && !busy && !done) {
			e.preventDefault(); e.stopPropagation(); hide();
		}
	}

	/* 焦点留在卡片里: Tab 走到头绕回来; Esc 由 escAnywhere 处理 */
	function trap(e) {
		if (e.key === 'Escape') return;
		if (e.key !== 'Tab') return;
		var f = Array.prototype.filter.call(card.querySelectorAll('button,input'), function (n) { return !n.disabled; });
		if (!f.length) return;
		var first = f[0], last = f[f.length - 1];
		if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }
		else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
	}

	var lastFocus = null;
	function show() {
		if (!overlay) build();
		if (overlay.style.display === 'flex') { input.focus(); return; }
		lastFocus = document.activeElement;
		input.value = '';
		reveal(false);
		say('');
		overlay.style.display = 'flex';
		void overlay.offsetWidth;
		overlay.classList.add('mu-on');
		setTimeout(function () { input.focus(); }, 60);
	}

	function hide() {
		if (!overlay) return;
		var wasOpen = overlay.style.display === 'flex';
		overlay.classList.remove('mu-on');
		overlay.style.display = 'none';
		if (lastFocus && lastFocus.focus) try { lastFocus.focus(); } catch (e) {}
		/* s90: 用户关掉认证框(没通过)时通知侧栏笼中猫复位。成功路径不走 hide(),直接刷新页面。 */
		if (wasOpen) fire('mwrt-unlock-close', { ok: false });
	}

	/* s90: 向 window 派发自定义事件(旧浏览器没有 CustomEvent 构造函数时走 createEvent)。
	 * 订阅者的异常不能影响认证框本身。 */
	function fire(name, detail) {
		try {
			var ev;
			if (typeof window.CustomEvent === 'function') ev = new CustomEvent(name, { detail: detail });
			else { ev = document.createEvent('CustomEvent'); ev.initCustomEvent(name, false, false, detail); }
			window.dispatchEvent(ev);
		} catch (e) {}
	}

	/* s90: 公开接口。侧栏笼中猫(mwrt-catcage.js)破笼后调用 open() 弹出同一个认证框。 */
	window.MwrtUnlock = {
		open: function () { show(); },
		isOpen: function () { return !!overlay && overlay.style.display === 'flex'; }
	};

	function setBusy(b) {
		busy = b;
		submit.disabled = b;
		input.readOnly = b;
		submit.classList.toggle('mu-busy', b);
		submit.setAttribute('aria-busy', b ? 'true' : 'false');
		label.textContent = b ? '正在验证…' : '领养猫咪';
	}

	function succeed() {
		done = true;
		card.classList.add('mu-ok');
		icon(badge, 'check');
		title.textContent = '领养成功';
		sub.textContent = '猫咪已领养';
		input.readOnly = true;
		submit.disabled = true;
		submit.classList.add('mu-busy');
		label.textContent = '正在刷新';
		say('领养成功，正在显示猫咪…', 'good');
		/* 服务端菜单缓存后端已清; 浏览器这边 ui.menu 还把菜单树缓存在
		 * sessionStorage(luci-session-store), 不清的话刷新后还是旧菜单。 */
		try { if (window.L && L.ui && L.ui.menu && L.ui.menu.flushCache) L.ui.menu.flushCache(); } catch (e) {}
		try { sessionStorage.removeItem('luci-session-store'); } catch (e) {}
		/* s90: 刷新后侧栏里的猫要播一次"入场礼花 + 玩偶从天而降"。标记写 sessionStorage,
		 * 由 mwrt-catcage.js 读到后立即清掉; 同时派发事件让笼中猫先收尾。 */
		try { sessionStorage.setItem('mwrt-cat-celebrate', '1'); } catch (e) {}
		fire('mwrt-unlock-ok', { ok: true });
		setTimeout(function () { location.reload(); }, 1200);
	}

	function send() {
		if (busy || done) return;
		var code = input.value || '';
		if (!code) { say('请输入口令', 'err'); input.focus(); return; }

		var token = (window.L && L.env && L.env.token) || '';
		var body = 'token=' + encodeURIComponent(token) + '&set=' + encodeURIComponent(code);

		say('');
		setBusy(true);
		var xhr = new XMLHttpRequest();
		xhr.open('POST', URL_, true);
		xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
		xhr.timeout = 20000;
		xhr.onload = function () {
			setBusy(false);
			var res = null;
			try { res = JSON.parse(xhr.responseText).resdd; } catch (e) {}
			if (res === 'yes') return succeed();
			if (res === 'no') say('口令不正确，核销失败，请重试', 'err');
			else say('认证失败（' + xhr.status + '），请稍后重试', 'err');
			input.focus();
			input.select();
		};
		xhr.onerror = xhr.ontimeout = function () {
			setBusy(false);
			say('请求失败，请检查网络后重试', 'err');
			input.focus();
		};
		xhr.send(body);
	}

	function editable(el) {
		if (!el) return false;
		if (el.isContentEditable) return true;
		return /^(input|textarea|select)$/i.test(el.tagName || '');
	}

	document.addEventListener('keydown', function (e) {
		if (e.ctrlKey || e.metaKey || e.altKey) return;
		if (editable(e.target) || editable(document.activeElement)) return;
		if (!e.key || e.key.length !== 1) return;
		buf = (buf + e.key.toLowerCase()).slice(-WORD.length);
		if (buf === WORD) {
			buf = '';
			show();
		}
	}, true);

	/* 移动端没有键盘, 敲不出 mwrtmwrt。改成连续点首页「当前固件版本」那块 8 次
	 * 弹出同一个认证框, 并配一段趣味动画:
	 *   第 1-2 次: 只计数, 没反应;
	 *   第 3 次  : 屏幕下方冒出一只被笼子锁住、只露出头的二次元猫;
	 *   第 3-7 次: 每点一次撸一下猫(眯眼/爱心), 笼子被它一点点撑开(--esc 递增);
	 *   第 8 次  : 猫挣脱牢笼跳出来, 随即弹出认证框。
	 * 固件版本元素 #f-fw 每轮轮询会重建, 用事件委托挂 document、按 closest('.fw-banner')
	 * 命中, 不直接绑节点。这块只在首页存在, 别的页面点不到、无副作用。
	 * 两次点击间隔超过 1.2 秒就清零并收起猫, 免得平时的误触累积成解锁。
	 * 全部内联 SVG + CSS, 不依赖任何图片 / 字体 / 主题样式。 */
	var CAT_CSS = CAT_STYLE();
	var catWrap = null, catN = 0, catT = 0, catTimer = null;
	var NEED = 8, SHOW_FROM = 3, GAP = 1600;

	function catReset() {
		catN = 0;
		if (catTimer) { clearTimeout(catTimer); catTimer = null; }
		if (catWrap) { catWrap.classList.remove('mc-on'); catWrap.classList.add('mc-off'); }
	}

	function onTap() {
		var now = Date.now();
		if (now - catT > GAP) catReset();
		catT = now;
		catN++;
		/* 每一次点击都给反馈(含前两次还没出猫时): 点哪儿冒一枚粉色猫爪肉球, 弹一下淡出。
		 * 这样"点版本卡片"从第一下就有可爱回应, 不是前两下石沉大海。 */
		pawPop();
		if (catN >= SHOW_FROM) catShow(catN);
		if (catTimer) { clearTimeout(catTimer); }
		/* 一段时间没继续点就收起(但别在跳出/弹框的那次收) */
		if (catN < NEED) catTimer = setTimeout(catReset, GAP + 400);
		if (catN >= NEED) {
			catEscape();
			var n = catN; catN = 0;
			setTimeout(show, 760);           /* 等跳出动画播完再弹框 */
			setTimeout(catReset, 1400);
		}
	}

	var catX = 0, catY = 0;
	document.addEventListener('click', function (e) {
		var t = e.target;
		if (!t || !t.closest) return;
		if (!t.closest('.fw-banner')) return;
		if (t.closest('.fw-act')) return;   /* 检查更新 / NEW 是正经操作, 不算 */
		/* 记录点击位置(视口坐标), 让猫就从手指点的地方冒出来, 而不是固定在屏幕下方。
		 * touch 事件没有 clientX 时回退到被点元素的中心。 */
		if (e.clientX || e.clientY) { catX = e.clientX; catY = e.clientY; }
		else { var r = t.getBoundingClientRect(); catX = r.left + r.width / 2; catY = r.top + r.height / 2; }
		onTap();
	}, true);

	/* ---- 笼中猫动画 ---- */
	function catBuild() {
		if (catWrap) return;
		var st = document.createElement('style');
		st.textContent = CAT_CSS;
		document.head.appendChild(st);
		catWrap = document.createElement('div');
		catWrap.id = 'mwrt-cat';
		catWrap.className = 'mc-off';
		catWrap.setAttribute('aria-hidden', 'true');
		catWrap.innerHTML = CAT_SVG();
		document.body.appendChild(catWrap);
	}

	/* 把猫浮层摆到点击点: 120px 见方, 让猫的下半(笼子底座)大致落在手指处、头往上探。
	 * 夹在视口内, 别超出屏幕边。 */
	function catPlace() {
		if (!catWrap) return;
		var W = 120, H = 120, m = 6;
		var vw = window.innerWidth || document.documentElement.clientWidth;
		var vh = window.innerHeight || document.documentElement.clientHeight;
		var left = catX - W / 2;
		var top = catY - H * 0.72;   /* 点击点落在笼子中下部, 猫头在其上方 */
		left = Math.max(m, Math.min(left, vw - W - m));
		top = Math.max(m, Math.min(top, vh - H - m));
		catWrap.style.left = left + 'px';
		catWrap.style.top = top + 'px';
	}

	function catShow(n) {
		catBuild();
		catPlace();
		/* --esc: 0~1 的挣脱进度, 从第 3 次(0)到第 7 次(接近 1)。栏杆弯曲 / 猫上移都看它 */
		var esc = Math.max(0, Math.min(1, (n - SHOW_FROM) / (NEED - 1 - SHOW_FROM)));
		catWrap.style.setProperty('--esc', esc.toFixed(3));
		catWrap.classList.remove('mc-off', 'mc-escape');
		catWrap.classList.add('mc-on');
		/* 撸猫: 每次点都重放一遍"被撸"动画(眯眼 + 冒爱心) */
		catWrap.classList.remove('mc-pet');
		void catWrap.offsetWidth;
		catWrap.classList.add('mc-pet');
	}

	function catEscape() {
		catBuild();
		catPlace();
		catWrap.style.setProperty('--esc', '1');
		catWrap.classList.remove('mc-off');
		catWrap.classList.add('mc-on', 'mc-escape');
	}

	/* 每次点击在点击点冒一枚猫爪肉球, 弹一下淡出后自删。独立于笼中猫, 前两次点也有。 */
	function pawPop() {
		catBuild();   /* 复用它注入的样式表 */
		var p = document.createElement('div');
		p.className = 'mc-paw';
		p.innerHTML = PAW_SVG();
		p.style.left = catX + 'px';
		p.style.top = catY + 'px';
		document.body.appendChild(p);
		setTimeout(function () { if (p.parentNode) p.parentNode.removeChild(p); }, 700);
	}

	/* 二次元猫 + 牢笼, 全内联 SVG。分三层:
	 *   .mc-cage-back 笼子后半(猫在它前面) / .mc-cat 猫 / .mc-cage 笼子前栏杆(猫被关在后面)
	 * 栏杆用 <path>, 由 CSS 的 --esc 控制中段外弯的程度(scaleX/路径变形靠 transform)。 */
	/* 猫爪肉球: 一个大掌垫 + 四个趾垫, 点击反馈用。粉色、圆润。 */
	function PAW_SVG() {
		return '<svg viewBox="0 0 40 40" width="40" height="40" aria-hidden="true">' +
			'<ellipse class="mc-pad" cx="20" cy="26" rx="10" ry="8.5"/>' +
			'<circle class="mc-pad" cx="10" cy="16" r="3.4"/>' +
			'<circle class="mc-pad" cx="17" cy="11" r="3.6"/>' +
			'<circle class="mc-pad" cx="24" cy="11" r="3.6"/>' +
			'<circle class="mc-pad" cx="31" cy="16" r="3.4"/>' +
			'</svg>';
	}

	function CAT_SVG() {
		return '' +
		'<div class="mc-stage">' +
		  '<svg class="mc-svg" viewBox="0 0 120 120" width="120" height="120" aria-hidden="true">' +
		    /* 地台阴影 */
		    '<ellipse class="mc-shadow" cx="60" cy="110" rx="34" ry="6"/>' +
		    /* 笼子底座 */
		    '<rect class="mc-base" x="18" y="98" width="84" height="8" rx="3"/>' +
		    /* 猫(一个 group, 跳出时整体位移) */
		    '<g class="mc-cat">' +
		      /* 身体(露出一点) */
		      '<path class="mc-body" d="M42 104 Q60 86 78 104 Z"/>' +
		      /* 头 */
		      '<g class="mc-head">' +
		        '<path class="mc-ear" d="M40 60 L46 44 L54 58 Z"/>' +
		        '<path class="mc-ear" d="M80 60 L74 44 L66 58 Z"/>' +
		        '<circle class="mc-face" cx="60" cy="68" r="22"/>' +
		        /* 腮红 */
		        '<ellipse class="mc-blush" cx="47" cy="72" rx="4" ry="2.6"/>' +
		        '<ellipse class="mc-blush" cx="73" cy="72" rx="4" ry="2.6"/>' +
		        /* 眼睛: 睁眼 + 眯眼两套, 撸猫时切换 */
		        '<g class="mc-eyes-open">' +
		          '<circle class="mc-eye" cx="52" cy="66" r="3.4"/>' +
		          '<circle class="mc-eye" cx="68" cy="66" r="3.4"/>' +
		          '<circle class="mc-eye-hl" cx="53.2" cy="64.8" r="1.1"/>' +
		          '<circle class="mc-eye-hl" cx="69.2" cy="64.8" r="1.1"/>' +
		        '</g>' +
		        '<g class="mc-eyes-happy">' +
		          '<path class="mc-eye-line" d="M48 66 Q52 62 56 66"/>' +
		          '<path class="mc-eye-line" d="M64 66 Q68 62 72 66"/>' +
		        '</g>' +
		        /* 鼻+嘴 */
		        '<path class="mc-nose" d="M58 73 L62 73 L60 75 Z"/>' +
		        '<path class="mc-mouth" d="M60 75 Q56 79 52 76 M60 75 Q64 79 68 76"/>' +
		        /* 胡须 */
		        '<path class="mc-whisker" d="M38 69 L28 67 M38 73 L28 74"/>' +
		        '<path class="mc-whisker" d="M82 69 L92 67 M82 73 L92 74"/>' +
		      '</g>' +
		    '</g>' +
		    /* 前栏杆(把猫关住) */
		    '<g class="mc-cage">' +
		      '<rect class="mc-top" x="16" y="34" width="88" height="7" rx="3"/>' +
		      '<path class="mc-bar" d="M30 40 L30 98"/>' +
		      '<path class="mc-bar" d="M45 40 L45 98"/>' +
		      '<path class="mc-bar" d="M60 40 L60 98"/>' +
		      '<path class="mc-bar" d="M75 40 L75 98"/>' +
		      '<path class="mc-bar" d="M90 40 L90 98"/>' +
		    '</g>' +
		    /* 撸猫时冒的爱心 */
		    '<g class="mc-hearts">' +
		      '<path class="mc-heart" d="M60 30 q-4 -5 -8 -1 q-3 3 0 6 l8 7 l8 -7 q3 -3 0 -6 q-4 -4 -8 1 z"/>' +
		    '</g>' +
		  '</svg>' +
		'</div>';
	}

	function CAT_STYLE() {
		return [
			/* 浮层: 定位到点击点(left/top 由 JS 按手指位置设), 不挡内容(pointer-events:none),
			 * 跟弹窗一样高 z-index。出现/消失用 scale+位移, 以浮层中心为原点。 */
			'#mwrt-cat{position:fixed;left:0;top:0;transform-origin:50% 70%;z-index:2147483645;',
			'pointer-events:none;--esc:0;filter:drop-shadow(0 12px 22px rgba(0,0,0,.28))}',
			'#mwrt-cat.mc-off{opacity:0;transform:translateY(16px) scale(.5);',
			'transition:opacity .3s ease,transform .3s cubic-bezier(.2,.9,.3,1.2)}',
			'#mwrt-cat.mc-on{opacity:1;transform:translateY(0) scale(1);',
			'transition:opacity .28s ease,transform .38s cubic-bezier(.2,.9,.3,1.3)}',
			'#mwrt-cat .mc-stage{width:120px;height:120px}',
			'#mwrt-cat .mc-svg{display:block;overflow:visible}',
			/* 配色: 二次元奶白猫 + 橘黄耳朵 / 蓝灰笼子 */
			'#mwrt-cat .mc-shadow{fill:rgba(0,0,0,.18)}',
			'#mwrt-cat .mc-base{fill:#5b6475}',
			'#mwrt-cat .mc-body{fill:#fff2e0;stroke:#e9d6bd;stroke-width:1.5}',
			'#mwrt-cat .mc-face{fill:#fff6ea;stroke:#e9d6bd;stroke-width:1.5}',
			'#mwrt-cat .mc-ear{fill:#ffb870;stroke:#e9975a;stroke-width:1.2;stroke-linejoin:round}',
			'#mwrt-cat .mc-blush{fill:#ffc2cf}',
			'#mwrt-cat .mc-eye{fill:#3a3340}',
			'#mwrt-cat .mc-eye-hl{fill:#fff}',
			'#mwrt-cat .mc-eye-line{fill:none;stroke:#3a3340;stroke-width:2.4;stroke-linecap:round}',
			'#mwrt-cat .mc-nose{fill:#ff9db0}',
			'#mwrt-cat .mc-mouth{fill:none;stroke:#b88b63;stroke-width:1.6;stroke-linecap:round}',
			'#mwrt-cat .mc-whisker{stroke:#d9c3a6;stroke-width:1.3;stroke-linecap:round}',
			/* 眼睛两套默认: 睁眼显示、眯眼藏 */
			'#mwrt-cat .mc-eyes-happy{opacity:0}',
			/* 笼子 */
			'#mwrt-cat .mc-top{fill:#6b7488}',
			'#mwrt-cat .mc-bar{fill:none;stroke:#7c8698;stroke-width:4;stroke-linecap:round}',
			''
		].join('') + CAT_STYLE_ANIM();
	}

	function CAT_STYLE_ANIM() {
		return [
			/* 挣脱进度 --esc(0~1): 猫头顶着栏杆往上拱, 中间几根栏杆被撑弯(scaleY 压短 + 外移),
			 * 顶梁随之被顶起翘一点。都用 calc 按 --esc 插值, 不用 keyframes。 */
			'#mwrt-cat .mc-cat{transform:translateY(calc(var(--esc) * -10px));transition:transform .35s cubic-bezier(.2,.9,.3,1.3)}',
			'#mwrt-cat .mc-cage{transition:transform .35s ease}',
			/* 中间三根(45/60/75)受力最大: 用 nth 分别给不同弯曲量 */
			'#mwrt-cat .mc-bar{transition:transform .35s cubic-bezier(.3,.8,.3,1.1),stroke .3s}',
			'#mwrt-cat .mc-bar:nth-of-type(2){transform:translateX(calc(var(--esc) * -4px)) scaleY(calc(1 - var(--esc)*.12))}',
			'#mwrt-cat .mc-bar:nth-of-type(3){transform:translateX(calc(var(--esc) * -7px)) rotate(calc(var(--esc) * -7deg));transform-origin:50px 98px}',
			'#mwrt-cat .mc-bar:nth-of-type(4){transform:scaleY(calc(1 - var(--esc)*.22)) translateX(calc(var(--esc) * 1px));transform-origin:60px 98px}',
			'#mwrt-cat .mc-bar:nth-of-type(5){transform:translateX(calc(var(--esc) * 7px)) rotate(calc(var(--esc) * 7deg));transform-origin:70px 98px}',
			'#mwrt-cat .mc-bar:nth-of-type(6){transform:translateX(calc(var(--esc) * 4px)) scaleY(calc(1 - var(--esc)*.12))}',
			'#mwrt-cat .mc-top{transition:transform .35s ease;transform:translateY(calc(var(--esc) * -3px)) rotate(calc(var(--esc) * -2deg));transform-origin:60px 37px}',
			/* 挣脱时栏杆泛红一点表示"要崩了" */
			'#mwrt-cat.mc-on .mc-bar{stroke:color-mix(in srgb,#7c8698,#ff7a7a calc(var(--esc)*55%))}',
			/* 撸猫: 眯眼笑 + 轻微点头 + 冒一颗爱心。每次点击重放(先移除再加类触发) */
			'#mwrt-cat.mc-pet .mc-eyes-open{opacity:0}',
			'#mwrt-cat.mc-pet .mc-eyes-happy{opacity:1}',
			'#mwrt-cat.mc-pet .mc-head{animation:mc-nod .45s ease}',
			'#mwrt-cat.mc-pet .mc-hearts{animation:mc-heart .9s ease}',
			'@keyframes mc-nod{0%{transform:translateY(0) rotate(0)}40%{transform:translateY(2px) rotate(-3deg)}100%{transform:translateY(0) rotate(0)}}',
			'#mwrt-cat .mc-head{transform-box:fill-box;transform-origin:center}',
			'#mwrt-cat .mc-hearts{opacity:0}',
			'@keyframes mc-heart{0%{opacity:0;transform:translateY(6px) scale(.4)}25%{opacity:1}100%{opacity:0;transform:translateY(-26px) scale(1)}}',
			'#mwrt-cat .mc-heart{fill:#ff6f91}',
			/* 跳出: 猫整体蹿出屏外上方 + 笼子炸开(栏杆向两侧倒) */
			'#mwrt-cat.mc-escape .mc-cat{animation:mc-jump .7s cubic-bezier(.2,.7,.2,1) forwards}',
			'@keyframes mc-jump{0%{transform:translateY(-10px)}35%{transform:translateY(-34px) scale(1.06)}100%{transform:translateY(-130px) scale(.9);opacity:0}}',
			'#mwrt-cat.mc-escape .mc-cage{animation:mc-burst .5s ease forwards}',
			'@keyframes mc-burst{100%{transform:scale(1.15);opacity:0}}',
			'#mwrt-cat.mc-escape .mc-bar:nth-of-type(3){transform:rotate(-40deg) translateY(10px);transform-origin:50px 98px;transition:transform .5s ease}',
			'#mwrt-cat.mc-escape .mc-bar:nth-of-type(5){transform:rotate(40deg) translateY(10px);transform-origin:70px 98px;transition:transform .5s ease}',
			/* 动画降级: 系统开了"减少动态效果"时, 还是出猫/弹框, 只是不播花哨过渡 */
			'@media (prefers-reduced-motion:reduce){#mwrt-cat .mc-cat,#mwrt-cat .mc-bar,#mwrt-cat .mc-top{transition:none}',
			'#mwrt-cat.mc-pet .mc-head,#mwrt-cat.mc-pet .mc-hearts,#mwrt-cat.mc-escape .mc-cat,#mwrt-cat.mc-escape .mc-cage{animation:none}}',
			/* 点击反馈: 猫爪肉球在点击点弹一下淡出。以自身中心对齐光标。 */
			'.mc-paw{position:fixed;z-index:2147483645;pointer-events:none;width:40px;height:40px;margin:-20px 0 0 -20px;',
			'animation:mc-paw .7s cubic-bezier(.2,.8,.3,1) forwards;transform-origin:50% 60%}',
			'.mc-paw .mc-pad{fill:#ff8fab;opacity:.95}',
			'@keyframes mc-paw{0%{opacity:0;transform:scale(.3) rotate(-12deg)}22%{opacity:1;transform:scale(1.12) rotate(-6deg)}',
			'45%{transform:scale(.96) rotate(-6deg)}100%{opacity:0;transform:scale(1) rotate(-6deg) translateY(-10px)}}',
			/* 逗猫棒光标: 悬停版本卡片时, 鼠标变成一根带绒球的逗猫棒(内联 SVG data-URI)。
			 * 热点设在绒球端(2,2)。触屏没有光标, 不影响。整条 .fw-banner 都给, 但 .fw-act
			 * (检查更新/NEW)是正经按钮, 覆盖回普通 pointer。 */
			'.fw-banner{cursor:url("data:image/svg+xml,' + TEASER_CURSOR() + '") 2 2,pointer}',
			'.fw-banner .fw-act,.fw-banner .fw-act *{cursor:pointer}',
			''
		].join('');
	}

	/* 逗猫棒光标 SVG(URL 编码后的 data-URI 内容): 斜杆 + 顶端绒球 + 挂一颗铃铛。
	 * 用 %23 代替 #, 空格用 %20, 保证能塞进 CSS url()。 */
	function TEASER_CURSOR() {
		var svg = '<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">' +
			'<line x1="6" y1="6" x2="26" y2="26" stroke="%23b5835a" stroke-width="2.4" stroke-linecap="round"/>' +
			'<circle cx="6" cy="6" r="5" fill="%23ff8fab"/>' +
			'<circle cx="6" cy="6" r="5" fill="none" stroke="%23ff6f91" stroke-width="1"/>' +
			'<circle cx="26" cy="27" r="2.2" fill="%23ffd166" stroke="%23e0a93b" stroke-width="0.8"/>' +
			'</svg>';
		return svg.replace(/</g, '%3C').replace(/>/g, '%3E').replace(/"/g, '%22').replace(/ /g, '%20');
	}
}());
