"""Find which debug-frame passes contain the enemy bar (compare the bar region after resizing the
pass to the window size against the final shot). Usage: python tools/bar_find.py <shot.png>"""
import glob, re, sys
import numpy as np
from PIL import Image

shot = sys.argv[1]
ref = np.asarray(Image.open(shot).convert('RGB').resize((1280, 800)), dtype=np.float32)
reg = ref[25:125, 380:910]
files = sorted(glob.glob(r'C:\Users\ivan\Desktop\XCDE-Compilar-V12\build\pass_3400_*.png'),
               key=lambda f: int(re.search(r'_(\d+)\.png$', f).group(1)))
for f in files:
    n = int(re.search(r'_(\d+)\.png$', f).group(1))
    im = Image.open(f)
    if im.size[0] < 160: continue
    x = np.asarray(im.convert('RGB').resize((1280, 800)), dtype=np.float32)[25:125, 380:910]
    d = float(np.abs(x - reg).mean())
    if d < 10:
        print('pass', n, 'size', im.size, 'diff', round(d, 2))
