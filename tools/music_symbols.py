"""Bind the optional music patch to the exact locally built accessibility DLL."""
import argparse
import pathlib
import re
import struct
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dll", type=pathlib.Path, default=root / "bin/KillerFrequencyAccess.dll")
parser.add_argument("--output", type=pathlib.Path, default=root / "src/music_symbols.h")
args = parser.parse_args()
dll = args.dll
data = dll.read_bytes()
pe = struct.unpack_from("<I", data, 0x3C)[0]
timestamp = struct.unpack_from("<I", data, pe + 8)[0]
optional_size = struct.unpack_from("<H", data, pe + 20)[0]
optional = pe + 24
image_base = struct.unpack_from("<Q", data, optional + 24)[0]
image_size = struct.unpack_from("<I", data, optional + 56)[0]
section_count = struct.unpack_from("<H", data, pe + 6)[0]
sections = []
for i in range(section_count):
    off = optional + optional_size + i * 40
    vsize, rva, raw_size, raw_offset = struct.unpack_from("<IIII", data, off + 8)
    sections.append((rva, max(vsize, raw_size), raw_offset))

symbols = {}
for line in subprocess.check_output(["nm", "--defined-only", str(dll)], text=True).splitlines():
    match = re.match(r"([0-9a-fA-F]+)\s+[a-zA-Z]\s+(\S+)$", line)
    if match:
        symbols[match[2]] = int(match[1], 16) - image_base
names = {
    "Refresh": "_ZN10navigationL7refreshEv",
    "Command": "_ZL7commandRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
    "Say": "_ZL3sayRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEb",
    "Destinations": "_ZN10navigationL12destinationsE",
    "Selected": "_ZN10navigationL8selectedE",
    "ManualSpeech": "_ZL12manualSpeech",
    "LastSubtitle": "_ZL12lastSubtitle",
    "Choices": "_ZL7choices",
    "Feedback": "_ZN10navigationL8feedbackERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE",
    "Tick": "_ZN10navigationL4tickEf",
    "Sample": "_ZN10navigationL6sampleEN7routing5PointERS1_",
    "Prepare": "_ZN10navigationL16prepareCollisionEPv",
    "ManualInput": "_ZN10navigationL11manualInputEv",
    "Moving": "_ZN10navigationL6movingE",
    "Planner": "_ZN10navigationL7plannerE",
    "MouseHeld": "_ZN10navigationL9mouseHeldE",
}
lines = ["#pragma once", "namespace music_symbols {", f"constexpr unsigned Timestamp = {timestamp};", f"constexpr unsigned ImageSize = {image_size};"]
for key, name in names.items():
    if name not in symbols:
        raise SystemExit(f"Required local mod symbol missing: {name}")
    lines.append(f"constexpr uintptr_t {key} = 0x{symbols[name]:x};")
refresh = symbols[names["Refresh"]]
for rva, size, raw_offset in sections:
    if rva <= refresh < rva + size:
        code = data[raw_offset + refresh - rva:raw_offset + refresh - rva + 16]
        lines.append("constexpr unsigned char RefreshBytes[] = {" + ",".join(f"0x{b:02x}" for b in code) + "};")
        break
else:
    raise SystemExit("Refresh code section not found")
lines.append("}")
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
print("Music patch bound to the exact local mod build.")
