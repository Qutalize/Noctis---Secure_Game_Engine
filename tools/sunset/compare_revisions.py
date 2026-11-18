"""Export fixed-pose render pairs and isolate the effect of supersampling."""
import json
from pathlib import Path
import sys

from verify_captures import WIDTH, HEIGHT, read_ppm, write_png


def changed(first, second, threshold):
    pixels = sum(max(abs(first[offset + lane] - second[offset + lane]) for lane in range(3)) > threshold
                 for offset in range(0, len(first), 3))
    return round(100 * pixels / (WIDTH * HEIGHT), 2)


def compare(baseline, native, supersampled, output):
    results = {}
    for shot in ("wide", "shallows", "ruins", "dining"):
        renders = {}
        for label, directory in (("before", baseline), ("after", native), ("after-aa", supersampled)):
            renders[label] = read_ppm(directory / (shot + ".ppm"))
            write_png(output / f"{shot}-{label}.png", renders[label])
        revision = changed(renders["before"], renders["after"], 8)
        aa = changed(renders["after"], renders["after-aa"], 3)
        if revision < .5 or aa < .02:
            raise ValueError(f"Changes were not observable in {shot}: scene={revision}%, AA={aa}%")
        results[shot] = {"scene_pixels_changed_rgb_gt_8_percent": revision,
                         "aa_pixels_changed_rgb_gt_3_percent": aa}
    report = {"baseline": "db8813c94bcb3f6c7b24b7c19dc83200933f7ae1", "output": [WIDTH, HEIGHT],
              "native_render": [WIDTH, HEIGHT], "ssaa_render": [WIDTH * 2, HEIGHT * 2], "shots": results}
    encoded = json.dumps(report, indent=2)
    (output / "revision-comparison.json").write_text(encoded + "\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    compare(*(Path(value) for value in sys.argv[1:]))
