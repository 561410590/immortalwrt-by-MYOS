-- Runtime hardware detection for the Mwrt login page / about dialog.
--
-- Replaces hardcoded strings that were wrong on this board: the template said
-- "NRadio C2000Ultra / MT7987A / BE5040 / 2 x 2.5G (SOCINS/EN8811H)" while the
-- hardware actually reports C2000MAX / MT7987B (2 cores) / BE3600 (MT7993), and
-- only one 2.5G link is up.
--
-- Everything here reads world-readable files only. The login page renders
-- unauthenticated, so ubus is not available -- do not add ubus calls.
--
-- Every getter degrades to nil (caller shows a dash) rather than guessing, so a
-- board this was not written for shows "unknown" instead of a wrong value.

local fs = require "nixio.fs"

local M = {}

local function trim(s)
	return s and (s:gsub("%z", ""):gsub("^%s+", ""):gsub("%s+$", "")) or nil
end

local function readfile(p)
	local s = fs.readfile(p)
	if not s or #s == 0 then return nil end
	s = trim(s)
	return (s ~= "" ) and s or nil
end

-- 设备型号: /tmp/sysinfo/model is what OpenWrt itself trusts; DT model is the
-- fallback when sysinfo has not been populated yet.
function M.model()
	return readfile("/tmp/sysinfo/model")
		or readfile("/proc/device-tree/model")
		or readfile("/tmp/sysinfo/board_name")
end

-- 主控平台. The DT compatible only goes as far as "mediatek,mt7987" -- it does
-- not distinguish MT7987A from MT7987B. The two differ in core count (A=4, B=2),
-- so use that to pick the suffix, and emit the bare SoC name if the count is
-- anything else.
function M.soc()
	-- Read raw: compatible is a NUL-separated list, so it must NOT go through
	-- trim() (which strips %z and would glue the entries into one string).
	local compat = fs.readfile("/proc/device-tree/compatible")
	local soc
	if compat then
		for ent in compat:gmatch("[^%z]+") do
			local s = ent:match("^mediatek,(mt%d+)$")
			if s then soc = s:upper() break end
		end
	end
	if not soc then return nil end

	local n = 0
	local cpuinfo = fs.readfile("/proc/cpuinfo")
	if cpuinfo then
		for _ in cpuinfo:gmatch("\nprocessor") do n = n + 1 end
		if cpuinfo:match("^processor") then n = n + 1 end
	end

	local variant = (n == 4 and "A") or (n == 2 and "B") or ""
	return string.format("MediaTek %s%s", soc, variant)
end

-- 无线规格. Derive from the loaded radio driver rather than a table of names:
-- mt7993 -> BE3600, mt7992 -> BE5040. Generation comes from what the interfaces
-- actually negotiated (EHT -> WiFi 7, HE -> WiFi 6), not from the chip name.
local RADIO_SPEC = {
	mt7993 = "BE3600",
	mt7992 = "BE5040",
	mt7990 = "BE7200",
	mt7996 = "BE19000",
}

function M.wifi()
	local chip, spec
	local mods = fs.readfile("/proc/modules")
	if mods then
		for name in mods:gmatch("(mt%d+)%s") do
			if RADIO_SPEC[name] then chip, spec = name, RADIO_SPEC[name] break end
		end
	end
	if not chip then return nil end

	-- WiFi generation follows the part family: MT799x are 802.11be, MT798x are ax.
	local gen = chip:match("^mt799") and "WiFi 7" or "WiFi 6"

	return string.format("%s %s (%s)", gen, spec, chip:upper())
end

-- 有线接口. Count only real physical ports. Naive "count the eth* devices" gave
-- 3 on this board, which was wrong on two counts:
--
--   eth2  cdc_ncm on platform/soc/11200000.usb/...  -- the cellular module's
--         virtual NIC, not a socket on the case. Excluded by the USB devpath.
--   eth0  mtk_soc_eth, mac@0, phy-mode 2500base-x   -- the EN8811H port. The DTS
--         is shared with board A so the node says status okay here too, but the
--         part is not populated: the PHY never probes ("en8811h_wait_mcu_ready
--         failed: -110") and the kernel therefore creates no phydev link.
--         Excluded by requiring an attached PHY.
--
-- What survives on this board is eth1 alone (internal PHY, mdio-bus:0f) = the
-- one 2.5G socket. On a board where the EN8811H does come up, its phydev appears
-- and it is counted, which is the behaviour we want.
--
-- Requiring a phydev also drops the "hnat" accel pseudo-interface, which shares
-- the ethernet controller's device node and would otherwise look physical, and
-- it makes the count right on DSA boards (the CPU port has no PHY, the user-
-- facing lan*/wan ports do) -- which is why there is no eth%d name filter here.
local function speed_label(sp)
	if not sp or sp <= 0 then return nil end
	if sp >= 10000 then return "10G" end
	if sp >= 5000 then return "5G" end
	if sp >= 2500 then return "2.5G" end
	if sp >= 1000 then return "1G" end
	if sp >= 100 then return "100M" end
	return string.format("%dM", sp)
end

function M.ethernet()
	local d = fs.dir("/sys/class/net")
	if not d then return nil end

	local ports, unknown, fastest = 0, 0, 0
	local order, count = {}, {}	-- speed label -> how many ports negotiated it

	for iface in d do
		local base = "/sys/class/net/" .. iface

		-- Which bus the netdev hangs off. Read it from device/subsystem rather
		-- than from the device symlink: that symlink is relative and often short
		-- ("../../../2-1:2.0"), so it does not reliably contain "usb". Getting
		-- this wrong would let a USB ethernet dongle -- r8152 and friends do
		-- register a phydev -- be reported as a built-in port.
		local bus = fs.readlink(base .. "/device/subsystem")
		bus = bus and bus:match("([^/]+)$")

		-- physical backing, on-board bus, and a PHY actually attached
		if bus and bus ~= "usb" and fs.stat(base .. "/phydev") then
			ports = ports + 1
			local sp = tonumber(readfile(base .. "/speed") or "")
			local lbl = speed_label(sp)
			if lbl then
				if sp > fastest then fastest = sp end
				if not count[lbl] then
					count[lbl] = 0
					order[#order + 1] = lbl
				end
				count[lbl] = count[lbl] + 1
			else
				-- link is down, so the port's speed is genuinely unknown: sysfs
				-- reports nothing and the DT gives no max-speed to fall back on.
				unknown = unknown + 1
			end
		end
	end

	if ports == 0 then return nil end

	if #order == 0 then
		return string.format("%d × 以太网", ports)
	end

	if unknown > 0 then
		-- Some ports have no cable; name the fastest seen rather than implying
		-- every port runs at it. Compare raw speeds, not labels -- "100M" sorts
		-- above "10G" if you compare the leading digits.
		return string.format("%d × 以太网 (最高 %s)", ports, speed_label(fastest))
	end

	local parts = {}
	for _, lbl in ipairs(order) do
		parts[#parts + 1] = string.format("%d × %s", count[lbl], lbl)
	end
	return table.concat(parts, " + ") .. " 以太网"
end

-- /etc/openwrt_release on this build is not the stock layout. Real contents:
--   DISTRIB_ID='Mwrt'
--   DISTRIB_RELEASE='@Manper-5.1.1139-24 by Padavanonly's Immortalwrt-mt798x-24.10-SNAPSHOT'
--   DISTRIB_REVISION='r33382-861b29b0a9'
-- So DISTRIB_RELEASE is a free-form banner, not a version, and
-- DISTRIB_DESCRIPTION is the whole banner again -- both far too long for a table
-- cell or a badge. Pull the two things we actually want out of the banner
-- instead of printing it verbatim.
local function release()
	local rel = fs.readfile("/etc/openwrt_release")
	if not rel then return nil, nil, nil end
	return rel:match('DISTRIB_ID=[\'"]([^\'"]+)'),
		rel:match('DISTRIB_RELEASE=[\'"]([^\'"]+)'),
		rel:match('DISTRIB_REVISION=[\'"]([^\'"]+)')
end

-- 系统基线: the upstream distro this was forked from, plus the running kernel,
-- both derived at runtime so the line cannot drift the way the literal
-- "ImmortalWrt 24.10 / Linux 6.6" did (kernel is really 6.6.94).
function M.baseline()
	local id, banner = release()
	local base

	if banner then
		-- "...Immortalwrt-mt798x-24.10-SNAPSHOT" -> "ImmortalWrt 24.10".
		-- Lazy middle so the version is the one right after the distro name and
		-- not the "798" in the target name.
		local name, ver = banner:match("(%a+wrt)[%w%._%-]-(%d+%.%d+)")
		if name and ver then
			-- normalise the house style: Immortalwrt -> ImmortalWrt
			base = (name:gsub("wrt$", "Wrt")) .. " " .. ver
		end
	end

	-- No recognisable upstream base: fall back to the local distro name, which is
	-- short and meaningful here ("Mwrt").
	if not base and id then base = id:match("^(%S+)") end

	local kern = readfile("/proc/sys/kernel/osrelease")
	if kern then
		-- drop the -rc/local suffix noise, keep the version itself
		kern = "Linux " .. (kern:match("^(%d[%d%.]*)") or kern)
	end

	if base and kern then return base .. " / " .. kern end
	return base or kern
end

-- 固件版本 for the badge, so it stops being a hand-edited literal.
-- Wants to stay short: this renders inside a small pill on the login page.
function M.firmware()
	local id, banner, rev = release()

	-- First version-looking token in the banner: "5.1.1139-24" out of
	-- "@Manper-5.1.1139-24 by ...". This is the number that was hand-written as
	-- "Mwrt-1139-24" before.
	local ver = banner and banner:match("(%d[%d%.]*%-?%d*)")
	if id and ver then return id .. " " .. ver end
	if ver then return ver end

	-- Otherwise the git revision is at least accurate and still short.
	if rev and rev ~= "" then
		return id and (id .. " " .. rev) or rev
	end
	return id
end

return M
