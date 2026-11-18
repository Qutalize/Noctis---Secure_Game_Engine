"""Materialize a pinned Poly Haven CC0 inventory for the coastal demo."""
import hashlib
import argparse
import json
from pathlib import Path
import subprocess
import urllib.request

from build_architecture import build_architecture
from build_desert import build_desert
from build_inland import build_inland
from prepare_people import convert_figure


def prepare(output, desert=False, inland=False):
    def manifest(filename):
        return json.loads(Path(__file__).with_name(filename).read_text(encoding="utf-8"))

    coastal = manifest("coastal-assets.json")
    if inland:
        inventory = {**manifest("inland-materials.json"), **manifest("landscape-materials.json")}
        inventory.pop("sparse_grass")
    elif desert:
        architecture = manifest("architecture-materials.json")
        inventory = {"rock_face_01": coastal["rock_face_01"],
                     "large_sandstone_blocks_01": architecture["large_sandstone_blocks_01"],
                     **manifest("desert-materials.json")}
    else:
        inventory = {**coastal, **manifest("architecture-materials.json")}
    def verified(url, expected):
        request = urllib.request.Request(url, headers={"User-Agent": "Noctis scenery asset build"})
        with urllib.request.urlopen(request, timeout=60) as response:
            original = response.read()
        if hashlib.sha256(original).hexdigest() != expected:
            raise ValueError(f"Upstream asset content changed: {url}")
        return original

    for model, files in inventory.items():
        directory = output / model
        for name, (url, expected) in files.items():
            original = verified(url, expected)
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.suffix == ".jpg":
                filters = ["-vf", "scale=512:512:flags=lanczos"] if desert and model in ("terrain_red_01", "worn_asphalt") else []
                if desert and model == "terrain_red_01" and name == "diff.jpg":
                    filters = ["-vf", "scale=512:512:flags=lanczos,eq=gamma=1.28:contrast=0.83"]
                subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-y", "-f", "image2pipe",
                                "-c:v", "mjpeg", "-i", "pipe:0", "-frames:v", "1",
                                *filters, "-pix_fmt", "rgb24", str(target.with_suffix(".png"))],
                               input=original, check=True)
            elif target.suffix == ".gltf" and model != "grass_medium_01":
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
        print(f"Verified CC0 asset: {model} ({len(files)} source files)")
    if inland:
        for name in ("pine_sapling_medium", "grass_medium_01"):
            foliage = output / name
            combined = foliage / "foliage.png"
            subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-y", "-i", str(foliage/"diff.png"),
                            "-i", str(foliage/"alpha.png"), "-filter_complex",
                            "[1:v]format=gray[mask];[0:v][mask]alphamerge", "-frames:v", "1",
                            "-pix_fmt", "rgba", str(combined)], check=True)
            combined.replace(foliage / "diff.png")
            (foliage / "alpha.png").unlink()
    people = {"taylor"} if desert else {"sage", "colleagues"}
    for name, (url, expected) in manifest("people-assets.json").items():
        if name in people:
            directory = output / "people"
            directory.mkdir(parents=True, exist_ok=True)
            convert_figure(verified(url, expected), directory, name)
            print(f"Verified CC0 figure: {name}")
    if inland:
        build_inland(output)
    elif desert:
        build_desert(output)
    else:
        build_architecture(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    scene = parser.add_mutually_exclusive_group()
    scene.add_argument("--desert", action="store_true")
    scene.add_argument("--inland", action="store_true")
    args = parser.parse_args()
    prepare(args.output, args.desert, args.inland)
