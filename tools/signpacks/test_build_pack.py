#!/usr/bin/env python3
"""Checks build_pack.py against a tiny hand-made map.  Run: python -I test_build_pack.py"""
import math
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_pack  # noqa: E402

BASE_LAT, BASE_LNG = 52.0, 13.0


def pt(x, y):
    """Local metres east/north of the base point to (lat, lng)."""
    return (BASE_LAT + y / 111320.0, BASE_LNG + x / (111320.0 * math.cos(math.radians(BASE_LAT))))


class Osm:
    def __init__(self):
        self.nodes, self.ways, self.next_id = [], [], 1000

    def node(self, x, y, **tags):
        self.next_id += 1
        self.nodes.append((self.next_id, pt(x, y), tags))
        return self.next_id

    def pos(self, nid):
        return next(p for i, p, _ in self.nodes if i == nid)

    def way(self, refs, **tags):
        self.next_id += 1
        self.ways.append((self.next_id, refs, tags))

    def xml(self):
        def attrs(tags):
            return "".join('<tag k="%s" v="%s"/>' % (k.replace("_colon_", ":"), v) for k, v in tags.items())
        out = ['<?xml version="1.0" encoding="UTF-8"?>', '<osm version="0.6" generator="test">']
        for nid, (la, lo), tags in self.nodes:
            out.append('<node id="%d" version="1" lat="%.7f" lon="%.7f">%s</node>' % (nid, la, lo, attrs(tags)))
        for wid, refs, tags in self.ways:
            nds = "".join('<nd ref="%d"/>' % r for r in refs)
            out.append('<way id="%d" version="1">%s%s</way>' % (wid, nds, attrs(tags)))
        out.append("</osm>")
        return "\n".join(out)


def build_map():
    m = Osm()
    expect = {}

    # A: residential, west to east; a forward-tagged stop and a backward-tagged give-way.
    a0 = m.node(0, 0)
    s1 = m.node(150, 0, highway="stop", direction="forward")
    a1 = m.node(250, 0)
    s2 = m.node(350, 0, highway="give_way", direction="backward")
    a2 = m.node(500, 0)
    m.way([a0, s1, a1, s2, a2], highway="residential")
    expect["forward"] = (m.pos(s1), 1, 90)
    expect["backward"] = (m.pos(s2), 2, 270)

    # B/D: an untagged sign 50 m before a T junction guards the approach towards it;
    # another one 300 m away from any junction is dropped.
    b0 = m.node(0, 200)
    s4 = m.node(100, 200, highway="stop")
    b1 = m.node(300, 200)
    s3 = m.node(350, 200, highway="give_way")
    j1 = m.node(400, 200)
    d0 = m.node(400, 100)
    d1 = m.node(400, 300)
    m.way([b0, s4, b1, s3, j1], highway="residential")
    m.way([d0, j1, d1], highway="residential")
    expect["untagged"] = (m.pos(s3), 2, 90)

    # F/G: a give-way node on the junction itself where a side street meets a primary road
    # (split into two ways there): only the side street's approach (heading north) warns.
    f0 = m.node(0, 400)
    jn = m.node(200, 400, highway="give_way")
    f2 = m.node(400, 400)
    g0 = m.node(200, 300)
    m.way([f0, jn], highway="primary")
    m.way([jn, f2], highway="primary")
    m.way([g0, jn], highway="residential")
    expect["junction"] = (m.pos(jn), 2, 0)

    # A stop on a footway is not a car sign.
    h0 = m.node(0, 600)
    s5 = m.node(150, 600, highway="stop")
    h1 = m.node(300, 600)
    m.way([h0, s5, h1], highway="footway")

    # Cameras: on a secondary road (any direction, 50), directional (70 from the way), off-road.
    j0 = m.node(0, 700)
    c1 = m.node(200, 700, highway="speed_camera", maxspeed="50")
    j2 = m.node(300, 700)
    c3 = m.node(350, 700, highway="speed_camera", direction="forward")
    j3 = m.node(400, 700)
    m.way([j0, c1, j2, c3, j3], highway="secondary", maxspeed="70")
    c4 = m.node(200, 800, highway="speed_camera", maxspeed="30 mph")
    expect["camera_any"] = (m.pos(c1), 3, None)
    expect["camera_dir"] = (m.pos(c3), 3, 90)
    expect["camera_off_road"] = (m.pos(c4), 3, None)

    # Numeric direction: the sign faces east, so it is for traffic heading west.
    k0 = m.node(0, 900)
    s6 = m.node(100, 900, highway="stop", direction="90")
    k1 = m.node(300, 900)
    m.way([k0, s6, k1], highway="residential")
    expect["numeric"] = (m.pos(s6), 1, 270)
    return m, expect


class BuildPackTest(unittest.TestCase):
    def setUp(self):
        self.m, self.expect = build_map()
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "t.osm")
            with open(path, "w") as f:
                f.write(self.m.xml())
            self.pack = build_pack.decode(build_pack.build(path, 20250101))

    def find(self, key):
        want = self.expect[key]
        (lat, lng), kind, bearing = want[0], want[1], want[2]
        hits = [r for r in self.pack["records"]
                if abs(r["lat"] - lat) < 1e-6 and abs(r["lng"] - lng) < 1e-6 and r["kind"] == kind]
        self.assertTrue(hits, "no record for %s" % key)
        return hits, bearing

    def check_bearing(self, key):
        hits, bearing = self.find(key)
        self.assertEqual(len(hits), 1, key)
        if bearing is None:
            self.assertIsNone(hits[0]["bearing"], key)
        else:
            self.assertIsNotNone(hits[0]["bearing"], key)
            self.assertLessEqual(build_pack.angle_diff(hits[0]["bearing"], bearing), 3, key)
        return hits[0]

    def test_header(self):
        self.assertEqual(self.pack["date"], 20250101)
        self.assertEqual(self.pack["count"], len(self.pack["records"]))

    def test_directed_signs(self):
        for key in ("forward", "backward", "untagged", "numeric"):
            self.check_bearing(key)

    def test_junction_node_warns_only_the_side_street(self):
        self.check_bearing("junction")

    def test_untagged_sign_far_from_a_junction_and_footway_signs_are_dropped(self):
        for xy in ((100, 200), (150, 600)):
            lat, lng = pt(*xy)
            self.assertFalse([r for r in self.pack["records"]
                              if abs(r["lat"] - lat) < 1e-6 and abs(r["lng"] - lng) < 1e-6], xy)

    def test_cameras(self):
        self.assertEqual(self.check_bearing("camera_any")["maxspeed"], 50)
        self.assertEqual(self.check_bearing("camera_dir")["maxspeed"], 70)   # from the road
        self.assertEqual(self.check_bearing("camera_off_road")["maxspeed"], 48)   # 30 mph

    def test_record_count(self):
        # forward, backward, untagged, junction (one approach), cameras x3, numeric.
        self.assertEqual(self.pack["count"], 8)


if __name__ == "__main__":
    unittest.main()
