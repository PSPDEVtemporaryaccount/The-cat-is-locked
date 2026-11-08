#!/usr/bin/env python3
"""Prepare the user's menu PNG artwork as PSP GU RGBA8888 textures."""
import os
import subprocess
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PICTURES = os.path.join(ROOT, "assets", "Pictures")
MAIN = os.path.join(PICTURES, "Main menu")
MENUS = os.path.join(PICTURES, "Game Menus")
OUT = os.path.join(ROOT, "build")
os.makedirs(OUT, exist_ok=True)

def convert(source_dir, source_name, output_name):
    path = os.path.join(source_dir, source_name)
    if not os.path.isfile(path):
        raise FileNotFoundError("Missing artwork: " + path)
    image = Image.open(path).convert("RGBA")
    if image.size != (480, 272):
        image = image.resize((480, 272), Image.Resampling.LANCZOS)
    texture = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    texture.paste(image, (0, 0))
    target = os.path.join(OUT, output_name)
    with open(target, "wb") as output:
        output.write(texture.tobytes())
    print("%s -> %s (512x512 RGBA)" % (source_name, target))

convert(MAIN, "Menu_principal_V2.png", "main_default.rgba")
convert(MAIN, "Menu_principal_V2_Jouer.png", "main_play.rgba")
convert(MAIN, "Menu_principal_V2_Paramètres.png", "main_settings.rgba")
convert(MAIN, "Menu_principal_V2_Quitter.png", "main_quit.rgba")
convert(MENUS, "Menu_JOUER_V2.png", "play_menu.rgba")
convert(MENUS, "Menu_Reprendre_V2.png", "play_resume.rgba")
convert(MENUS, "Menu_CommencerAventure.png", "start_adventure.rgba")
convert(MENUS, "Paramètres_V2.png", "settings.rgba")

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
