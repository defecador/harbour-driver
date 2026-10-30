#!/usr/bin/env python3
"""Writes index.json for a folder of .drsp packs.

usage: make_index.py <packs dir> <countries.json> > index.json
"""
import hashlib
import json
import os
import struct
import sys


def main():
    packs_dir, countries_file = sys.argv[1], sys.argv[2]
    with open(countries_file) as f:
        names = {c["id"]: c for c in json.load(f)}
    packs = []
    for fn in sorted(os.listdir(packs_dir)):
        if not fn.endswith(".drsp"):
            continue
        pid = fn[:-5]
        path = os.path.join(packs_dir, fn)
        data = open(path, "rb").read()
        magic, _ver, count, date = struct.unpack_from("<4s3i", data, 0)
        if magic != b"DRSP" or count == 0:
            continue
        c = names.get(pid, {"name": pid, "iso": ""})
        entry = {"id": pid, "name": c["name"], "iso": c["iso"], "continent": c.get("continent", ""), "bytes": len(data), "count": count,
                 "date": date, "sha256": hashlib.sha256(data).hexdigest()}
        limits = os.path.join(packs_dir, pid + ".drsl")
        if os.path.exists(limits):
            ldata = open(limits, "rb").read()
            if ldata[:4] == b"DRSL":
                entry["limitsBytes"] = len(ldata)
                entry["limitsSha256"] = hashlib.sha256(ldata).hexdigest()
        packs.append(entry)
    json.dump({"version": 1, "packs": packs}, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
