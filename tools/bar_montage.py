"""Montage of the enemy-bar region across a debug frame's passes, to locate which pass draws
the frosted panel above the bar (GitHub #21). Usage: python tools/bar_montage.py <frame> <out.png>"""
import glob, os, sys, re
from PIL import Image, ImageDraw

frame = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else rf'C:\Users\ivan\Desktop\XCDE-Compilar-V12\build\bar_montage_{frame}.png'
lo = int(sys.argv[3]) if len(sys.argv) > 3 else 0
hi = int(sys.argv[4]) if len(sys.argv) > 4 else 1 << 30
files = sorted(glob.glob(rf'C:\Users\ivan\Desktop\XCDE-Compilar-V12\build\pass_{frame}_*.png'),
               key=lambda f: int(re.search(r'_(\d+)\.png$', f).group(1)))
files = [f for f in files if lo <= int(re.search(r'_(\d+)\.png$', f).group(1)) <= hi]
crop = (260, 0, 1060, 140)   # top-center strip: the enemy bar + its frosted panel at 16:10
cell_w, cell_h = 400, 70
cols = 4
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (cols * (cell_w + 8) + 8, rows * (cell_h + 18) + 8), (25, 25, 25))
d = ImageDraw.Draw(sheet)
for k, f in enumerate(files):
    n = int(re.search(r'_(\d+)\.png$', f).group(1))
    im = Image.open(f).convert('RGB')
    c = im.crop(crop).resize((cell_w, cell_h))
    x = 8 + (k % cols) * (cell_w + 8)
    y = 8 + (k // cols) * (cell_h + 18)
    sheet.paste(c, (x, y))
    d.text((x + 2, y + cell_h + 2), f'pass {n} {im.size[0]}x{im.size[1]}', fill=(255, 255, 0))
sheet.save(out)
print('saved', out, len(files), 'passes')
