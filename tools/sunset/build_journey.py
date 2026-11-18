"""Validate the continuous coastal take and assemble the three chapters."""
import argparse
from array import array
import csv
import hashlib
import json
import math
from pathlib import Path
import random
import subprocess
import wave

from build_pv import FPS, RATE, make_ambience
from verify_captures import WIDTH, HEIGHT, read_ppm, write_png


COAST = 26
DURATION = 50
DAY_HOLD_FRAMES = 121


def ffmpeg(*args):
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-y", *args],
                   check=True, timeout=240)


def probe(path):
    return json.loads(subprocess.check_output(
        ["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(path)]))


def require_video(picture, frames):
    if (picture["codec_name"], picture["width"], picture["height"], picture["r_frame_rate"],
            int(picture["nb_frames"])) != ("h264", WIDTH, HEIGHT, f"{FPS}/1", frames):
        raise ValueError("Unexpected chapter format")


def encode_coast(frames, output):
    output.mkdir(parents=True, exist_ok=True)
    count = COAST * FPS
    with (frames / "voyage.csv").open(newline="", encoding="ascii") as stream:
        rows = list(csv.DictReader(stream))
    if [int(row["frame"]) for row in rows] != list(range(count)):
        raise ValueError("Coastal camera trace is incomplete")
    if {path.name for path in frames.glob("*.ppm")} != {f"frame-{i:04d}.ppm" for i in range(count)}:
        raise ValueError("Coastal images are incomplete")

    def values(row, names):
        result = tuple(float(row[name]) for name in names)
        if not all(map(math.isfinite, result)):
            raise ValueError("Non-finite camera or vessel placement")
        return result

    cameras = [values(row, ("eye_x", "eye_y", "eye_z", "yaw", "pitch")) for row in rows]
    lighting = [float(row["daylight"]) for row in rows]
    boats = [values(row, ("boat_x", "boat_z", "wake")) for row in rows]
    if len(set(cameras[:DAY_HOLD_FRAMES])) != 1 or lighting[120] - lighting[0] < .8:
        raise ValueError("The day-to-night take must hold composition and change the lighting")
    if not all(math.isfinite(v) and 0. <= v <= 1. for v in lighting) or any(
            a > b for a,b in zip(lighting, lighting[1:])):
        raise ValueError("The coastal clock must advance monotonically")
    step = max(math.dist(a[:3],b[:3]) for a,b in zip(cameras, cameras[1:]))
    turn = max(math.hypot(math.remainder(b[3]-a[3], math.tau),b[4]-a[4])
               for a,b in zip(cameras, cameras[1:]))
    water_travel = math.dist(boats[0][:2], boats[-1][:2])
    if (step > .85 or turn > .13 or min(eye[1] for eye in cameras) < 1.5
            or cameras[-1][2] < 57. or water_travel < 78. or boats[0][2] != 0. or boats[-1][2] < .95):
        raise ValueError(f"Invalid camera motion or offshore departure: {step=}, {turn=}, {water_travel=}")

    stills = {"harbor-day": 0, "harbor-dusk": 120, "pier-to-boat": 341, "offshore": 541}
    pictures = {}
    for name, frame in stills.items():
        pictures[name] = read_ppm(frames / f"frame-{frame:04d}.ppm")
        write_png(output / (name + ".png"), pictures[name])
    day, dusk = pictures["harbor-day"], pictures["harbor-dusk"]
    changed = sum(max(abs(day[i+j]-dusk[i+j]) for j in range(3)) > 12
                  for i in range(0, len(day), 3)) / (WIDTH*HEIGHT)
    if changed < .38 or len(set(pictures.values())) != len(pictures):
        raise ValueError("Captured lighting or scene does not visibly change")
    video = output / "voyage.mp4"
    ffmpeg("-framerate", str(FPS), "-i", str(frames/"frame-%04d.ppm"), "-an", "-c:v", "libx264",
           "-crf", "15", "-preset", "fast", "-pix_fmt", "yuv420p", str(video))
    require_video(probe(video)["streams"][0], count)
    report = {"coast_seconds": COAST, "coast_frames": count, "msaa": 4,
              "fixed_camera_day_to_dusk_changed_fraction": round(changed, 4),
              "max_camera_step_m": round(step, 4), "max_camera_turn_rad": round(turn, 4),
              "boat_travel_m": round(water_travel, 2), "shot_frames": stills}
    (output/"coastal-voyage.json").write_text(json.dumps(report,indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report))


def soundtrack(path):
    """Original instrumental cue and weather layers authored for this cut."""
    length = RATE * DURATION
    score = array("f", [0.]) * length
    # Notes are in MIDI pitch. The plucked phrases enter ahead of the section changes.
    chords = ((50,57,61,64,66), (54,61,64,68,71), (47,54,61,62,64),
              (43,50,57,59,66), (52,59,64,66,71), (45,52,59,62,66))
    for phrase in range(12):
        pitches = chords[phrase % len(chords)]
        start = .4 + phrase * 4.05
        for offset,note,sustain,level in (
                (0.,pitches[0],4.4,.063), (.12,pitches[1],4.2,.042),
                (.44,pitches[3]+12,1.42,.078), (1.17,pitches[2]+12,1.8,.075),
                (2.22,pitches[4]+12,1.9,.064), (3.,pitches[3]+12,1.14,.066)):
            hz = 440. * 2. ** ((note-69)/12.)
            first = round((start+offset)*RATE)
            pluck = sustain < 2.0
            for i in range(min(round(sustain*RATE), length-first)):
                age = i/RATE
                phase = math.tau*hz*age
                env = min(1.,age/(.023 if pluck else .7),(sustain-age)/.6)
                body = math.sin(phase)+.23*math.sin(phase*2.002)+.08*math.sin(phase*3.01)
                score[first+i] += level*env*body*(math.exp(-age*1.95) if pluck else .76)

    coast = make_ambience()
    rng = random.Random(34841)
    samples = array("h")
    low, high = 0.,0.
    for i,tone in enumerate(score):
        t = i/RATE
        noise = rng.uniform(-1.,1.)
        low += .008*(noise-low)
        high += .29*(noise-high)
        rain = max(0.,min(1.,(t-15.2)/.8,(20.65-t)/1.0))
        harbor = max(0.,min(1.,(t-23.1)/2.0))
        offshore = max(0.,min(1.,(t-38.)/4.))
        motor = max(0.,min(1.,(t-19.5)/1.,(24.5-t)/1.))
        land = .105*low + .086*rain*(noise-high) + .012*motor*math.sin(math.tau*47.*t)
        foam = offshore*(.19*low + .064*(high-low))*(.72+.28*math.sin(t*3.14))
        coast_index = 2 * (int(max(0.,t-24.)*RATE) % (len(coast)//2))
        fade = max(0.,min(1., t/.55,(DURATION-t)/1.85))
        for k in (0,1):
            side = -1. if k == 0 else 1.
            sea = harbor*.70*coast[coast_index+k]/32768 + foam
            sound = tone+land+sea+side*(.012*rain*noise+.10*tone*math.sin(t*.49))
            samples.append(round(32767*math.tanh(sound*1.5)*fade))
    if not .10 < max(map(abs,samples))/32768 < .96:
        raise ValueError("Unexpected soundtrack level")
    with wave.open(str(path),"wb") as handle:
        handle.setparams((2,2,RATE,length,"NONE","not compressed"))
        handle.writeframes(samples.tobytes())


def assemble(desert, inland, coast, output):
    output.mkdir(parents=True, exist_ok=True)
    for path,seconds in ((desert,12),(inland,12),(coast,COAST)):
        picture, = probe(path)["streams"]
        require_video(picture, seconds*FPS)
    audio = output/"journey-score.wav"
    soundtrack(audio)
    video = output/"Noctis-Journey-to-Sea.mp4"
    ffmpeg("-i",str(desert),"-i",str(inland),"-i",str(coast),"-i",str(audio),"-filter_complex",
           f"[0:v][1:v][2:v]concat=n=3:v=1:a=0,fps={FPS},fade=t=in:d=0.34,fade=t=out:st=48.55:d=1.45[v]",
           "-map","[v]","-map","3:a:0","-c:v","libx264","-crf","18","-preset","fast",
           "-maxrate","2500k","-bufsize","3000k","-pix_fmt","yuv420p","-c:a","aac","-b:a","160k",
           "-movflags","+faststart",str(video))
    report = probe(video)
    picture,sound = report["streams"]
    require_video(picture, DURATION*FPS)
    if (sound["codec_name"],sound["channels"],int(sound["sample_rate"])) != ("aac",2,RATE):
        raise ValueError("Unexpected music track format")
    length = float(report["format"]["duration"])
    if abs(length-DURATION) > .06 or video.stat().st_size > 19*1024*1024:
        raise ValueError("Unexpected length or size for the assembled PV")
    ffmpeg("-xerror","-i",str(video),"-f","null","-")
    result = {"seconds": length, "frames": DURATION*FPS, "size": [WIDTH,HEIGHT], "fps": FPS,
              "audio": "AAC stereo 48000 Hz, original synthesized score and ambience",
              "chapters": {"desert":0,"grassland":12,"rain_forest":16,"factory":20,"day_to_dusk":24,
                           "camera_moves_to_pier":29,"departure":38,"offshore":45.5},
              "bytes": video.stat().st_size, "sha256": hashlib.sha256(video.read_bytes()).hexdigest()}
    (output/"journey.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="operation", required=True)
    coast = commands.add_parser("coast")
    coast.add_argument("frames",type=Path)
    coast.add_argument("output",type=Path)
    montage = commands.add_parser("assemble")
    for name in ("desert","inland","coast","output"):
        montage.add_argument(name,type=Path)
    args = parser.parse_args()
    if args.operation == "coast":
        encode_coast(args.frames,args.output)
    else:
        assemble(args.desert,args.inland,args.coast,args.output)
