#!/usr/bin/env python3
"""Builds the Qt 6 (Android/desktop) QML tree from the Sailfish one.

usage: gen_qt6_qml.py <repo> <out_dir>

Copies qml/ to <out_dir>, rewrites Qt 5 era imports, applies the small
platform patches below, then lays android/qml/ over the top (files there
replace the originals). The Sailfish sources are never modified.
"""
import os
import re
import shutil
import sys

repo, out = sys.argv[1], sys.argv[2]
shutil.rmtree(out, ignore_errors=True)
shutil.copytree(os.path.join(repo, "qml"), out)

# (file, old, new): every patch must apply, so drift in the Sailfish QML is
# caught at build time instead of silently producing a different UI.
PATCHES = [
    ("theme/M.qml", "import Sailfish.Silica 1.0 as Silica\n", ""),
    ("theme/M.qml", "Silica.Theme.colorScheme === Silica.Theme.LightOnDark",
     "Qt.styleHints.colorScheme === Qt.ColorScheme.Dark"),
    ("theme/M.qml", "readonly property real topInset: AppTools ? AppTools.cm : 0",
     "readonly property real topInset: AppTools ? AppTools.insetTop : 0"),
]

for rel, old, new in PATCHES:
    p = os.path.join(out, rel)
    s = open(p, encoding="utf-8").read()
    if old not in s:
        sys.exit("patch does not apply to %s: %r" % (rel, old))
    open(p, "w", encoding="utf-8").write(s.replace(old, new))

IMPORT = re.compile(r"^(import\s+)([A-Za-z][\w.]*)\s+\d+(?:\.\d+)*(\s+as\s+\w+)?\s*$", re.M)
for root, _, files in os.walk(out):
    for f in files:
        if not f.endswith(".qml"):
            continue
        p = os.path.join(root, f)
        s = open(p, encoding="utf-8").read()
        s = s.replace("import QtGraphicalEffects 1.0", "import Qt5Compat.GraphicalEffects")
        s = IMPORT.sub(lambda m: m.group(1) + m.group(2) + (m.group(3) or ""), s)
        open(p, "w", encoding="utf-8").write(s)

overlay = os.path.join(repo, "android", "qml")
for root, _, files in os.walk(overlay):
    for f in files:
        src = os.path.join(root, f)
        dst = os.path.join(out, os.path.relpath(src, overlay))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)

# The cover page is a Sailfish concept.
shutil.rmtree(os.path.join(out, "cover"), ignore_errors=True)
