-- Mwrt 文件管理 · 传输: 下载/预览(Range)、文本分段读取、流式上传
-- 下载走浏览器原生能力; 不可信内容(HTML/脚本/SVG 等)一律按附件下载或纯文本展示, 不作为可信内容执行。
local nixio = require "nixio"
local fs = require "nixio.fs"
local http = require "luci.http"
local core = require "luci.mwfiles.core"

local M = {}

local CHUNK = 65536

-- 允许 inline 展示的类型(白名单)。其余一律 attachment + octet-stream。
local INLINE = {
	png = "image/png", jpg = "image/jpeg", jpeg = "image/jpeg", gif = "image/gif", webp = "image/webp",
	bmp = "image/bmp", ico = "image/x-icon", avif = "image/avif",
	svg = "image/svg+xml",      -- 只会被 <img> 加载或在 CSP sandbox 下打开, 脚本不会执行
	mp4 = "video/mp4", m4v = "video/mp4", webm = "video/webm", ogv = "video/ogg", mov = "video/quicktime",
	mp3 = "audio/mpeg", m4a = "audio/mp4", aac = "audio/aac", wav = "audio/wav", ogg = "audio/ogg",
	oga = "audio/ogg", opus = "audio/ogg", flac = "audio/flac",
	pdf = "application/pdf",
}
M.INLINE = INLINE

local function pct(s)
	return (s:gsub("[^%w%-%._~]", function(c) return string.format("%%%02X", c:byte()) end))
end

local function ascii_name(s)
	return (s:gsub('[^%w%.%-_ ]', "_"))
end

local function fail(code, msg)
	http.status(code, msg)
	http.prepare_content("text/plain; charset=UTF-8")
	http.header("X-Content-Type-Options", "nosniff")
	http.write(msg .. "\n")
end

-- mode: "dl" 强制下载; "view" 白名单类型内联
function M.serve(rootid, rel, mode)
	local r, err = core.resolve(rootid, rel, true)
	if not r then
		return fail(err.err == "not_found" and 404 or 403, err.msg)
	end
	local st = fs.stat(r.real)
	if not st or st.type ~= "reg" then return fail(400, "这不是一个普通文件") end
	local size = st.size or 0
	local name = r.name or "download"
	local ext = (name:match("%.([^./]+)$") or ""):lower()
	local mime = INLINE[ext]
	local inline = (mode == "view" and mime ~= nil)
	if not inline then mime = "application/octet-stream" end

	local fd = nixio.open(r.real, "r")
	if not fd then return fail(403, "没有权限读取该文件") end

	-- Range(只支持单区间)
	local from, to, partial = 0, size - 1, false
	local rh = http.getenv("HTTP_RANGE")
	if rh and size > 0 then
		local a, b = rh:match("^bytes=(%d*)%-(%d*)$")
		if a then
			if a == "" and b ~= "" then
				local n = tonumber(b)
				from = math.max(0, size - n); to = size - 1
			elseif a ~= "" then
				from = tonumber(a)
				to = (b ~= "") and math.min(tonumber(b), size - 1) or (size - 1)
			end
			if from > to or from >= size then
				fd:close()
				http.status(416, "Range Not Satisfiable")
				http.header("Content-Range", "bytes */" .. size)
				http.prepare_content("text/plain")
				http.write("")
				return
			end
			partial = true
		end
	end
	local len = (size == 0) and 0 or (to - from + 1)

	if partial then
		http.status(206, "Partial Content")
		http.header("Content-Range", string.format("bytes %d-%d/%d", from, to, size))
	else
		http.status(200, "OK")
	end
	http.header("Content-Type", mime)
	http.header("Content-Length", tostring(len))
	-- 只有请求里真的带了 Range(部分 Web 服务器环境不会把它传给脚本)才声明支持分段, 不假装
	if rh then http.header("Accept-Ranges", "bytes") end
	http.header("X-Content-Type-Options", "nosniff")
	http.header("Cache-Control", "private, no-cache")
	local cleaned = select(1, core.utf8_clean(name))
	http.header("Content-Disposition", string.format("%s; filename=\"%s\"; filename*=UTF-8''%s",
		inline and "inline" or "attachment", ascii_name(cleaned), pct(cleaned)))
	if inline and ext ~= "pdf" then
		-- 内联内容只允许被当作媒体/图片使用, 不允许执行任何脚本
		http.header("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; sandbox")
	end

	if len > 0 then
		if from > 0 then fd:seek(from, "set") end
		local left = len
		while left > 0 do
			local chunk = fd:read(math.min(CHUNK, left))
			if not chunk or #chunk == 0 then break end
			http.write(chunk)
			left = left - #chunk
		end
	else
		http.write("")
	end
	fd:close()
end

-- ------------------------------------------------------------ 文本分段读取
local function trim_incomplete_utf8(s)
	-- 去掉末尾被截断的多字节字符(最多 3 字节)
	local n = #s
	for back = 1, math.min(3, n) do
		local c = s:byte(n - back + 1)
		if c >= 0xC0 then
			local need = (c >= 0xF0) and 4 or (c >= 0xE0) and 3 or 2
			if back < need then return s:sub(1, n - back) end
			return s
		elseif c < 0x80 then
			return s
		end
	end
	return s
end

function M.read(rootid, rel, offset, limit)
	local r, err = core.resolve(rootid, rel, true)
	if not r then return err end
	local st = fs.stat(r.real)
	if not st or st.type ~= "reg" then return core.err("not_file", "这不是一个普通文件") end
	offset = math.max(0, tonumber(offset) or 0)
	limit = math.min(262144, math.max(1024, tonumber(limit) or 131072))
	local size = st.size or 0
	local fd = nixio.open(r.real, "r")
	if not fd then return core.err("denied", "没有权限读取该文件") end
	if offset > 0 then fd:seek(offset, "set") end
	local data = fd:read(limit) or ""
	fd:close()
	if data:find("\0", 1, true) then
		return { ok = true, binary = true, size = size }
	end
	local eof = (offset + #data) >= size
	if not eof then data = trim_incomplete_utf8(data) end
	local text, bad = core.utf8_clean(data)
	return { ok = true, text = text, offset = offset, next = offset + #data, size = size, eof = eof, bad = bad }
end

-- ------------------------------------------------------------ 上传前检查(冲突/空间/权限), 不读请求体
function M.upcheck(rootid, rel, name, size, root_settings)
	local st = core.settings()
	if not core.valid_name(name) then return core.err("bad_name", "文件名不合法") end
	local d, err = core.resolve(rootid, rel, true)
	if not d then return err end
	local dst = fs.stat(d.real)
	if not dst or dst.type ~= "dir" then return core.err("not_dir", "目标不是目录") end
	local ok, e2 = core.writable_root(d.root)
	if not ok then return e2 end
	local tk, te = core.top_ok(d, true)
	if not tk then return te end
	size = tonumber(size) or 0
	if st.max_upload_mb > 0 and size > st.max_upload_mb * 1048576 then
		return core.err("toolarge", string.format("文件超过了管理员设置的上传上限(%d MB)", st.max_upload_mb))
	end
	local v = core.vol_stat(d.root.path, d.root.kind == "remote")
	if v and v.free and size + 1048576 > v.free then
		return core.err("nospace", "目标存储卷剩余空间不足", { free = v.free })
	end
	local exist = fs.lstat(d.real .. "/" .. name)
	local res = { ok = true, free = v and v.free, max_mb = st.max_upload_mb }
	if exist then
		res.exists = true
		res.is_dir = exist.type == "dir"
		res.suggest = core.unique_name(d.real, name)
	end
	return res
end

-- ------------------------------------------------------------ 流式上传(请求体为原始字节流)
-- q: { root, path, name, conflict(overwrite|rename|fail), size, mtime }
function M.upload(q)
	local chk = M.upcheck(q.root, q.path, q.name, q.size)
	if not chk.ok then return chk end
	local d = core.resolve(q.root, q.path, true)
	local gok, ge = core.gcheck("write", { d.real })
	if not gok then return ge end
	local name = q.name
	local conflict = q.conflict
	local declared = tonumber(q.size)

	if chk.exists then
		if chk.is_dir then return core.err("exists_dir", "已存在同名文件夹, 无法覆盖") end
		if conflict == "rename" then name = chk.suggest
		elseif conflict ~= "overwrite" then return core.err("exists", "已存在同名文件") end
	end

	local dir = d.real
	-- 清理这个目录里遗留的 6 小时前的临时分片
	local now = os.time()
	for n in (fs.dir(dir) or function() end) do
		if n:match("^%.mwf%-%x+%.part$") then
			local s = fs.lstat(dir .. "/" .. n)
			if s and s.mtime and now - s.mtime > 21600 then fs.unlink(dir .. "/" .. n) end
		end
	end

	local tmp = dir .. "/.mwf-" .. core.rnd(6) .. ".part"
	local fd, eno, emsg = nixio.open(tmp, nixio.open_flags("wronly", "creat", "excl"), "644")
	if not fd then
		return core.err("denied", "无法在目标目录创建文件(没有权限或空间不足)")
	end

	local st = core.settings()
	local limit = st.max_upload_mb > 0 and st.max_upload_mb * 1048576 or nil
	local state = { total = 0, failed = nil }
	local function failwith(code, msg) if not state.failed then state.failed = { code, msg } end end

	http.setfilehandler(function(meta, chunk, eof)
		if state.failed then return end
		if chunk and #chunk > 0 then
			state.total = state.total + #chunk
			if limit and state.total > limit then
				failwith("toolarge", string.format("文件超过了管理员设置的上传上限(%d MB)", st.max_upload_mb))
				return
			end
			local off = 1
			while off <= #chunk do
				local n, e = fd:write(off == 1 and chunk or chunk:sub(off))
				if not n then
					failwith((e == nixio.const.ENOSPC or e == nixio.const.EDQUOT) and "nospace" or "write", "写入失败: 存储空间不足或存储已断开")
					return
				end
				off = off + n
			end
		end
	end)
	-- 触发请求体解析(原始流会被逐块交给上面的回调)
	http.formvalue("mwfiles-upload")
	fd:close()

	if not state.failed and declared and state.total ~= declared then
		failwith("incomplete", "上传不完整(收到 " .. state.total .. " / " .. declared .. " 字节)")
	end
	if state.failed then
		fs.unlink(tmp)
		return core.err(state.failed[1], state.failed[2])
	end

	local final = dir .. "/" .. name
	-- 最终再核一次: 名字保护 + 覆盖时不能是目录
	local ex = fs.lstat(final)
	if ex and ex.type == "dir" then fs.unlink(tmp) return core.err("exists_dir", "已存在同名文件夹, 无法覆盖") end
	if ex and conflict ~= "overwrite" and conflict ~= "rename" then fs.unlink(tmp) return core.err("exists", "已存在同名文件") end
	if not fs.rename(tmp, final) then
		fs.unlink(tmp)
		return core.err("denied", "保存失败(没有权限或目标不可写)")
	end
	local mt = tonumber(q.mtime)
	if mt and mt > 0 and mt < 4102444800 then fs.utimes(final, mt, mt) end
	return { ok = true, name = name, size = state.total }
end

return M
