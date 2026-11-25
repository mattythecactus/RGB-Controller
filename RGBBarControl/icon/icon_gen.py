#!/usr/bin/env python3
"""Generate RGB-Controller icon0.png (512x512).
Design: dark background, a glowing rainbow DualShock light bar across
the middle, big "RGB" text above it and "CONTROLLER" below.
Run: python3 icon_gen.py  -> writes icon0.png in the current directory.
"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W = H = 512
img = Image.new("RGB", (W, H), (10, 12, 24))
draw = ImageDraw.Draw(img)

# Subtle diagonal sheen
for i in range(0, 512, 4):
    c = 10 + i // 64
    draw.line([(i, 0), (0, i)], fill=(c, c + 2, c + 8))

# Rainbow light bar: horizontal gradient bar, rounded, with glow
bar_x0, bar_y0, bar_x1, bar_y1 = 56, 250, 456, 310
glow = Image.new("RGB", (W, H), (0, 0, 0))
gd = ImageDraw.Draw(glow)
for x in range(bar_x0, bar_x1):
    t = (x - bar_x0) / (bar_x1 - bar_x0)
    # rainbow: red -> yellow -> green -> cyan -> blue -> magenta
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(t * 0.85, 1.0, 1.0)
    gd.line([(x, bar_y0), (x, bar_y1)], fill=(int(r * 255), int(g * 255), int(b * 255)))
glow = glow.filter(ImageFilter.GaussianBlur(18))
img = Image.alpha_composite(img.convert("RGBA"), glow.convert("RGBA")).convert("RGB")
draw = ImageDraw.Draw(img)
for x in range(bar_x0, bar_x1):
    t = (x - bar_x0) / (bar_x1 - bar_x0)
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(t * 0.85, 1.0, 1.0)
    draw.line([(x, bar_y0), (x, bar_y1)], fill=(int(r * 255), int(g * 255), int(b * 255)))
# White core line in the middle of the bar
draw.rectangle([bar_x0, 274, bar_x1, 286], fill=(255, 255, 255))

# Text
def font(size):
    for path in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(path, size)
        except Exception:
            continue
    return ImageFont.load_default()

f_big = font(150)
f_small = font(44)
for txt, fnt, y in (("RGB", f_big, 70), ("CONTROLLER", f_small, 350)):
    bbox = draw.textbbox((0, 0), txt, font=fnt)
    w = bbox[2] - bbox[0]
    draw.text(((W - w) / 2, y), txt, font=fnt, fill=(255, 255, 255))

img.save("icon0.png", "PNG")
print("wrote icon0.png", img.size)
