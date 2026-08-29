"""Turn checked-in golden bytes into C data; never call the implementation codec."""
import json
import pathlib
import re
import sys

source, destination = map(pathlib.Path, sys.argv[1:])
rows = json.loads(source.read_text())["messages"]
lines = ["/* Generated from mock_messages.json; expected bytes are not computed by C. */",
         "typedef struct { const char *name; uint8_t type, source; uint32_t session;",
         "uint16_t sequence; size_t length; uint8_t wire[NOSTOS_WIRE_MAX]; } fixture_t;",
         "static const fixture_t fixtures[] = {"]
seen = set()
for row in rows:
    assert re.fullmatch(r"[A-Z_]+", row["name"])
    assert row["type"] not in seen
    seen.add(row["type"])
    wire = bytes.fromhex(row["wire_hex"])
    assert 9 <= len(wire) <= 64
    assert wire[:3] == bytes([2, row["type"], row["source_id"]])
    assert int.from_bytes(wire[3:7], "little") == row["session_id"]
    assert int.from_bytes(wire[7:9], "little") == row["sequence"]
    lines.append('{"%s", %d, %d, %d, %d, %d, {%s}},' % (
        row["name"], row["type"], row["source_id"], row["session_id"],
        row["sequence"], len(wire), ",".join(f"0x{x:02x}" for x in wire)))
lines += ["};", ""]
destination.write_text("\n".join(lines))
