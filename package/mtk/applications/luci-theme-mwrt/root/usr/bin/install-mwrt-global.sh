#!/bin/sh
#
# install-mwrt-global.sh —— 安装/自愈与主题无关的全局脚本块
#
# 注入两样东西, 所有主题(argon / bootstrap)都生效:
#   mwrt-unlock.js      后台连续键入 mwrtmwrt 弹出插件认证框
#   mwrt-notice.js      右上角全局消息中心(多条叠加, 部分可已读关闭;
#                       新固件提示也在这里, v2 起替代 mwrt-ota-notify.js)
#
# 写法照 install-wizard.sh: 注入点是 luci-base 的全局模板
# /usr/share/ucode/luci/template/footer.ut, 插在 include(themes/${theme}/footer)
# 之前(主题 footer 会输出 </body></html>)。登录页也会渲染 footer, 所以要求
# ctx.authsession —— 没登录时两段脚本都没用。
#
# 幂等: 重复跑不会重复注入; 块里的版本号不对就整段换掉。luci-base 升级会把
# footer.ut 覆盖回去, 所以挂 cron 自愈。
#
# v3 起资源地址带 ?v=<VER>-{{ pkgs_update_time }}, 并在最后调 mwrt-cachebust:
# 刷了内容不同的新固件, pkgs_update_time 就跟着变, 浏览器不会再拿旧缓存。
# 顺带给 bootstrap 系主题 header.ut 里写死版本号的 cascade/mobile/cbi 也补上。
#
# 用法:  ./install-mwrt-global.sh          安装或自愈
#        ./install-mwrt-global.sh --remove 卸载注入块

set -u

FOOTER=/usr/share/ucode/luci/template/footer.ut
RES=/www/luci-static/resources
MARK_BEGIN="{# mwrt-global:begin #}"
MARK_END="{# mwrt-global:end #}"
VER=5
PV='{{ pkgs_update_time }}'

strip_block() {
	[ -f "$FOOTER" ] || return 0
	grep -q 'mwrt-global:begin' "$FOOTER" || return 0
	awk -v b="$MARK_BEGIN" -v e="$MARK_END" '
		index($0,b){skip=1} !skip{print} index($0,e){skip=0}
	' "$FOOTER" > "$FOOTER.tmp" && mv "$FOOTER.tmp" "$FOOTER"
}

drop_cache() {
	rm -f /tmp/luci-indexcache* 2>/dev/null
	rm -rf /tmp/luci-modulecache 2>/dev/null
}

case "${1:-}" in
	--remove)
		strip_block
		drop_cache
		echo "已卸载注入块"
		exit 0
		;;
esac

[ -f "$FOOTER" ] || { echo "错误: 找不到 $FOOTER"; exit 1; }
for f in mwrt-unlock.js mwrt-notice.js; do
	[ -f "$RES/$f" ] || { echo "错误: 请先部署 $RES/$f"; exit 1; }
done

# 版本号不对(老块)就整段换
if grep -q 'mwrt-global:begin' "$FOOTER" && ! grep -qF "mwrt-unlock.js?v=$VER-$PV\"" "$FOOTER"; then
	strip_block
fi

if grep -q 'mwrt-global:begin' "$FOOTER"; then
	echo "footer.ut 已注入,跳过"
else
	ANCHOR='{% include(`themes/${theme}/footer`) %}'
	grep -qF "$ANCHOR" "$FOOTER" || { echo "错误: footer.ut 里找不到主题 include 锚点"; exit 1; }

	# busybox 的 sed -i 插多行不可靠,用 awk
	awk -v anchor="$ANCHOR" -v b="$MARK_BEGIN" -v e="$MARK_END" -v ver="$VER-$PV" -v q="'" '
	index($0, anchor) && !done {
		print b
		print "{%"
		print "  // 与主题无关的全局脚本: 插件认证框 + 新固件提示。只给已登录页面，暗号仅在 MWRT 主题启用。"
		print "  let _mg_show = false;"
		print "  try { _mg_show = !!ctx?.authsession; } catch (_e) { _mg_show = false; }"
		print "%}"
		print "{% if (_mg_show): %}"
		print "{% if (theme == " q "argon" q "): %}"
		print "\t<script src=\"{{ resource }}/mwrt-unlock.js?v=" ver "\" data-url=\"{{ dispatcher.build_url(" q "admin/Outofnetwork" q ") }}\"></script>"
		print "{% endif %}"
		print "\t<script src=\"{{ resource }}/mwrt-notice.js?v=" ver "\" data-ota=\"{{ dispatcher.build_url(" q "admin/system/myos_update" q ") }}\"></script>"
		print "{% endif %}"
		print e
		done = 1
	}
	{ print }
	' "$FOOTER" > "$FOOTER.tmp" || { echo "错误: awk 处理失败"; rm -f "$FOOTER.tmp"; exit 1; }

	if ! grep -q 'mwrt-global:begin' "$FOOTER.tmp"; then
		echo "错误: 注入后校验失败,已回滚"
		rm -f "$FOOTER.tmp"
		exit 1
	fi

	mv "$FOOTER.tmp" "$FOOTER"
	echo "已注入 $FOOTER"
fi

# bootstrap / bootstrap-dark / bootstrap-light 的 header.ut 把 luci 版本号写死在
# ?v= 里, 新旧固件一样。只给还没带 pkgs_update_time 的那几行补上, 幂等。
for t in /usr/share/ucode/luci/template/themes/*/header.ut; do
	[ -f "$t" ] || continue
	grep -E '/(cascade|mobile)\.css\?v=|/cbi\.js\?v=' "$t" | grep -qv 'pkgs_update_time' || continue
	sed -i -E "/pkgs_update_time/! s#(/(cascade|mobile)\.css|/cbi\.js)\?v=([^\"{]*)\"#\1?v=\3-$PV\"#" "$t"
	echo "已补版本号 $t"
done

# 按静态资源指纹校正 pkgs_update_time(内容没变就什么都不做)
[ -x /usr/bin/mwrt-cachebust ] && /usr/bin/mwrt-cachebust

drop_cache
echo "完成。"
