"""Validate paired Sunset Harbor captures and export lossless review images."""
import csv
import hashlib
import json
import statistics
import struct
import sys
import zlib
from pathlib import Path

WIDTH, HEIGHT = 960, 540
SHOTS = ("wide", "close")
ASSETS = ("simple", "detailed")


def read_ppm(path):
    with path.open("rb") as data:
        if data.readline() != b"P6\n" or data.readline() != b"960 540\n" or data.readline() != b"255\n":
            raise ValueError(f"Unexpected PPM header: {path}")
        pixels = data.read()
    if len(pixels) != WIDTH * HEIGHT * 3 or len(set(pixels)) < 32:
        raise ValueError(f"Empty or invalid render: {path}")
    return pixels


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def write_png(path, pixels):
    row = WIDTH * 3
    lines = b"".join(b"\0" + pixels[offset:offset + row] for offset in range(0, len(pixels), row))
    header = struct.pack(">2I5B", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header) +
                     png_chunk(b"IDAT", zlib.compress(lines)) + png_chunk(b"IEND", b""))


def main(directory):
    with (directory / "samples.csv").open(encoding="ascii", newline="") as source:
        rows = list(csv.DictReader(source))
    if len(rows) != 24:
        raise ValueError(f"Expected 24 frame samples, got {len(rows)}")

    results = {}
    for shot in SHOTS:
        pixels = {asset: read_ppm(directory / f"{shot}-{asset}.ppm") for asset in ASSETS}
        assets = {}
        for asset in ASSETS:
            selection = [row for row in rows if (row["shot"], row["asset"]) == (shot, asset)]
            if [int(row["sample"]) for row in selection] != list(range(6)):
                raise ValueError(f"Missing samples: {shot}/{asset}")
            triangles = {int(row["asset_triangles"]) for row in selection}
            if len(triangles) != 1:
                raise ValueError(f"Inconsistent triangle counts: {shot}/{asset}")
            times = [float(row["warp_cpu_submit_wait_ms"]) for row in selection]
            if not all(0 < elapsed < 60_000 for elapsed in times):
                raise ValueError(f"Invalid frame duration: {shot}/{asset}")
            assets[asset] = {
                "architecture_triangles": triangles.pop(),
                "warp_cpu_submit_wait_median_ms": round(statistics.median(times), 3),
                "rgb_sha256": hashlib.sha256(pixels[asset]).hexdigest(),
            }
            write_png(directory / f"{shot}-{asset}.png", pixels[asset])
        if assets["simple"]["architecture_triangles"] >= assets["detailed"]["architecture_triangles"]:
            raise ValueError("Detailed asset has no extra geometry")

        simple, detailed = pixels["simple"], pixels["detailed"]
        sum_difference = 0
        changed = 0
        for offset in range(0, len(simple), 3):
            difference = [abs(simple[offset + lane] - detailed[offset + lane]) for lane in range(3)]
            sum_difference += sum(difference)
            changed += max(difference) > 8
        percentage = 100 * changed / (WIDTH * HEIGHT)
        if percentage < 1:
            raise ValueError(f"Asset toggle has no observable effect in {shot}")
        results[shot] = {
            "assets": assets,
            "percent_pixels_with_any_rgb_difference_gt_8": round(percentage, 2),
            "mean_absolute_rgb_difference_0_to_255": round(sum_difference / len(simple), 3),
        }
    if results["wide"]["assets"] == results["close"]["assets"]:
        raise ValueError("Both camera shots rendered identical evidence")
    first = read_ppm(directory / "wide-detailed.ppm")
    later = read_ppm(directory / "wide-water-later.ppm")
    left_sea_pixels = [3 * (row * WIDTH + column)
                       for row in range(340, 470) for column in range(40, 290)]
    changed_sea = sum(max(abs(first[offset + channel] - later[offset + channel])
                          for channel in range(3)) > 8 for offset in left_sea_pixels)
    if changed_sea < len(left_sea_pixels) * .02:
        raise ValueError("Sea appears static in the fixed-camera temporal comparison")
    write_png(directory / "wide-water-later.png", later)
    report = {
        "adapter": "D3D12 WARP", "resolution": [WIDTH, HEIGHT], "shots": results,
        "wide_fixed_camera_sea_motion": {
            "time_delta_s": .62, "roi_xyxy": [40, 340, 290, 470],
            "percent_pixels_with_any_rgb_difference_gt_8": round(100 * changed_sea / len(left_sea_pixels), 2),
        },
    }
    encoded = json.dumps(report, indent=2)
    (directory / "results.json").write_text(encoded + "\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    main(Path(sys.argv[1]))
