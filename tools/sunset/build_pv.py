import argparse
from array import array
import csv
import json
import math
from pathlib import Path
import random
import subprocess
import sys
import wave


RATE = 48_000
FPS = 24
SECONDS = 20
CALLS = ((1.15, .72, 770, -.65), (2.03, .52, 910, -.40), (2.64, .42, 865, -.20),
         (6.48, .69, 715, .42), (7.28, .54, 840, .25), (9.67, .66, 810, .54),
         (10.44, .47, 920, .31), (15.2, .73, 720, -.28), (16.09, .48, 840, -.45))


def write_pcm(path, samples):
    if sys.byteorder != "little":
        samples = array("h", samples)
        samples.byteswap()
    with wave.open(str(path), "wb") as audio:
        audio.setparams((2, 2, RATE, RATE * SECONDS, "NONE", "not compressed"))
        audio.writeframes(samples.tobytes())


def make_ambience():
    rng = random.Random(741)
    samples = array("h")
    low, middle, high = 0., 0., 0.
    for index in range(RATE * SECONDS):
        time = index / RATE
        noise = rng.uniform(-1., 1.)
        low += .013 * (noise - low)
        middle += .092 * (noise - middle)
        high += .45 * (noise - high)
        surge = (.5 + .5 * math.sin(1.81 * time + .78 * math.sin(.87 * time))) ** 3
        wash = (.5 + .5 * math.sin(3.12 * time + .64 * math.sin(1.41 * time))) ** 2
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
    return samples


def make_villa_music():
    samples = array("f", [0.]) * (RATE * SECONDS * 2)
    chords = ((57, 64, 69, 72, 76, 71), (53, 60, 65, 69, 72, 67),
              (55, 62, 67, 71, 74, 69), (52, 59, 64, 67, 71, 69))
    for phrase in range(8):
        notes = chords[phrase % len(chords)]
        for beat, midi in enumerate(notes):
            onset = round((phrase * 2.5 + beat * .37) * RATE)
            frequency = 440. * 2. ** ((midi - 69) / 12.)
            pan = (.16, .34, .62, .76, .57, .25)[beat]
            for index in range(int(1.6 * RATE)):
                age = index / RATE
                phase = math.tau * frequency * age
                envelope = min(1., age * 440.) * math.exp(-age * 3.5) * min(1., (1.6 - age) * 12.)
                tone = envelope * (.135 * math.sin(phase) + .041 * math.sin(phase * 2.01) +
                                   .020 * math.exp(-5. * age) * math.sin(phase * 3.016))
                position = 2 * ((onset + index) % (RATE * SECONDS))
                samples[position] += tone * math.sqrt(1. - pan)
                samples[position + 1] += tone * math.sqrt(pan)
    return array("h", (round(sample * 32767) for sample in samples))


def mix_audio(frames, output):
    with (frames / "listener.csv").open(newline="", encoding="ascii") as source:
        rows = list(csv.DictReader(source))
    if [int(row["frame"]) for row in rows] != list(range(FPS * SECONDS)):
        raise ValueError("Incomplete listener trajectory")
    gains = [float(row["villa_gain"]) for row in rows]
    if not all(0. < gain <= 1. for gain in gains) or max(gains) < .34 or min(gains) > .015:
        raise ValueError("The camera must move between distant and near-villa audio")
    sea = make_ambience()
    music = make_villa_music()
    write_pcm(output / "sunset-ambience.wav", sea)
    write_pcm(output / "villa-music.wav", music)
    mixed = array("h")
    for frame in range(RATE * SECONDS):
        visual_frame = frame * FPS / RATE
        column = int(visual_frame)
        fraction = visual_frame - column
        gain = gains[column] * (1. - fraction) + gains[min(column + 1, len(gains) - 1)] * fraction
        fade = min(1., frame / (RATE * .2), (RATE * SECONDS - frame) / (RATE * .6))
        for channel in (0, 1):
            offset = frame * 2 + channel
            mixed.append(round((.82 * sea[offset] + gain * music[offset]) * fade))
    peak = max(map(abs, mixed)) / 32768
    if not (.10 < peak < .99):
        raise RuntimeError(f"Invalid ambience peak: {peak}")
    write_pcm(frames / "pv-mix.wav", mixed)
    return round(peak, 4), [round(min(gains), 4), round(max(gains), 4)]


def build_pv(frames, output):
    expected_frames = {f"frame-{index:04d}.ppm" for index in range(FPS * SECONDS)}
    if {path.name for path in frames.glob("frame-*.ppm")} != expected_frames:
        raise ValueError("The frame sequence is incomplete")
    peak, gain_range = mix_audio(frames, output)
    video = output / "Sunset-Resort-PV.mp4"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                    "-framerate", str(FPS), "-i", str(frames / "frame-%04d.ppm"), "-i", str(frames / "pv-mix.wav"),
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
              "source_audio_peak_fraction": peak, "villa_music_gain_min_max": gain_range,
              "video_bytes": video.stat().st_size}
    encoded = json.dumps(report, indent=2)
    (output / "pv-results.json").write_text(encoded + "\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build_pv(args.frames, args.output)
