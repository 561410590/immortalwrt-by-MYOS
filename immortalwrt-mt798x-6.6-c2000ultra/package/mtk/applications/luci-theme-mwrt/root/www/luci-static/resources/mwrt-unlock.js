/* MWRT-only local unlock: mwrtmwrt opens the dialog; POST validates the local code. */
(function () {
	'use strict';

	if (window.__mwrtUnlock) return;
	window.__mwrtUnlock = true;

	var me = document.currentScript;
	var URL_ = (me && me.getAttribute('data-url')) || '';
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
		title = mk('div', 'mu-title', '显示代理插件');
		title.setAttribute('role', 'heading');
		title.setAttribute('aria-level', '2');
		title.id = 'mwrt-unlock-title';
		sub = mk('p', 'mu-sub', '输入解锁口令，验证通过后自动解锁插件菜单');
		sub.id = 'mwrt-unlock-sub';

		field = mk('div', 'mu-field');
		var lock = icon(mk('span', 'mu-lock'), 'lock');
		input = mk('input', 'mu-input');
		input.type = 'password';
		input.id = 'mwrt-unlock-input';
		input.autocomplete = 'off';
		input.spellcheck = false;
		input.placeholder = '请输入解锁口令';
		input.setAttribute('aria-label', '解锁口令');
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
		label = mk('span', null, '解锁');
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
		document.body.appendChild(overlay);
	}

	function reveal(on) {
		input.type = on ? 'text' : 'password';
		icon(eye, on ? 'eyeOff' : 'eye');
		eye.setAttribute('aria-label', on ? '隐藏口令' : '显示口令');
		eye.setAttribute('aria-pressed', on ? 'true' : 'false');
	}

	/* 焦点留在卡片里: Tab 走到头绕回来; Esc 关闭 */
	function trap(e) {
		if (e.key === 'Escape') { if (!busy && !done) { e.preventDefault(); hide(); } return; }
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
		overlay.classList.remove('mu-on');
		overlay.style.display = 'none';
		if (lastFocus && lastFocus.focus) try { lastFocus.focus(); } catch (e) {}
	}

	function setBusy(b) {
		busy = b;
		submit.disabled = b;
		input.readOnly = b;
		submit.classList.toggle('mu-busy', b);
		submit.setAttribute('aria-busy', b ? 'true' : 'false');
		label.textContent = b ? '正在验证…' : '解锁';
	}

	function succeed() {
		done = true;
		card.classList.add('mu-ok');
		icon(badge, 'check');
		title.textContent = '解锁成功';
		sub.textContent = '插件菜单已解锁';
		input.readOnly = true;
		submit.disabled = true;
		submit.classList.add('mu-busy');
		label.textContent = '正在刷新';
		say('解锁成功，正在刷新菜单…', 'good');
		/* 服务端菜单缓存后端已清; 浏览器这边 ui.menu 还把菜单树缓存在
		 * sessionStorage(luci-session-store), 不清的话刷新后还是旧菜单。 */
		try { if (window.L && L.ui && L.ui.menu && L.ui.menu.flushCache) L.ui.menu.flushCache(); } catch (e) {}
		try { sessionStorage.removeItem('luci-session-store'); } catch (e) {}
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
			if (res === 'no') say('口令不正确，请重试', 'err');
			else if (res === 'nonet') say('保存失败，请重试', 'err');
			else if (res === 'busy') say('尝试过于频繁，请一分钟后再试', 'err');
			else say('解锁失败（' + xhr.status + '），请稍后重试', 'err');
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
}());
