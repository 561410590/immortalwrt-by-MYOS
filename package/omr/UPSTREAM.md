# OpenMPTCProuter integration for C2000 Ultra

Imported package sources:

- Repository: https://github.com/Ysurac/openmptcprouter-feeds
- Commit: `69610fb8500c423c02e8c86af2ad5dab815e7bfb` (2026-09-09)
- Build reference: https://github.com/Ysurac/openmptcprouter
- Build reference commit: `74eacd096f9f0500fc3e8b3ca1c6c40cd227ffa1`
- Target: ImmortalWrt 24.10 / Linux 6.6 / firewall4 / NRadio C2000-Ultra

Package directories are vendored, not linked to the analysis checkout in
`/tmp`. Original copyright notices and licenses are retained. The Glorytun,
Shadowsocks-libev, ipcalc and tracebox recipes pin their own source commits.

`openmptcprouter-ultra` selects the official MPTCP scripts, tracker, VPS API,
Glorytun TCP, MPTCP-capable Shadowsocks-libev, nftables rules and LuCI pages.
The mainline kernel path manager and default scheduler are used. Legacy MPTCP
Kconfig symbols and optional OMR BPF scheduler patches are not part of this
integration.

The package installation rules select the required OMR runtime files. Upstream
distribution-wide defaults for network topology, Wi-Fi, DNS, storage, firmware
updates, branding and package repositories are not installed. Configuration
uses the upstream LuCI wizard and existing OMR services.

The IP helper library is imported from this tree's existing
`feeds/packages/net/unbound/files/iptools.sh` (GPL-2.0) and installed under
the original `/usr/lib/unbound/` path, so the OMR scripts do not require a
second DNS daemon or a rewritten helper library.

Do not add custom integration scripts, RPC wrappers, route ownership layers,
replacement wizards or test scripts. The rejected `90-omr-ultra`, `ultra.sh`,
`ultraapply`/`ultrastatus` and associated custom tests have been removed.

Server-side project: https://github.com/Ysurac/openmptcprouter-vps
Live aggregation requires a compatible, configured server and at least two
independent active uplinks. Firmware construction and live acceptance are
separate from package/source checks.
