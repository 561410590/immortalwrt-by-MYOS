#!/usr/bin/lua

local nixio = require "nixio"
local shared = require "myos_nradio_data"

local PORT = 8888
local MAX_REQUEST = 1023
local ACK = "OK\r\n"

local server, message = nixio.socket("inet", "stream")
if not server then
	io.stderr:write("unable to create APP discovery socket: ",
		tostring(message), "\n")
	os.exit(1)
end

server:setopt("socket", "reuseaddr", 1)
local bound, bind_message = server:bind("0.0.0.0", PORT)
if not bound then
	io.stderr:write("unable to bind APP discovery port: ",
		tostring(bind_message), "\n")
	server:close()
	os.exit(1)
end

local listening, listen_message = server:listen(100)
if not listening then
	io.stderr:write("unable to listen on APP discovery port: ",
		tostring(listen_message), "\n")
	server:close()
	os.exit(1)
end

while true do
	pcall(shared.led_tick)
	local ready = nixio.poll({ { fd = server, events = nixio.poll_flags("in") } }, 10000)
	if ready and ready > 0 then
		local client = server:accept()
		if client then
			client:setopt("socket", "rcvtimeo", 3, 0)
			client:setopt("socket", "sndtimeo", 3, 0)
			local request = client:recv(MAX_REQUEST)
			if request and #request > 0 then client:send(ACK) end
			client:close()
		end
	elseif not ready then
		nixio.nanosleep(1)
	end
end
