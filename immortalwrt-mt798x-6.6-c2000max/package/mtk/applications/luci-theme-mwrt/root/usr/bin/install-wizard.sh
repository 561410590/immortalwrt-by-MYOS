#!/bin/sh
#
# install-wizard.sh —— 安装/自愈菜单里那个「向导」的点击入口
#
# 幂等:重复跑不会重复注入,可以放 cron 里当兜底。写法照 install-disclaimer.sh,
# 两个注入块各有自己的标记,互不干扰,顺序是 disclaimer -> wizard -> 主题 footer。
#
# 注入点是 luci-base 的全局模板 /usr/share/ucode/luci/template/footer.ut,
# 不是主题目录 —— 之前吃过亏,写进 themes/argon/ 的定制换主题就没了。
# 必须插在 include(themes/${theme}/footer) 之前,因为主题 footer 会输出
# </body></html>,插在后面浏览器会当成正文外的垃圾。
#
# 注入的是什么:一个点击拦截器(mwrt-wizard-launch.js)。它只做一件事 ——
# 用户点菜单里的「向导」时,把那次点击接住,就地在当前页面上开弹窗,不导航。
# 要在 footer 里全局注入而不是放在向导页自己里,是因为菜单在**每个**页面上
# 都有,而菜单由主题渲染(menu-argon.js),应用包挂不上 click 处理。
#
# !! 这里没有任何自动弹出 !!
#   老版本(mwrt-wizard-boot.js)会在首次登录时 window.location.replace 到
#   向导页。现在明确不要那个行为:向导只在用户主动点菜单时出现。所以
#   服务端闸门也跟着变了 —— 不再判 mwrt.wizard.done,只要是已登录的页面
#   就注入拦截器,否则"配置完一次之后菜单里的向导就再也点不开"。
#
#   mwrt.wizard.done 这个键保留,但语义降级成"用户见过/配过了"的记录:
#   后端 apply 结束时写 1,APP 和其他脚本可以读,不再控制任何弹出行为。
#
# 登录页(sysauth.ut)也会渲染 footer,所以仍然要求 ctx.authsession 存在:
# 没登录时拦截器毫无用处(rpc 全是 403),白下发一个文件。
#
# 用法:  ./install-wizard.sh          安装或自愈(含从老版本迁移)
#        ./install-wizard.sh --reset  把 done 置 0(清掉"已配过"的记录)
#        ./install-wizard.sh --remove 卸载注入块

set -u

FOOTER=/usr/share/ucode/luci/template/footer.ut
JS=/www/luci-static/resources/mwrt-wizard-launch.js
# 向导本体(baseclass,被 L.require('mwrt.wizard') 拉起来的那个)
BASE=/www/luci-static/resources/mwrt/wizard.js
# 直接按地址进来时的落点(很薄的一层 view 壳)
VIEW=/www/luci-static/resources/view/mwrt/wizard.js
MARK_BEGIN="{# mwrt-wizard:begin #}"
MARK_END="{# mwrt-wizard:end #}"

# 两份配置镜像:mwrt 为准,WTMmodem 跟着走(读 WTMmodem 的脚本另有其人)
set_done() {
	uci -q set mwrt.wizard=wizard
	uci -q set mwrt.wizard.done="$1"
	uci -q set WTMmodem.wizard=wizard 2>/dev/null
	uci -q set WTMmodem.wizard.done="$1" 2>/dev/null
	uci -q commit mwrt
	uci -q commit WTMmodem 2>/dev/null
}

# 把注入块摘掉。迁移和 --remove 共用。
strip_block() {
	[ -f "$FOOTER" ] || return 0
	grep -q 'mwrt-wizard:begin' "$FOOTER" || return 0
	awk -v b="$MARK_BEGIN" -v e="$MARK_END" '
		index($0,b){skip=1} !skip{print} index($0,e){skip=0}
	' "$FOOTER" > "$FOOTER.tmp" && mv "$FOOTER.tmp" "$FOOTER"
}

drop_cache() {
	rm -f /tmp/luci-indexcache* 2>/dev/null
	rm -rf /tmp/luci-modulecache 2>/dev/null
}

case "${1:-}" in
	--reset)
		set_done 0
		rm -f /tmp/run/mwrt/wizard_applied 2>/dev/null
		echo "已把 mwrt.wizard.done 置 0(只是清掉记录;向导本来就随时可以从菜单点开)"
		exit 0
		;;
	--remove)
		strip_block
		drop_cache
		echo "已卸载注入块(mwrt.wizard.done 保留)"
		echo "注意:菜单项还在,但点击不再就地开弹窗,会跳转到向导页"
		exit 0
		;;
esac

[ -f "$FOOTER" ] || { echo "错误: 找不到 $FOOTER"; exit 1; }
[ -f "$JS" ]     || { echo "错误: 请先部署 $JS"; exit 1; }
[ -f "$BASE" ]   || { echo "错误: 请先部署 $BASE"; exit 1; }
[ -f "$VIEW" ]   || { echo "错误: 请先部署 $VIEW"; exit 1; }

# 没有 done 这个键就补一个 0 —— 不覆盖已有值
if [ -z "$(uci -q get mwrt.wizard.done)" ]; then
	set_done 0
fi

# 从老版本迁移:块在、但里面还是那个会自动跳转的 boot.js,就整段换掉。
# 不换的话固件升级后会保留"首次登录自动弹"的老行为,而那个文件已经不在了
# —— 变成每次登录都请求一个 404。
if grep -q 'mwrt-wizard:begin' "$FOOTER" && grep -q 'mwrt-wizard-boot\.js' "$FOOTER"; then
	echo "检测到老版本注入块(自动跳转),正在迁移…"
	strip_block
fi

# 老的跳转脚本如果还躺在那儿就收掉,避免有人手工引用到它
rm -f /www/luci-static/resources/mwrt-wizard-boot.js 2>/dev/null

# 已经注入过就不动(幂等)
if grep -q 'mwrt-wizard:begin' "$FOOTER"; then
	echo "footer.ut 已注入,跳过"
else
	ANCHOR='{% include(`themes/${theme}/footer`) %}'
	grep -qF "$ANCHOR" "$FOOTER" || { echo "错误: footer.ut 里找不到主题 include 锚点"; exit 1; }

	# busybox 的 sed -i 插多行不可靠,用 awk
	awk -v anchor="$ANCHOR" -v b="$MARK_BEGIN" -v e="$MARK_END" '
	index($0, anchor) && !done {
		print b
		print "{%"
		print "  // 菜单里「向导」的点击拦截器。不做任何自动跳转/自动弹出,"
		print "  // 只在用户点菜单时就地开弹窗。所以这里不判 wizard.done ——"
		print "  // 判了的话配过一次之后菜单里的向导就再也点不开了。"
		print "  // 登录页也会渲染 footer,未登录时拦截器没用(rpc 全 403),不下发。"
		print "  let _qsw_show = false;"
		print "  try { _qsw_show = !!ctx?.authsession; } catch (_e) { _qsw_show = false; }"
		print "%}"
		print "{% if (_qsw_show): %}"
		print "\t<script src=\"{{ resource }}/mwrt-wizard-launch.js?v=1-{{ pkgs_update_time }}\"></script>"
		print "{% endif %}"
		print e
		done = 1
	}
	{ print }
	' "$FOOTER" > "$FOOTER.tmp" || { echo "错误: awk 处理失败"; rm -f "$FOOTER.tmp"; exit 1; }

	if ! grep -q 'mwrt-wizard:begin' "$FOOTER.tmp"; then
		echo "错误: 注入后校验失败,已回滚"
		rm -f "$FOOTER.tmp"
		exit 1
	fi

	mv "$FOOTER.tmp" "$FOOTER"
	echo "已注入 $FOOTER"
fi

# 老块里是写死的 ?v=1, 新旧固件同一个地址, 浏览器会一直用旧缓存。补上 pkgs_update_time
# (opkg/status 的 mtime, mwrt-cachebust 按静态资源指纹校正), 幂等。
sed -i 's#/mwrt-wizard-launch\.js?v=1"#/mwrt-wizard-launch.js?v=1-{{ pkgs_update_time }}"#' "$FOOTER"

drop_cache

echo "完成。"
echo "  入口     : 菜单「系统 -> 向导」,随时可点"
echo "  自动弹出 : 无(首次启动也不弹)"
echo "  配过记录 : done=$(uci -q get mwrt.wizard.done)"
