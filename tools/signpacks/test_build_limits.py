#!/usr/bin/env python3
"""Checks build_limits.py on a tiny hand-made map.  Run: python -I test_build_limits.py"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_limits  # noqa: E402
from test_build_pack import Osm, pt  # noqa: E402


class BuildLimitsTest(unittest.TestCase):
    def setUp(self):
        m = Osm()
        a = [m.node(x, 0) for x in (0, 100, 200)]
        m.way(a, highway="residential", maxspeed="50")                          # plain, two-way
        b = [m.node(x, 100) for x in (0, 100, 200)]
        m.way(b, highway="residential", maxspeed="30", oneway="yes")           # one-way
        c = [m.node(x, 200) for x in (0, 100, 200)]
        m.way(c, highway="secondary", **{"maxspeed:forward": "70", "maxspeed:backward": "50"})
        d = [m.node(x, 300) for x in (0, 100, 200)]
        m.way(d, highway="residential", maxspeed="DE:urban")                    # zone value
        e = [m.node(x, 400) for x in (0, 100, 200)]
        m.way(e, highway="motorway", maxspeed="DE:motorway")                    # no limit
        f = [m.node(x, 500) for x in (0, 100, 200)]
        m.way(f, highway="residential")                                         # no tag
        g = [m.node(x, 600) for x in (0, 100, 200)]
        m.way(g, highway="footway", maxspeed="10")                              # not for cars
        h = [m.node(x, 700) for x in (0, 100, 200)]
        m.way(h, highway="primary", maxspeed="30 mph")                          # mph
        with tempfile.TemporaryDirectory() as dd:
            path = os.path.join(dd, "t.osm")
            with open(path, "w") as f:
                f.write(m.xml())
            self.out = build_limits.decode(build_limits.build(path, 20250101))

    def at(self, y):
        lat, _ = pt(0, y)
        return [(lim, fl) for lim, fl, pts in self.out["ways"] if abs(pts[0][0] - lat) < 1e-6]

    def test_plain_and_oneway(self):
        self.assertEqual(self.at(0), [(50, 0)])
        self.assertEqual(self.at(100), [(30, 1)])

    def test_directional_limits_become_two_pieces(self):
        self.assertEqual(sorted(self.at(200)), [(50, 2), (70, 1)])

    def test_zone_values_and_units(self):
        self.assertEqual(self.at(300), [(50, 0)])
        self.assertEqual(self.at(700), [(48, 0)])

    def test_ways_without_a_usable_limit_are_left_out(self):
        self.assertEqual(self.at(400), [])
        self.assertEqual(self.at(500), [])
        self.assertEqual(self.at(600), [])
        self.assertEqual(self.out["date"], 20250101)

    def test_simplify_keeps_corners_and_drops_collinear_points(self):
        straight = [(52.0, 13.0 + i * 1e-4) for i in range(6)]
        self.assertEqual(len(build_limits.simplify(straight, 8.0)), 2)
        corner = [(52.0, 13.0), (52.0, 13.001), (52.001, 13.001)]
        self.assertEqual(len(build_limits.simplify(corner, 8.0)), 3)

    def test_split_by_tile_overlaps_one_point(self):
        pts = [(52.5, 13.399), (52.5, 13.3995), (52.5, 13.4005), (52.5, 13.401)]
        parts = build_limits.split_by_tile(pts)
        self.assertEqual(len(parts), 2)
        self.assertEqual(parts[0][-1], pts[2])   # the crossing segment stays with the first tile
        self.assertEqual(parts[1][0], pts[2])


if __name__ == "__main__":
    unittest.main()
