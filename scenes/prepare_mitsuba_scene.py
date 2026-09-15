#!/usr/bin/env python3
"""Prepare a Benedikt Bitterli Mitsuba scene for Lumen.

    python3 scenes/prepare_mitsuba_scene.py <scene.zip or extracted dir> [--integrator restirpt] [--depth 10]

Extracts the archive next to this script (scenes/<name>/), writes scenes/<name>/scene.xml
from scene_v0.6.xml with the integrator/depth swapped, and reports scene features the Lumen
Mitsuba loader does not support. Everything else (camera convention, area emitters, rectangles,
inline BSDFs, ...) is handled by the loader itself, so the original file is left untouched.
"""
import argparse, collections, os, re, sys, zipfile
import xml.etree.ElementTree as ET

SUPPORTED_SHAPES = {"obj", "rectangle", "disk", "cube", "sphere"}
SUPPORTED_EMITTERS = {"area", "sunsky", "sun", "directional", "constant"}
SUPPORTED_BSDFS = {"diffuse", "roughdiffuse", "plastic", "roughplastic", "dielectric", "roughdielectric",
                   "thindielectric", "conductor", "roughconductor", "glass",
                   # wrappers the loader unwraps
                   "twosided", "mask", "bumpmap", "normalmap", "coating", "roughcoating"}


def report_unsupported(xml_path):
    root = ET.parse(xml_path).getroot()
    issues = []
    shapes = collections.Counter(s.get("type") for s in root.iter("shape"))
    for t, n in shapes.items():
        if t not in SUPPORTED_SHAPES:
            issues.append(f"{n} shape(s) of type '{t}' will be skipped")
    emitters = collections.Counter(e.get("type") for e in root.iter("emitter"))
    for t, n in emitters.items():
        if t not in SUPPORTED_EMITTERS:
            issues.append(f"{n} emitter(s) of type '{t}' will be skipped")
    bsdfs = collections.Counter(b.get("type") for b in root.iter("bsdf"))
    for t, n in bsdfs.items():
        if t not in SUPPORTED_BSDFS:
            issues.append(f"{n} BSDF(s) of type '{t}' fall back to diffuse")
    if bsdfs.get("mask"):
        issues.append(f"{bsdfs['mask']} 'mask' BSDF(s): opacity is ignored (rendered opaque)")
    if bsdfs.get("bumpmap") or bsdfs.get("normalmap"):
        issues.append("bump/normal maps are ignored (base BSDF only)")
    if any(root.iter("medium")):
        issues.append("participating media are ignored")
    return issues


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", help="scene zip from benedikt-bitterli.me or an already extracted directory")
    ap.add_argument("--integrator", default="restirpt")
    ap.add_argument("--depth", type=int, default=10)
    args = ap.parse_args()

    scenes_dir = os.path.dirname(os.path.abspath(__file__))
    if os.path.isdir(args.source):
        scene_dir = os.path.abspath(args.source)
    else:
        with zipfile.ZipFile(args.source) as z:
            top = z.namelist()[0].split("/")[0]
            z.extractall(scenes_dir)
        scene_dir = os.path.join(scenes_dir, top)
    src = os.path.join(scene_dir, "scene_v0.6.xml")
    if not os.path.exists(src):
        sys.exit(f"no scene_v0.6.xml in {scene_dir}")

    text = open(src).read()
    text, n_int = re.subn(r'<integrator type="[a-z]+"', f'<integrator type="{args.integrator}"', text, count=1)
    text, n_dep = re.subn(r'(<integer name="maxDepth" value=")\d+(")', rf'\g<1>{args.depth}\2', text, count=1)
    if not n_int or not n_dep:
        sys.exit("could not find integrator/maxDepth in scene_v0.6.xml")
    dst = os.path.join(scene_dir, "scene.xml")
    open(dst, "w").write(text)
    print(f"wrote {os.path.relpath(dst, scenes_dir)} (integrator={args.integrator}, maxDepth={args.depth})")
    for issue in report_unsupported(dst):
        print(f"  ! {issue}")


if __name__ == "__main__":
    main()
