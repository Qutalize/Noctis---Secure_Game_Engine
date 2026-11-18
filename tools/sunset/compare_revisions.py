"""Export fixed-pose render pairs and isolate the effect of supersampling."""
import json
from pathlib import Path
import sys

from verify_captures import WIDTH, HEIGHT, read_ppm, write_png


def changed(first, second, threshold):
    pixels = sum(max(abs(first[offset + lane] - second[offset + lane]) for lane in range(3)) > threshold
                 for offset in range(0, len(first), 3))
    return round(100 * pixels / (WIDTH * HEIGHT), 2)


def compare(baseline, native, supersampled, multisampled, output):
    results = {}
    for shot in ("wide", "shallows", "pier", "villa", "ruins", "dining"):
        renders = {}
        for label, directory in (("before", baseline), ("after", native), ("after-aa", supersampled),
                                 ("after-msaa", multisampled)):
            renders[label] = read_ppm(directory / (shot + ".ppm"))
            write_png(output / f"{shot}-{label}.png", renders[label])
        revision = changed(renders["before"], renders["after"], 8)
        aa = changed(renders["after"], renders["after-aa"], 3)
        msaa = changed(renders["after"], renders["after-msaa"], 3)
        if aa < .02 or msaa < .02:
            raise ValueError(f"AA changes were not observable in {shot}: SSAA={aa}%, MSAA={msaa}%")
        results[shot] = {"scene_pixels_changed_rgb_gt_8_percent": revision,
                         "ssaa_pixels_changed_rgb_gt_3_percent": aa,
                         "msaa_pixels_changed_rgb_gt_3_percent": msaa}
    for shot in ("pier", "villa", "ruins"):
        if results[shot]["scene_pixels_changed_rgb_gt_8_percent"] < .5:
            raise ValueError(f"No observable architecture change in {shot}")
    report = {"baseline": "1219ab938b674f6d561990c8ded8c6f9b4facc91", "output": [WIDTH, HEIGHT],
              "msaa_samples": 4,
              "native_render": [WIDTH, HEIGHT], "ssaa_render": [WIDTH * 2, HEIGHT * 2], "shots": results}
    encoded = json.dumps(report, indent=2)
    (output / "revision-comparison.json").write_text(encoded + "\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    compare(*(Path(value) for value in sys.argv[1:]))
