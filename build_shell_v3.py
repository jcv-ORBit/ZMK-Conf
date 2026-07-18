#!/usr/bin/env python3
"""ORBit top shell v3 (X2D era): LED pockets + light funnels for SK6812-E
reverse-mount NeoPixels under each glow slot, wire channels, blackout baffles.
Shell FILE coords: z0 = assembly top face, +z = down; assembly y = -file y.
Zone slots (file): y=+13.9, x centers -38,-27,25,44,52,60 (6.0 x 0.8).
Slider slot (file): y=-13.9, x 41..59."""
import numpy as np
import trimesh
from shapely.geometry import box

d = '/Users/hermes/untitled folder/v6/Working_Dir/'
s = trimesh.load(d + 'ORBit_top_shell_captouch_glow.3mf')
names = list(s.geometry.keys())
plate, overlay = s.geometry[names[0]], s.geometry[names[1]]
rest = [s.geometry[n] for n in names[2:]]
print('plate+overlay+', len(rest), 'other solids')

def prism(poly, z0, z1):
    m = trimesh.creation.extrude_polygon(poly, height=z1-z0)
    m.apply_translation([0, 0, z0])
    return m

def bx(x0, x1, y0, y1, z0, z1):
    return prism(box(x0, y0, x1, y1), z0, z1)

def funnel(xc, yc, z_top_rect, z_bot_rect):
    """convex hull light funnel: LED seat footprint tapering to slot mouth"""
    (w1, h1, z1), (w2, h2, z2) = z_top_rect, z_bot_rect
    pts = []
    for w, h, z in [(w1, h1, z1), (w2, h2, z2)]:
        for sx in (-1, 1):
            for sy in (-1, 1):
                pts.append([xc + sx*w/2, yc + sy*h/2, z])
    return trimesh.convex.convex_hull(np.array(pts))

# plate has non-manifold duplicate topology; run all booleans natively in manifold3d
import manifold3d as m3d

def to_man(mesh):
    mm = mesh.copy(); mm.merge_vertices(digits_vertex=4)
    return m3d.Manifold(m3d.Mesh(vert_properties=np.asarray(mm.vertices, dtype=np.float32),
                                 tri_verts=np.asarray(mm.faces, dtype=np.uint32)))

def to_tri(man):
    mm = man.to_mesh()
    g = trimesh.Trimesh(vertices=np.asarray(mm.vert_properties, dtype=np.float64),
                        faces=np.asarray(mm.tri_verts, dtype=np.int64))
    g.merge_vertices(); g.process()
    return g

main_m = to_man(plate) + to_man(overlay)
print('main union status:', main_m.status())

subs, adds = [], []
led_sites = [(x, 13.9) for x in (-38, -27, 25, 44, 52, 60)] + \
            [(45.5, -13.9), (54.5, -13.9)]
for xc, yc in led_sites:
    subs.append(bx(xc-1.95, xc+1.95, yc-1.75, yc+1.75, 1.0, 3.0))      # LED seat pocket
    subs.append(funnel(xc, yc, (3.9, 3.5, 1.05), (6.2, 1.2, 0.35)))    # light funnel to slot

# baffle ribs (blackout between zones), z2.8..4.4 below underside
for xr in (-32.5, 33.8, 48.0, 56.0):
    adds.append(bx(xr-0.4, xr+0.4, 12.1, 15.2, 2.1, 4.0))
adds.append(bx(49.6, 50.4, -15.2, -12.1, 2.1, 4.0))                    # between slider LEDs

for a in adds:
    main_m = main_m + to_man(a)

# wire channels (cut AFTER ribs so the chain tunnels through them)
subs.append(bx(-52.0, 33.5, 13.3, 14.5, 1.4, 2.4))    # zone run, west of columns
subs.append(bx(40.5, 63.5, 13.3, 14.5, 1.4, 2.4))     # zone run, east of columns
subs.append(bx(40.0, 60.5, -14.5, -13.3, 1.4, 2.4))   # slider run
subs.append(bx(65.3, 66.5, -14.0, 14.0, 1.4, 2.4))    # east crossover (over wall seat)

for sb in subs:
    main_m = main_m - to_man(sb)
print('final status:', main_m.status(), 'vol', round(main_m.volume()/1000, 2), 'cm3')
main = to_tri(main_m)
print('main v3 watertight:', main.is_watertight)
print('bounds:', np.round(main.bounds, 2).tolist())

scene = trimesh.Scene()
scene.add_geometry(main, geom_name='shell_main_v3')
for n in names[2:]:
    scene.add_geometry(s.geometry[n], geom_name=f'part_{n}')
scene.export(d + 'ORBit_top_shell_v3_x2d.3mf')
main.export(d + 'ORBit_top_shell_v3_main.stl')
print('exported shell v3 (3mf scene,', 1+len(rest), 'solids) + main stl')
