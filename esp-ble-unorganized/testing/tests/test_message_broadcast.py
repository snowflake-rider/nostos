"""Synthetic log fixtures only: these tests are NOT hardware evidence."""
import copy
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / "message_broadcast.py"
spec = importlib.util.spec_from_file_location("message_broadcast", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def fixture(ident=0x13):
    snapshots = {}
    for board, address in (("D6", "0x0005"), ("76", "0x0003"), ("B6", "0x0006")):
        snapshots[board] = {
            "config": {"name": board, "primary": address, "net": "0x0000", "app": "0x0001",
                       "pub": "0xc001", "sub_C001": "1", "event_ready": "1",
                       "ttl": "7", "period": "0", "retransmit": "0", "relay": "0"},
            "uart": {"pending": "0", "valid": "100", "noop": "5", "invalid": "9", "hw_errors": "0"},
            "rx": {"valid": "100", "invalid": "0", "self": "0", "not_ready": "0"},
            "tx": {"accepted": "100", "failed": "0", "full": "0", "expired": "0"},
            "uart_tx": {"accepted": "100", "failed": "0", "full": "0", "expired": "0"},
            "stack": {"complete_ok": "100", "failed": "0"},
            "diag": {"port": "1", "baud": "115201", "data": "8", "parity": "none",
                     "stop": "1", "flow": "0", "buffered": "0"},
        }
    after = copy.deepcopy(snapshots)
    after["D6"]["uart"]["valid"] = "101"
    after["D6"]["tx"]["accepted"] = "101"
    after["D6"]["stack"]["complete_ok"] = "101"
    for board in ("76", "B6"):
        after[board]["rx"]["valid"] = "101"
        after[board]["uart_tx"]["accepted"] = "101"
    rows = [
        ("STM32", f"MESSAGE_TEST_TX id=0x{ident:02X} uart=OK seq=1"),
        ("D6", f"UART_RX id=0x{ident:02x} result=queued"),
        ("D6", f"MESH_TX id=0x{ident:02x} source=0x0005 api=accepted"),
        ("76", f"MESH_RX source=0x0005 id=0x{ident:02x} result=queued"),
        ("B6", f"MESH_RX source=0x0005 id=0x{ident:02x} result=queued"),
    ]
    return [{"board": b, "line": line} for b, line in rows], snapshots, after


class DeliveryTests(unittest.TestCase):
    def judge(self, records, before, after, ident=0x13):
        return module.evaluate(ident, records, before, after, "D6", ("76", "B6"), 1)

    def test_all_eight_ids_and_usb_log_order(self):
        for ident in (0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x30, 0x31):
            records, before, after = fixture(ident)
            # Per-device USB order need not match the physical pipeline order.
            result = self.judge(list(reversed(records)), before, after, ident)
            self.assertEqual(result["verdict"], "PASS_OBSERVED", result["issues"])

    def test_send_accepted_does_not_replace_peer_reception(self):
        records, before, after = fixture()
        result = self.judge(records[:-1], before, after)
        self.assertEqual(result["verdict"], "FAIL")
        self.assertEqual(result["counts"]["B6.MESH_RX"], 0)

    def test_wrong_id_source_duplicate_failed_send_and_reset(self):
        for line in ("MESH_RX source=0x0007 id=0x13 result=queued",
                     "MESH_RX source=0x0005 id=0x31 result=queued",
                     "MESH_RX source=0x0005 id=0x13 result=invalid"):
            records, before, after = fixture()
            records[-1]["line"] = line
            self.assertEqual(self.judge(records, before, after)["verdict"], "FAIL")
        for extra in ({"board": "76", "line": "MESH_RX source=0x0005 id=0x13 result=queued"},
                      {"board": "D6", "line": "UART1_READY"},
                      {"board": "STM32", "line": "BUTTON n=4 id=0x13 uart=OK"}):
            records, before, after = fixture()
            self.assertEqual(self.judge(records + [extra], before, after)["verdict"], "FAIL")
        records, before, after = fixture()
        records[0]["line"] = "MESSAGE_TEST_TX id=0x13 uart=ERROR seq=1"
        self.assertEqual(self.judge(records, before, after)["verdict"], "FAIL")

    def test_missing_status_counter_changes_and_relay(self):
        for section, key, value in (("uart", "valid", "0"), ("uart", "hw_errors", "1"),
                                    ("tx", "failed", "1"), ("config", "relay", "1"),
                                    ("config", "app", "0x0002"), ("diag", "buffered", "240")):
            records, before, after = fixture()
            after["D6"][section][key] = value
            self.assertEqual(self.judge(records, before, after)["verdict"], "FAIL")
        records, before, after = fixture()
        del after["76"]["stack"]
        self.assertEqual(self.judge(records, before, after)["verdict"], "FAIL")

    def test_status_collection_requires_every_board(self):
        records = [{"board": "D6", "line": "I (1) X: STATUS name=D6 primary=0x0005"}]
        result = module.collect_snapshots(records, ("D6", "76", "B6"))
        self.assertEqual(result["D6"]["config"]["primary"], "0x0005")
        self.assertTrue(module.readiness(result))

    def test_sequence_and_empty_transmission_do_not_pass(self):
        records, before, after = fixture()
        records[0]["line"] = "MESSAGE_TEST_TX id=0x13 uart=OK seq=99"
        self.assertEqual(self.judge(records, before, after)["verdict"], "FAIL")
        self.assertEqual(self.judge([], before, before)["verdict"], "FAIL")


if __name__ == "__main__":
    unittest.main()
