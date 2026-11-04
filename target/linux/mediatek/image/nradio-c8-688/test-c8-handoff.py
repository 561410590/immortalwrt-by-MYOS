"""Policy and AT readback checks for C8's cross-system IP passthrough handoff."""
import importlib.machinery
import importlib.util
from pathlib import Path
import subprocess
import sys
import types
import unittest
from unittest.mock import Mock, patch

if sys.platform == "win32":
    sys.modules["fcntl"] = types.ModuleType("fcntl")


source = Path(__file__).parent / "native-overlay/usr/libexec/myos-c8-native"
if not source.is_file():
    source = Path(__file__).with_name("myos-c8-phy-native")
loader = importlib.machinery.SourceFileLoader("c8", str(source))
spec = importlib.util.spec_from_loader(loader.name, loader)
c8 = importlib.util.module_from_spec(spec)
loader.exec_module(c8)


class Handoff(unittest.TestCase):
    def policy(self, values):
        def command(args, **kwargs):
            if args[-2] == "export":
                return subprocess.CompletedProcess(args, 0, stdout="", stderr="")
            key = args[-1]
            return subprocess.CompletedProcess(args, 0 if key in values else 1,
                                               stdout=values.get(key, ""), stderr="")
        with patch.object(c8.subprocess, "run", side_effect=command):
            return c8.official_mode_from_config(Path("/saved-official-config"))

    def test_official_saved_sim_and_all_passthrough_gates(self):
        values = {"cpesel.sim.cur": "2", "network.cpe.ippass": "1",
                  "network.cpe.forcephy": "1", "cpecfg.cpesim1.ippass": "0",
                  "cpecfg.cpesim2.ippass": "1"}
        self.assertEqual(self.policy(values), {"slot": 2, "enabled": True, "target_mode": 3})
        for key in ("network.cpe.ippass", "network.cpe.forcephy", "cpecfg.cpesim2.ippass"):
            with self.subTest(disabled=key):
                self.assertEqual(self.policy(dict(values, **{key: "0"}))["target_mode"], 1)
            with self.subTest(absent=key):
                self.assertEqual(self.policy({k: v for k, v in values.items() if k != key})["target_mode"], 1)

    def handoff(self, enabled, replies, apply=True):
        with patch.object(c8, "require_modem"), patch.object(c8, "block_handoff_at") as block, \
                patch.object(c8, "official_ethernet_mode",
                return_value={"slot": 2, "enabled": enabled, "target_mode": 3 if enabled else 1}), \
                patch.object(c8, "at", side_effect=replies) as at:
            result = c8.prepare_official_mode(apply=apply)
            self.assertEqual(block.call_count, int(apply))
            return result, [call.args[0] for call in at.call_args_list]

    def test_both_enabled_keeps_mode3_without_at_write(self):
        result, commands = self.handoff(True, ["Mode: 3"])
        self.assertFalse(result["changed"])
        self.assertEqual(commands, ["AT^TDCFG?"])

    def test_target_off_closes_passthrough_and_checks_readback(self):
        result, commands = self.handoff(False, ["Mode: 3", "OK", "Mode: 1"])
        self.assertTrue(result["changed"])
        self.assertEqual(commands, ["AT^TDCFG?", 'AT^TDCFG=infcfg,mode,1', "AT^TDCFG?"])

    def test_target_on_enables_passthrough_and_checks_readback(self):
        result, commands = self.handoff(True, ["Mode: 1", "OK", "Mode: 3"])
        self.assertTrue(result["changed"])
        self.assertIn('AT^TDCFG=infcfg,mode,3', commands)

    def test_readonly_plan_does_not_change_mode(self):
        result, commands = self.handoff(False, ["Mode: 3"], apply=False)
        self.assertFalse(result["applied"])
        self.assertEqual(commands, ["AT^TDCFG?"])

    def test_wrong_readback_stops_handoff(self):
        with self.assertRaisesRegex(RuntimeError, "readback failed"):
            self.handoff(False, ["Mode: 3", "OK", "Mode: 3"])

    def test_factory_restart_waits_for_at_readback(self):
        process = Mock(pid=12345)
        process.wait.return_value = 0
        pidfile = Mock()
        pidfile.exists.return_value = False
        with patch.object(c8, "physical_slot", side_effect=[2, None, None, 2]), \
                patch.object(c8, "sim_status", return_value={"ready": False}), \
                patch.object(c8, "uci", return_value="2"), \
                patch.object(c8, "Path", return_value=pidfile), \
                patch.object(c8.subprocess, "Popen", return_value=process) as start, \
                patch.object(c8.time, "sleep") as sleep:
            c8.configure_slot(2)
            self.assertEqual(start.call_args.kwargs["env"]["MYOS_C8_SIM_REINIT"], "1")
            self.assertEqual([entry.args for entry in sleep.call_args_list], [(1,), (1,)])

    def test_saved_selection_is_reported_when_at_is_unavailable(self):
        with patch.object(c8, "require_modem"), \
                patch.object(c8, "configure_slot", side_effect=RuntimeError("AT unavailable")), \
                patch.object(c8, "physical_slot", return_value=None), \
                patch.object(c8, "uci", return_value="2"), \
                patch.object(c8, "sim_status", return_value={}):
            result = c8.select_sim({"origin": "module_settings", "slot": 2})
            self.assertTrue(result["selection_saved"])
            self.assertFalse(result["gpio_verified"])

    def test_mode_preparation_allows_factory_sim_initialization_after_restart(self):
        power = Mock()
        power.is_file.return_value = True
        with patch.object(c8, "physical_slot", return_value=2), \
                patch.object(c8, "uci", return_value="1"), \
                patch.object(c8, "Path", return_value=power), \
                patch.object(c8, "run", return_value=""), \
                patch.object(c8, "at", side_effect=["Mode: 1", "OK", "Mode: 3", "Mode: 3"]), \
                patch.object(c8, "sim_status", side_effect=AssertionError("CPIN belongs to factory init")):
            self.assertEqual(c8.prepare_ethernet_mode(),
                             {"mode": 3, "passthrough": True, "changed": True})

    def test_disabled_autodial_can_be_restored_before_data_connection(self):
        replies = ['^SETAUTODIAL:0\r\nOK', 'OK',
                   '^SETAUTODIAL:1,2,"IPV4V6","","","",0\r\nOK',
                   '^NDISSTATQRY: 1,0,,"IPV4",1,0,,"IPV6"\r\nOK']
        with patch.object(c8, "require_modem"), patch.object(c8, "at", side_effect=replies) as at:
            result = c8.netifd_connect("cpe")
            self.assertTrue(result["profile_changed"])
            self.assertEqual([call.args[0] for call in at.call_args_list],
                             ['AT^SETAUTODIAL?', 'AT^SETAUTODIAL=1,2',
                              'AT^SETAUTODIAL?', 'AT^NDISSTATQRY=8'])

    def test_autodial_still_disabled_after_enable_stops_connection(self):
        with patch.object(c8, "require_modem"), \
                patch.object(c8, "at", side_effect=['^SETAUTODIAL:0\nOK', 'OK',
                                                   '^SETAUTODIAL:0\nOK']) as at:
            with self.assertRaisesRegex(RuntimeError, "readback failed"):
                c8.netifd_connect("cpe")
            self.assertNotIn('AT^NDISDUP=8,1', [call.args[0] for call in at.call_args_list])


if __name__ == "__main__":
    unittest.main()
