import argparse
from array import array
import json
import math
from pathlib import Path
import random
import subprocess
import sys
import wave


RATE = 48_000
FPS = 24
SECONDS = 12
CALLS = ((1.15, .72, 770, -.65), (2.03, .52, 910, -.40), (2.64, .42, 865, -.20),
         (6.48, .69, 715, .42), (7.28, .54, 840, .25), (9.67, .66, 810, .54),
         (10.44, .47, 920, .31))


def write_ambience(path):
    rng = random.Random(741)
    samples = array("h")
    low, middle, high = 0., 0., 0.
    for index in range(RATE * SECONDS):
        time = index / RATE
        noise = rng.uniform(-1., 1.)
        low += .013 * (noise - low)
        middle += .092 * (noise - middle)
        high += .45 * (noise - high)
        surge = (.5 + .5 * math.sin(math.tau * time / 3.3 - .8)) ** 3
        wash = (.5 + .5 * math.sin(math.tau * time / 2.1 + .58)) ** 2
        surf = 1.06 * low * (.55 + .94 * surge) + (high - middle) * (.045 + .23 * wash)
        spread = (noise - high) * (.024 + .035 * surge)
        left, right = surf - spread, surf + spread
        for start, length, pitch, pan in CALLS:
            age = time - start
            if 0. <= age < length:
                progress = age / length
                envelope = min(1., age * 55.) * (1. - progress) ** .80
                phase = math.tau * (pitch * age + 200. * age * age - 290. * age ** 3)
                vibrato = .23 * math.sin(math.tau * 29. * age)
                voice = .18 * envelope * (.72 + .28 * math.sin(math.tau * 18. * age)) * (
                    .54 * math.sin(phase + vibrato) + .32 * math.sin(2. * phase + .35) +
                    .15 * math.sin(3. * phase - .4) + .13 * noise)
                left += voice * math.sqrt(.5 * (1. - pan))
                right += voice * math.sqrt(.5 * (1. + pan))
        fade = min(1., time / .27, (SECONDS - time) / .44)
        samples.extend((round(32767 * math.tanh(left * 1.15) * fade),
                        round(32767 * math.tanh(right * 1.15) * fade)))
    peak = max(map(abs, samples)) / 32768
    if not (.10 < peak < .99):
        raise RuntimeError(f"Invalid ambience peak: {peak}")
    if sys.byteorder != "little":
        samples.byteswap()
    with wave.open(str(path), "wb") as audio:
        audio.setparams((2, 2, RATE, RATE * SECONDS, "NONE", "not compressed"))
        audio.writeframes(samples.tobytes())
    return round(peak, 4)


def build_pv(frames, output):
    expected_frames = {f"frame-{index:04d}.ppm" for index in range(FPS * SECONDS)}
    if {path.name for path in frames.glob("frame-*.ppm")} != expected_frames:
        raise ValueError("The frame sequence is incomplete")
    ambience = output / "sunset-ambience.wav"
    peak = write_ambience(ambience)
    video = output / "Sunset-Resort-PV.mp4"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                    "-framerate", str(FPS), "-i", str(frames / "frame-%04d.ppm"), "-i", str(ambience),
                    "-c:v", "libx264", "-crf", "19", "-preset", "fast", "-pix_fmt", "yuv420p",
                    "-c:a", "aac", "-b:a", "128k", "-movflags", "+faststart", "-shortest", str(video)],
                   check=True)
    probe = json.loads(subprocess.check_output(
        ["ffprobe", "-v", "error", "-show_format", "-show_streams", "-of", "json", str(video)]))
    picture, sound = probe["streams"]
    if (picture["codec_name"], picture["width"], picture["height"], picture["r_frame_rate"],
        int(picture["nb_frames"])) != ("h264", 960, 540, "24/1", FPS * SECONDS):
        raise ValueError("Unexpected picture format")
    if (sound["codec_name"], sound["channels"], int(sound["sample_rate"])) != ("aac", 2, RATE):
        raise ValueError("Unexpected audio format")
    duration = float(probe["format"]["duration"])
    if abs(duration - SECONDS) > .06 or video.stat().st_size > 8 * 1024 * 1024:
        raise ValueError("Unexpected video duration or size")
    subprocess.run(["ffmpeg", "-v", "error", "-xerror", "-i", str(video), "-f", "null", "-"], check=True)
    report = {"duration_s": duration, "video_frames": int(picture["nb_frames"]), "fps": FPS,
              "dimensions": [picture["width"], picture["height"]], "video_codec": "h264",
              "audio_codec": "aac", "audio_channels": 2, "audio_sample_rate_hz": RATE,
              "synthesized_gull_calls_s": [call[0] for call in CALLS],
              "source_audio_peak_fraction": peak, "video_bytes": video.stat().st_size}
    encoded = json.dumps(report, indent=2)
    (output / "pv-results.json").write_text(encoded + "\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build_pv(args.frames, args.output)
