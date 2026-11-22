local M = {}
local modem = require "luci.controller.modem"

local lines = {
    {name="poe", prefix="cellular_cpe1_"},
    {name="main", prefix="cellular_cpe_"},
    {name="wan", prefix="wired_wan_"},
}
local to_native = {cpe1="poe", cpe="main", wan="wan"}
local to_stock = {poe="cpe1", main="cpe", wan="wan"}

function M.read(data)
    local state = modem.route_state()
    local positions, present = {}, {}
    for index, name in ipairs(state.priority) do positions[name] = index end
    for _, item in ipairs(data.wans) do present[item.name] = true end
    if not present.wan then
        data.wans[#data.wans+1] = {name="wan", type="wired", translate="WAN", index=#data.wans+1}
    end
    for _, item in ipairs(data.wans) do
        local name = to_native[item.name]
        if name then
            local line = state.lines[name]
            item.disabled = line.enabled and "0" or "1"
            item.pos = positions[name] or item.index
            item.weight = line.weight
        end
    end
    table.sort(data.wans, function(a,b) return tonumber(a.pos) < tonumber(b.pos) end)
    data.net_kind = state.mode == "Single" and "" or state.mode
    data.dividing = state.dividing
    data.dividing_default = to_stock[state.default] or ""
    return data
end

function M.apply()
    local http = require "luci.http"
    local state = modem.route_state()
    local form = http.formvalue() or {}
    state.mode = form.net_kind == "" and "Single" or form.net_kind or state.mode
    state.dividing = form.dividing or state.dividing
    state.default = to_native[form.dividing_default] or state.default
    local submitted, stored, priority = {}, {}, {}
    for index, name in ipairs(state.priority) do stored[name] = index end
    for index, line in ipairs(lines) do
        local name, prefix = line.name, line.prefix
        local configured = state.lines[name]
        if form[prefix .. "disabled"] == "0" then configured.enabled = true end
        if form[prefix .. "disabled"] == "1" then configured.enabled = false end
        configured.weight = form[prefix .. "weight"] or configured.weight
        if configured.enabled and state.mode == "Priority" then
            submitted[name] = tonumber(form[prefix .. "metric"])
        end
        stored[name] = stored[name] or index
        priority[#priority+1] = name
    end
    table.sort(priority, function(a,b)
        local first, second = submitted[a] or 100+stored[a], submitted[b] or 100+stored[b]
        if first == second then return stored[a] < stored[b] end
        return first < second
    end)
    state.priority = priority
    if not state.lines[state.default] or not state.lines[state.default].enabled then
        for _, name in ipairs(priority) do
            if state.lines[name].enabled then state.default = name; break end
        end
    end
    for _, option in ipairs({"proto","ipaddr","netmask","gateway","mtu","macaddr","username","password"}) do
        local value = form["wired_wan_" .. option]
        if value ~= nil then state.wan[option] = value end
    end
    if form.wired_wan_peerdns ~= nil then state.wan.peerdns = form.wired_wan_peerdns == "1" end
    if type(form.wired_wan_dns) == "string" then
        state.wan.dns = {}
        for address in form.wired_wan_dns:gmatch("%S+") do
            state.wan.dns[#state.wan.dns+1] = address
        end
    end
    return modem.action_route_set(state)
end

return M
