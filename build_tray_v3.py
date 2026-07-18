#!/usr/bin/env python3
"""ORBit bottom tray v3 (X2D era): sleeve bead groove, widened USB mouth,
amp bay extension for Adafruit 3006 (16.5x19.05), rect speaker deck for
PUI AS01508 (15x11x3.5). Everything else preserved."""
import numpy as np
import trimesh
from shapely.geometry import box

d = '/Users/hermes/untitled folder/v6/Working_Dir/'
t = trimesh.load(d + 'ORBit_bottom_tray_captouch_audio.3mf')
tray = t.geometry[list(t.geometry.keys())[0]] if hasattr(t, 'geometry') else t
print('tray in:', tray.is_watertight, np.round(tray.bounds, 2).tolist())

def prism(poly, z0, z1):
    m = trimesh.creation.extrude_polygon(poly, height=z1-z0)
    m.apply_translation([0, 0, z0])
    return m

def bx(x0, x1, y0, y1, z0, z1):
    return prism(box(x0, y0, x1, y1), z0, z1)

# outer silhouette from a clean z-slice — use world-coordinate loops directly
from shapely.geometry import Polygon
sec = tray.section(plane_origin=[0, 0, 5.0], plane_normal=[0, 0, 1])
loops = [np.array(pl) for pl in sec.discrete]
outer = max(loops, key=lambda p: (p[:, 0].max()-p[:, 0].min()) * (p[:, 1].max()-p[:, 1].min()))
sil = Polygon(outer[:, :2]).buffer(0)
print('silhouette bounds:', np.round(sil.bounds, 2))
assert abs(sil.bounds[0] + 67.6) < 0.1 and abs(sil.bounds[3] - 17.35) < 0.1, 'bad silhouette'

subs, adds = [], []
# 1. sleeve bead groove: 0.35 deep ring, z1.2..2.4
subs.append(prism(sil.buffer(0.6, join_style=1).difference(sil.buffer(-0.35, join_style=1)), 1.2, 2.4))
# 2. widen USB mouth to +/-6.0 (was +/-4.7), keep floor z9.9
subs.append(bx(-69, -65.2, -6.0, 6.0, 9.9, 16.5))
# 3. open amp bay eastward (remove east fence above CAP-fence height)
subs.append(bx(34.9, 40.0, -9.7, 9.7, 5.0, 12.0))
# 4. clear old round speaker corral / audio fences east of amp bay
subs.append(bx(40.0, 61.5, -15.5, 8.0, 5.2, 14.5))

# 5. amp bay extension rails + end wall (seat plane z5.8 preserved)
adds.append(bx(34.0, 39.3,  8.8,  9.6, 5.2, 5.8))
adds.append(bx(34.0, 39.3, -9.6, -8.8, 5.2, 5.8))
adds.append(bx(39.3, 39.9, -9.6,  9.6, 5.2, 8.8))

# 6. speaker deck: interior 15.4 x 11.4, wall 1.2, seat lip at z5.6
cx, cy = 50.5, -6.5
ox0, ox1 = cx-15.4/2-1.2, cx+15.4/2+1.2   # 41.6 .. 59.4
oy0, oy1 = cy-11.4/2-1.2, cy+11.4/2+1.2   # -13.4 .. 0.4
wallring = box(ox0, oy0, ox1, oy1).difference(box(ox0+1.2, oy0+1.2, ox1-1.2, oy1-1.2))
adds.append(prism(wallring, 5.4, 10.0))
lip = box(ox0+1.2, oy0+1.2, ox1-1.2, oy1-1.2).difference(
      box(ox0+2.2, oy0+2.2, ox1-2.2, oy1-2.2))
adds.append(prism(lip, 5.4, 5.6))          # speaker rests z5.6, face up at ~z9.1
adds.append(bx(42.0, 44.4, -13.2, -10.8, 2.0, 5.6))   # south legs down to floor remnants
adds.append(bx(56.6, 59.0, -13.2, -10.8, 2.0, 5.6))

print('subtract', len(subs), '/ union', len(adds))
out = trimesh.boolean.difference([tray] + subs, engine='manifold')
out = trimesh.boolean.union([out] + adds, engine='manifold')
print('tray v3:', out.is_watertight, np.round(out.bounds, 2).tolist(), 'vol', round(out.volume/1000, 2), 'cm3')
out.export(d + 'ORBit_bottom_tray_v3_x2d.3mf')
out.export(d + 'ORBit_bottom_tray_v3_x2d.stl')
print('exported tray v3')
