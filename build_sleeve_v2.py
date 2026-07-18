#!/usr/bin/env python3
"""ORBit TPU top sleeve v2 'Orbital Trace' — built from approved concept board.
Device/tray coords, +z up. Print roof-down (flip in slicer)."""
import numpy as np
import trimesh
from shapely.geometry import LineString, Point, Polygon
from shapely import affinity

OUT = '/Users/hermes/untitled folder/v6/Working_Dir/'

def rrect(cx, cy, w, h, r):
    from shapely.geometry import box
    return box(cx-w/2, cy-h/2, cx+w/2, cy+h/2).buffer(-0).buffer(0).intersection(
        box(cx-w/2, cy-h/2, cx+w/2, cy+h/2)).buffer(0) if r <= 0 else \
        box(cx-w/2+r, cy-h/2+r, cx+w/2-r, cy+h/2-r).buffer(r, quad_segs=24)

def prism(poly, z0, z1):
    m = trimesh.creation.extrude_polygon(poly, height=z1-z0)
    m.apply_translation([0, 0, z0])
    return m

def stadium(x0, y0, x1, y1, w, z0, z1):
    return prism(LineString([(x0, y0), (x1, y1)]).buffer(w/2, quad_segs=16), z0, z1)

solids_add, solids_sub = [], []

# ---- body ----
outer_poly  = rrect(0, 0, 138.0, 37.5, 5.5)
cavity_poly = rrect(0, 0, 135.6, 35.1, 4.7)
body   = prism(outer_poly, 1.0, 20.4)
cavity = prism(cavity_poly, -1.0, 19.2)

# grip bead: 0.3 inward ring, z1.2..2.4 (0.2 lead-in strip below stays clean)
bead = prism(cavity_poly.difference(cavity_poly.buffer(-0.3)), 1.2, 2.4)

# ---- aperture + chamfer ----
ap = trimesh.creation.cylinder(radius=14.5, height=3.0, sections=128)
ap.apply_translation([9, 0, 19.5])
solids_sub.append(ap)
cone = trimesh.creation.cone(radius=16.5, height=16.5, sections=128)
cone.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0]))
cone.apply_translation([9, 0, 21.2])   # apex at (9,0,4.7), 45 deg flank
solids_sub.append(cone)

# ---- underside pockets (cavity ceiling z19.2) ----
# key pad thinning to 1.0
for x0, x1 in [(-48, -28), (-26, -8), (25.7, 37), (38.6, 50), (51.5, 65)]:
    solids_sub.append(prism(rrect((x0+x1)/2, 0, x1-x0, 23.0 if x1 < 51 else 26.0, 1.5), 19.2, 19.4))
# glow lens dots, 0.6 remaining, zone edge y=-13.9
for xc in [-38, -27, 25, 44, 52, 60]:
    solids_sub.append(stadium(xc-1.35, -13.9, xc+1.35, -13.9, 1.3, 19.2, 19.8))
# slider lens channel, 0.5 remaining, y=+13.9
solids_sub.append(stadium(41.6, 13.9, 58.4, 13.9, 1.2, 19.2, 19.9))
# slider position ticks (ghost marks nearer the edge)
for xc in [41, 45.5, 50, 54.5, 59]:
    solids_sub.append(stadium(xc, 15.1, xc, 15.7, 0.5, 19.2, 19.6))

# speed-line grooves 0.5 wide, cut to z19.75
west = [(-52, -9.5, 0), (-57, -10.5, 3.5), (-57, -10.5, -3.5),
        (-49, -13, 7), (-49, -13, -7), (-40, -17, 10.5), (-40, -17, -10.5)]
east = [(27.5, 65, 0), (28.5, 60, 3.5), (28.5, 60, -3.5),
        (31, 52, 7), (31, 52, -7), (34, 44, 10.5), (34, 44, -10.5)]
for x0, x1, yy in west + east:
    solids_sub.append(stadium(x0, yy, x1, yy, 0.5, 19.2, 19.75))
# key-boundary micro-ticks
for xc in [-28, -26, 37, 38.6, 50, 51.5]:
    solids_sub.append(stadium(xc, -1.4, xc, 1.4, 0.3, 19.2, 19.75))

# orbit ellipse ring rx22 ry7.4 rot -16deg about (9,0), 0.4 wide, 0.35 deep
ell_o = affinity.scale(Point(0, 0).buffer(1, quad_segs=64), 22.2, 7.6)
ell_i = affinity.scale(Point(0, 0).buffer(1, quad_segs=64), 21.8, 7.2)
ring = affinity.translate(affinity.rotate(ell_o.difference(ell_i), -16, origin=(0, 0)), 9, 0)
solids_sub.append(prism(ring, 19.2, 19.55))

# emblem at (-59.5, 0): circle ring r2 + 6 flanking ticks
em_ring = Point(-59.5, 0).buffer(2.18, quad_segs=32).difference(Point(-59.5, 0).buffer(1.83, quad_segs=32))
solids_sub.append(prism(em_ring, 19.2, 19.55))
for x0, x1, yy in [(-64.1, -62.3, 0), (-56.7, -54.9, 0),
                   (-63.3, -62.4, 1.6), (-63.3, -62.4, -1.6),
                   (-56.6, -55.7, 1.6), (-56.6, -55.7, -1.6)]:
    solids_sub.append(stadium(x0, yy, x1, yy, 0.3, 19.2, 19.55))

# ---- wall features ----
# pinstripe rings 0.5 tall, 0.35 deep, full perimeter at z13.2 and z15.0
ring_poly = rrect(0, 0, 139.0, 38.5, 5.9).difference(rrect(0, 0, 137.3, 36.8, 5.15))
for zc in [13.2, 15.0]:
    solids_sub.append(prism(ring_poly, zc-0.25, zc+0.25))

# USB port window, west face: 10.4 x 6.9 r1.5 (yz-plane), z9.5..16.4 over wall mouth
win = trimesh.creation.extrude_polygon(rrect(0, 12.95, 13.2, 6.9, 1.8), height=3.0)
# map poly x -> device y, poly y -> device z, extrusion z -> device x (wall depth)
win.apply_transform(np.array([[0, 0, 1, -70.0],
                              [1, 0, 0, 0],
                              [0, 1, 0, 0],
                              [0, 0, 0, 1.0]]))  # spans x -70..-67, y +/-5.2, z 9.5..16.4
solids_sub.append(win)

# ---- booleans ----
print('boolean: body - cavity ...')
sleeve = trimesh.boolean.difference([body, cavity], engine='manifold')
print('union bead ...')
sleeve = trimesh.boolean.union([sleeve, bead], engine='manifold')
print(f'subtract {len(solids_sub)} feature solids ...')
sleeve = trimesh.boolean.difference([sleeve] + solids_sub, engine='manifold')

print('watertight:', sleeve.is_watertight)
print('bounds:', np.round(sleeve.bounds, 2).tolist())
print('volume cm3:', round(sleeve.volume/1000, 2))
print('faces:', len(sleeve.faces))
sleeve.export(OUT + 'ORBit_tpu_top_sleeve_v2.3mf')
sleeve.export(OUT + 'ORBit_tpu_top_sleeve_v2.stl')
print('exported ORBit_tpu_top_sleeve_v2.3mf / .stl')
