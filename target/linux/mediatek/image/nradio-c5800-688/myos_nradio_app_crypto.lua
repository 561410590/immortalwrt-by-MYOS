local M = {}
local json = require "cjson"

local function native_crypto(mode, value)
	local quoted = "'" .. tostring(value):gsub("'", "'\\''") .. "'"
	local pipe = io.popen("/usr/sbin/nradio_crypto " .. mode .. " -s " .. quoted, "r")
	if not pipe then return nil end
	local result = pipe:read("*a")
	pipe:close()
	return result ~= "" and result or nil
end

local IP = {
	58,50,42,34,26,18,10,2,60,52,44,36,28,20,12,4,
	62,54,46,38,30,22,14,6,64,56,48,40,32,24,16,8,
	57,49,41,33,25,17,9,1,59,51,43,35,27,19,11,3,
	61,53,45,37,29,21,13,5,63,55,47,39,31,23,15,7
}

local FP = {
	40,8,48,16,56,24,64,32,39,7,47,15,55,23,63,31,
	38,6,46,14,54,22,62,30,37,5,45,13,53,21,61,29,
	36,4,44,12,52,20,60,28,35,3,43,11,51,19,59,27,
	34,2,42,10,50,18,58,26,33,1,41,9,49,17,57,25
}

local E = {
	32,1,2,3,4,5,4,5,6,7,8,9,
	8,9,10,11,12,13,12,13,14,15,16,17,
	16,17,18,19,20,21,20,21,22,23,24,25,
	24,25,26,27,28,29,28,29,30,31,32,1
}

local P = {
	16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
	2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25
}

local PC1 = {
	57,49,41,33,25,17,9,1,58,50,42,34,26,18,
	10,2,59,51,43,35,27,19,11,3,60,52,44,36,
	63,55,47,39,31,23,15,7,62,54,46,38,30,22,
	14,6,61,53,45,37,29,21,13,5,28,20,12,4
}

local PC2 = {
	14,17,11,24,1,5,3,28,15,6,21,10,
	23,19,12,4,26,8,16,7,27,20,13,2,
	41,52,31,37,47,55,30,40,51,45,33,48,
	44,49,39,56,34,53,46,42,50,36,29,32
}

local SHIFTS = { 1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1 }

local SBOX = {
	{14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7,0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8,4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0,15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13},
	{15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10,3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5,0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15,13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9},
	{10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8,13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1,13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7,1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12},
	{7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15,13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9,10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4,3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14},
	{2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9,14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6,4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14,11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3},
	{12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11,10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8,9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6,4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13},
	{4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1,13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6,1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2,6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12},
	{13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7,1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2,7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8,2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11}
}

local B64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local B64 = {}
do
	local chars = B64_CHARS
	for i = 1, #chars do B64[chars:sub(i, i)] = i - 1 end
end

local function base64_encode(s)
	local out = {}
	for i = 1, #s, 3 do
		local a, b, c = s:byte(i, i + 2)
		local value = a * 65536 + (b or 0) * 256 + (c or 0)
		for shift = 18, 0, -6 do
			local index = math.floor(value / (2 ^ shift)) % 64 + 1
			out[#out + 1] = (shift == 6 and not b or shift == 0 and not c)
				and "=" or B64_CHARS:sub(index, index)
		end
	end
	return table.concat(out)
end

local function base64_decode(s)
	s = tostring(s or ""):gsub("%s", "")
	local out, val, bits = {}, 0, -8
	for i = 1, #s do
		local c = s:sub(i, i)
		if c == "=" then break end
		local n = B64[c]
		if n == nil then return nil end
		val = (val * 64 + n) % 16777216
		bits = bits + 6
		if bits >= 0 then
			local b = math.floor(val / (2 ^ bits)) % 256
			out[#out + 1] = string.char(b)
			bits = bits - 8
		end
	end
	return table.concat(out)
end

local function str_to_bits(s)
	local bits = {}
	for i = 1, #s do
		local b = s:byte(i)
		for j = 7, 0, -1 do bits[#bits + 1] = math.floor(b / (2 ^ j)) % 2 end
	end
	return bits
end

local function bits_to_str(bits)
	local out = {}
	for i = 1, #bits, 8 do
		local b = 0
		for j = 0, 7 do b = b * 2 + (bits[i + j] or 0) end
		out[#out + 1] = string.char(b)
	end
	return table.concat(out)
end

local function permute(bits, tab)
	local out = {}
	for i = 1, #tab do out[i] = bits[tab[i]] or 0 end
	return out
end

local function rotate_left(bits, n)
	local out, len = {}, #bits
	for i = 1, len do out[i] = bits[((i + n - 1) % len) + 1] end
	return out
end

local function xor_bits(a, b)
	local out = {}
	for i = 1, #a do out[i] = (a[i] == b[i]) and 0 or 1 end
	return out
end

local function join_bits(a, b)
	local out = {}
	for i = 1, #a do out[#out + 1] = a[i] end
	for i = 1, #b do out[#out + 1] = b[i] end
	return out
end

local function subkeys(key)
	local keybits = permute(str_to_bits(key), PC1)
	local c, d = {}, {}
	for i = 1, 28 do c[i] = keybits[i]; d[i] = keybits[i + 28] end
	local keys = {}
	for round = 1, 16 do
		c = rotate_left(c, SHIFTS[round])
		d = rotate_left(d, SHIFTS[round])
		keys[round] = permute(join_bits(c, d), PC2)
	end
	return keys
end

local DEFAULT_SUBKEYS = subkeys("96784c1f")
local ok_nixio, nixio = pcall(require, "nixio")
local bit = ok_nixio and nixio.bit
local SPBOX, ROUND_GROUPS = {}, {}
if bit then
	for round = 1, 16 do
		ROUND_GROUPS[round] = {}
		for box = 1, 8 do
			local value = 0
			for j = 1, 6 do value = value * 2 + DEFAULT_SUBKEYS[round][(box - 1) * 6 + j] end
			ROUND_GROUPS[round][box] = value
		end
	end
	for box = 1, 8 do
		SPBOX[box] = {}
		for input = 0, 63 do
			local row = math.floor(input / 32) * 2 + input % 2
			local col = math.floor(input / 2) % 16
			local val = SBOX[box][row * 16 + col + 1]
			local output = 0
			for j = 1, 32 do
				local source = P[j] - (box - 1) * 4
				local digit = source >= 1 and source <= 4 and math.floor(val / (2 ^ (4 - source))) % 2 or 0
				output = output * 2 + digit
			end
			SPBOX[box][input] = output
		end
	end
end

local function feistel_word(r, key)
	local band, bor, bxor, rshift, lshift = bit.band, bit.bor, bit.bxor, bit.rshift, bit.lshift
	local value = SPBOX[1][bxor(bor(lshift(band(r, 1), 5), rshift(r, 27)), key[1])]
	for box = 2, 7 do
		value = bxor(value, SPBOX[box][bxor(band(rshift(r, 31 - box * 4), 63), key[box])])
	end
	return bxor(value, SPBOX[8][bxor(bor(lshift(band(r, 31), 1), rshift(r, 31)), key[8])])
end

local function feistel(r, key)
	local x = xor_bits(permute(r, E), key)
	local s = {}
	for box = 1, 8 do
		local o = (box - 1) * 6
		local row = x[o + 1] * 2 + x[o + 6]
		local col = x[o + 2] * 8 + x[o + 3] * 4 + x[o + 4] * 2 + x[o + 5]
		local val = SBOX[box][row * 16 + col + 1]
		for j = 3, 0, -1 do s[#s + 1] = math.floor(val / (2 ^ j)) % 2 end
	end
	return permute(s, P)
end

local function crypt_block(block, decrypt)
	local bits = permute(str_to_bits(block), IP)
	local l, r = {}, {}
	for i = 1, 32 do l[i] = bits[i]; r[i] = bits[i + 32] end
	if bit then
		local lw, rw = 0, 0
		for i = 1, 32 do lw = lw * 2 + l[i]; rw = rw * 2 + r[i] end
		for step = 1, 16 do
			local round = decrypt and (17 - step) or step
			lw, rw = rw, bit.bxor(lw, feistel_word(rw, ROUND_GROUPS[round]))
		end
		for i = 1, 32 do
			l[i] = math.floor(lw / (2 ^ (32 - i))) % 2
			r[i] = math.floor(rw / (2 ^ (32 - i))) % 2
		end
		return bits_to_str(permute(join_bits(r, l), FP))
	end
	for step = 1, 16 do
		local round = decrypt and (17 - step) or step
		local old_r = r
		r = xor_bits(l, feistel(r, DEFAULT_SUBKEYS[round]))
		l = old_r
	end
	return bits_to_str(permute(join_bits(r, l), FP))
end

function M.decrypt_app_data(data, random)
	if random ~= nil then
		if type(random) ~= "string" or random == "" then return nil end
		return native_crypto("-d", json.encode({ data = data, random = random }))
	end
	local raw = base64_decode(data)
	if not raw or #raw == 0 or (#raw % 8) ~= 0 then return nil end
	local out = {}
	for i = 1, #raw, 8 do out[#out + 1] = crypt_block(raw:sub(i, i + 7), true) end
	local plain = table.concat(out)
	local pad = plain:byte(#plain) or 0
	if pad < 1 or pad > 8 then return nil end
	for i = #plain - pad + 1, #plain do if plain:byte(i) ~= pad then return nil end end
	return plain:sub(1, #plain - pad)
end

function M.encrypt_app_data(plain)
	if type(plain) ~= "string" then return nil end
	local pad = 8 - (#plain % 8)
	local padded = plain .. string.rep(string.char(pad), pad)
	local out = {}
	for i = 1, #padded, 8 do out[#out + 1] = crypt_block(padded:sub(i, i + 7), false) end
	return base64_encode(table.concat(out))
end

function M.encrypt_app_response(plain, random)
	if not random then return { data = M.encrypt_app_data(plain) } end
	local result = native_crypto("-r -e", plain)
	if not result then return nil end
	local ok, envelope = pcall(json.decode, result)
	if not ok or type(envelope) ~= "table" or not envelope.data or not envelope.random then return nil end
	return envelope
end

return M
