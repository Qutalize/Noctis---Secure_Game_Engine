"""Encode the three-cut wilderness chapter and verify every decoded frame."""
import argparse
import json
from pathlib import Path
import subprocess

from verify_captures import read_ppm, write_png


FRAME_RATE = 24
DURATION = 12
SHOT_FRAMES = {"basin": 22, "road": 142, "sunset-pass": 253}


def build(frames, output):
    expected = {f"frame-{index:04d}.ppm" for index in range(FRAME_RATE * DURATION)}
    if {frame.name for frame in frames.glob("*.ppm")} != expected:
        raise ValueError("Incomplete desert render sequence")
    output.mkdir(parents=True, exist_ok=True)
    samples = {}
    for name, frame in SHOT_FRAMES.items():
        pixels = read_ppm(frames / f"frame-{frame:04d}.ppm")
        samples[name] = pixels
        write_png(output / f"{name}.png", pixels)
    if len(set(samples.values())) != len(SHOT_FRAMES):
        raise ValueError("The desert camera shots did not change")

    video = output / "Desert-Road-PV.mp4"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                    "-framerate", str(FRAME_RATE), "-i", str(frames / "frame-%04d.ppm"),
                    "-an", "-c:v", "libx264", "-crf", "18", "-preset", "fast",
                    "-pix_fmt", "yuv420p", "-maxrate", "2800k", "-bufsize", "2800k",
                    "-movflags", "+faststart", str(video)], check=True)
    report = json.loads(subprocess.check_output(
        ["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(video)]))
    picture, = report["streams"]
    if (picture["codec_name"], picture["width"], picture["height"], picture["r_frame_rate"],
        int(picture["nb_frames"])) != ("h264", 960, 540, "24/1", FRAME_RATE*DURATION):
        raise ValueError("Unexpected desert video format")
    duration = float(report["format"]["duration"])
    if abs(duration-DURATION) > .06 or video.stat().st_size > 5*1024*1024:
        raise ValueError(f"Unexpected desert video length or size: {duration}, {video.stat().st_size}")
    subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-i", str(video), "-f", "null", "-"], check=True)
    measured = {"adapter": "D3D12 WARP", "msaa_samples": 4, "duration_seconds": duration,
                "frames": int(picture["nb_frames"]), "resolution": [960, 540], "video_bytes": video.stat().st_size,
                "shot_frames": SHOT_FRAMES}
    encoded = json.dumps(measured, indent=2)
    (output / "desert-pv.json").write_text(encoded+"\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    arguments = argparse.ArgumentParser()
    arguments.add_argument("frames", type=Path)
    arguments.add_argument("output", type=Path)
    options = arguments.parse_args()
    build(options.frames, options.output)
