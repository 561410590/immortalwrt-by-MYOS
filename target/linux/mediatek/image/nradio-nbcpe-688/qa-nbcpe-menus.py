#!/usr/bin/env python3
"""Exercise the installed LuCI dispatcher and both theme menu renderers offline."""
import argparse
import json
import os
import re
import subprocess
from pathlib import Path


def checked(command, *, code=None, input=None):
    fd = None
    if code is not None:
        fd = os.memfd_create('nbcpe-menu-fixture', 0)
        os.write(fd, code.encode())
        os.lseek(fd, 0, os.SEEK_SET)
        command = [*command, f'/proc/self/fd/{fd}']
    try:
        result = subprocess.run([str(p) for p in command], input=input, text=True,
                                capture_output=True, pass_fds=(() if fd is None else (fd,)), timeout=45)
        if result.returncode:
            raise RuntimeError(result.stderr[-8000:] or result.stdout[-8000:])
        return result.stdout
    finally:
        if fd is not None:
            os.close(fd)


def lua_literal(value):
    if value is None:
        return 'nil'
    if isinstance(value, bool):
        return 'true' if value else 'false'
    if isinstance(value, (int, float)):
        return str(value)
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, list):
        return '{' + ','.join(lua_literal(v) for v in value) + '}'
    return '{' + ','.join('[' + lua_literal(k) + ']=' + lua_literal(v) for k, v in value.items()) + '}'


LUA_FIXTURE = r'''
local function encode(v)
    local t=type(v)
    if t=='nil' then return 'null' end
    if t=='boolean' or t=='number' then return tostring(v) end
    if t=='string' then
        return '"'..v:gsub('[%z\1-\31\\"]',function(c)
            return string.format('\\u%04x',string.byte(c))
        end)..'"'
    end
    assert(t=='table','non-JSON menu value: '..t)
    local out={} local count=0 local array=true
    for k in pairs(v) do count=count+1 if type(k)~='number' then array=false end end
    array=array and count>0 and #v==count
    if array then
        for i=1,#v do out[#out+1]=encode(v[i]) end
        return '['..table.concat(out,',')..']'
    end
    for k,x in pairs(v) do out[#out+1]=encode(tostring(k))..':'..encode(x) end
    return '{'..table.concat(out,',')..'}'
end
local function forbidden() error('fixture attempted a command or write',2) end
local real_open=io.open
local mockfiles={['/tmp/sysinfo/board_name']='HCMT7981-emmc-NB68\n',
    ['/tmp/sysinfo/model']='NBCPE-688\n',['/tmp/modconf-AK68.conf']='MT5700\n'}
local function reader(data)
    return {read=function(_,format) return data end,close=function() return true end}
end
io.open=function(p,mode)
    assert(not mode or mode=='r' or mode=='rb','write requested')
    if mockfiles[p] then return reader(mockfiles[p]) end
    if p:sub(1,1)=='/' then return real_open(ROOT..p,mode) end
    return real_open(p,mode)
end
io.popen=forbidden os.execute=forbidden os.remove=forbidden os.rename=forbidden
local fs={access=function(p) return META[p]~=nil end,
    stat=function(p,key)
        local m=META[p] if not m then return nil end
        local s={type=m.type=='directory' and 'dir' or 'reg',size=m.size}
        return key and s[key] or s
    end,
    readfile=function(p) return mockfiles[p] end,
    writefile=forbidden,mkdir=forbidden,unlink=forbidden}
local cursor={}
function cursor:get(conf,sect,opt)
    local s=(UCI[conf] or {})[sect]
    if not s then return nil end
    return opt and s[opt] or s['.type']
end
function cursor:get_all(conf,sect) return (UCI[conf] or {})[sect] end
function cursor:get_first(conf,kind,opt,default)
    for _,s in pairs(UCI[conf] or {}) do if s['.type']==kind then return s[opt] or default end end
    return default
end
function cursor:foreach(conf,kind,cb)
    for _,s in pairs(UCI[conf] or {}) do if not kind or kind==s['.type'] then cb(s) end end
end
cursor.set=forbidden cursor.delete=forbidden cursor.commit=forbidden
cursor.save=forbidden cursor.load=function() return true end
cursor.set_session_id=function() end
local uci={cursor=function() return cursor end,set_session_id=function() end}
local i18n={translate=function(s) return s end,setlanguage=function() end}
local sys={exec=forbidden,call=forbidden}
local util={shellquote=function(s) return "'"..s:gsub("'","'\\''").."'" end}
local http={}
local jsonc={stringify=encode,parse=function() return nil end}
local cjson={encode=encode,decode=function() return nil end}
_G.fs=fs _G.nixio={fs=fs}
_G.luci={model={uci=uci},sys=sys,http=http,i18n=i18n,util=util}
for name,value in pairs({['nixio.fs']=fs,['nixio']=_G.nixio,['luci.model.uci']=uci,
    ['luci.sys']=sys,['luci.http']=http,['luci.i18n']=i18n,['luci.util']=util,['luci.jsonc']=jsonc,
    ['cjson']=cjson}) do
    package.loaded[name]=value
end
_G.L={http=http,ctx={request_path={},request_args={}},dispatcher={lang='en',
    build_url=function(...) return '/cgi-bin/luci/'..table.concat({...},'/') end,
    menu_json=function() return {} end,is_authenticated=function() return nil end}}
package.path=ROOT..'/usr/lib/lua/?.lua;'..ROOT..'/usr/lib/lua/?/init.lua;'..package.path
local dispatcher=require 'luci.dispatcher'
local out={}
for _,path in ipairs(CONTROLLERS) do
    local ok,result=pcall(dispatcher.process_lua_controller,path)
    assert(ok,path..': '..tostring(result))
    out[path]=result or {}
end
print(encode(out))
'''


def ucode_function(source, name):
    start = source.index('function ' + name + '(')
    end = source.find('\nfunction ', start + 1)
    return source[start:] if end < 0 else source[start:end]


NODE_FIXTURE = r'''
const assert=require('node:assert/strict'), vm=require('node:vm');
let input=''; process.stdin.on('data',s=>input+=s); process.stdin.on('end',()=>main(JSON.parse(input)));
function main(data) {
  class Element {
    constructor(tag='div'){this.tagName=tag.toUpperCase();this.children=[];this.dataset={};this.attributes={};this.style={};this.value='';this._text='';this.open=false;this.hidden=false;
      const names=new Set();this.classList={add:(...x)=>x.forEach(v=>names.add(v)),remove:(...x)=>x.forEach(v=>names.delete(v)),contains:x=>names.has(x),toggle:(x,on)=>{if(on===undefined)on=!names.has(x);on?names.add(x):names.delete(x);}};}
    set className(s){this._class=s;for(const v of s.split(/\s+/))if(v)this.classList.add(v);} get className(){return this._class||'';}
    set textContent(s){this._text=String(s);this.children=[];} get textContent(){return this._text+this.children.map(c=>c.textContent||'').join('');}
    append(...nodes){for(const n of nodes.flat()){if(n==null)continue;if(typeof n==='string'){const t=new Element('#text');t._text=n;this.append(t);}else if(n.tagName==='#FRAGMENT')this.append(...n.children);else{n.parentElement=this;this.children.push(n);}}}
    appendChild(n){this.append(n);return n;} replaceChildren(...n){this.children=[];this.append(...n);}
    setAttribute(k,v){this.attributes[k]=String(v);if(k==='class')this.className=String(v);if(k==='href')this.href=String(v);}
    removeAttribute(k){delete this.attributes[k];} addEventListener(){} focus(){} getClientRects(){return [1];}
    matches(s){if(s==='a')return this.tagName==='A';if(s==='details[open]')return this.tagName==='DETAILS'&&this.open;if(s==='[aria-current="page"]')return this.attributes['aria-current']==='page';if(s.startsWith('.'))return this.classList.contains(s.slice(1));return false;}
    querySelectorAll(s){let out=[];for(const n of this.children){if(n.matches(s))out.push(n);out.push(...n.querySelectorAll(s));}return out;}
    querySelector(s){if(s.startsWith(':scope > '))return this.children.find(n=>n.matches(s.slice(9)))||null;return this.querySelectorAll(s)[0]||null;}
  }
  const registry={};for(const s of ['#sidebar','#menu-tree','#menu-search','.sidebar-backdrop','.menu-empty','#menu-source','#close-menu','#mainmenu','#tabmenu','a.showSide','.darkMask'])registry[s]=new Element();
  const document={body:new Element('body'),querySelector:s=>registry[s]||null,querySelectorAll:()=>[],getElementById:()=>null,
    createElement:t=>new Element(t),createElementNS:(ns,t)=>{const n=new Element(t);n.namespaceURI=ns;return n;},createDocumentFragment:()=>new Element('#fragment'),addEventListener(){}};
  const L={isObject:v=>v!==null&&typeof v==='object'&&!Array.isArray(v),naturalCompare:(a,b)=>a.localeCompare(b),
    url:(...p)=>'/cgi-bin/luci/'+p.join('/'),env:{requestpath:['admin'],dispatchpath:data.theme==='myos'?['admin','status','nbcpe']:['admin','modeminfo']},bind:(fn,self)=>fn.bind(self)};
  const context={document,window:{},L,URL,location:{href:'http://192.168.66.1/cgi-bin/luci/',pathname:L.url(...L.env.dispatchpath)},
    matchMedia:()=>({matches:false,addEventListener(){}}),_:(s)=>s,console,baseclass:{singleton:o=>o,extend:o=>o},session:{},request:{},dom:{}};
  const start=data.ui.indexOf('const UIMenu=');const end=data.ui.indexOf('const UITable=',start);
  assert(start>=0&&end>start,'installed UI menu helper missing');
  const menu=vm.runInNewContext(data.ui.slice(start,end)+'\nUIMenu;',context);
  menu.menu=data.tree; context.ui={menu,createHandlerFn:()=>()=>{}};
  if(data.theme==='myos'){
    vm.runInNewContext(data.theme_js,context,{filename:'installed MYOS menu.js'});
    context.window.C2000Menu.setTree(data.tree,{getChildren:n=>menu.getChildren(n),url:p=>L.url(...p),homeUrl:L.url('admin','status','nbcpe'),activePath:context.location.pathname});
  }else{
    context.E=(tag,attrs,children)=>{if(Array.isArray(tag)){const f=new Element('#fragment');f.append(...tag);return f;}const n=new Element(tag);for(const[k,v]of Object.entries(attrs||{}))if(k!=='click')n.setAttribute(k,v);n.append(...(Array.isArray(children)?children:[children]));return n;};
    const renderer=vm.runInNewContext('(function(){'+data.theme_js+'\n})()',context,{filename:'installed MWRT menu-argon.js'});
    renderer.render(data.tree);
  }
  const nav=registry[data.theme==='myos'?'#menu-tree':'#mainmenu'];
  const links=nav.querySelectorAll('a').map(n=>({href:n.href,title:n.textContent.trim()}));
  const homes=links.filter(n=>/首页|\bhome\b/i.test(n.title));
  assert.equal(homes.length,1,JSON.stringify({theme:data.theme,homes,links}));
  const target=data.theme==='myos'?'admin/status/nbcpe':'admin/modeminfo';
  assert.equal(homes[0].href,L.url(...target.split('/')));
  assert(links.some(n=>n.href===L.url('admin','status','overview')&&n.title==='系统状态'),'base status page was lost or still titled home');
  assert(!links.some(n=>/quickstart|quitstart|router3d|\/status\/(?:c5800|c2000max)(?:\/|$)/i.test(n.href)),'legacy or QuickStart menu visible');
  console.log(JSON.stringify({theme:data.theme,visible_links:links.length,home:homes[0],overview:'系统状态',quickstart:0}));
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--root', type=Path)
    args = parser.parse_args()
    source = args.source
    root = args.root or source / 'build_dir/target-aarch64_cortex-a53_musl/linux-mediatek_filogic/root-nbcpe-myos'
    assets = source / 'target/linux/mediatek/image/nradio-nbcpe-688'
    host = assets / 'ucode-host'
    lua = source / 'staging_dir/hostpkg/bin/lua'
    dispatcher = (root / 'usr/share/ucode/luci/dispatcher.uc').read_text()
    functions = '\n'.join(ucode_function(dispatcher, name) for name in (
        'check_fs_depends', 'check_uci_depends_options', 'check_uci_depends_section',
        'check_uci_depends', 'check_depends', 'check_acl_depends', 'build_pagetree',
        'ctx_append', 'node_weight', 'clone', 'resolve_firstchild'))
    controller_base = root / 'usr/lib/lua/luci/controller'
    controllers = sorted(controller_base.glob('*.lua')) + sorted(controller_base.glob('*/*.lua'))
    json_files = sorted((root / 'usr/share/luci/menu.d').glob('*.json'))
    json_data = {'/' + p.relative_to(root).as_posix(): json.loads(p.read_text()) for p in json_files}
    controller_names = ['/' + p.relative_to(root).as_posix() for p in controllers]
    metadata = {}
    for p in root.rglob('*'):
        try:
            metadata['/' + p.relative_to(root).as_posix()] = {
                'type': 'directory' if p.is_dir() else 'file', 'size': p.lstat().st_size,
                'user_exec': bool(p.lstat().st_mode & 0o100)}
        except OSError:
            pass
    outputs = []
    for theme, media in [('myos', '/luci-static/c2000max-ui'), ('mwrt', '/luci-static/argon')]:
        uci = {'luci': {'main': {'.type': 'core', '.name': 'main', 'mediaurlbase': media}},
               'network': {'cpe': {'.type': 'interface', '.name': 'cpe', 'proto': 'wwan', 'mode': 'odu', 'disabled': '0'}},
               'oem': {'feature': {'.type': 'feature', '.name': 'feature', 'cpe': '1', 'cpeth': '1'}}}
        lua_code = 'local ROOT=' + lua_literal(str(root)) + '\nlocal META=' + lua_literal(metadata)
        lua_code += '\nlocal UCI=' + lua_literal(uci) + '\nlocal CONTROLLERS=' + lua_literal(controller_names) + '\n' + LUA_FIXTURE
        lua_data = json.loads(checked([lua], code=lua_code))
        dataset = {'files': list(json_data) + controller_names, 'json': json_data,
                   'lua': lua_data, 'meta': metadata, 'uci': uci}
        uc_code = 'let fixture=' + json.dumps(dataset, ensure_ascii=False) + ';\n'
        uc_code += r'''
let indexcache=null;
function glob(...patterns) { return fixture.files; }
function stat(path) { return fixture.meta[path]; }
function lsdir(path) { return fixture.meta[path]?.type=='directory' ? ['fixture'] : []; }
function read_jsonfile(path) { return fixture.json[path]; }
function load_luabridge() { return true; }
let runtime={call: (module,method,path)=>fixture.lua[path]};
let uci={get:(c,s,o)=>fixture.uci[c]?.[s]?.[o],get_all:(c,s)=>fixture.uci[c]?.[s],load:()=>true,
    foreach:(c,t,cb)=>{for(let name,s in fixture.uci[c]) if(t==null||s['.type']==t) if(cb(s)===false)break;}};
function is_authenticated() { return {acls:{'access-group':{}}}; }
'''
        uc_code += functions
        uc_code += r'''
let tree=build_pagetree();
let acl={};
function gather(node) { for(let a in node.depends?.acl) acl[a]={read:true,write:true}; for(let k,c in node.children)gather(c); }
gather(tree);
let ctx={path:[]};
resolve_firstchild(tree.children.admin,{acls:{'access-group':acl}},false,ctx);
printf('%J\n',{tree,landing:join('/',ctx.path)});
'''
        built = json.loads(checked([host / 'ucode', '-L', host], code=uc_code))
        expected = 'status/nbcpe' if theme == 'myos' else 'modeminfo'
        assert built['landing'] == expected, (theme, built['landing'], expected)
        js_path = ('www/luci-static/c2000max-ui/menu.js' if theme == 'myos'
                   else 'www/luci-static/resources/menu-argon.js')
        payload = {'theme': theme, 'tree': built['tree'],
                   'ui': (root / 'www/luci-static/resources/ui.js').read_text(),
                   'theme_js': (root / js_path).read_text()}
        result = json.loads(checked(['node', '-e', NODE_FIXTURE], input=json.dumps(payload, ensure_ascii=False)))
        result.update({'dispatcher_landing': 'admin/' + built['landing'],
                       'installed_json_files': len(json_files), 'installed_controllers': len(controllers)})
        outputs.append(result)
    print(json.dumps({'menus': outputs, 'result': 'PASS',
                      'fixture': 'installed Lua process_lua_controller, ucode build_pagetree/dependencies/firstchild, UI getChildren and both JS renderers; mocked NBCPE board/theme/ACL, blocked commands and writes'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
