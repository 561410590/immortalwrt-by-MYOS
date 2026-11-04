/*
 * mwrt-aurora-boot.js —— aurora 皮肤的启动脚本
 *
 * 作为 <body> 开头的同步脚本执行: 此时 <head> 里的样式表已经解析进 CSSOM,
 * 但文档还没首次绘制, 所以下面的停用不会闪一下旧配色。
 *
 * 干三件事:
 *   1. 锁定浅色 —— aurora 靠 html[data-darkmode] 切换深浅, 这里固定 false。
 *      想恢复"跟随系统"就把 FORCE_LIGHT 改成 false。
 *   2. 停用 argon 的两个本地定制表:
 *        argon-menu-custom.css    深色霓虹侧边栏 + 深色顶栏
 *        argon-global-custom.css  #388087 青绿控件皮
 *      这两份整份都是 !important, 留着就得逐条对着压; 直接标成
 *      media="not all" 更干净, 且不改硬盘上的任何文件。
 *   3. 停用 argon 内联的 dark.css —— 它藏在 <head> 的 <style> 里,
 *      包在 @media (prefers-color-scheme: dark) 中。系统处于深色时它会跟
 *      aurora 的浅色板打架, 而我们已经锁定浅色, 所以一并停掉。
 *
 * 全部包在 try/catch 里: 出错就退回 argon 原样, 页面功能不受影响。
 */
(function () {
	'use strict';

	var FORCE_LIGHT = true;

	/* 1. 深浅模式 */
	try {
		var html = document.documentElement;
		if (FORCE_LIGHT) {
			html.setAttribute('data-darkmode', 'false');
		} else {
			var mq = window.matchMedia('(prefers-color-scheme: dark)');
			var apply = function () {
				html.setAttribute('data-darkmode', String(mq.matches));
			};
			apply();
			if (mq.addEventListener) mq.addEventListener('change', apply);
			else if (mq.addListener) mq.addListener(apply);
		}
	} catch (e) { /* 老浏览器: 保持浅色 */ }

	/* 把一个样式表节点彻底静音 */
	/* bian kuang dang wei(hou tai si zhou): mwrtst san duan kongjian xie
	   localStorage(mwrt-border), mei ye gei body/.main she neilian style
	   (padding/border-radius important) —— CSS class ya bu dong yuansheng 5px,
	   zhiyou neilian ke kao. round=5px/14px mini=3px/7px none=0/0. */
	var MWRT_BD={round:["5px","14px"],mini:["3px","7px"],none:["0px","0px"]};
	function applyBorderPref(){
		try{
			var v=null;try{v=window.localStorage.getItem("mwrt-border");}catch(e){return;}
			if(!MWRT_BD[v])return;
			var pad=MWRT_BD[v][0],rad=MWRT_BD[v][1];
			/* s79: di bian bu neng zhi jie xie pad —— neilian important hui ba yidongduan
			   gei di bu dao hang lan yuliu de padding-bottom(52px+safe-area) ya diao,
			   ye mian zui xia mian de an niu bei dao hang lan dang zhu. di bian zou
			   --mwrt-nav-reserve(chrome.css s79 zhi zai you di lan shi ding yi), PC hui luo pad. */
			if(document.body){
				document.body.style.setProperty("padding",pad,"important");
				document.body.style.setProperty("padding-bottom","var(--mwrt-nav-reserve, "+pad+")","important");
			}
			var mn=document.querySelector(".main");
			if(mn)mn.style.setProperty("border-radius",rad,"important");
		}catch(e){}
	}
	function watchBorderPref(){
		try{
			applyBorderPref();
			if(document.readyState==="loading"){document.addEventListener("DOMContentLoaded",applyBorderPref);}
			setTimeout(applyBorderPref,0);setTimeout(applyBorderPref,300);
		}catch(e){}
	}
	try{watchBorderPref();}catch(e){}

	function mute(node) {
		try {
			node.setAttribute('media', 'not all');
			node.disabled = true;
			node.setAttribute('data-mwrt-muted', '1');
		} catch (e) { /* ignore */ }
	}

	/* 2. 两个定制表 */
	try {
		var DROP = ['argon-menu-custom.css', 'argon-global-custom.css'];
		var links = document.querySelectorAll('link[rel~="stylesheet"][href]');
		for (var i = 0; i < links.length; i++) {
			var href = links[i].getAttribute('href') || '';
			for (var j = 0; j < DROP.length; j++) {
				if (href.indexOf(DROP[j]) !== -1) { mute(links[i]); break; }
			}
		}
	} catch (e) { /* ignore */ }

	/* 3. 内联的 dark.css */
	if (FORCE_LIGHT) {
		try {
			var styles = document.querySelectorAll('head style');
			for (var k = 0; k < styles.length; k++) {
				var txt = styles[k].textContent || '';
				if (txt.indexOf('prefers-color-scheme') !== -1 &&
				    txt.indexOf('.main .main-left') !== -1) {
					mute(styles[k]);
				}
			}
		} catch (e) { /* ignore */ }
	}

	/* ── 4. 页脚收进"关于本机"浮层 ─────────────────────────────────────────
	 * 原样是一整行版本信息横在内容最底下。之前试过 sticky/fixed 贴右下角,
	 * 但它一直压着内容, 所以改成: 右下角一枚小图标, 默认不展开, 点一下弹出
	 * 浮层, 再点图标 / 点关闭 / 点浮层外面 / 按 Esc 都收起。
	 *
	 * 这里只做三件改动, 展开收起和全部外观都交给
	 * mwrt-aurora-chrome.css §14(靠 body.mwrt-about-open 这个类切换):
	 *   1. 给 footer 插一条标题栏(标题 + 关闭按钮)。
	 *   2. 往 body 上挂那枚触发图标, 接好开合逻辑。
	 *   3. 最后才把原来"A / B / C / 长版本串"这一长行拆成每项一行的键值表
	 *      (模板里各项之间的 " / " 是裸文本节点, CSS 选不到, 只能在这儿拆)。
	 *      顺序是有意的: 重排最容易出岔子, 放最后并单独 try/catch, 失败也
	 *      不会连带吃掉已经建好的触发图标。
	 *
	 * 整段包 try/catch, 出错就保持原样的页脚, 页面功能不受影响。
	 * ──────────────────────────────────────────────────────────────────── */

	/* footer.htm 的 pollShowtime() 每几秒重写一次 #osv-switch-link 的
	   innerHTML, 内容是 '<a ...>NRadio-鲲鹏无限</a>/ ' —— 末尾那个 "/" 是
	   给一长行排版用的分隔符, 拆成表格后就成了多余的一撇, 而且 CSS 选不到
	   文本节点。所以挂个观察器, 每次被重写就把它抹掉。
	   抹完不再产生新的 childList 变化, 观察器自然停下, 不会自激。 */
	function stripTrailingSlash(sp) {
		var n = sp.lastChild;
		while (n && n.nodeType === 3) {
			var t = n.nodeValue.replace(/\s*\/\s*$/, '');
			if (t.replace(/\s+/g, '')) { n.nodeValue = t; return; }
			sp.removeChild(n);
			n = sp.lastChild;
		}
	}

	/* 单独一个函数, 免得在循环里建闭包捕获到会变的循环变量 */
	function watchOsv(sp) {
		stripTrailingSlash(sp);
		try {
			new MutationObserver(function () {
				stripTrailingSlash(sp);
			}).observe(sp, { childList: true });
		} catch (e) { /* 老浏览器: 末尾多一撇, 不影响功能 */ }
	}

	/* 一行"标签 + 值"。mono=true 时值走等宽字(修订号/构建串那种)。 */
	function mkRow(label, value, mono) {
		var row = document.createElement('div');
		row.className = 'mwrt-ft-row';
		if (label) {
			var k = document.createElement('span');
			k.className = 'mwrt-ft-k';
			k.textContent = label;
			row.appendChild(k);
		}
		var v = document.createElement('span');
		v.className = 'mwrt-ft-v' + (mono ? ' mwrt-ft-mono' : '');
		if (typeof value === 'string') v.textContent = value;
		else if (value) v.appendChild(value);
		row.appendChild(v);
		return row;
	}

	function mkGroup() {
		var g = document.createElement('div');
		g.className = 'mwrt-ft-grp';
		return g;
	}

	/* ubus 的 builddate 是 unix 秒, 原样显示没人看得懂 */
	function fmtDate(sec) {
		var n = parseInt(sec, 10);
		if (!n) return '';
		var d = new Date(n * 1000);
		if (isNaN(d.getTime())) return '';
		function p(x) { return (x < 10 ? '0' : '') + x; }
		return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate());
	}

	/* ── 设备信息: 型号 / 平台 / 处理器 / 内核 / … ───────────────────────────
	   数据来自 ubus 的 `system board`, 走 LuCI 自己的 /ubus/ 代理。会话 id 和
	   代理路径都在页面内联的 `L = new LuCI({...})` 里(env.sessionid /
	   env.ubuspath), 所以不需要另外拿 token。

	   用 XMLHttpRequest 而不是 fetch: 这个文件通篇 ES5, 保持一致, 也少一层
	   Promise 的兼容假设。

	   异步拿到再往浮层顶部插 —— 拿不到(没有 L、会话过期、ACL 不允许)就什么都
	   不插, 浮层退回"只有原来那几行"的样子, 不报错、不空占位。 */
	function loadBoard(box, anchor) {
		var env = (window.L && window.L.env) || {};
		if (!env.sessionid) return;
		var xhr = new XMLHttpRequest();
		xhr.open('POST', env.ubuspath || '/ubus/', true);
		xhr.setRequestHeader('Content-Type', 'application/json');
		xhr.onreadystatechange = function () {
			if (xhr.readyState !== 4) return;
			var b;
			try {
				var r = JSON.parse(xhr.responseText);
				/* ubus 的返回是 [状态码, 数据], 状态码 0 才是成功 */
				if (!r.result || r.result[0] !== 0) return;
				b = r.result[1];
			} catch (e) { return; }
			if (!b) return;
			try { renderBoard(box, anchor, b); } catch (e) { /* ignore */ }
		};
		xhr.send(JSON.stringify({
			jsonrpc: '2.0', id: 1, method: 'call',
			params: [env.sessionid, 'system', 'board', {}]
		}));
	}

	function renderBoard(box, anchor, b) {
		var rel = b.release || {};
		var frag = document.createDocumentFragment();

		/* 主角一行: 型号做标题, 下面一行小字放构建目标。
		   用 model("NRadio-C2000Ultra")而不是 board_name
		   ("NRadio-C2000MAX-Ultra") —— board_name 是 DTS 里的编译标识,
		   model 才是对外的机型名。 */
		var name = b.model || b.board_name || b.hostname;
		if (name) {
			var hero = document.createElement('div');
			hero.className = 'mwrt-ft-hero';
			var ic = document.createElement('span');
			ic.className = 'mwrt-ft-hero-ic';
			var tx = document.createElement('div');
			tx.className = 'mwrt-ft-hero-tx';
			var nm = document.createElement('div');
			nm.className = 'mwrt-ft-hero-name';
			nm.textContent = name;
			tx.appendChild(nm);
			var sub = [rel.target, rel.distribution].filter(Boolean).join(' · ');
			if (sub) {
				var sb = document.createElement('div');
				sb.className = 'mwrt-ft-hero-sub';
				sb.textContent = sub;
				tx.appendChild(sb);
			}
			hero.appendChild(ic);
			hero.appendChild(tx);
			frag.appendChild(hero);
		}

		var g = mkGroup();
		var SPEC = [
			['主机名', b.hostname],
			['处理器', b.system],
			['内核', b.kernel, true],
			['根文件', b.rootfs_type],
			['构建', fmtDate(rel.builddate), true]
		];
		for (var i = 0; i < SPEC.length; i++) {
			if (SPEC[i][1]) g.appendChild(mkRow(SPEC[i][0], SPEC[i][1], SPEC[i][2]));
		}
		if (g.firstChild) frag.appendChild(g);

		box.insertBefore(frag, anchor || box.firstChild);
	}

	/* 把页脚那一长行重排成键值表。单独一个函数、由 setupAbout 最后调用并
	   单独包 try/catch —— 重排是三步里最容易因为模板被改而出岔子的一步,
	   不能让它的失败连带吃掉前面已经建好的触发图标(那就彻底点不开了)。 */
	function buildRows(box) {
		/* 一长行 → 键值表。
		   先把子节点收成"条目": 元素节点各算一条, 文本节点去掉首尾的 "/"
		   之后还有内容的(也就是末尾那段版本串)算一条, 只剩分隔符的丢掉。 */
		var items = [];
		var kids = Array.prototype.slice.call(box.childNodes);
		var modemenu = null, i;
		for (i = 0; i < kids.length; i++) {
			var n = kids[i];
			if (n.nodeType === 3) {
				var t = n.nodeValue.replace(/\s+/g, ' ')
				                   .replace(/^\s*\/\s*/, '')
				                   .replace(/\s*\/\s*$/, '')
				                   .trim();
				if (t) items.push({ text: t });
			} else if (n.nodeType === 1) {
				/* #modemenu 是 LuCI 的模式面包屑, 默认 display:none,
				   但可能被别处填内容, 原样留在末尾, 不动它 */
				if (n.id === 'modemenu') modemenu = n;
				else items.push({ node: n });
			}
		}

		/* 标签按顺序贴。条目数不是预期的 4 条(模板被改过)就不贴标签,
		   只保留"每项一行", 免得标签和内容对错位。 */
		var LABELS = ['系统', '面板', '主题', '固件'];
		var labeled = (items.length === LABELS.length);

		/* 分两组: 前三条是短值, 并成一组; 最后那条固件构建串又长又是等宽字,
		   单独一组、标签压在值上面独占一行。
		   之前靠 CSS 的 :last-of-type 认这一条 —— 但 #modemenu 原样留在末尾,
		   选择器得绕着它走, 很脆。改成显式分组, CSS 只看类名。 */
		var frag = document.createDocumentFragment();
		var g = mkGroup();
		for (i = 0; i < items.length; i++) {
			var last = (i === items.length - 1);
			var val;
			if (items[i].node) {
				/* 节点原样搬过来 —— appendChild 会把它从 box 里摘走,
				   所以下面清空 box 时不会连带删掉 */
				val = items[i].node;
				if (val.id === 'osv-switch-link') watchOsv(val);
			} else {
				val = items[i].text;
			}
			var row = mkRow(labeled ? LABELS[i] : '', val, !items[i].node);
			if (labeled && last) {
				var wide = mkGroup();
				wide.className += ' mwrt-ft-grp-wide';
				wide.appendChild(row);
				frag.appendChild(g);
				frag.appendChild(wide);
				g = null;
			} else {
				g.appendChild(row);
			}
		}
		if (g && g.firstChild) frag.appendChild(g);

		while (box.firstChild) box.removeChild(box.firstChild);
		box.appendChild(frag);
		if (modemenu) box.appendChild(modemenu);

		/* 设备信息插在最前面。放在重排之后要: 这时 box 的第一个子节点已经是
		   我们自己建的组, 插在它前面顺序才对。 */
		loadBoard(box, box.firstChild);
	}

	function setupAbout() {
		var ft = document.querySelector('footer.mobile-hide');
		if (!ft || ft.getAttribute('data-mwrt-about')) return;
		var box = ft.firstElementChild;
		if (!box || box.tagName !== 'DIV') return;
		ft.setAttribute('data-mwrt-about', '1');

		/* 标题栏 */
		var bar = document.createElement('div');
		bar.className = 'mwrt-ft-bar';
		var title = document.createElement('span');
		title.className = 'mwrt-ft-title';
		title.textContent = '关于本机';
		var close = document.createElement('button');
		close.type = 'button';
		close.className = 'mwrt-ft-close';
		close.setAttribute('aria-label', '关闭');
		bar.appendChild(title);
		bar.appendChild(close);
		ft.insertBefore(bar, box);

		/* 触发图标 */
		var btn = document.createElement('button');
		btn.type = 'button';
		btn.id = 'mwrt-about-btn';
		btn.setAttribute('aria-label', '关于本机');
		btn.setAttribute('aria-expanded', 'false');
		document.body.appendChild(btn);

		function toggle(on) {
			if (on) document.body.classList.add('mwrt-about-open');
			else document.body.classList.remove('mwrt-about-open');
			btn.setAttribute('aria-expanded', on ? 'true' : 'false');
		}
		function isOpen() {
			return document.body.classList.contains('mwrt-about-open');
		}

		btn.addEventListener('click', function (e) {
			e.stopPropagation();
			toggle(!isOpen());
		});
		close.addEventListener('click', function (e) {
			e.stopPropagation();
			toggle(false);
		});
		/* 点浮层外面收起。挂在 document 上, 所以上面两个处理器都要
		   stopPropagation, 否则点完立刻又被这里收/开一次。 */
		document.addEventListener('click', function (e) {
			if (isOpen() && !ft.contains(e.target)) toggle(false);
		});
		document.addEventListener('keydown', function (e) {
			if (isOpen() && (e.key === 'Escape' || e.keyCode === 27)) toggle(false);
		});

		/* 重排放最后, 且单独兜一层: 到这里图标和开合逻辑都已经就绪,
		   重排万一出岔子, 最多是页脚内容还是原来那一长行, 仍然点得开。 */
		try { buildRows(box); } catch (e) { /* ignore */ }
		var pref=document.createElement('div');
		pref.className='mwrt-bd-seg';
		pref.setAttribute('aria-label','PC 模式边框');
		[['round','圆角'],['mini','微边圆角'],['none','无圆角']].forEach(function(item){
			var choice=document.createElement('button');
			choice.type='button'; choice.textContent=item[1];
			choice.setAttribute('data-bd',item[0]);
			choice.addEventListener('click',function(){
				try{localStorage.setItem('mwrt-border',item[0]);}catch(e){}
				applyBorderPref();
				Array.prototype.forEach.call(pref.children,function(b){b.classList.toggle('on',b===choice);});
			});
			var current='round';try{current=localStorage.getItem('mwrt-border')||'round';}catch(e){}
			choice.classList.toggle('on',current===item[0]);pref.appendChild(choice);
		});
		box.appendChild(pref);
	}

	/* ── 5. 标签栏横向滚动: 原生滚动条 → 箭头 + 边缘渐变 ───────────────────
	 * passwall 这类页面标签一多, argon 让 ul.tabs 横向滚动并露出滚动条。
	 * 这里给溢出的标签栏外面包一层 .mwrt-tabs-wrap, 两端各插一枚圆形箭头,
	 * 滚动时按"还能往哪滚"点亮对应箭头 + 渐变遮罩(样式在 chrome.css §s66)。
	 * 不溢出的标签栏一律不动。整段 try/catch, 出错退回原生滚动。
	 * ──────────────────────────────────────────────────────────────────── */
	function mkArrow(dir) {
		var b = document.createElement('button');
		b.type = 'button';
		b.className = 'mwrt-tabs-arrow mwrt-tabs-' + dir;
		b.setAttribute('aria-label', dir === 'prev' ? '向左' : '向右');
		b.tabIndex = -1;
		return b;
	}

	function enhanceTabs(ul) {
		if (!ul || ul.getAttribute('data-mwrt-tabs')) return;
		/* 没溢出就不折腾 —— 留 2px 容差, 免得整数像素抖动误判 */
		if (ul.scrollWidth <= ul.clientWidth + 2) return;
		ul.setAttribute('data-mwrt-tabs', '1');

		var wrap = document.createElement('div');
		wrap.className = 'mwrt-tabs-wrap';
		ul.parentNode.insertBefore(wrap, ul);
		wrap.appendChild(ul);

		var prev = mkArrow('prev');
		var next = mkArrow('next');
		wrap.appendChild(prev);
		wrap.appendChild(next);

		/* 按当前滚动位置点亮箭头和渐变: 左边还有 → 亮左, 右边还有 → 亮右 */
		function update() {
			var max = ul.scrollWidth - ul.clientWidth;
			var x = ul.scrollLeft;
			var left = x > 2;
			var right = x < max - 2;
			if (left) { prev.className = 'mwrt-tabs-arrow mwrt-tabs-prev show'; }
			else { prev.className = 'mwrt-tabs-arrow mwrt-tabs-prev'; }
			if (right) { next.className = 'mwrt-tabs-arrow mwrt-tabs-next show'; }
			else { next.className = 'mwrt-tabs-arrow mwrt-tabs-next'; }
			if (left) { if (wrap.className.indexOf('has-left') < 0) wrap.className += ' has-left'; }
			else { wrap.className = wrap.className.replace(/\s*has-left/g, ''); }
			if (right) { if (wrap.className.indexOf('has-right') < 0) wrap.className += ' has-right'; }
			else { wrap.className = wrap.className.replace(/\s*has-right/g, ''); }
		}

		/* 点箭头滚一屏的七成, 留点重叠让人知道接上了 */
		function step(dir) {
			var d = Math.max(80, Math.round(ul.clientWidth * 0.7));
			ul.scrollLeft += dir * d;
		}
		prev.addEventListener('click', function (e) { e.preventDefault(); step(-1); });
		next.addEventListener('click', function (e) { e.preventDefault(); step(1); });

		/* 竖滚轮也能横向翻, 鼠标用户不必找箭头 */
		ul.addEventListener('wheel', function (e) {
			if (e.deltaY === 0) return;
			var max = ul.scrollWidth - ul.clientWidth;
			if ((e.deltaY < 0 && ul.scrollLeft > 0) ||
			    (e.deltaY > 0 && ul.scrollLeft < max)) {
				ul.scrollLeft += e.deltaY;
				e.preventDefault();
			}
		}, { passive: false });

		ul.addEventListener('scroll', update);
		try {
			window.addEventListener('resize', function () {
				/* 窗口变宽到不再溢出就把增强撤掉, 免得留着空箭头 */
				update();
			});
		} catch (e) { /* ignore */ }
		update();
	}

	function scanTabs(root) {
		try {
			var uls = (root || document).querySelectorAll('ul.tabs');
			for (var i = 0; i < uls.length; i++) enhanceTabs(uls[i]);
		} catch (e) { /* ignore */ }
	}

	function setupTabs() {
		scanTabs(document);
		/* LuCI 的 JS 视图是异步渲染的, 标签栏可能在 DOMContentLoaded 之后才进
		   DOM; 挂个观察器, 新出现的 ul.tabs 也接管。 */
		try {
			new MutationObserver(function (muts) {
				for (var i = 0; i < muts.length; i++) {
					var added = muts[i].addedNodes;
					for (var j = 0; j < added.length; j++) {
						var n = added[j];
						if (n.nodeType !== 1) continue;
						if (n.tagName === 'UL' && /\btabs\b/.test(n.className)) enhanceTabs(n);
						else scanTabs(n);
					}
				}
			}).observe(document.body, { childList: true, subtree: true });
		} catch (e) { /* ignore */ }
	}

	/* ── 6. 侧栏左下角固件版本块 ──────────────────────────────────────────────
	 * 侧栏(.main-left)底部原本是大片空白。这里插一块扁平的版本信息:
	 * 发行版徽标(Mwrt) + 版本号主串 + 构建日期。数据走 LuCI 的 /ubus/ 代理
	 * (和 loadBoard 同一条路), 拿 release.version / distribution / builddate。
	 * CSS 在 chrome.css §s68(.mwrt-sb-ver), 靠 margin-top:auto 推到最底。
	 * 整段 try/catch, 拿不到就不插, 侧栏保持原样。
	 * ──────────────────────────────────────────────────────────────────────── */
	function pickVer(ver) {
		/* release.version 形如 "@Manper-5.1.1150-24 by Padavanonly's ..."
		   只取版本号核心(数字点串), 太长的编译说明不要。 */
		if (!ver) return '';
		var m = ver.match(/(\d+(?:\.\d+)+(?:-\d+)?)/);
		return m ? m[1] : ver.split(/\s+/)[0].replace(/^@/, '');
	}

	function renderSbVer(ml, b) {
		var rel = b.release || {};
		var ver = pickVer(rel.version);

		var box = document.createElement('div');
		box.className = 'mwrt-sb-ver';

		if (ver) {
			var top = document.createElement('div');
			top.className = 'msv-top';
			var dist = document.createElement('span');
			dist.className = 'msv-dist';
			dist.textContent = 'MYOS';
			var vv = document.createElement('span');
			vv.className = 'msv-ver';
			vv.textContent = ver;
			top.appendChild(dist);
			top.appendChild(vv);
			box.appendChild(top);
		}

		var links = document.createElement('div');
		links.className = 'mwrt-sb-links';
		[
			[ '打赏支持', 'https://nradio.mayebano.shop/' ],
			[ '冠名支持', 'https://laoshenshiwo.cn/' ]
		].forEach(function(item) {
			var link = document.createElement('a');
			link.textContent = item[0]; link.href = item[1];
			link.target = '_blank'; link.rel = 'noopener noreferrer';
			links.appendChild(link);
		});
		box.appendChild(links);

		ml.appendChild(box);
	}

	function setupSidebarVer() {
		var ml = document.querySelector('.main .main-left');
		if (!ml || ml.getAttribute('data-mwrt-ver')) return;
		var env = (window.L && window.L.env) || {};
		if (!env.sessionid) return;
		ml.setAttribute('data-mwrt-ver', '1');

		var xhr = new XMLHttpRequest();
		xhr.open('POST', env.ubuspath || '/ubus/', true);
		xhr.setRequestHeader('Content-Type', 'application/json');
		xhr.onreadystatechange = function () {
			if (xhr.readyState !== 4) return;
			var b;
			try {
				var r = JSON.parse(xhr.responseText);
				if (!r.result || r.result[0] !== 0) return;
				b = r.result[1];
			} catch (e) { return; }
			if (!b) return;
			try { renderSbVer(ml, b); } catch (e) { /* ignore */ }
		};
		xhr.send(JSON.stringify({
			jsonrpc: '2.0', id: 1, method: 'call',
			params: [env.sessionid, 'system', 'board', {}]
		}));
	}

	try {
		if (document.readyState === 'loading') {
			document.addEventListener('DOMContentLoaded', function () {
				try { setupAbout(); } catch (e) { /* ignore */ }
				try { setupTabs(); } catch (e) { /* ignore */ }
				try { setupSidebarVer(); } catch (e) { /* ignore */ }
			});
		} else {
			setupAbout();
			try { setupTabs(); } catch (e) { /* ignore */ }
			try { setupSidebarVer(); } catch (e) { /* ignore */ }
		}
	} catch (e) { /* ignore */ }
})();
