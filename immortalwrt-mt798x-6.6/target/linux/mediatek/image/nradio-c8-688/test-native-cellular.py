#!/usr/bin/env python3
"""Offline C8 native-adapter checks; never contacts a modem or writes GPIO."""
import importlib.machinery
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import types
import unittest
from unittest.mock import mock_open, patch

sys.dont_write_bytecode = True
if "fcntl" not in sys.modules and sys.platform == "win32":
    sys.modules["fcntl"] = types.SimpleNamespace(LOCK_EX=2, LOCK_NB=4, flock=lambda *args: None)
PATH = Path(__file__).parent / "native-overlay/usr/libexec/myos-c8-native"
loader = importlib.machinery.SourceFileLoader("c8_native", str(PATH))
spec = importlib.util.spec_from_loader(loader.name, loader)
native = importlib.util.module_from_spec(spec)
loader.exec_module(native)


class NativeTests(unittest.TestCase):
    def test_official_two_slot_map(self):
        self.assertEqual(native.SIM_MAP, {1: {"sel0": 0, "sel1": 1, "usim": 1},
                                         2: {"sel0": 1, "sel1": 1, "usim": 1}})
        self.assertEqual(native.GPIO_OFFSETS, {"sel0": 29, "sel1": 30})

    def test_policy_semantics(self):
        self.assertEqual(native.desired_lock("2", [78]), ["0"])
        self.assertEqual(native.desired_lock("1", [78]), ["3", "1", "1", "78"])
        self.assertEqual(native.desired_lock("0", [78]), ["3", "0", "1", "78"])

    def test_scoped_session_without_cid(self):
        self.assertTrue(native.session_state('^NDISSTATQRY: 1,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n'))
        self.assertFalse(native.session_state('^NDISSTATQRY: 0,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n'))
        self.assertTrue(native.session_state('^NDISSTATQRY: 0,0,,"IPV4",1,0,,"IPV6"\r\nOK\r\n'))
        self.assertTrue(native.session_state('^NDISSTATQRY: 8,1,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n'))
        self.assertIsNone(native.session_state("OK\r\n"))

    def test_manual_origin_required_before_any_io(self):
        with patch.object(native, "require_modem") as modem, patch.object(native, "gpiochip_base") as gpio:
            with self.assertRaises(RuntimeError):
                native.select_sim({"slot": 2})
            modem.assert_not_called()
            gpio.assert_not_called()

    def test_manual_sim_restores_data_once_after_registration(self):
        for failed in (False, True):
            events, values = [], {"sel0": "0", "sel1": "1"}
            class GPIOPath:
                def __init__(self, path):
                    self.path = str(path)
                def exists(self):
                    return True
                def resolve(self):
                    return "/devices/11d00000.pinctrl/" + self.path
                def __truediv__(self, child):
                    return GPIOPath(self.path + "/" + child)
                def write_text(self, value):
                    name = "sel0" if "sel0" in self.path else "sel1"
                    values[name] = "1" if value == "high" else "0"
                def read_text(self):
                    return values["sel0" if "sel0" in self.path else "sel1"]
            registration = iter(("searching", "register", "register"))
            def ubus(service, method, *args):
                if service == "infocd":
                    state = next(registration)
                    events.append(state)
                    return {"STAT": state}
                return {}
            def at(command):
                events.append(command)
                if command == "AT^SCICHG?":
                    return "^SCICHG: 0\r\nOK\r\n"
                return '^NDISSTATQRY: 1,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n'
            def restore(interface):
                self.assertEqual(events[-1], "register")
                events.append("restore")
                if failed:
                    raise RuntimeError("CID8 rejected")
                return {"ok": True}
            with self.subTest(recovery_failure=failed), patch.object(native, "Path", GPIOPath), \
                    patch.object(native, "require_modem"), patch.object(native, "gpiochip_base", return_value=455), \
                    patch.object(native, "set_uci"), patch.object(native, "run"), \
                    patch.object(native, "ubus", side_effect=ubus), patch.object(native, "at", side_effect=at), \
                    patch.object(native, "netifd_connect", side_effect=restore) as connect, \
                    patch.object(native, "network_snapshot", side_effect=[{"online": False}, {"online": True}]), \
                    patch.object(native.time, "sleep"):
                result = native.select_sim({"origin": "module_settings", "slot": 2})
            self.assertEqual(result["ok"], not failed)
            self.assertTrue(result["gpio_verified"])
            self.assertEqual(result["slot"], 2)
            self.assertEqual(values, {"sel0": "1", "sel1": "1"})
            connect.assert_called_once_with("cpe")
            self.assertLess(events.index("register"), events.index("restore"))
            if failed:
                self.assertIn("CID8 rejected", result["error"])
            self.assertFalse(any("CFUN" in event or "SCICHG=" in event for event in events))

    def test_redial_only_data_session(self):
        calls = []
        def at(command):
            calls.append(command)
            return '^NDISSTATQRY: 1,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n'
        with patch.object(native, "require_modem"), patch.object(native, "at", side_effect=at), patch.object(native.time, "sleep"):
            self.assertTrue(native.reconnect()["ok"])
        self.assertEqual(calls, ["AT^NDISDUP=8,0", "AT^NDISDUP=8,1", "AT^NDISSTATQRY=8"])

    def test_unknown_slot_blocks_policy_before_at(self):
        with patch.object(native, "require_modem"), patch.object(native, "physical_slot", return_value=None), patch.object(native, "at") as at:
            with self.assertRaises(RuntimeError):
                native.apply_settings({"policy": "2"})
            at.assert_not_called()

    def test_default_none_saved_policy_preserved(self):
        for saved in ("", "0", "1", "2"):
            def uci(key, default=""):
                return (saved or default) if key.endswith(".mobility") else default
            with patch.object(native, "uci", side_effect=uci), patch.object(native, "physical_slot", return_value=None):
                self.assertEqual(native.settings()["policy"], saved or "2")

    def test_gpio_unknown_not_inferred_from_native_simno(self):
        raw = {"result": [{"up": 0, "status": {"name": "cpe", "simno": "1", "mode": "NR"}}]}
        with patch.object(native, "physical_slot", return_value=None):
            value = native.build_status(raw, {}, {"STAT": "register", "SIM": "ready"}, {}, 0, {"slot": "1"})
        self.assertEqual(value["sim"]["slot"], "")
        self.assertEqual(value["sim"]["configured_slot"], "1")
        self.assertFalse(value["data_connected"])
        self.assertFalse(value["modem"]["supported"])

    def test_radio_write_then_cfun_sequence(self):
        commands, current = [], {"nr": ["0"], "lte": ["0"], "option": ["1", "1", "1"]}
        def at(command):
            commands.append(command)
            if command == "AT^SYSCFGEX?":
                return '^SYSCFGEX: "080302",3fffffff,2,4,7fffffffffffffff,,\r\nOK\r\n'
            if command == "AT^C5GOPTION?":
                return "^C5GOPTION: " + ",".join(current["option"]) + "\r\nOK\r\n"
            if command == "AT+CFUN?":
                return "+CFUN: 1\r\nOK\r\n"
            for radio in ("nr", "lte"):
                prefix = "AT^" + radio.upper() + "FREQLOCK"
                if command == prefix + "?":
                    value = current[radio]
                    return "^" + radio.upper() + "FREQLOCK: " + ",".join(value) + "\r\nOK\r\n"
                if command.startswith(prefix + "="):
                    current[radio] = next(native.csv.reader([command.split("=", 1)[1]]))
            return "OK\r\n"
        with patch.object(native, "require_modem"), patch.object(native, "physical_slot", return_value=1), patch.object(native, "at", side_effect=at):
            result = native.apply_settings({"policy": "1", "mode": "auto", "nr_bands": "78", "lte_bands": "3"}, save=False)
        self.assertTrue(result["ok"])
        writes = [i for i, value in enumerate(commands) if "FREQLOCK=" in value]
        self.assertTrue(writes and max(writes) < commands.index("AT+CFUN=0"))
        self.assertLess(commands.index("AT+CFUN=0"), commands.index("AT+CFUN=1"))
        self.assertIn('AT^NRFREQLOCK=3,1,1,"78"', commands)
        self.assertFalse(any("SCICHG=" in value or "HVSST=" in value for value in commands))

    def test_nsa_uses_official_capability_option(self):
        calls = []
        state = {"option": ["1", "1", "1"]}
        def at(command):
            calls.append(command)
            if "FREQLOCK?" in command:
                return "^" + ("NR" if "NRFREQ" in command else "LTE") + "FREQLOCK: 0\r\nOK\r\n"
            if command == "AT^SYSCFGEX?":
                return '^SYSCFGEX: "080302",3fffffff,2,4,7fffffffffffffff,,\r\nOK\r\n'
            if command == "AT^C5GOPTION?":
                return "^C5GOPTION: " + ",".join(state["option"]) + "\r\nOK\r\n"
            if command == "AT^C5GOPTION=0,1,0":
                state["option"] = ["0", "1", "0"]
            if command == "AT+CFUN?":
                return "+CFUN: 1\r\nOK\r\n"
            return "OK\r\n"
        with patch.object(native, "require_modem"), patch.object(native, "physical_slot", return_value=1), patch.object(native, "at", side_effect=at):
            self.assertTrue(native.apply_settings({"policy": "2", "mode": "nsa"}, save=False)["ok"])
        self.assertIn("AT^C5GOPTION=0,1,0", calls)
        self.assertFalse(any('SYSCFGEX="0803"' in value for value in calls))

    def test_none_policy_actually_unlocks_both_radios(self):
        calls, current = [], {"nr": ["3", "0", "1", "78"], "lte": ["3", "0", "1", "3"]}
        def at(command):
            calls.append(command)
            if command == "AT^SYSCFGEX?":
                return '^SYSCFGEX: "080302",3fffffff,2,4,7fffffffffffffff,,\r\nOK\r\n'
            if command == "AT^C5GOPTION?":
                return "^C5GOPTION: 1,1,1\r\nOK\r\n"
            if command == "AT+CFUN?":
                return "+CFUN: 1\r\nOK\r\n"
            for radio in current:
                prefix = "AT^" + radio.upper() + "FREQLOCK"
                if command == prefix + "?":
                    return "^" + radio.upper() + "FREQLOCK: " + ",".join(current[radio]) + "\r\nOK\r\n"
                if command == prefix + "=0":
                    current[radio] = ["0"]
            return "OK\r\n"
        with patch.object(native, "require_modem"), patch.object(native, "physical_slot", return_value=1), patch.object(native, "at", side_effect=at):
            self.assertTrue(native.apply_settings({"policy": "2", "mode": "auto"}, save=False)["ok"])
        self.assertEqual(current, {"nr": ["0"], "lte": ["0"]})
        self.assertLess(calls.index("AT^NRFREQLOCK=0"), calls.index("AT+CFUN=0"))
        self.assertLess(calls.index("AT^LTEFREQLOCK=0"), calls.index("AT+CFUN=0"))

    def test_collector_has_no_control_replay(self):
        import ast
        tree = ast.parse(PATH.read_text(encoding="utf-8"))
        sample = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "sample")
        calls = {n.func.id for n in ast.walk(sample) if isinstance(n, ast.Call) and isinstance(n.func, ast.Name)}
        self.assertFalse(calls & {"at", "select_sim", "reconnect", "apply_settings", "set_uci"})

    def test_metadata_uses_only_owner_read_queries(self):
        import ast
        tree = ast.parse(PATH.read_text(encoding="utf-8"))
        metadata = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "modem_metadata")
        queries = [n.args[0].value for n in ast.walk(metadata)
                   if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id == "at"]
        self.assertEqual(set(queries), {"ATI", "AT+CGMR", "AT+CGSN"})

    def test_netifd_default_profile_connects_without_policy_or_apn_writes(self):
        calls, state = [], {"connected": False}
        def at(command):
            calls.append(command)
            if command == "AT+CGDCONT?":
                return '+CGDCONT: 8,"IP","current.apn","",0,0\r\nOK\r\n'
            if command == "AT^NDISDUP=8,1":
                state["connected"] = True
            if command == "AT^NDISSTATQRY=8":
                return '^NDISSTATQRY: %d,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n' % state["connected"]
            return "OK\r\n"
        with patch.object(native, "require_modem"), patch.object(native, "at", side_effect=at):
            self.assertTrue(native.netifd_connect("cpe")["ok"])
        self.assertEqual(calls, ["AT+CGDCONT?", "AT^NDISSTATQRY=8", "AT^NDISDUP=8,1", "AT^NDISSTATQRY=8"])

    def test_netifd_matching_profile_does_not_write(self):
        calls = []
        def at(command):
            calls.append(command)
            return {"AT+CGDCONT?": '+CGDCONT: 8,"IPV4V6","internet"\r\nOK\r\n',
                    "AT^AUTHDATA?": '^AUTHDATA: 8,0,"","",""\r\nOK\r\n',
                    "AT^NDISSTATQRY=8": '^NDISSTATQRY: 1,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n'}[command]
        with patch.object(native, "require_modem"), patch.object(native, "at", side_effect=at):
            self.assertTrue(native.netifd_connect("cpe", "internet")["ok"])
        self.assertFalse(any("=" in command for command in calls if command != "AT^NDISSTATQRY=8"))

    def test_netifd_pdp_change_is_only_cid8_data_session(self):
        calls = []
        state = {"pdp": ["8", "IPV4V6", "old"], "auth": ["8", "0", "", "", ""], "connected": True}
        def at(command):
            calls.append(command)
            if command == "AT+CGDCONT?":
                return '+CGDCONT: 1,"IP","untouched"\r\n+CGDCONT: ' + ",".join(state["pdp"]) + "\r\nOK\r\n"
            if command == "AT^AUTHDATA?":
                return "^AUTHDATA: " + ",".join(state["auth"]) + "\r\nOK\r\n"
            if command.startswith("AT+CGDCONT="):
                state["pdp"] = next(native.csv.reader([command.split("=", 1)[1]]))
            if command.startswith("AT^AUTHDATA="):
                row = next(native.csv.reader([command.split("=", 1)[1]]))
                state["auth"] = row[:2] + row[3:] + row[2:3]
            if command.startswith("AT^NDISDUP=8,"):
                state["connected"] = command.endswith(",1")
            if command == "AT^NDISSTATQRY=8":
                return '^NDISSTATQRY: %d,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n' % state["connected"]
            return "OK\r\n"
        with patch.object(native, "require_modem"), patch.object(native, "at", side_effect=at):
            result = native.netifd_connect("cpe", "internet", "pap", "user", "secret")
        self.assertTrue(result["ok"])
        self.assertEqual(state["pdp"], ["8", "IPV4V6", "internet"])
        self.assertEqual(state["auth"], ["8", "1", "secret", "user", ""])
        self.assertLess(calls.index("AT^NDISDUP=8,0"), calls.index('AT+CGDCONT=8,"IPV4V6","internet"'))
        self.assertLess(calls.index('AT^AUTHDATA=8,1,"","secret","user"'), calls.index("AT^NDISDUP=8,1"))
        writes = [command for command in calls if "=" in command and command != "AT^NDISSTATQRY=8"]
        self.assertTrue(all(command.startswith(("AT^NDISDUP=8,", "AT+CGDCONT=8,", "AT^AUTHDATA=8,")) for command in writes))

    def test_netifd_rejected_pdp_never_reconnects_or_resets(self):
        calls = []
        def at(command):
            calls.append(command)
            if command.startswith("AT+CGDCONT="):
                raise RuntimeError("PDP rejected")
            if command == "AT+CGDCONT?":
                return '+CGDCONT: 8,"IPV4V6","old"\r\nOK\r\n'
            if command == "AT^AUTHDATA?":
                return '^AUTHDATA: 8,0,"","",""\r\nOK\r\n'
            return '^NDISSTATQRY: 0,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n'
        with patch.object(native, "require_modem"), patch.object(native, "at", side_effect=at):
            with self.assertRaises(RuntimeError):
                native.netifd_connect("cpe", "internet")
        self.assertNotIn("AT^NDISDUP=8,1", calls)
        self.assertFalse(any("CFUN" in value or "SCICHG" in value or "SETAUTODIAL" in value for value in calls))

    def test_netifd_unknown_modem_or_invalid_parameters_issue_no_at(self):
        with patch.object(native, "require_modem", side_effect=RuntimeError("unsupported")), patch.object(native, "at") as at:
            with self.assertRaises(RuntimeError):
                native.netifd_connect("cpe")
            at.assert_not_called()
        for args in (("cpe", 'bad"apn'), ("cpe", "internet", "both"), ("cpe1",),
                     ("cpe", "internet", "pap", "user", "bad\r\nAT^SCICHG=1,0")):
            with patch.object(native, "require_modem"), patch.object(native, "at") as at:
                with self.assertRaises(ValueError):
                    native.netifd_connect(*args)
                at.assert_not_called()

    def test_netifd_teardown_only_disconnects_data(self):
        calls, state = [], {"connected": True}
        def at(command):
            calls.append(command)
            if command == "AT^NDISDUP=8,0":
                state["connected"] = False
            return '^NDISSTATQRY: %d,0,,"IPV4",0,0,,"IPV6"\r\nOK\r\n' % state["connected"]
        with patch.object(native, "require_modem"), patch.object(native, "at", side_effect=at):
            self.assertTrue(native.netifd_disconnect("cpe")["ok"])
        self.assertEqual(calls, ["AT^NDISSTATQRY=8", "AT^NDISDUP=8,0", "AT^NDISSTATQRY=8"])

    def test_netifd_cli_preserves_empty_and_dash_prefixed_parameters(self):
        argv = [str(PATH), "netifd-connect", "--", "cpe", "internet", "pap", "", "-secret", "ipv4v6"]
        with patch.object(sys, "argv", argv), patch("builtins.open", mock_open()), \
                patch.object(native.fcntl, "flock"), \
                patch.object(native, "netifd_connect", return_value={"ok": True}) as connect, patch("builtins.print"):
            native.main()
        connect.assert_called_once_with("cpe", "internet", "pap", "", "-secret", "ipv4v6")

    def test_netifd_shell_has_no_policy_or_automatic_gate(self):
        root = PATH.parent.parent.parent
        ncm = (root / "lib/netifd/proto/ncm.sh").read_text(encoding="utf-8")
        self.assertIn("netifd-connect --", ncm)
        self.assertIn("netifd-disconnect", ncm)
        self.assertIn("profile=8", ncm)
        executable = "\n".join(line for line in ncm.splitlines() if not line.lstrip().startswith("#"))
        self.assertNotIn("automatic", executable)
        self.assertNotIn("setup_apn", ncm)
        wwan = (root / "lib/netifd/proto/wwan.sh").read_text(encoding="utf-8")
        setup = wwan.split("proto_wwan_setup()", 1)[1].split("netmanager_ifconfig()", 1)[0]
        self.assertIn("netifd-prepare", setup)
        self.assertIn('cpetools.sh -i "$interface" -u', setup)
        self.assertNotIn('cpetools.sh -i "$interface" -c preinit ||', setup)

    def test_netifd_shell_setup_connects_then_adds_dynamic_addresses(self):
        source = (PATH.parent.parent.parent / "lib/netifd/proto/ncm.sh").read_text(encoding="utf-8")
        source = source.replace("/usr/libexec/myos-c8-native", "native_stub").replace("cpetools.sh", "cpetools_stub")
        stubs = '''
uci() { printf '0'; }
cat() { printf '{}'; }
json_get_vars() { apn=; auth=; username=; password=; pdptype=ipv4v6; }
json_get_values() { initialize='ATQ0 ATV1 ATE1 ATS0=0'; }
json_load() { :; }
json_select() { :; }
json_init() { :; }
json_add_string() { printf 'JSON %s %s\\n' "$1" "$2"; }
json_close_object() { :; }
json_dump() { printf '{}'; }
proto_add_data() { :; }
proto_close_data() { :; }
proto_add_dynamic_defaults() { :; }
proto_send_update() { :; }
proto_init_update() { printf 'UPDATE\\n'; }
proto_notify_error() { printf 'ERROR %s\\n' "$2"; }
cpetools_stub() { printf 'AT_INITIALIZE\\n'; }
brctl() { :; }
ubus() { printf 'DYNAMIC\\n'; }
native_stub() {
  printf 'NATIVE'; printf ' <%s>' "$@"; printf '\\n'
  [ "$1" != netifd-connect ] || [ "$fail_connect" = 0 ]
}
interface=cpe; vendor=tdtech_mt5700; ifname=eth1
automatic=1
'''
        for failed in (0, 1):
            script = "INCLUDE_ONLY=1\n" + source + stubs + "\nfail_connect=" + str(failed) + "\nproto_ncm_setup cpe\n"
            result = subprocess.run([self.shell(), "-c", script], capture_output=True, text=True)
            with self.subTest(connect_failure=failed):
                self.assertIn("NATIVE <netifd-connect> <--> <cpe> <> <> <> <> <ipv4v6>", result.stdout)
                self.assertEqual(result.stdout.count("AT_INITIALIZE"), 4)
                if failed:
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertNotIn("DYNAMIC", result.stdout)
                    self.assertIn("ERROR CONNECT_FAILED", result.stdout)
                else:
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stdout.count("DYNAMIC"), 2)
                    self.assertIn("JSON name cpe_4", result.stdout)
                    self.assertIn("JSON name cpe_6", result.stdout)
                    self.assertLess(result.stdout.index("NATIVE <netifd-connect>"), result.stdout.index("UPDATE"))

    def shell(self):
        shell = shutil.which("sh") or shutil.which("bash")
        candidate = Path("C:/Program Files/Git/bin/bash.exe")
        if not shell and candidate.is_file():
            shell = str(candidate)
        if not shell:
            self.skipTest("POSIX shell unavailable")
        return shell

    def test_gate_real_getopts_combined_and_attached_options(self):
        source = (PATH.parent.parent / "bin/cpetools.sh").read_text(encoding="utf-8")
        source = source.replace('exec /usr/libexec/myos-c8-native apply-saved', 'printf "UPDATE\\n"; exit 0')
        source = source.replace('exec /bin/sh /opt/myos-c8-official/share/cpetools.sh "$@"', 'printf "FORWARDED\\n"; exit 0')
        blocked = [["-s2"], ["-us2"], ["-S"], ["-rR"], ["-icpe", "-cusim_set"], ["-i", "cpe", "-cpreinit"],
                   ["-c", "pdp"], ["-dt1"], ["-k"], ["-l0"], ["-L1"], ["-cAT^scichg=0,1"], ["-cAT^HVSST=1,0"]]
        allowed = [["-icpe", "-ccpin"], ["-t1", "-icpe", "-cATQ0"], ["-i", "-s2", "-ccpin"], ["-cAT^SCICHG?"]]
        for args in blocked:
            with self.subTest(blocked=args):
                result = subprocess.run([self.shell(), "-c", source, "gate", *args], capture_output=True, text=True)
                self.assertEqual(result.returncode, 77, result.stderr)
        for args in allowed:
            with self.subTest(allowed=args):
                result = subprocess.run([self.shell(), "-c", source, "gate", *args], capture_output=True, text=True)
                self.assertEqual((result.returncode, result.stdout.strip()), (0, "FORWARDED"), result.stderr)

    def test_native_shell_syntax(self):
        root = PATH.parent.parent.parent
        for path in root.rglob("*"):
            if path.is_file():
                source = path.read_bytes()
                if source.startswith((b"#!/bin/sh", b"#!/bin/ash")):
                    text = source.decode("utf-8")
                    with self.subTest(path=str(path.relative_to(root))):
                        result = subprocess.run([self.shell(), "-n"], input=text, capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
