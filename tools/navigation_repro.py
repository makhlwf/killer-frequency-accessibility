"""Replay F11's command handler and require a music destination to start."""
import argparse
import pathlib
import time

parser = argparse.ArgumentParser()
parser.add_argument("--expect", default="record|vinyl|cassette|tape")
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1] / "bin"
log = root / "accessibility.log"
offset = log.stat().st_size
command = root / "command.txt"
if command.exists():
    raise SystemExit("A diagnostic command is already pending; let it finish first.")
command.write_text("navgo", encoding="utf-8")
deadline = time.monotonic() + 14
output = ""
while time.monotonic() < deadline:
    time.sleep(.15)
    with log.open("rb") as stream:
        stream.seek(offset)
        output = stream.read().decode("utf-8", errors="replace")
    if any(marker in output for marker in ("No clear walking", "Close the menu", "Walking is not available", "Requested leaving", "Walking to ", "No navigation destinations", "could not perform", "Selected nearby record", "You are holding a music record")):
        break
print(output.strip() or "No response from the running game's command handler.")
import re
passed = "Selected nearby record" in output or "You are holding a music record" in output or bool(re.search(r"(?:Walking to|Interacting\.|Requested.*)(?:.*)(?:" + args.expect + r")", output, re.I))
print("PASS: music assistance responded" if passed else "FAIL: F11 did not start navigation to the requested music")
raise SystemExit(0 if passed else 1)
