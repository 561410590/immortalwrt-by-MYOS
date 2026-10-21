local modem = require "myos_c8_modem"
local m = SimpleForm("c8_sms", translate("C8-688 短信"))
m.reset = false
m.submit = translate("执行")
local s = m:section(SimpleSection)
local operation = s:option(ListValue, "operation", translate("操作"))
operation:value("read", translate("读取"))
operation:value("send", translate("发送"))
operation:value("del", translate("删除"))
operation.default = "read"
local number = s:option(Value, "phone_num", translate("收件号码"))
number:depends("operation", "send")
local message = s:option(TextValue, "msg", translate("短信内容"))
message.rows = 5
message:depends("operation", "send")
local ids = s:option(Value, "ids", translate("短信索引"))
ids:depends("operation", "del")
local result = s:option(TextValue, "result", translate("短信记录"))
result.readonly, result.rows = true, 16
function result.cfgvalue() return m.sms_result or "" end
function m.handle(self, state, data)
	if state ~= FORM_VALID then return true end
	local reply = modem.sms({ action = data.operation, phone_num = data.phone_num, msg = data.msg, ids = data.ids })
	self.sms_result = require("luci.jsonc").stringify(reply, true)
	if tonumber(reply.code) ~= 0 then self.errmessage = reply.message or translate("短信操作失败") end
	return true
end
return m
