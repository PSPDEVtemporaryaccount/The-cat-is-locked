#!/usr/bin/env python3
"""Decode the repository's PNG menu artwork into PSP GU RGBA8888 textures."""
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "Pictures")
OUT = os.path.join(ROOT, "build")
os.makedirs(OUT, exist_ok=True)

def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c

def decode_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Not a PNG: " + path)
    pos = 8
    compressed = bytearray()
    palette = None
    transparency = b""
    width = height = bit_depth = color_type = None
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos+4])[0]
        kind = data[pos+4:pos+8]
        chunk = data[pos+8:pos+8+length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, bit_depth, color_type, comp, filt, interlace = struct.unpack(">IIBBBBB", chunk)
            if comp != 0 or filt != 0 or interlace != 0:
                raise ValueError("Unsupported PNG compression/interlace: " + path)
        elif kind == b"PLTE":
            palette = chunk
        elif kind == b"tRNS":
            transparency = chunk
        elif kind == b"IDAT":
            compressed.extend(chunk)
        elif kind == b"IEND":
            break
    if bit_depth != 8:
        raise ValueError("Expected 8-bit PNG: " + path)
    channels = {0:1, 2:3, 3:1, 4:2, 6:4}.get(color_type)
    if channels is None:
        raise ValueError("Unsupported PNG color type: " + str(color_type))
    bpp = channels
    stride = width * channels
    raw = zlib.decompress(bytes(compressed))
    rows = []
    offset = 0
    previous = bytearray(stride)
    for _ in range(height):
        filt = raw[offset]
        offset += 1
        scan = bytearray(raw[offset:offset+stride])
        offset += stride
        for i in range(stride):
            left = scan[i-bpp] if i >= bpp else 0
            up = previous[i]
            upper_left = previous[i-bpp] if i >= bpp else 0
            if filt == 1:
                scan[i] = (scan[i] + left) & 255
            elif filt == 2:
                scan[i] = (scan[i] + up) & 255
            elif filt == 3:
                scan[i] = (scan[i] + ((left + up) >> 1)) & 255
            elif filt == 4:
                scan[i] = (scan[i] + paeth(left, up, upper_left)) & 255
            elif filt != 0:
                raise ValueError("Unknown PNG filter " + str(filt))
        rows.append(scan)
        previous = scan
    rgba = bytearray(width * height * 4)
    out = 0
    for row in rows:
        for x in range(width):
            i = x * channels
            if color_type == 6:
                r, g, b, a = row[i:i+4]
            elif color_type == 2:
                r, g, b = row[i:i+3]
                a = 255
            elif color_type == 0:
                r = g = b = row[i]
                a = 255
            elif color_type == 4:
                r = g = b = row[i]
                a = row[i+1]
            else:
                index = row[i]
                if palette is None or index * 3 + 2 >= len(palette):
                    raise ValueError("Invalid indexed PNG palette")
                r, g, b = palette[index*3:index*3+3]
                a = transparency[index] if index < len(transparency) else 255
            rgba[out:out+4] = bytes((r, g, b, a))
            out += 4
    return width, height, rgba

def fill_rect(pixels, width, height, x1, y1, x2, y2, color):
    x1, y1 = max(0, x1), max(0, y1)
    x2, y2 = min(width, x2), min(height, y2)
    rgba = bytes(color)
    for y in range(y1, y2):
        start = (y * width + x1) * 4
        pixels[start:(y * width + x2) * 4] = rgba * (x2 - x1)

def write_texture(source_name, output_name, texture_height, masks=()):
    width, height, pixels = decode_png(os.path.join(SRC, source_name))
    if width != 480 or height > texture_height:
        raise ValueError("%s must be 480 pixels wide and at most %d high (got %dx%d)" %
                         (source_name, texture_height, width, height))
    for rect, color in masks:
        fill_rect(pixels, width, height, *rect, color)
    texture = bytearray(512 * texture_height * 4)
    for y in range(height):
        src = y * width * 4
        dst = y * 512 * 4
        texture[dst:dst + width * 4] = pixels[src:src + width * 4]
    with open(os.path.join(OUT, output_name), "wb") as f:
        f.write(texture)
    print("%s: %dx%d -> %s (%d bytes)" %
          (source_name, width, height, output_name, len(texture)))

write_texture("Menu_principal_asset.png", "menu_main.rgba", 512, [
    ((16, 142, 66, 185), (255, 255, 255, 255)),
    ((174, 142, 224, 185), (255, 255, 255, 255)),
])
write_texture("menu_JOUER_1.png", "menu_play_resume.rgba", 512)
write_texture("Menu_JOUER_2.png", "menu_play_new.rgba", 512)
write_texture("Menu_PARAMÈTRES.png", "menu_settings.rgba", 512, [
    ((236, 168, 278, 216), (85, 85, 85, 255)),
    ((420, 168, 465, 216), (85, 85, 85, 255)),
])
write_texture("Menu_REPRISE DE LA PARTIE.png", "menu_resume.rgba", 512, [
    ((240, 20, 280, 65), (85, 85, 85, 255)),
    ((410, 20, 450, 65), (85, 85, 85, 255)),
    ((275, 30, 465, 250), (85, 85, 85, 255)),
])
