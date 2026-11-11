"""Verify the frost box-crop fix: the frosted backdrop behind a HUD panel should now show the scene
from the SAME screen region (the centred 16:9 box), not a vertically shifted/zoomed copy.

Usage: python tools/frost_check.py <fixed_shot.png> <scene_pass.png> <old_shot.png>
  fixed_shot: the final frame after the fix (window-sized, e.g. 1280x954 at 4:3)
  scene_pass: a debug-frame pass with the scene, no UI (e.g. pass 260)
  old_shot:   the final frame before the fix (same window size), optional
"""
import sys
import numpy as np
from PIL import Image

fixed = np.asarray(Image.open(sys.argv[1]).convert('RGB'), dtype=np.float32)
scene = np.asarray(Image.open(sys.argv[2]).convert('RGB'), dtype=np.float32)
H, W = fixed.shape[:2]

# guest plate region (enemy status bar top): x 368..912, y 17..97 in the 1280x720 guest frame.
# At 4:3 the box offset is (W - 1280*u)/2, (H - 720*u)/2 with u = min(W/1280, H/720).
u = min(W / 1280.0, H / 720.0)
bx = int((W - 1280 * u) / 2)
by = int((H - 720 * u) / 2)
x0, x1 = bx + 368, bx + 912
y0, y1 = by + 17, by + 97

plate = fixed[y0:y1, x0:x1]
behind = scene[y0:y1, x0:x1]  # the scene at the plate's own position (correct content)

# the frost is blurred + tinted, so compare low-frequency structure: downsample both to 64x10
def low(im):
    return np.asarray(Image.fromarray(im.astype(np.uint8)).resize((64, 10), Image.BOX), dtype=np.float32)

d_same = float(np.abs(low(plate) - low(behind)).mean())
print('plate vs scene-at-same-position (low freq): %.1f' % d_same)

# what the old (stretched) sampling would show: the scene at the plate's UNMAPPED guest position
# (guest y 17..97 of the whole window = rows 17*H/720..97*H/720)
oy0, oy1 = int(17 * H / 720.0), int(97 * H / 720.0)
old_pos = scene[oy0:oy1, x0:x1]
d_old = float(np.abs(low(plate) - low(old_pos)).mean())
print('plate vs scene-at-unmapped-position:        %.1f' % d_old)

if len(sys.argv) > 3:
    old = np.asarray(Image.open(sys.argv[3]).convert('RGB'), dtype=np.float32)
    old_plate = old[y0:y1, x0:x1]
    print('A/B: old plate vs scene-same: %.1f | new plate vs scene-same: %.1f' % (
        float(np.abs(low(old_plate) - low(behind)).mean()), d_same))
