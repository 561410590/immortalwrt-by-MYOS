#!/usr/bin/env python3
"""Install verified upstream MT5700 V2.2.2 with NBCPE protocol corrections."""
import argparse
import hashlib
import json
import re
import tarfile
from pathlib import Path, PurePosixPath


BOOTSTRAP = """<script>
(function () {
  try {
    localStorage.setItem('atHost', location.hostname);
    localStorage.setItem('atPort', '8766');
    localStorage.setItem('myosWebuiHost', location.hostname);
  } catch (error) {}
  document.addEventListener('click', function (event) {
    var link = event.target.closest && event.target.closest('.ant-drawer a[href]');
    if (!link || link.hash.indexOf('#/') !== 0) return;
    setTimeout(function () {
      var mask = document.querySelector('.ant-drawer-open .ant-drawer-mask');
      if (mask) mask.click();
    }, 0);
  }, true);
})();
</script>
<script src="/5700/nbcpe-protocol.js"></script>
"""

EMBED_STYLE = """
html.nbcpe-embedded, html.nbcpe-embedded body {
  height:auto!important; min-height:0!important; overflow:hidden!important;
}
.nbcpe-embedded #root, .nbcpe-embedded .ant-pro-layout,
.nbcpe-embedded .ant-layout { min-height:0!important; height:auto!important; }
.nbcpe-embedded body, .nbcpe-embedded #root { width:100%!important; max-width:100%!important; }
.nbcpe-embedded .ant-pro-layout-container { min-width:0!important; max-width:100%!important; }
.nbcpe-embedded .ant-pro-page-container { min-width:0!important; }
.nbcpe-embedded .ant-table-wrapper { max-width:100%; overflow-x:auto; }
.nbcpe-embedded .ant-pro-layout-content {
  min-width:0!important; max-width:none!important; margin:0!important; padding:12px!important;
}
.nbcpe-embedded .ant-pro-page-container-children-content { margin:0!important; padding:0!important; }
.nbcpe-embedded .ant-card-body { padding:16px!important; }
.nbcpe-embedded .ant-card-head { min-height:48px!important; padding-inline:16px!important; }
.nbcpe-embedded .ant-card-head-title { white-space:normal; }
.nbcpe-embedded .ant-statistic-content {
  font-size:clamp(17px,2.2vw,24px)!important; line-height:1.45!important; overflow-wrap:anywhere;
}
.nbcpe-embedded .ant-statistic-title { font-size:12px!important; }
.nbcpe-embedded .ant-layout-has-sider > div:has(+ aside.ant-pro-sider):not(.ant-drawer *),
.nbcpe-embedded aside.ant-pro-sider:not(.ant-drawer *) {
  flex:0 0 168px!important; width:168px!important; min-width:168px!important; max-width:168px!important;
}
.nbcpe-embedded .ant-pro-sider-logo,
.nbcpe-embedded .ant-pro-sider-collapsed-button,
.nbcpe-embedded .ant-pro-sider-links,
.nbcpe-embedded .ant-pro-sider-extra,
.nbcpe-embedded .ant-pro-base-menu-vertical-item-icon { display:none!important; }
.nbcpe-embedded .ant-pro-base-menu-vertical-item-title-collapsed { justify-content:flex-start!important; }
.nbcpe-embedded .ant-pro-base-menu-vertical-item-text {
  display:inline!important; opacity:1!important; visibility:visible!important;
  width:auto!important; max-width:none!important; height:auto!important; overflow:visible!important;
}
.nbcpe-embedded .ant-menu-inline-collapsed > .ant-menu-item {
  padding-inline:12px!important; text-align:left!important;
}
.nbcpe-embedded .ant-pro-sider .ant-menu-title-content { margin-inline:0!important; }
@media(max-width:640px) {
  .nbcpe-embedded .ant-layout-has-sider:not(.ant-drawer *) { flex-direction:column!important; }
  .nbcpe-embedded .ant-layout-has-sider > div:has(+ aside.ant-pro-sider):not(.ant-drawer *) { display:none!important; }
  .nbcpe-embedded aside.ant-pro-sider:not(.ant-drawer *) {
    position:relative!important; flex:none!important; width:100%!important;
    min-width:0!important; max-width:none!important; height:auto!important;
  }
  .nbcpe-embedded aside.ant-pro-sider:not(.ant-drawer *) .ant-menu-root {
    display:flex!important; flex-wrap:wrap!important; gap:4px; width:100%!important; padding:4px!important;
  }
  .nbcpe-embedded aside.ant-pro-sider:not(.ant-drawer *) .ant-menu-item {
    flex:1 1 100px!important; width:auto!important; height:36px!important;
    line-height:36px!important; margin:0!important; padding-inline:8px!important;
  }
  .nbcpe-embedded .ant-pro-layout-container { width:100%!important; min-width:0!important; }
  .nbcpe-embedded .ant-drawer .ant-pro-sider-menu,
  .nbcpe-embedded .ant-drawer .ant-menu-root,
  .nbcpe-embedded .ant-drawer .ant-menu-item-group,
  .nbcpe-embedded .ant-drawer .ant-menu-item-group-list {
    display:block!important; flex:none!important; height:auto!important;
    min-height:0!important; max-height:none!important;
  }
  .nbcpe-embedded .ant-pro-layout-content { padding:8px!important; }
  .nbcpe-embedded .ant-card-body { padding:12px!important; }
  .nbcpe-embedded .ant-statistic-content { font-size:18px!important; }
}
"""

EMBED_BOOTSTRAP = """<script>
(function () {
  if (window.self === window.top) return;
  document.documentElement.classList.add('nbcpe-embedded');
  var style = document.createElement('style');
  style.textContent = %s;
  document.head.appendChild(style);
  function start() {
    var lastHeight = 0, pending = false;
    function measure() {
      pending = false;
      var root = document.getElementById('root');
      if (!root) return;
      var height = Math.ceil(Math.max(root.getBoundingClientRect().height, root.scrollHeight) + 8);
      if (height === lastHeight) return;
      lastHeight = height;
      window.parent.postMessage({type:'myos-nbcpe-webui-height',height:height}, location.origin);
    }
    function schedule() {
      if (!pending) { pending = true; requestAnimationFrame(measure); }
    }
    new MutationObserver(schedule).observe(document.body, {childList:true,subtree:true});
    if (window.ResizeObserver) new ResizeObserver(schedule).observe(document.getElementById('root'));
    window.addEventListener('resize', schedule);
    schedule();
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
  else start();
})();
</script>
""" % json.dumps(EMBED_STYLE)


def replace_exact(text, before, after, count=1):
    actual = text.count(before)
    if actual != count:
        raise RuntimeError('Upstream patch anchor changed: ' + before[:100] + ' (' + str(actual) + ')')
    return text.replace(before, after)


def replace_section(text, start, end, replacement):
    if text.count(start) != 1:
        raise RuntimeError('Ambiguous upstream section: ' + start)
    left = text.index(start)
    right = text.index(end, left)
    return text[:left] + replacement + text[right:]


def patch_javascript(name, text):
    # A page owns its timers/subscriptions; the application owns the shared AT socket.
    cleanup = {
        'p__CPE__AT__Terminal__index.2ff1b893.async.js': ('function(){Z.disconnect()}', 'function(){}'),
        'p__CPE__Network__Dial__index.a360ac4c.async.js': ('f.current=!1,i(),L.current.disconnect()', 'f.current=!1,i()'),
        'p__CPE__Network__Info__index.30901ff8.async.js': ('n(),E.disconnect()', 'n()'),
        'p__CPE__Network__Settings__index.893ddec9.async.js': ('function(){D.disconnect()}', 'function(){}'),
        'p__CPE__SMS__Settings__index.c3a79efc.async.js': ('o=!1,T.disconnect()', 'o=!1'),
        'p__CPE__System__Info__index.a04926af.async.js': ('a(),F.disconnect()', 'a()'),
        'p__CPE__System__Upgrade__index.d7af163b.async.js': ('function(){F.disconnect()}', 'function(){}'),
    }
    if name in cleanup:
        text = replace_exact(text, *cleanup[name])
    if name == 'umi.ec9b4b52.js':
        text = replace_section(text, '{key:"loadConfig",value:function()', ',{key:"setupWebSocket"',
            '{key:"loadConfig",value:function(){this.requireAuth=false;return Promise.resolve({at:{host:location.hostname,port:8766},status:"true"})}}')
        text = replace_exact(text, 'I()(this,"commandTimeout",1e3)', 'I()(this,"commandTimeout",15e3)')
        text = text.replace('||"192.168.1.1"', '||location.hostname').replace('||8765', '||8766')
        text = replace_exact(text, 'L.pendingCommands.delete(J),Ie({success:!1,error:',
            'L.pendingCommands.delete(J),L.connected=false,L.ws&&L.ws.close(),Ie({success:!1,error:')
        text = replace_exact(text, 'L.smsCollecting=!1,L.smsResolve=null,L.smsBuffer=[],Ie({success:!1,error:',
            'L.smsCollecting=!1,L.smsResolve=null,L.smsBuffer=[],L.connected=false,L.ws&&L.ws.close(),Ie({success:!1,error:')
        text = replace_exact(text, '{key:"clearPendingCommands",value:function(S){this.pendingCommands.forEach',
            '{key:"clearPendingCommands",value:function(S){clearTimeout(this.smsTimer);this.smsTimer=null;this.smsCollecting=false;this.smsBuffer=[];if(this.smsResolve){this.smsResolve({success:false,error:S});this.smsResolve=null}this.pendingCommands.forEach')
        text = replace_exact(text, '{key:"handleWebSocketMessage",value:function(S){var C=this;',
            '{key:"handleWebSocketMessage",value:function(S){var C=this;if(window.NBCPE.acceptEnvelope(this,S))return;')
        text = replace_exact(text, 'this.host=S.replace(/^\\[|\\]$/g,""),this.port=C,',
            'this.host=location.hostname,this.port=8766,C=8766,')
    elif name == 'p__CPE__Network__Info__index.30901ff8.async.js':
        text = replace_section(text, 'wn=function()', ',Ba=(0,j.useState)', '''wn=async function(){
            var context=await window.NBCPE.connection(E);
            ur(context.apn||"未知");cr(null);fr(null);
            if(context.cid!==1)return;
            var reply=await E.sendCommand("AT^DSAMBR=1");
            var rates=window.NBCPE.ambr(reply);
            if(rates){cr(rates.down/1000);fr(rates.up/1000)}
        }''')
        text = replace_exact(text, 'Ta=(0,j.useState)(0)', 'Ta=(0,j.useState)(null)')
        text = replace_exact(text, 'Na=(0,j.useState)(0)', 'Na=(0,j.useState)(null)')
        text = replace_exact(text, 'children:[bn," ",', 'children:[bn==null?"未提供":bn," ",')
        text = replace_exact(text, 'children:[Sn," ",', 'children:[Sn==null?"未提供":Sn," ",')
        for variable in ('bn', 'Sn'):
            before = 'children:[' + variable + '==null?"未提供":' + variable + '," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"Mbps"})]'
            after = before.replace('children:"Mbps"', 'children:' + variable + '==null?"":"Mbps"')
            text = replace_exact(text, before, after)
        text = replace_exact(text, 'w=g,y=parseInt(w[1])', 'w=g,y=parseInt(w[2])')
        text = replace_exact(text, 'T=w.length>4?parseInt(w[4]):255', 'T=w.length>3?parseInt(w[3]):255')
        text = replace_exact(text, 'P=w.length>2?parseInt(w[2]):255', 'P=w.length>4?parseInt(w[4]):255')
        text = replace_exact(text, 'O=w.length>3?parseInt(w[3]):255', 'O=w.length>1?parseInt(w[1]):255')
        text = replace_exact(text, 'O===0?-90:O>=65?-25:-90+O', 'O===0?-120:O>=96?-25:-121+O')
        text = replace_exact(text, 'M=a,l=parseInt(M[1])', 'M=a,l=parseInt(M[2])')
        text = replace_exact(text, 'w=M.length>4?parseInt(M[4]):255', 'w=M.length>3?parseInt(M[3]):255')
        text = replace_exact(text, 'S=M.length>2?parseInt(M[2]):255', 'S=M.length>4?parseInt(M[4]):255')
        text = replace_exact(text, 'I=t,w=parseInt(I[1])', 'I=t,w=parseInt(I[2])')
        text = replace_exact(text, 'O=parseInt(P[8],10)', 'O=parseInt(P[P[0].trim()==="NR"?8:7],10)')
        text = replace_exact(text, 'Wa=We==="7"?3:4', 'Wa=4')
        text = replace_exact(text, 'Z.length<3', 'Z.length<4')
        text = replace_exact(text, 't={mcc:I,mnc:w,channel:y,cid:T,pci:S,lac:p,rscp:F||0,signalPercent:Se(F||0),ecio:P||0,sysMode:x}',
            't={mcc:I,mnc:w,channel:y,cid:T,pci:S,lac:p,rscp:F||0,signalPercent:Se(F||0),ecio:P||0,sysMode:x,sinr:x==="NR"?window.NBCPE.measure(h[10],"sinr"):undefined}')
        text = text.replace('parseInt(ye,16).toString()', 'window.NBCPE.hex(ye)')
        text = text.replace('parseInt(ee,16).toString()', 'window.NBCPE.hex(ee)')
        text = text.replace('E.sendCommand("AT+CGEQOSRDP=8")', 'E.sendCommand("AT+CGEQOSRDP="+(window.NBCPE.activeCid||1))')
        text = replace_exact(text, 'return t.next=7,E.sendCommand("AT+CGEQOSRDP=1");case 7:i=t.sent;',
            't.next=8;case 7:')
        text = text.replace('E.sendCommand("AT^DHCP?")', 'E.sendCommand("AT^DHCP="+(window.NBCPE.activeCid||1))')
        text = text.replace('E.sendCommand("AT^DHCPV6?")', 'E.sendCommand("AT^DHCPV6="+(window.NBCPE.activeCid||1))')
        text = replace_exact(text, 'E.sendCommand("AT^DSFLOWQRY")', 'E.sendCommand("AT^DSFLOWQRY="+(window.NBCPE.activeCid||8))')
        text = replace_exact(text, 'return function(){return f.apply(this,arguments)}}(),wr=function()',
            'return function(){return f.apply(this,arguments)}}(),nbcpeClear=Ha,Ha=function(){if(window.confirm("将清零模组所有连接的累计流量和连接时间，是否继续？"))return nbcpeClear()},wr=function()')
    elif name == 'p__CPE__Network__Settings__index.893ddec9.async.js':
        text = replace_exact(text, 'c=le(r.band),t=', 'c=r.scs,t=')
        text = replace_exact(text, 'return function(r){return u.apply(this,arguments)}}(),en=function()',
            'return function(r){return u.apply(this,arguments)}}(),nbcpeLock=fr,fr=async function(r,a){if(r.type==="NR"){var scs=await window.NBCPE.resolveScs(D,r,le(r.band));if(scs===null)return false;r=Object.assign({},r,{scs:scs})}return nbcpeLock(r,a)},en=function()')
        text = replace_section(text, 'en=function()', ',X=function(s)',
            'en=async function(r){try{var reply=await D.sendCommand(r.type==="NR"?"AT^NRFREQLOCK?":"AT^LTEFREQLOCK?");return window.NBCPE.verifyCellLock(reply,r)}catch(error){return false}}')
    elif name == 'p__CPE__Network__Dial__index.a360ac4c.async.js':
        text = replace_exact(text, r'/\^SETAUTODIAL:(\d+)', r'/\^SETAUTODIAL:\s*(\d+)')
    return text


def install(archive_path, www):
    assets = Path(__file__).resolve().parent
    archive_path = assets / 'mt5700-webui-v2.2.2-upstream.tar.xz'
    provenance = json.loads((assets / 'mt5700-webui-upstream.json').read_text(encoding='utf-8'))
    actual = hashlib.sha256(archive_path.read_bytes()).hexdigest()
    if actual != provenance['archive_sha256']:
        raise RuntimeError('Verified upstream Web UI archive checksum mismatch')
    files = 0
    installed_paths = set()
    with tarfile.open(archive_path, 'r:xz') as archive:
        for entry in archive.getmembers():
            path = PurePosixPath(entry.name)
            if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] != '5700':
                raise RuntimeError('Unexpected Web UI archive path: ' + entry.name)
            if '__MACOSX' in path.parts or path.name.startswith('._'):
                continue
            if entry.isdir():
                continue
            if not entry.isfile():
                raise RuntimeError('Unexpected Web UI archive member: ' + entry.name)
            data = archive.extractfile(entry).read()
            if path.suffix == '.html':
                text = data.decode('utf-8').replace('/scripts/loading.js', '/5700/scripts/loading.js')
                data = text.replace('<head>', '<head>\n' + BOOTSTRAP + EMBED_BOOTSTRAP, 1).encode('utf-8')
            elif path.suffix == '.js':
                data = patch_javascript(path.name, data.decode('utf-8')).encode('utf-8')
            elif path.name == 'config.json':
                data = (json.dumps({'atHost': '', 'atPort': 8766}) + '\n').encode()
            target = www.joinpath(*path.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            target.chmod(0o644)
            installed_paths.add(path.as_posix())
            files += 1
    # The stock rootfs already contains an older Web UI. Replace only this
    # application's obsolete hashed build products; keep icons and other files.
    compiled = re.compile(r'(?:(?:\d+|p__CPE__[A-Za-z0-9_]+|t__plugin-layout__Layout)\.[0-9a-f]{8}\.(?:async\.js|chunk\.css)|(?:umi|preload_helper)\.[0-9a-f]{8}\.(?:js|css))')
    obsolete = []
    for target in (www / '5700').iterdir():
        if target.is_file() and compiled.fullmatch(target.name) and '5700/' + target.name not in installed_paths:
            target.unlink()
            obsolete.append(target.name)
    if not (www / '5700/index.html').is_file():
        raise RuntimeError('Upstream Web UI index is missing')
    for name in ('nbcpe-protocol.js', 'mt5700-webui-upstream.json'):
        (www / '5700' / name).write_bytes((assets / name).read_bytes())
        (www / '5700' / name).chmod(0o644)
    (www / '5700/config.json').write_text(json.dumps({'at': {'host': '', 'port': 8766}, 'status': 'false'}) + '\n', encoding='utf-8')
    print(json.dumps({'webui_files': files + 3, 'removed_obsolete_assets': len(obsolete), 'upstream_version': '2.2.2', 'upstream_commit': provenance['commit'], 'port': 8766}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--www', type=Path, required=True)
    arguments = parser.parse_args()
    install(arguments.archive, arguments.www)
