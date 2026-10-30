#!/usr/bin/env python3
"""Lists the extracts to build sign packs for, as a JSON array.

usage: countries.py [--index index-v1-nogeom.json]

Reads Geofabrik's extract index (downloaded when not given) and keeps one
entry per country of every continent, with the continent the app groups it
under. Groups and overlapping regions are left out.
"""
import argparse
import json
import urllib.request

INDEX_URL = "https://download.geofabrik.de/index-v1-nogeom.json"
# Only Europe is built for now; add more continents here (Geofabrik id: name).
CONTINENTS = {"europe": "Europe"}
SKIP = {"alps", "azores", "britain-and-ireland", "dach", "great-britain", "guernsey-jersey", "sea", "us",
        "us-midwest", "us-northeast", "us-pacific", "us-south", "us-west"}


def countries(index):
    out = []
    for feat in index["features"]:
        p = feat["properties"]
        parent = p.get("parent")
        if parent in CONTINENTS and p["id"] not in SKIP:
            continent = CONTINENTS[parent]
        else:
            continue
        name = p["name"]
        if p["id"].startswith("us/"):
            name = "US: " + name
        iso = (p.get("iso3166-1:alpha2") or [""])[0]
        out.append({"id": p["id"].replace("/", "-"), "name": name, "iso": iso, "continent": continent,
                    "url": p["urls"]["pbf"]})
    return sorted(out, key=lambda c: (c["continent"], c["name"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--index")
    args = ap.parse_args()
    if args.index:
        with open(args.index) as f:
            index = json.load(f)
    else:
        with urllib.request.urlopen(INDEX_URL, timeout=60) as r:
            index = json.load(r)
    print(json.dumps(countries(index)))


if __name__ == "__main__":
    main()
