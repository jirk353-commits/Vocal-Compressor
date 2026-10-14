"""Draws Vocal Compressor installer art: cream panel, navy meter box with gold/blue fill and orange fader pill."""
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
fontpath = sys.argv[1]
ORANGE, NAVY, GOLD, BLUE = (243, 156, 18), (30, 48, 72), (230, 165, 28), (31, 143, 214)

def plate(w, h):
    img = Image.new('RGBA', (w, h)); d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / h
        d.line([(0, y), (w, y)], fill=(int(241 - 22*t), int(240 - 22*t), int(236 - 22*t), 255))
    return img

def box(img, x, y, w, h, s=1.0):
    sh = Image.new('RGBA', img.size, (0, 0, 0, 0))
    ImageDraw.Draw(sh).rounded_rectangle([x, y + 6*s, x + w, y + h + 6*s], int(30*s), fill=(0, 0, 0, 90))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(8*s)))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([x, y, x + w, y + h], int(30*s), fill=(236, 235, 231), outline=(170, 168, 163), width=max(1, int(2*s)))
    ix, iy, iw, ih = x + 22*s, y + 22*s, w - 44*s, h - 44*s
    d.rounded_rectangle([ix, iy, ix + iw, iy + ih], int(16*s), fill=(14, 22, 38))
    sx, sy, sw, shh = ix + 8*s, iy + 8*s, iw - 16*s, ih - 16*s
    d.rounded_rectangle([sx, sy, sx + sw, sy + shh], int(10*s), fill=NAVY)
    zero = sy + shh * 0.4
    d.rectangle([sx + 6*s, sy + shh * 0.12, sx + sw - 6*s, zero], fill=BLUE)
    d.rectangle([sx + 6*s, zero, sx + sw - 6*s, sy + shh * 0.68], fill=GOLD)
    py = sy + shh * 0.55
    d.rectangle([x + 10*s, py - 2*s, x + w - 10*s, py + 2*s], fill=ORANGE)
    pw, ph = 70*s, 26*s
    d.rounded_rectangle([sx + sw/2 - pw/2, py - ph/2, sx + sw/2 + pw/2, py + ph/2], int(13*s), fill=NAVY, outline=ORANGE, width=max(2, int(3*s)))

def text(d, xy, s, f, col): d.text(xy, s, font=f, fill=col)

W, H = 328, 628
img = plate(W, H); box(img, 74, 70, 180, 280)
d = ImageDraw.Draw(img)
f1 = ImageFont.truetype(fontpath, 34); f2 = ImageFont.truetype(fontpath, 22); f3 = ImageFont.truetype(fontpath, 14)
def c(y, s, f, col): text(d, ((W - d.textlength(s, font=f)) / 2, y), s, f, col)
c(390, "VOCAL", f1, (43, 45, 49))
c(432, "COMPRESSOR", f1, (43, 45, 49))
c(482, "COMP + VOX IN ONE", f3, (201, 138, 30))
d.line([(60, 525), (W - 60, 525)], fill=ORANGE, width=2)
c(550, "ROHIT JI", f2, (165, 42, 42))
img.convert('RGB').save('wizard-large.bmp'); img.convert('RGB').resize((164, 314), Image.LANCZOS).save('wizard-large-100.bmp')
s = plate(110, 110); box(s, 22, 8, 66, 94, 0.36)
s.convert('RGB').save('wizard-small.bmp'); s.convert('RGB').resize((55, 55), Image.LANCZOS).save('wizard-small-100.bmp')
ic = Image.new('RGBA', (512, 512), (0, 0, 0, 0))
m = Image.new('L', (480, 480), 0); ImageDraw.Draw(m).rounded_rectangle([0, 0, 479, 479], 100, fill=255)
ic.paste(plate(480, 480), (16, 16), m); box(ic, 136, 60, 240, 392, 1.3)
ic.save('icon.ico', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
