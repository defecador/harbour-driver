#!/usr/bin/env python3
"""Builds a Driver sign pack (.drsp) from an OpenStreetMap extract.

usage: build_pack.py <extract.osm.pbf|.osm> <out.drsp> [--date YYYYMMDD]

Keeps stop signs, give-way signs and speed cameras that sit on roads cars can
drive, and works out which direction of travel each one applies to. The file
layout is documented in src/SignPacks.h (parsePack).

The extract is read in several passes that only keep small id sets, so even
a country-sized file needs far less memory than a node location index would.
Data (c) OpenStreetMap contributors, ODbL.
"""
import argparse
import datetime
import math
import struct
import sys
from collections import defaultdict

import osmium

KINDS = {"stop": 1, "give_way": 2, "speed_camera": 3}
# Road class ranks: a higher number has priority over a lower one.
RANKS = {"motorway": 7, "trunk": 6, "primary": 5, "secondary": 4, "tertiary": 3,
         "unclassified": 2, "residential": 2, "living_street": 2, "service": 1, "road": 1}
WINDOW = 12          # nodes either side of a sign that are examined
JUNCTION_REACH = 60  # metres: how far to look for the junction an untagged sign guards
CROSS_MIN, CROSS_MAX = 35.0, 145.0   # angle range that counts as a crossing road
COMPASS = {"N": 0, "NNE": 22.5, "NE": 45, "ENE": 67.5, "E": 90, "ESE": 112.5, "SE": 135,
           "SSE": 157.5, "S": 180, "SSW": 202.5, "SW": 225, "WSW": 247.5, "W": 270,
           "WNW": 292.5, "NW": 315, "NNW": 337.5}


def rank_of(highway):
    if not highway:
        return 0
    if highway.endswith("_link"):
        highway = highway[:-5]
    return RANKS.get(highway, 0)


def parse_maxspeed(value):
    if not value:
        return 0
    v = value.strip().lower()
    mph = "mph" in v
    digits = "".join(ch for ch in v.split()[0] if ch.isdigit()) if v.split() else ""
    if not digits:
        return 0
    kmh = int(digits) * (1.609 if mph else 1)
    return max(0, min(250, int(round(kmh))))


def direction_of(tags, key):
    """'forward' | 'backward' | 'both' | float degrees | None."""
    for k in ("direction", key + ":direction", "traffic_sign:direction"):
        v = tags.get(k)
        if not v:
            continue
        v = v.strip()
        if v in ("forward", "backward"):
            return v
        if v in ("both", "all"):
            return "both"
        if v.upper() in COMPASS:
            return float(COMPASS[v.upper()])
        try:
            return float(v) % 360
        except ValueError:
            continue
    return None


def oneway_of(tags, highway):
    v = tags.get("oneway")
    if v in ("yes", "true", "1"):
        return 1
    if v == "-1":
        return -1
    if v in ("no", "false", "0"):
        return 0
    if tags.get("junction") in ("roundabout", "circular"):
        return 1
    if highway and highway.split("_")[0] == "motorway":
        return 1
    return 0


def bearing(a, b):
    """Bearing in degrees from (lat, lng) a to b, flat-earth like NavGeo."""
    x = (b[1] - a[1]) * math.cos(math.radians(a[0]))
    y = b[0] - a[0]
    if x == 0 and y == 0:
        return 0.0
    return math.degrees(math.atan2(x, y)) % 360


def meters(a, b):
    x = (b[1] - a[1]) * math.cos(math.radians(a[0])) * 111320.0
    y = (b[0] - a[0]) * 111320.0
    return math.hypot(x, y)


def angle_diff(a, b):
    d = abs(a - b) % 360
    return 360 - d if d > 180 else d


class Way:
    __slots__ = ("id", "rank", "refs", "oneway", "name", "ref", "maxspeed")

    def __init__(self, wid, rank, refs, oneway, name, ref, maxspeed):
        self.id, self.rank, self.refs = wid, rank, refs
        self.oneway, self.name, self.ref, self.maxspeed = oneway, name, ref, maxspeed


def drivable_ways(path):
    """Yields (id, highway, tags, node refs) for the ways cars can use."""
    fp = osmium.FileProcessor(path, osmium.osm.WAY).with_filter(osmium.filter.KeyFilter("highway"))
    for w in fp:
        hw = w.tags.get("highway")
        if not rank_of(hw):
            continue
        access = w.tags.get("motor_vehicle") or w.tags.get("access")
        if access in ("no", "private") and w.tags.get("motor_vehicle") != "yes":
            continue
        yield w.id, hw, w.tags, [n.ref for n in w.nodes]


def build(path, date):
    # Pass 1: the signs themselves.
    signs = {}
    fp = osmium.FileProcessor(path, osmium.osm.NODE).with_filter(osmium.filter.KeyFilter("highway"))
    for n in fp:
        hw = n.tags.get("highway")
        if hw in KINDS and n.location.valid():
            signs[n.id] = {
                "pos": (n.location.lat, n.location.lon), "kind": KINDS[hw], "key": hw,
                "dir": direction_of(n.tags, hw), "all": n.tags.get("stop") == "all",
                "maxspeed": parse_maxspeed(n.tags.get("maxspeed")),
            }
    print("signs: %d" % len(signs), file=sys.stderr)

    # Pass 2: drivable ways carrying a sign.
    parents = {}
    sign_ways = defaultdict(list)
    near_nodes = set()
    for wid, hw, tags, refs in drivable_ways(path):
        idx = [i for i, r in enumerate(refs) if r in signs]
        if not idx:
            continue
        parents[wid] = Way(wid, rank_of(hw), refs, oneway_of(tags, hw),
                           tags.get("name", ""), tags.get("ref", ""), parse_maxspeed(tags.get("maxspeed")))
        for i in idx:
            sign_ways[refs[i]].append(wid)
            near_nodes.update(refs[max(0, i - WINDOW):i + WINDOW + 1])
    print("parent ways: %d" % len(parents), file=sys.stderr)

    # Pass 3: every drivable way touching those nodes (to find junctions).
    incident = defaultdict(list)   # node -> [(way id, rank, oneway, prev, next)]
    wanted = set(near_nodes)
    for wid, hw, tags, refs in drivable_ways(path):
        rank = rank_of(hw)
        ow = oneway_of(tags, hw)
        for i, r in enumerate(refs):
            if r in near_nodes:
                prev = refs[i - 1] if i > 0 else None
                nxt = refs[i + 1] if i + 1 < len(refs) else None
                incident[r].append((wid, rank, ow, prev, nxt))
                wanted.update(x for x in (prev, nxt) if x is not None)

    # Pass 4: coordinates of everything needed.
    coords = {}
    fp = osmium.FileProcessor(path, osmium.osm.NODE).with_filter(osmium.filter.IdFilter(wanted))
    for n in fp:
        if n.location.valid():
            coords[n.id] = (n.location.lat, n.location.lon)
    print("coordinates: %d" % len(coords), file=sys.stderr)

    def is_junction(node, wid):
        """True when another way leaves `node` in a direction this way doesn't go."""
        here = coords.get(node)
        if here is None:
            return False
        own, other = [], []
        for w2, _rank, _ow, prev, nxt in incident.get(node, ()):
            for nb in (prev, nxt):
                if nb is not None and nb in coords:
                    (own if w2 == wid else other).append(bearing(here, coords[nb]))
        if not own:
            return False
        return any(all(angle_diff(o, b) > CROSS_MIN for b in own) for o in other)

    def approaches(node):
        """Directions of travel arriving at `node`, with the road class of each."""
        here = coords.get(node)
        out = []
        if here is None:
            return out
        for _wid, rank, ow, prev, nxt in incident.get(node, ()):
            if prev is not None and prev in coords and ow != -1:
                out.append((bearing(coords[prev], here), rank))
            if nxt is not None and nxt in coords and ow != 1:
                out.append((bearing(coords[nxt], here), rank))
        return out

    def arrival(way, i, forward):
        """Direction of travel on `way` arriving at node index i."""
        refs = way.refs
        pos = coords.get(refs[i])
        if pos is None:
            return None
        if forward:
            src = coords.get(refs[i - 1]) if i > 0 else None
            if src is None:
                nxt = coords.get(refs[i + 1]) if i + 1 < len(refs) else None
                return bearing(pos, nxt) if nxt else None
            return bearing(src, pos)
        src = coords.get(refs[i + 1]) if i + 1 < len(refs) else None
        if src is None:
            prv = coords.get(refs[i - 1]) if i > 0 else None
            return (bearing(prv, pos) + 180) % 360 if prv else None
        return bearing(src, pos)

    def reach_junction(way, i, step):
        """Metres along the way to the nearest junction in one direction, or None."""
        dist, prev = 0.0, coords.get(way.refs[i])
        j = i + step
        while 0 <= j < len(way.refs) and abs(j - i) <= WINDOW:
            cur = coords.get(way.refs[j])
            if cur is None or prev is None:
                return None
            dist += meters(prev, cur)
            if dist > JUNCTION_REACH:
                return None
            if is_junction(way.refs[j], way.id):
                return dist
            prev = cur
            j += step
        return None

    records = []   # (lat, lng, kind, bearing or None, maxspeed)

    def emit(sign, brg, maxspeed=0):
        records.append((sign["pos"][0], sign["pos"][1], sign["kind"], brg, maxspeed))

    for sid, s in signs.items():
        ways = [parents[w] for w in sign_ways.get(sid, ()) if w in parents]
        camera = s["kind"] == 3
        speed = s["maxspeed"] if camera else 0
        if camera and not speed and ways:
            speed = ways[0].maxspeed

        if camera:
            if len(ways) == 1 and s["dir"] in ("forward", "backward"):
                w = ways[0]
                i = w.refs.index(sid)
                b = arrival(w, i, s["dir"] == "forward")
                emit(s, b, speed)
            else:
                emit(s, None, speed)
            continue

        if not ways:
            continue
        if s["all"] or s["dir"] == "both":
            emit(s, None)
            continue

        node_junction = len(ways) >= 2 or any(
            is_junction(sid, w.id) for w in ways)
        if node_junction and not (len(ways) == 1 and s["dir"] in ("forward", "backward")):
            # A sign on a junction node guards the approaches that have to give way.
            arr = approaches(sid)
            seen = set()
            for brg, rank in arr:
                cross = [r for b2, r in arr if CROSS_MIN <= angle_diff(brg, b2) <= CROSS_MAX]
                if cross and max(cross) >= rank:
                    key = round(brg / 2)
                    if key not in seen:
                        seen.add(key)
                        emit(s, brg)
            continue

        w = ways[0]
        i = w.refs.index(sid)
        if s["dir"] in ("forward", "backward"):
            b = arrival(w, i, s["dir"] == "forward")
            if b is not None:
                emit(s, b)
        elif isinstance(s["dir"], float):
            emit(s, (s["dir"] + 180) % 360)   # numeric: the way the sign faces
        else:
            ahead = reach_junction(w, i, 1) if w.oneway != -1 else None
            back = reach_junction(w, i, -1) if w.oneway != 1 else None
            if ahead is not None and (back is None or ahead < back - 5):
                b = arrival(w, i, True)
            elif back is not None and (ahead is None or back < ahead - 5):
                b = arrival(w, i, False)
            else:
                b = None
            if b is not None:
                emit(s, b)

    records.sort(key=lambda r: (r[0], r[1]))
    print("records: %d" % len(records), file=sys.stderr)
    return encode(records, date)


def encode(records, date):
    e7 = lambda v: int(round(v * 1e7))
    lats = [e7(r[0]) for r in records] or [0]
    lngs = [e7(r[1]) for r in records] or [0]
    out = bytearray(b"DRSP")
    out += struct.pack("<7i", 1, len(records), date, min(lats), min(lngs), max(lats), max(lngs))
    for r in records:
        brg = 255 if r[3] is None else int(round(r[3] % 360 / 2.0)) % 180
        out += struct.pack("<iiBBBB", e7(r[0]), e7(r[1]), r[2], brg, min(255, r[4]), 0)
    return bytes(out)


def decode(data):
    magic, version, count, date, mnla, mnlo, mxla, mxlo = struct.unpack_from("<4s7i", data, 0)
    assert magic == b"DRSP" and version == 1 and len(data) == 32 + 12 * count
    recs = []
    for k in range(count):
        la, lo, kind, brg, speed, _flags = struct.unpack_from("<iiBBBB", data, 32 + 12 * k)
        recs.append({"lat": la / 1e7, "lng": lo / 1e7, "kind": kind,
                     "bearing": None if brg == 255 else brg * 2, "maxspeed": speed})
    return {"count": count, "date": date, "records": recs}


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
