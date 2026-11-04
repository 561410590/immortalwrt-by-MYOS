# MT5700 modern WebUI frontend fixes

These findings were reproduced against an MT5700M-CN through the Go backend.
The backend returned the raw AT responses correctly; the issues below were in
the Semi/Vite frontend and are suitable for an upstream pull request.

## 1. Thermal switch label overlaps the control

`Info.tsx` supplied `checkedText="已开启"` and `uncheckedText="已关闭"` to the
default 40 px Semi `Switch`. The four-character label is wider than the switch
track and renders vertically/over adjacent content. The page already renders a
separate “当前状态” value, so the embedded switch text is redundant and was
removed.

## 2. Firmware upgrade steps are truncated on phones

The upgrade page always used a five-item horizontal `Steps`. Semi applies
single-line ellipsis to each step title/description, leaving only one or two
characters at a 390 px viewport. The component now switches to vertical layout
below 520 px and allows its text to wrap.

## 3. Auto-dial state only accepts the long response

The parser only accepted the seven-field response, while a disabled MT5700
legitimately returns the short form `^SETAUTODIAL:0`. A failed match reset the
state to hard-coded defaults, so the displayed enable state and related values
could be wrong. The parser now accepts both short and long forms and updates
only fields actually returned by the modem.

## 4. Data-interface mode falls back to “Ethernet” incorrectly

When `^SETAUTODIAL:0` is returned, no dial-mode field exists. The old fallback
unconditionally selected mode 2 (“转网口模式”). For an OpenWrt/QModem USB data
session this is wrong. When the dial-mode field is absent, the page queries
`AT^NDISSTATQRY?`; an active NDIS session selects mode 1 (“USB网络接口”). If no
authoritative signal exists, the UI keeps the mode unidentified instead of
inventing a value.

## 5. DMZ refresh button remains in loading state

`loading.dmz` was initialized to `true`, but initial loading calls
`fetchInfcfg()` (which already parses DMZ) and never calls `fetchDMZ()`. Nothing
therefore reset `loading.dmz` to `false`. It now starts as `false`; explicit DMZ
refresh still sets and clears it around the request.
