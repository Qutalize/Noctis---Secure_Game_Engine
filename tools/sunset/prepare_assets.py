"""Materialize a pinned Poly Haven CC0 inventory for the coastal demo."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import urllib.request


def prepare(output):
    inventory = json.loads(Path(__file__).with_name("coastal-assets.json").read_text(encoding="utf-8"))
    for model, files in inventory.items():
        directory = output / model
        for name, (url, expected) in files.items():
            request = urllib.request.Request(url, headers={"User-Agent": "Noctis coastal asset build"})
            with urllib.request.urlopen(request, timeout=40) as response:
                original = response.read()
            if hashlib.sha256(original).hexdigest() != expected:
                raise ValueError(f"Upstream asset content changed: {model}/{name}")
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.suffix == ".jpg":
                subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-y", "-f", "image2pipe",
                                "-c:v", "mjpeg", "-i", "pipe:0", "-frames:v", "1",
                                "-pix_fmt", "rgb24", str(target.with_suffix(".png"))],
                               input=original, check=True)
            elif target.suffix == ".gltf":
                gltf = json.loads(original)
                for image in gltf["images"]:
                    image["uri"] = image["uri"].removesuffix(".jpg") + ".png"
                if model == "lantern_chandelier_01":
                    for mesh in gltf["meshes"]:
                        mesh["primitives"] = [
                            part for part in mesh["primitives"]
                            if gltf["materials"][part["material"]].get("alphaMode") != "BLEND"
                        ]
                target.write_text(json.dumps(gltf, separators=(",", ":")), encoding="utf-8")
            else:
                target.write_bytes(original)
        print(f"Verified CC0 model: {model} ({len(files)} source files)")


if __name__ == "__main__":
    prepare(Path(sys.argv[1]))
