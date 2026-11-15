"""Focused executable controls; never substitute these for real-provider parity."""
from pathlib import Path
import subprocess
import sys

consumer, negative, directory, configuration = sys.argv[1:]
logs = Path(directory)
cases = [
    ("help", ["--help"], 0),
    ("missing", [], 2),
    ("extra", ["--help", "extra"], 2),
    ("nonexistent", ["/nonexistent/minimal-provider.so"], 1),
    ("invalid-abi", [str(Path(consumer).resolve())], 1),
]
for name, args, expected in cases:
    result = subprocess.run([consumer, *args], capture_output=True, timeout=20)
    prefix = logs / f"{configuration}-cli-{name}"
    prefix.with_suffix(".stdout").write_bytes(result.stdout)
    prefix.with_suffix(".stderr").write_bytes(result.stderr)
    prefix.with_suffix(".exit").write_text(f"{result.returncode}\n")
    if result.returncode != expected or b"PASS:" in result.stdout:
        raise SystemExit(f"{name}: expected exit {expected}, got {result.returncode}")
subprocess.run([negative], check=True, timeout=20)
print("PASS: five executable CLI controls and fourteen pure negative controls")