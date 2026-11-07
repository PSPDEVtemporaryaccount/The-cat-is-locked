#!/usr/bin/env python3
"""Prepare the user's menu PNG artwork as PSP GU RGBA8888 textures."""
import os
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
convert(MENUS, "menu_JOUER_1.png", "play_resume.rgba")
convert(MENUS, "Menu_JOUER_2.png", "play_new.rgba")
convert(MENUS, "Menu_PARAMÈTRES.png", "settings.rgba")
convert(MENUS, "Menu_REPRISE DE LA PARTIE.png", "saves.rgba")
