"""Bundle the N5000 NROS WebUI with its MYOS adapters."""

import argparse
from io import BytesIO
from pathlib import Path
import tarfile


def installed_mode(mode):
    def apply(member):
        member.mode = mode
        member.uid = member.gid = 0
        member.uname = member.gname = "root"
        return member
    return apply


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path)
    args = parser.parse_args()
    stock = args.stock
    local = Path(__file__).resolve().parent
    output = local / "nros-2.3.9-webui.tar.gz"
    vendor_lua = {
        "lua/cloudd/api.lua": [
            (b'print("break")', b'-- Keep cloudd output out of CGI headers.'),
        ],
        "lua/luci/sgi/cgi.lua": [
            (b'function run()\n\tfirst_access()',
             b'function run()\n\t-- MYOS owns wireless initialization and mesh settings.'),
        ],
        "lua/luci/nradio.lua": [
            (b"""r == "wireless" and (cattree.nodes['acl']._menu_selected""",
             b"""r == "wireless" and ((cattree.nodes['acl'] and cattree.nodes['acl']._menu_selected)"""),
            (b"""y == "acl" and cattree.nodes['acl']._menu_selected""",
             b"""y == "acl" and cattree.nodes['acl'] and cattree.nodes['acl']._menu_selected"""),
        ],
        "lua/luci/view/nradio_status/index_stand.htm": [
            (b"if(!wifi_hd_count)\n\t{", b"if(!ter_max_client)\n\t{"),
            (b"data.result.global && data.result.global.cpu_percent)",
             b"data.result.global && data.result.global.cpu_percent != null)"),
            (b"data.result.global && data.result.global.mem_percent)",
             b"data.result.global && data.result.global.mem_percent != null)"),
        ],
        "lua/luci/controller/nradio/network.lua": [
            (b'call("action_save_wan")', b'post("action_save_wan")'),
        ],
        "lua/luci/model/cbi/nradio_adv/dhcp.lua": [
            (b'local second = m:get(section, "leasetime") or 43200\n\treturn tonumber(second) / 60',
             b'''local value = tostring(m:get(section, "leasetime") or 43200):lower()
    local number, unit = value:match("^(%d+)([smhdw]?)$")
    local scale = {s=1, m=60, h=3600, d=86400, w=604800}
    return number and tonumber(number) * (scale[unit] or 1) / 60 or value'''),
        ],
        "lua/luci/view/nradio_network/index.htm": [

            (b'<input type="button" class="cbi-button cbi-button-apply" id="btn-save"  value="<%:Save & Apply%>" />',
             '<a class="cbi-button cbi-button-apply" id="myos-wired-settings">MYOS 有线 WAN 设置</a>'.encode()),
            (b'\t\tshow_mode_box(g_data.net_kind);\n\t\tload_wan_data(wan_data);',
             b'\t\t$(".mode_container").addClass("hide");\n\t\tload_wan_data(wan_data);\n\t\t$(".wans_box_info :input, .wan_detail :input").prop("disabled", true);'),
            (b"$('#btn-save').click(function(){",
             b"""$('#myos-wired-settings').attr('href', new URL('/cgi-bin/luci/admin/network/network', location.protocol + '//' + location.hostname));
$('#btn-save').click(function(){"""),
        ],
    }
    files = {
        "www/cgi-bin/luci": ("cgi.sh", 0o755),
        "lua/nros_compat.lua": ("nros_compat.lua", 0o644),
        "lua/luci/controller/nradio_adv/myos_theme.lua": ("nros-theme-controller.lua", 0o644),
        "lua/luci/view/nradio_adv/myos_theme.htm": ("nros-theme.htm", 0o644),
        "config/luci": ("nros-luci", 0o600),
    }

    with tarfile.open(output, "w:gz") as bundle:
        bundle.add(stock / "www", arcname="www",
                   filter=lambda member: None if member.name == "www/cgi-bin/luci" else member)
        bundle.add(stock / "www/cgi-bin/luci", arcname="stock-cgi", filter=installed_mode(0o755))
        for library in ("libssl.so.1.1", "libcrypto.so.1.1"):
            bundle.add(stock / "usr/lib" / library, arcname="lib/" + library,
                       filter=installed_mode(0o644))
        bundle.add(stock / "usr/lib/lua", arcname="lua",
                   filter=lambda member: None if member.name in vendor_lua else member)
        for installed, substitutions in vendor_lua.items():
            original = stock / "usr/lib" / installed
            data = original.read_bytes()
            for before, after in substitutions:
                data = data.replace(before, after)
            if installed == "lua/luci/controller/nradio/network.lua":
                start = data.index(b"function action_save_wan()")
                stop = data.index(b"function reload_network_relate(", start)
                adapter = """

function action_save_wan()
    luci.http.status(409, "Conflict")
    luci.nradio.luci_call_result({code=409, message="请在 MYOS 网络接口设置有线 WAN"})
end
""".encode()
                data = data[:start] + adapter + data[stop:]
            member = installed_mode(0o644)(bundle.gettarinfo(original, arcname=installed))
            member.size = len(data)
            bundle.addfile(member, BytesIO(data))
        for name in ("config", "uci"):
            folder = tarfile.TarInfo(name)
            folder.type = tarfile.DIRTYPE
            folder.mode = 0o700
            bundle.addfile(folder)
        for installed, (filename, mode) in files.items():
            bundle.add(local / filename, arcname=installed, filter=installed_mode(mode))
    print(output)


if __name__ == "__main__":
    main()
