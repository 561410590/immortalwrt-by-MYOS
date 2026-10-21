#!/bin/sh
#
# install-luci-perf.sh —— 给 LuCI 后台减掉两笔每次点击都要付的开销
#
# 背景(实测,C2000MAX-Ultra / MT7987):
#   这台机器上 LuCI 是以 **CGI** 方式跑的 —— /www/cgi-bin/luci 是个
#   `#!/usr/bin/env ucode` 脚本,每个请求 fork 一个全新解释器。证据是三个
#   常驻 handler 全都不在:
#       uci 里 lua_prefix 指着 /usr/lib/lua/luci/sgi/uhttpd.lua  →  文件不存在
#       /usr/lib/uhttpd_lua.so    →  没装
#       /usr/lib/uhttpd_ucode.so  →  没装(只有 uhttpd_ubus.so)
#   拆开测:解释器启动 2ms + import luci.dispatcher 17ms + dispatch/渲染 60ms
#   = 每请求固定 79ms,与页面大小无关(85KB/48KB/84KB 三个页面 TTFB 都是 72ms)。
#   CPU 不是瓶颈:governor=performance 锁 2GHz、92% 空闲、负载 0.16。
#   那 79ms 要靠把 uhttpd-mod-ucode 编进固件才能治,不在本脚本范围。
#
# 本脚本只处理 head 里两笔不必要的开销,两项都不需要重编:
#
#   1) 翻译字典改走静态文件。原来 <head> 里挂着
#        /cgi-bin/luci/admin/translations/zh-cn   223KB,58ms,cache-control: no-cache
#      —— 走 CGI(所以 58ms),而且明确禁止浏览器缓存,于是**每次导航都重下 223KB**,
#      首屏还得等它。改成 mwrt-gen-translations 预渲染的静态文件后:0.7ms 送出,
#      第二次以后是 304 / 0 字节。字节与端点输出完全一致(md5 已核对)。
#      模板里带存在性判断,文件缺失自动回退原端点 —— 退回今天的行为,不会坏。
#
#   2) 去掉重复的 luci.js。argon 的 header.htm 是旧 Lua dispatcher 年代的产物,
#      那时它就是完整的 <head>。现在 luci-base 的 header.ut 先 include 它、之后
#      又自己发一次 luci.js(紧跟内联的 `L = new LuCI({...})`),于是同一个文件被
#      两个不同 ?v= 取两遍:29KB 白下、LuCI 类解析两次。
#      对照 bootstrap 的 header.ut —— 它发 0 次 —— 可以确定多出来的是 argon 这行,
#      所以删 argon 的、保留 header.ut 的(后者对没有自带 header 的主题也生效,
#      删错了那些主题会直接白页)。
#      加载顺序不受影响:window.L 只由那段内联脚本创建,它仍排在 header.ut 的
#      luci.js 之后;mwrt-aurora-boot.js 本来就在它之前跑,用的是
#      `(window.L && window.L.env) || {}` 带守卫的写法;cbi.js 顶层不碰 L。
#
# 注入点是主题模板 /usr/lib/lua/luci/view/themes/argon/header.htm,不是
# luci-base 的 header.ut —— 这两笔都是 argon 专属的问题(translations 那行和多余
# 的 luci.js 都只在 argon 的 header.htm 里)。换主题就不需要这个补丁,所以写在
# 主题文件里是对的;这跟 install-aurora-skin.sh 改全局 header.ut 的理由正好相反。
#
# 幂等:重复跑不会重复注入,可以放 cron 里当兜底(luci-theme-argon 升级会把
# header.htm 覆盖回去)。跟 aurora-skin / disclaimer 同一个路子。
#
# 用法:  ./install-luci-perf.sh          安装或自愈
#        ./install-luci-perf.sh --remove 卸载

set -u

HEADER=/usr/lib/lua/luci/view/themes/argon/header.htm
GEN=/usr/bin/mwrt-gen-translations
RES=/www/luci-static/resources
MARK=mwrt-perf

drop_caches() {
	rm -f /tmp/luci-indexcache* 2>/dev/null
	rm -rf /tmp/luci-modulecache 2>/dev/null
}

# 3) 资源版本号跟着固件内容走(header.htm 和登录页 header_login.htm 都做)。
#    argon 把 ?v= 写死成 2.3.1 / 2 / 3.5.1, cbi.js 用 luci 版本号 —— 新旧固件
#    一模一样, uhttpd 又不发 Cache-Control, 浏览器会一直用旧的 css/js, 只能手动
#    清缓存。这里给它们统一补上 -<opkg/status 的 mtime>, 就是 luci-base 给
#    luci.js 用的同一个 pkgs_update_time(Lua 模板里拿不到 ucode 的 env,
#    自己 stat 一次)。mwrt-cachebust 按静态资源指纹校正这个 mtime。
#    trans-<lang>.js 已经按自己的 mtime 带版本, 不动。
PV_FILE=/usr/lib/opkg/status
LOGIN=/usr/lib/lua/luci/view/themes/argon/header_login.htm

pv_patch() {
	f=$1
	[ -f "$f" ] || return 0
	# 定义行: 紧跟在顶部 Lua 块的 `local fs = require "nixio.fs"` 后面
	if ! grep -q 'local _pv = ' "$f"; then
		grep -q '^[[:space:]]*local fs = require "nixio.fs"' "$f" || return 0
		awk -v pf="$PV_FILE" '
			{ print }
			/^[[:space:]]*local fs = require "nixio.fs"/ && !done {
				print "\tlocal _pv = fs.stat(\"" pf "\", \"mtime\") or 0  -- mwrt-perf: pv"
				done = 1
			}
		' "$f" > "$f.tmp" && mv "$f.tmp" "$f"
	fi
	# 只有定义行确实在, 才去改引用(否则模板里会多出一个未定义变量)
	grep -q 'local _pv = ' "$f" || return 0
	sed -i -E '/_pv%>/! {
		s#(/(css|js)/[^"?]*\.(css|js)\?v=[0-9.]+)"#\1-<%=_pv%>"#
		s#(/cbi\.js\?v=<%=ver\.luciversion%>)"#\1-<%=_pv%>"#
	}' "$f"
}

pv_unpatch() {
	f=$1
	[ -f "$f" ] || return 0
	sed -i -e 's#-<%=_pv%>"#"#g' -e '/local _pv = .*mwrt-perf: pv/d' "$f"
}

# 原始的两行(luci-theme-argon 2.3.1)。--remove 要还原成这个样子。
ORIG_TRANS='	<script src="<%=url('"'"'admin/translations'"'"', luci.i18n.context.lang)%>?v=<%=ver.luciversion%>"></script>'
ORIG_LUCIJS='	<script src="<%=resource%>/luci.js?v=<%=ver.luciversion%>"></script>'

case "${1:-}" in
	--remove)
		if [ -f "$HEADER" ] && grep -q "$MARK" "$HEADER"; then
			# 有备份就直接还原,最干净
			if [ -f "$HEADER.bak-luci-perf" ]; then
				mv "$HEADER.bak-luci-perf" "$HEADER"
			else
				# 没备份则按标记把两块换回原样。两段注入都是成对的
				# begin/end,整段替换成原来那一行 —— 注释本身是多行的,
				# 只匹配首行会让后续几行漏成模板里的裸文本。
				awk -v ot="$ORIG_TRANS" -v ol="$ORIG_LUCIJS" '
					/mwrt-perf: trans:begin/  { skip=1; print ot; next }
					/mwrt-perf: trans:end/    { skip=0; next }
					/mwrt-perf: lucijs:begin/ { skip=1; print ol; next }
					/mwrt-perf: lucijs:end/   { skip=0; next }
					!skip { print }
				' "$HEADER" > "$HEADER.tmp" && mv "$HEADER.tmp" "$HEADER"
			fi
			echo "已还原 $HEADER"
		fi
		pv_unpatch "$HEADER"
		pv_unpatch "$LOGIN"
		rm -f "$RES"/trans-*.js
		sed -i '/mwrt-gen-translations/d' /etc/crontabs/root 2>/dev/null
		sed -i '/install-luci-perf.sh/d' /etc/crontabs/root 2>/dev/null
		/etc/init.d/cron reload >/dev/null 2>&1
		drop_caches
		echo "已卸载"
		exit 0
		;;
esac

[ -f "$HEADER" ] || { echo "错误: 找不到 $HEADER"; exit 1; }

# 先把静态字典生成出来 —— 模板里有存在性判断,文件没到位就会走回退分支,
# 那样注入了也没效果。
if [ -x "$GEN" ]; then
	"$GEN" ${1:+"$1"} >/dev/null 2>&1 || true
fi

if grep -q "$MARK" "$HEADER"; then
	echo "$HEADER 已注入,无需改动"
	pv_patch "$HEADER"
	pv_patch "$LOGIN"
	drop_caches
	exit 0
fi

# 锚点校验:两行都得在,而且 luci.js 必须恰好一行(多于一行说明模板结构变了,
# 不能瞎删)。
grep -qF "url('admin/translations'" "$HEADER" || {
	echo "错误: header.htm 里找不到 translations 锚点"; exit 1; }
n=$(grep -cF '<%=resource%>/luci.js?v=' "$HEADER")
[ "$n" -le 1 ] || { echo "错误: 期望恰好 1 行 luci.js,实际 $n 行"; exit 1; }

# busybox 的 sed -i 插多行不可靠,用 awk。
awk '
	# 1) translations:换成"有静态文件就用,没有就回退端点"
	/url\(.admin\/translations./ {
		print "\t<%# mwrt-perf: trans:begin —— 原来这里直挂 /cgi-bin/luci/admin/translations/<lang>,"
		print "\t    走 CGI(223KB / 58ms)且 cache-control: no-cache,每次导航都重下。"
		print "\t    改用 mwrt-gen-translations 预渲染的静态文件:0.7ms + 304 可缓存。"
		print "\t    ?v= 取文件 mtime,重新生成时才失效。文件不在就回退原端点。 %>"
		print "\t<%-"
		print "\t\tlocal _fs   = require \"nixio.fs\""
		print "\t\tlocal _lang = luci.i18n.context.lang"
		print "\t\tlocal _st   = _fs.stat(\"/www/luci-static/resources/trans-\" .. _lang .. \".js\")"
		print "\t-%>"
		print "\t<% if _st then -%>"
		print "\t<script src=\"<%=resource%>/trans-<%=_lang%>.js?v=<%=_st.mtime%>\"></script>"
		print "\t<% else -%>"
		print "\t<script src=\"<%=url(\047admin/translations\047, _lang)%>?v=<%=ver.luciversion%>\"></script>"
		print "\t<% end -%>"
		print "\t<%# mwrt-perf: trans:end %>"
		next
	}
	# 2) luci.js:删掉,改由 luci-base 的 header.ut 单独发
	/<%=resource%>\/luci\.js\?v=/ {
		print "\t<%# mwrt-perf: lucijs:begin —— 这行删了。luci-base 的 header.ut 已经在内联"
		print "\t    `L = new LuCI({...})` 之前发过 luci.js,这里再发一次会让浏览器按两个"
		print "\t    不同的 ?v= 取两遍(29KB 白下 + 类解析两次)。bootstrap 的 header.ut"
		print "\t    发 0 次,可见多余的是 argon 这行。"
		print "\t    mwrt-perf: lucijs:end %>"
		next
	}
	{ print }
' "$HEADER" > "$HEADER.tmp" || { echo "错误: awk 处理失败"; rm -f "$HEADER.tmp"; exit 1; }

# 注入后校验:标记要在、trans 分支要在、luci.js 要没了
if ! grep -q "$MARK" "$HEADER.tmp" \
   || ! grep -q 'trans-<%=_lang%>\.js' "$HEADER.tmp" \
   || grep -qF '<%=resource%>/luci.js?v=' "$HEADER.tmp"; then
	echo "错误: 注入后校验失败,已回滚"
	rm -f "$HEADER.tmp"
	exit 1
fi

mv "$HEADER.tmp" "$HEADER"
pv_patch "$HEADER"
pv_patch "$LOGIN"
drop_caches

echo "已注入 $HEADER"
echo "  静态字典 : $RES/trans-<lang>.js"
echo "  卸载     : $0 --remove"
