"""Encode the three-cut route previews and verify every decoded frame."""
import argparse
import json
from pathlib import Path
import subprocess

from verify_captures import read_ppm, write_png


FRAME_RATE = 24
DURATION = 12
SHOT_FRAMES = {"basin": 22, "road": 142, "sunset-pass": 253}
INLAND_SHOT_FRAMES = {"grassland": 49, "wetland": 149, "factory": 248}


def build(frames, output, inland=False):
    title = "Inland-Tour" if inland else "Desert-Road"
    shot_frames = INLAND_SHOT_FRAMES if inland else SHOT_FRAMES
    expected = {f"frame-{index:04d}.ppm" for index in range(FRAME_RATE * DURATION)}
    if {frame.name for frame in frames.glob("*.ppm")} != expected:
        raise ValueError(f"Incomplete render sequence: {title}")
    output.mkdir(parents=True, exist_ok=True)
    samples = {}
    for name, frame in shot_frames.items():
        pixels = read_ppm(frames / f"frame-{frame:04d}.ppm")
        samples[name] = pixels
        write_png(output / f"{name}.png", pixels)
    if len(set(samples.values())) != len(shot_frames):
        raise ValueError(f"Camera shots did not change: {title}")

    video = output / f"{title}-PV.mp4"
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
        raise ValueError(f"Unexpected video format: {title}")
    duration = float(report["format"]["duration"])
    if abs(duration-DURATION) > .06 or video.stat().st_size > 5*1024*1024:
        raise ValueError(f"Unexpected video length or size: {title}, {duration}, {video.stat().st_size}")
    subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-i", str(video), "-f", "null", "-"], check=True)
    measured = {"adapter": "D3D12 WARP", "msaa_samples": 4, "duration_seconds": duration,
                "frames": int(picture["nb_frames"]), "resolution": [960, 540], "video_bytes": video.stat().st_size,
                "shot_frames": shot_frames}
    encoded = json.dumps(measured, indent=2)
    (output / ("inland-pv.json" if inland else "desert-pv.json")).write_text(encoded+"\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    arguments = argparse.ArgumentParser()
    arguments.add_argument("frames", type=Path)
    arguments.add_argument("output", type=Path)
    arguments.add_argument("--inland", action="store_true")
    options = arguments.parse_args()
    build(options.frames, options.output, options.inland)
