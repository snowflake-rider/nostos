import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / "scripts/v2_profile.py"
SPEC = importlib.util.spec_from_file_location("v2_profile", MODULE)
profile = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(profile)


class V2ProfileTests(unittest.TestCase):
    def test_repository_profiles_are_distinct_and_render_expected_config(self):
        profiles = profile.load_profiles()
        self.assertEqual([profiles["peers"][board]["source"] for board in profile.BOARD_ORDER], [1, 2, 3])
        self.assertEqual([profiles["peers"][board]["mesh_address"] for board in profile.BOARD_ORDER],
                         ["0x0003", "0x0005", "0x0006"])
        with tempfile.TemporaryDirectory() as directory:
            for board in profile.BOARD_ORDER:
                output = Path(directory) / board / "sdkconfig"
                values = profile.render_sdkconfig(board, output, profiles)
                text = output.read_text()
                self.assertIn("CONFIG_NOSTOS_PROTOCOL_V2=y", text)
                self.assertIn(f"CONFIG_NOSTOS_LOCAL_SOURCE={profiles['peers'][board]['source']}", text)
                self.assertEqual(values["CONFIG_NOSTOS_SOURCE1_ADDRESS"], 3)
                self.assertEqual(values["CONFIG_NOSTOS_SOURCE2_ADDRESS"], 5)
                self.assertEqual(values["CONFIG_NOSTOS_SOURCE3_ADDRESS"], 6)

    def test_duplicate_identity_is_rejected(self):
        data = json.loads(profile.PROFILE_FILE.read_text())
        data["peers"]["B6"]["mesh_address"] = "0x0005"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "PROFILE_IDENTITY_DUPLICATE"):
                profile.load_profiles(path)


if __name__ == "__main__":
    unittest.main()
