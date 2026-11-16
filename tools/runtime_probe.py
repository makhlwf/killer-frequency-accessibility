"""Run a bounded, explicitly selected Unity probe on the game's main thread."""
import argparse
import json
import pathlib
import threading
import frida

parser = argparse.ArgumentParser()
parser.add_argument("pid", type=int)
parser.add_argument("action")
parser.add_argument("output", type=pathlib.Path)
args = parser.parse_args()
source = pathlib.Path(__file__).with_name("runtime_probe.js").read_text(encoding="utf-8")
session = frida.attach(args.pid)
script = session.create_script(source)
done = threading.Event()
result = {}

def message(value, data):
    if value.get("type") == "send":
        result.update(value["payload"])
        done.set()
    elif value.get("type") == "error":
        result["error"] = value
        done.set()

script.on("message", message)
try:
    script.load()
    script.exports_sync.schedule(args.action)
    if not done.wait(25):
        raise TimeoutError("Main-thread probe did not execute in 25 seconds")
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"output": str(args.output), "keys": list(result), "error": result.get("error")}))
finally:
    script.unload()
    session.detach()
