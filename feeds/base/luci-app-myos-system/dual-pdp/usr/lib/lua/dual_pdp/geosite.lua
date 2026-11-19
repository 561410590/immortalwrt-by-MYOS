local M={}
local paths={'/usr/share/v2ray/geosite.dat','/usr/share/dual-pdp/geosite.dat'}
local function check(v) if not v then error('GeoSite 文件格式异常或超出大小限制',0) end;return v end
local function varint(s,p,last)
 local n,m=0,1
 for _=1,5 do
  local b=check(p<=last and s:byte(p));p=p+1;n=n+(b%128)*m
  if b<128 then return n,p end;m=m*128
 end
 check(false)
end
local function fields(s,first,last,visit)
 local p=first
 while p<=last do
  local tag,value;tag,p=varint(s,p,last);value,p=varint(s,p,last)
  local wire=tag%8;local key=math.floor(tag/8);check(key>0)
  if wire==0 then visit(key,wire,value)
  else
   check(wire==2 and value<=last-p+1)
   if visit(key,wire,p,p+value-1)==false then return end
   p=p+value
  end
 end
end
local function available()
 for _,candidate in ipairs(paths) do
  local f=io.open(candidate,'rb')
  if f then local len=f:seek('end');f:close();if len and len>0 then return candidate end end
 end
end
function M.ensure()
 local path=available()
 if path then return path end
 if os.execute('/usr/libexec/dual-pdp-geosite')~=0 then
  local info=io.open('/var/run/dual-pdp-manager/geosite-status');local msg=info and info:read('*l');if info then info:close() end
  error(msg or 'GeoSite 下载失败，请检查网络后重试',0)
 end
 -- A concurrent package installation may have supplied the system copy.
 return assert(available(),'GeoSite 域名库未找到')
end
function M.read(group,emit,path)
 local f=io.open(path or M.ensure(),'rb')
 check(f);local len=f:seek('end');if not len or len>12*1024*1024 then f:close();check(false) end
 f:seek('set',0);local s=f:read('*a');f:close();check(#s==len)
 local found,total=false,0
 fields(s,1,#s,function(k,w,first,last)
  if k~=1 or w~=2 then return end
  local label
  fields(s,first,last,function(i,t,a,b) if i==1 and t==2 then label=s:sub(a,b):upper();return false end end)
  if label~=group then return end
  found=true
  fields(s,first,last,function(i,t,a,b)
   if i~=2 or t~=2 then return end
   local kind,value=0
   fields(s,a,b,function(j,u,x,y)
    if j==1 and u==0 then kind=x elseif j==2 and u==2 then value=s:sub(x,y) end
   end)
   check(value and #value>0 and #value<=253 and kind>=0 and kind<=3)
   if kind==1 then
    -- Accept portable POSIX ERE only. Refuse incompatible RE2 constructs.
    value=value:gsub('%(%?:','(')
    check(not value:find('[%c%s]') and not value:match('\\[%w]') and not value:find('(?',1,true))
   else
    value=value:lower();check(not value:find('[^a-z0-9._%-]'))
   end
   total=total+1;check(total<=16000);emit(kind,value)
  end)
  return false
 end)
 if not found or total==0 then error('GeoSite 没有可用的 '..group..' 分组',0) end
end
return M
