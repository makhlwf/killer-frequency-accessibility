"""Convert verified map descriptions to a small native lookup table."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
descriptions = json.loads((root / "map-research/descriptions.json").read_text(encoding="utf-8-sig"))
rows = []
for name, description in descriptions.items():
    # This texture is an atlas of several independent photos. UV-region matching
    # is not implemented, so announcing every photo would reveal other clues.
    if name == "T_EvidenceMaps_01":
        continue
    rows.append("    {" + json.dumps(name, ensure_ascii=False) + ", " + json.dumps(description, ensure_ascii=False) + "},")
output = "#pragma once\nnamespace mapreading {\nstruct Entry {const char* texture; const char* description;};\nstatic const Entry entries[] = {\n" + "\n".join(rows) + "\n};\n}\n"
(root / "src/map_descriptions.h").write_text(output, encoding="utf-8")
print(f"Prepared {len(rows)} map/clue descriptions; multi-photo atlas withheld.")
