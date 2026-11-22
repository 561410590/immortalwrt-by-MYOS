#!/bin/sh
base="$(cd "$(dirname "$0")/../.." && pwd)"
export NROS_BASE="$base"
export LUA_PATH="$base/lua/?.lua;$base/lua/?/init.lua;;"
export LUA_CPATH="/usr/lib/lua/?.so;$base/lua/?.so;;"
export LD_LIBRARY_PATH="$base/lib"
exec /usr/bin/lua -l nros_compat "$base/stock-cgi"
