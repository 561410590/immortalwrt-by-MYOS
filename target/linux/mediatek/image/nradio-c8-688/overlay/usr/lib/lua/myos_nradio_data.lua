local M = require("myos_nradio_data_generic")
local identity = require("myos_c8_identity")
local modem = require("myos_c8_modem")
local generic_led = M.led

M.identity_get = identity.get
M.device_id = identity.device_id
M.device_code = identity.device_code
M.adopt_device_code = identity.adopt_device_code
M.is_native_nbcpe = function() return false end
M.is_native_c5800 = function() return false end
M.app_device_name = function() return "C8-688" end
M.find_qmodem = function() return "c8", nil, {} end

for _, name in ipairs({ "summary", "cpe_item", "dashboard", "status", "sms", "sms_supported",
    "invalidate", "neighbour" }) do
    M[name] = modem[name]
end
M.apply_native_cpesel = function(slot, mode)
    if modem.apply_native_cpesel then return modem.apply_native_cpesel(slot, mode) end
    return false
end
M.sms_record = function() return nil end
M.sms_record_ack = function() return false end
M.led_supported = function()
    local file = io.open("/sys/class/leds/hc:blue:status/brightness", "r")
    if not file then return false end
    file:close()
    return true
end
M.led_tick = function()
    return os.execute("/usr/libexec/myos-c8-led apply >/dev/null 2>&1") == 0
end
-- Reuse the APP's partial-update semantics; both themes use this same UCI state.
M.led = function(data) return generic_led(data) end

return M
