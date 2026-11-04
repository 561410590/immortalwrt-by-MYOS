local M = {}

-- Compatibility with the client's three fixed temporary-key RCMD operations.
-- Reconstruct each command; never execute an arbitrary cloud command.
function M.command(cmd, device_id)
	if type(cmd) ~= "string" or #cmd > 8192 then return nil end
	local id = cmd:match("NRSSH_([0-9a-f]+)_")
	if not id or #id ~= 32 then return nil end
	local mac = tostring(device_id or ""):gsub(":", ""):lower()
	if #mac ~= 12 or not mac:match("^[0-9a-f]+$") then return nil end
	mac = mac:gsub("(%x%x)", "%1:"):sub(1, 17)
	local marker = "nradio-session-" .. id
	local receipt = "NRSSH_" .. id
	local guard = "[ \"$(cat /sys/class/net/br-lan/address)\" = '" .. mac .. "' ] && [ \"$(id -u)\" = 0 ]"
	local remove = "sed -i '/ " .. marker .. "$/d' /etc/dropbear/authorized_keys"
	local inspect = "if " .. guard .. " && [ -d /etc/dropbear ] && command -v sed >/dev/null && command -v grep >/dev/null; then printf '" .. receipt .. "_READY\\n'; fi"
	local revoke = "if " .. guard .. "; then " .. remove .. " && ! grep -Fq ' " .. marker .. "' /etc/dropbear/authorized_keys && printf '" .. receipt .. "_REVOKED\\n'; fi"
	local expected, suffix
	if cmd == inspect then
		expected, suffix = inspect, "_READY"
	elseif cmd == revoke then
		expected, suffix = revoke, "_REVOKED"
	else
		local key = cmd:match("'(ssh%-rsa [A-Za-z0-9+/=]+) nradio%-session%-[0-9a-f]+'")
		if not key or #key < 64 or #key > 4096 then return nil end
		local authorize = "if " .. guard .. "; then umask 077; if printf '\\n%s\\n' '" .. key .. " " .. marker ..
			"' >> /etc/dropbear/authorized_keys && grep -Fqx '" .. key .. " " .. marker ..
			"' /etc/dropbear/authorized_keys; then (sleep 120; " .. remove ..
			") </dev/null >/dev/null 2>&1 & printf '" .. receipt .. "_GRANTED\\n'; fi; fi"
		if cmd ~= authorize then return nil end
		expected, suffix = authorize, "_GRANTED"
	end
	local pipe = io.popen(expected .. " 2>&1", "r")
	if not pipe then return "SSH session operation could not start", "1" end
	local output = pipe:read(8192) or ""
	pipe:close()
	if output == receipt .. suffix .. "\n" then return output, "0" end
	return "SSH session operation failed device identity or key verification", "1"
end

return M
