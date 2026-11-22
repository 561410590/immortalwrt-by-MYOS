"""Package the original C5800 WebUI and the separate MYOS compatibility files."""

import argparse
from io import BytesIO
from pathlib import Path
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rootfs", type=Path, help="Extracted C5800 NROS 2.3.3 rootfs")
    args = parser.parse_args()
    local = Path(__file__).resolve().parent
    stock = args.rootfs
    output = local / "nros-2.3.3-webui.tar.gz"
    terminal_asset = "www/luci-static/nradio/js/main.min.js"
    theme_headers = {
        "lua/luci/view/themes/nradio/header.htm",
        "lua/luci/view/themes/nradio/header_simple.htm",
    }
    network_controller = "lua/luci/controller/nradio/network.lua"
    radio_library = "lua/luci/nradio.lua"
    status_views = {
        "lua/luci/view/nradio_status/index_common.htm",
        "lua/luci/view/nradio_status/index_simple.htm",
        "lua/luci/view/nradio_status/index_stand.htm",
    }
    patched_stock = theme_headers | status_views | {network_controller, radio_library, "lua/luci/view/nradio_network/index.htm"}
    compatible_lua = {
        "lua/cloudd/api.lua": "cloudd-api.lua",
        "lua/luci/model/cbi/nradio_adv/dhcp.lua": "dhcp.lua",
        "lua/luci/model/cbi/nradio_adv/apcli.lua": "apcli.lua",
    }

    def installed_mode(mode):
        def apply(member):
            member.mode = mode
            member.uid = member.gid = 0
            member.uname = member.gname = "root"
            return member
        return apply

    with tarfile.open(output, "w:gz") as bundle:
        def add_stock_bytes(original, installed, data):
            member = installed_mode(0o644)(bundle.gettarinfo(original, arcname=installed))
            member.size = len(data)
            bundle.addfile(member, BytesIO(data))

        bundle.add(stock / "www", arcname="www",
                   filter=lambda member: None if member.name in ("www/cgi-bin/luci", terminal_asset) else member)
        original = stock / terminal_asset
        script = original.read_bytes()
        for field in (b"txbytes", b"rxbytes"):
            call = b"nradio_byte_format(db." + field + b")"
            script = script.replace(call, b"(db." + field + b"==null?'-':" + call + b")")
        add_stock_bytes(original, terminal_asset, script)
        bundle.add(stock / "www/cgi-bin/luci", arcname="stock-cgi")
        bundle.add(stock / "usr/lib/lua", arcname="lua",
                   filter=lambda member: None if member.name in compatible_lua or member.name in patched_stock else member)
        for installed in sorted(theme_headers):
            original = stock / "usr/lib" / installed
            data = original.read_bytes().replace(
                b"main.min.js?v=git-26.257.38817-208f0a4",
                b"main.min.js?v=git-26.257.38817-208f0a4-c5800.1",
            )
            add_stock_bytes(original, installed, data)
        original = stock / "usr/lib" / network_controller
        data = original.read_bytes().replace(b'call("action_save_wan")', b'post("action_save_wan")')
        start = data.index(b"function action_save_wan()")
        end = data.index(b"function reload_network_relate(", start)
        data = data[:start] + b'function action_save_wan()\n\treturn require("nros_wan").apply()\nend\n\n' + data[end:]
        add_stock_bytes(original, network_controller, data)
        original = stock / "usr/lib" / radio_library
        data = original.read_bytes().replace(
            b"""r == "wireless" and (cattree.nodes['acl']._menu_selected""",
            b"""r == "wireless" and ((cattree.nodes['acl'] and cattree.nodes['acl']._menu_selected)""",
        ).replace(
            b"""y == "acl" and cattree.nodes['acl']._menu_selected""",
            b"""y == "acl" and cattree.nodes['acl'] and cattree.nodes['acl']._menu_selected""",
        )
        add_stock_bytes(original, radio_library, data)
        for installed in sorted(status_views):
            original = stock / "usr/lib" / installed
            data = original.read_bytes()
            if installed.endswith("index_common.htm"):
                data = data.replace(
                    b"function flow_show(wan_item,upload_bytes,download_bytes){",
                    b"function flow_show(wan_item,upload_bytes,download_bytes,scope){",
                )
                point = b'\t$(".box_wans_"+wan_item.name+" .flowdown_unit").html(bytes_data[1]?bytes_data[1]:"B");'
                data = data.replace(point, point + """
	var box = $(".box_wans_"+wan_item.name);
	box.find(".flowtotal_img + .speedflow_notebox").text(scope === "interface" ? "接口累计" : "<%:StatusFlowTotal%>");
	if(scope === "interface"){
		if(upload_bytes == null){
			box.find(".flowup_data").text("—");
			box.find(".flowup_unit").text("");
		}
		if(download_bytes == null){
			box.find(".flowdown_data").text("—");
			box.find(".flowdown_unit").text("");
		}
	}
""".encode())
            else:
                data = data.replace(b"flow_show(wan_item,db.sim_flow,db.sim_mon);",
                                    b"flow_show(wan_item,db.sim_flow,db.sim_mon,db.flow_scope);")
                if installed.endswith("index_stand.htm"):
                    data = data.replace(b"if(!wifi_hd_count)\n\t{", b"if(!ter_max_client)\n\t{")
            add_stock_bytes(original, installed, data)
        for installed, source in compatible_lua.items():
            bundle.add(local / source, arcname=installed, filter=installed_mode(0o644))
        bundle.add(local / "cgi.sh", arcname="www/cgi-bin/luci", filter=installed_mode(0o755))
        bundle.add(local / "nros_compat.lua", arcname="lua/nros_compat.lua", filter=installed_mode(0o644))
        bundle.add(local / "nros_wan.lua", arcname="lua/nros_wan.lua", filter=installed_mode(0o644))
        bundle.add(local / "nros-theme-controller.lua",
                   arcname="lua/luci/controller/nradio_adv/myos_theme.lua", filter=installed_mode(0o644))
        bundle.add(local / "nros-theme.htm",
                   arcname="lua/luci/view/nradio_adv/myos_theme.htm", filter=installed_mode(0o644))
        bundle.add(local / "wan.htm",
                   arcname="lua/luci/view/nradio_network/index.htm", filter=installed_mode(0o644))
        for name in ("config", "uci"):
            folder = tarfile.TarInfo(name)
            folder.type = tarfile.DIRTYPE
            folder.mode = 0o700
            bundle.addfile(folder)
        bundle.add(local / "nros-luci", arcname="config/luci", filter=installed_mode(0o600))

    print(output)


if __name__ == "__main__":
    main()
