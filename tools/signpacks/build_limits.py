#!/usr/bin/env python3
"""Builds a Driver speed limit file (.drsl) from an OpenStreetMap extract.

usage: build_limits.py <extract.osm.pbf|.osm> <out.drsl> [--date YYYYMMDD]

Keeps the maxspeed of roads cars can drive on (numeric values and the common
zone values such as "DE:urban"), as simplified polylines grouped into 0.1
degree tiles. The layout is documented in src/SpeedLimitStore.h.
Data (c) OpenStreetMap contributors, ODbL.
"""
import argparse
import datetime
import math
import os
import struct
import sys

import osmium

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_pack import drivable_ways, meters, oneway_of, parse_maxspeed

SIMPLIFY_METERS = 12.0
# Parking aisles, driveways and walking-speed streets add size, not value.
SKIP_CLASSES = ("service", "living_street", "road")
# Limits implied by "<country>:<zone>" values.
ZONES = {"urban": {"*": 50}, "zone30": {"*": 30}, "zone:30": {"*": 30},
         "rural": {"DE": 100, "AT": 100, "CH": 80, "FR": 80, "ES": 90, "IT": 90, "NL": 80, "BE": 70,
                   "PL": 90, "CZ": 90, "SK": 90, "HU": 90, "PT": 90, "SE": 70, "NO": 80, "DK": 80}}


def limit_of(value):
    """Limit in km/h for a maxspeed value, 0 when unknown or unlimited."""
    if not value:
        return 0
    kmh = parse_maxspeed(value)
    if kmh:
        return kmh
    if ":" in value:
        country, zone = value.split(":", 1)
        table = ZONES.get(zone.strip().lower())
        if table:
            return table.get(country.strip().upper(), table.get("*", 0))
    return 0


def pieces_for(tags, highway):
    """[(limit, flags)] for a way: flags 1 = forward travel only, 2 = backward only."""
    plain = limit_of(tags.get("maxspeed"))
    fwd = limit_of(tags.get("maxspeed:forward"))
    bwd = limit_of(tags.get("maxspeed:backward"))
    out = []
    if fwd or bwd:
        if fwd:
            out.append((fwd, 1))
        elif plain:
            out.append((plain, 1))
        if bwd:
            out.append((bwd, 2))
        elif plain:
            out.append((plain, 2))
        return out
    if plain:
        ow = oneway_of(tags, highway)
        out.append((plain, 1 if ow == 1 else 2 if ow == -1 else 0))
    return out


def simplify(points, tolerance):
    """Douglas-Peucker on (lat, lng) points, tolerance in metres."""
    if len(points) < 3:
        return points
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    lat0 = points[0][0]
    k = math.cos(math.radians(lat0))

    def xy(p):
        return ((p[1] - points[0][1]) * k * 111320.0, (p[0] - points[0][0]) * 111320.0)

    pts = [xy(p) for p in points]
    while stack:
        a, b = stack.pop()
        ax, ay = pts[a]
        bx, by = pts[b]
        dx, dy = bx - ax, by - ay
        seg = dx * dx + dy * dy
        worst, idx = 0.0, -1
        for i in range(a + 1, b):
            px, py = pts[i]
            if seg == 0:
                d = math.hypot(px - ax, py - ay)
            else:
                t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / seg))
                d = math.hypot(px - (ax + t * dx), py - (ay + t * dy))
            if d > worst:
                worst, idx = d, i
        if worst > tolerance and idx > 0:
            keep[idx] = True
            stack.append((a, idx))
            stack.append((idx, b))
    return [p for p, kp in zip(points, keep) if kp]


def tile_of(p):
    return (math.floor(p[0] * 10), math.floor(p[1] * 10))


def split_by_tile(points):
    """Pieces that stay inside one tile; a piece ends with the first point of the next tile."""
    pieces, cur = [], [points[0]]
    tile = tile_of(points[0])
    for q in points[1:]:
        if tile_of(q) == tile:
            cur.append(q)
        else:
            cur.append(q)
            pieces.append(cur)
            cur, tile = [q], tile_of(q)
    if len(cur) >= 2:
        pieces.append(cur)
    return [p for p in pieces if len(p) >= 2]


def varint(v):
    out = bytearray()
    while v >= 0x80:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    out.append(v)
    return bytes(out)


def zigzag(v):
    return (v << 1) ^ (v >> 31)


def e7(v):
    return int(round(v * 1e7))


def encode(ways, date):
    """ways: [(limit, flags, [(lat, lng), ...])] already split by tile."""
    tiles = {}
    lats, lngs = [], []
    for limit, flags, pts in ways:
        key = tile_of(pts[0])
        b = tiles.setdefault(key, bytearray())
        b += bytes([limit, flags]) + varint(len(pts))
        la, lo = e7(pts[0][0]), e7(pts[0][1])
        b += struct.pack("<ii", la, lo)
        for p in pts[1:]:
            nla, nlo = e7(p[0]), e7(p[1])
            b += varint(zigzag(nla - la)) + varint(zigzag(nlo - lo))
            la, lo = nla, nlo
        lats += [e7(p[0]) for p in pts]
        lngs += [e7(p[1]) for p in pts]
    keys = sorted(tiles)
    out = bytearray(b"DRSL")
    out += struct.pack("<7i", 1, len(keys), date, min(lats or [0]), min(lngs or [0]), max(lats or [0]), max(lngs or [0]))
    offset = 32 + 16 * len(keys)
    for k in keys:
        out += struct.pack("<iiII", k[0], k[1], offset, len(tiles[k]))
        offset += len(tiles[k])
    for k in keys:
        out += tiles[k]
    return bytes(out)


def decode(data):
    magic, version, ntiles, date = struct.unpack_from("<4s3i", data, 0)
    assert magic == b"DRSL" and version == 1
    ways = []
    for i in range(ntiles):
        _tl, _tg, off, ln = struct.unpack_from("<iiII", data, 32 + 16 * i)
        p, end = off, off + ln
        while p < end:
            limit, flags = data[p], data[p + 1]
            p += 2
            n, shift = 0, 0
            while True:
                b = data[p]
                p += 1
                n |= (b & 0x7F) << shift
                shift += 7
                if not b & 0x80:
                    break
            la, lo = struct.unpack_from("<ii", data, p)
            p += 8
            pts = [(la / 1e7, lo / 1e7)]
            for _ in range(n - 1):
                vals = []
                for _k in range(2):
                    v, shift = 0, 0
                    while True:
                        b = data[p]
                        p += 1
                        v |= (b & 0x7F) << shift
                        shift += 7
                        if not b & 0x80:
                            break
                    vals.append((v >> 1) ^ -(v & 1))
                la += vals[0]
                lo += vals[1]
                pts.append((la / 1e7, lo / 1e7))
            ways.append((limit, flags, pts))
    return {"date": date, "ways": ways}


def build(path, date):
    collected = []   # (limit, flags, refs)
    needed = set()
    for _wid, hw, tags, refs in drivable_ways(path):
        if hw in SKIP_CLASSES:
            continue
        for limit, flags in pieces_for(tags, hw):
            collected.append((limit, flags, refs))
        if len(refs) >= 2 and ("maxspeed" in tags or "maxspeed:forward" in tags or "maxspeed:backward" in tags):
            needed.update(refs)
    print("ways with a limit: %d" % len(collected), file=sys.stderr)
    coords = {}
    fp = osmium.FileProcessor(path, osmium.osm.NODE).with_filter(osmium.filter.IdFilter(needed))
    for n in fp:
        if n.location.valid():
            coords[n.id] = (n.location.lat, n.location.lon)
    out = []
    for limit, flags, refs in collected:
        pts = [coords[r] for r in refs if r in coords]
        if len(pts) < 2:
            continue
        for piece in split_by_tile(simplify(pts, SIMPLIFY_METERS)):
            out.append((limit, flags, piece))
    print("pieces: %d" % len(out), file=sys.stderr)
    return encode(out, date)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("extract")
    ap.add_argument("out")
    ap.add_argument("--date", type=int, default=int(datetime.date.today().strftime("%Y%m%d")))
    args = ap.parse_args()
    data = build(args.extract, args.date)
    with open(args.out, "wb") as f:
        f.write(data)
    print("wrote %s (%d bytes)" % (args.out, len(data)), file=sys.stderr)


if __name__ == "__main__":
    main()
