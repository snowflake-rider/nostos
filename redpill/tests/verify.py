"""Check original-code hashes, local links, CLI behavior, and lesson outputs."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve()
manifest = json.loads((root / "originals/manifest.json").read_text())
archive_count = 0
for entry in manifest:
    variants = entry.get("sha256")
    if not variants:
        continue
    records = variants.items() if isinstance(variants, dict) else [(None, variants)]
    for version, expected in records:
        name = f"day{entry['day']:02d}" + (f"-{version}" if version else "")
        body = (root / "originals" / (name + ".md")).read_text()
        raw = body.split("\n````c\n", 1)[1].rsplit("````\n", 1)[0]
        assert hashlib.sha256(raw.encode()).hexdigest() == expected, name
        archive_count += 1
assert archive_count == 22

# Every source mapping must resolve after module-file renames.
modules = [entry['module'] for entry in manifest if 'module' in entry]
assert len(modules) == 21 and len(set(modules)) == 21
for module in modules:
    assert (root / (module + '.c')).is_file(), module
    assert (root / (module + '.h')).is_file(), module
assert not list(root.glob('day[0-9][0-9]_*.c'))
assert not list(root.glob('day[0-9][0-9]_*.h'))

def run(*args, expected=0):
    result = subprocess.run([str(binary), *args], capture_output=True, text=True, timeout=10)
    assert result.returncode == expected, (args, result.stdout, result.stderr)
    return result.stdout

listing = run("--list")
days = [int(day) for day in re.findall(r"^\s+(\d+)\s+", listing, re.M)]
assert days == list(range(4, 19)) + list(range(22, 27)) + [33]
all_output = run("--all")
def stable_output(output):
    # Pool addresses legitimately change between separate processes (ASLR).
    return re.sub(r"(?m)^(?:Allocated:|Re-allocated:).*$", "<pool addresses>", output)
assert stable_output(all_output) == stable_output(run())
assert len(re.findall(r"^--- Day \d+", all_output, re.M)) == 21
for invalid in ("", "19", "0", "-1", "x", "8x", "9" * 100, "--wat"):
    run(invalid, expected=1)
run("4", "5", expected=1)
assert "PASS:" in run("--test")
assert run("4").count("Verify: OK") == 4
assert "[SWAR]      : 32" in run("5")
assert "0x78123456" in run("6")
assert "Calculated Checksum: 0x45" in run("7")
assert run("8").count(">> Success!") == 2
assert "[Struct] After : Lee(2), Kim(1)" in run("9")
assert "Generated Matrix (3x4)" in run("10")
assert re.findall(r"Result: (-?\d+)", run("11")) == ["3", "1", "2", "0"]
offset = run("12")
assert re.findall(r"\[Standard\] Offset of \w: (\d+)", offset) == re.findall(
    r"\[Object Macro\] Offset of \w: (\d+)", offset)
pool = run("13")
allocated = re.search(r"Allocated: (0x\w+), (0x\w+), (0x\w+)", pool)
assert allocated and allocated[2] in re.search(r"Re-allocated: (0x\w+)", pool)[0]
assert "48 65 6c 6c 6f" in run("14")
assert "Fail to Put: 8 (Buffer Full!)" in run("15")
assert "3 -> 2 -> 1 -> NULL" in run("16")
assert run("17").count("Index 2 allocated") == 2
assert re.findall(r"Executing Task (\d+)", run("18")) == ["5", "3", "2", "1", "4"]
assert re.findall(r"Event\] Timer (\d+)", run("22")) == ["2", "1", "3"]
assert re.findall(r"Token \d+: (.+)", run("23")) == ["GPS", "37.5665", "126.9780", "20260213"]
assert "[State Changed] to 1" in run("24")
assert re.findall(r"^\s*10 \|.*\|\s+([\d.]+)", run("25"), re.M) == ["20.1"]
assert run("26") == run("26")
assert run("33").endswith("hello\n")
for path in root.glob("*.md"):
    for link in re.findall(r"\]\(([^)]+)\)", path.read_text()):
        if "://" in link or link.startswith("#"):
            continue
        assert (path.parent / link.split("#")[0]).exists(), (path.name, link)
main_defs = []
for path in list(root.glob("*.c")) + list((root / "tests").glob("*.c")):
    main_defs.extend((path, m.start()) for m in re.finditer(r"^int main\(", path.read_text(), re.M))
assert len(main_defs) == 1 and main_defs[0][0].name == "main.c"
print(f"PASS: {archive_count} original hashes; 21 demos, CLI/output and local-link checks")
