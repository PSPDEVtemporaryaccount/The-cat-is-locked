#!/usr/bin/env python3
"""Prepare the user's menu PNG artwork as PSP GU RGBA8888 textures."""
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "build")
os.makedirs(OUT, exist_ok=True)

# Decode the supplied elevator MP3 to compact PSP-friendly mono PCM.
AUDIO_SOURCE = os.path.join(ROOT, "assets", "Audio", "1 min de musique dascenseur 🎶.mp3")
AUDIO_OUTPUT = os.path.join(OUT, "menu_audio.pcm")
if not os.path.isfile(AUDIO_SOURCE):
    raise FileNotFoundError("Missing menu music: " + AUDIO_SOURCE)
subprocess.run([
    "ffmpeg", "-y", "-v", "error", "-i", AUDIO_SOURCE,
    "-vn", "-ac", "1", "-ar", "44100", "-f", "s16le",
    "-acodec", "pcm_s16le", AUDIO_OUTPUT
], check=True)
if os.path.getsize(AUDIO_OUTPUT) < 4096:
    raise RuntimeError("Decoded menu music is unexpectedly short")
print("Elevator music decoded -> %s (%d bytes)" % (AUDIO_OUTPUT, os.path.getsize(AUDIO_OUTPUT)))
