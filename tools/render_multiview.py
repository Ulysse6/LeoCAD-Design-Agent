#!/usr/bin/env python3
"""
Multi-View Synthetic LEGO Renderer for LeoCAD AI Dataset.
Renders canonical multi-angle images (front iso, rear iso, side, top)
from normalized LDraw models using LeoCAD's headless CLI renderer.
"""

import os
import sys
import json
import argparse
import subprocess

LEOCAD_BIN = os.path.abspath("build/release/leocad")
INDEX_FILE = os.path.abspath("knowledge_base/index.json")
NORM_DIR = os.path.abspath("knowledge_base/normalized")
DATASET_DIR = os.path.abspath("dataset/models")

VIEWS = [
    {
        "name": "iso_front",
        "args": ["--camera-angles", "25", "45"]
    },
    {
        "name": "iso_rear",
        "args": ["--camera-angles", "25", "225"]
    },
    {
        "name": "side",
        "args": ["--viewpoint", "right"]
    },
    {
        "name": "top",
        "args": ["--viewpoint", "top"]
    }
]

def render_model(set_number, ldr_path, out_dir, width=512, height=512, force=False):
    os.makedirs(out_dir, exist_ok=True)
    results = {}

    env = os.environ.copy()
    if "DISPLAY" not in env or not env["DISPLAY"]:
        env["DISPLAY"] = ":0"

    for v in VIEWS:
        view_name = v["name"]
        out_png = os.path.join(out_dir, f"{view_name}.png")

        if os.path.exists(out_png) and os.path.getsize(out_png) > 1000 and not force:
            results[view_name] = out_png
            continue

        cmd = [
            LEOCAD_BIN,
            ldr_path,
            "-i", out_png,
            "-w", str(width),
            "-h", str(height)
        ] + v["args"]

        try:
            res = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=15)
            if os.path.exists(out_png) and os.path.getsize(out_png) > 500:
                results[view_name] = out_png
            else:
                print(f"    [-] Warning rendering {view_name} for {set_number}: {res.stderr.strip() or res.stdout.strip()}")
        except Exception as e:
            print(f"    [-] Error rendering {view_name} for {set_number}: {e}")

    return results

def main():
    parser = argparse.ArgumentParser(description="Render multi-angle synthetic views of normalized LEGO models.")
    parser.add_argument("--limit", type=int, default=None, help="Limit number of models to render")
    parser.add_argument("--set", type=str, default=None, help="Render a specific set number (e.g. 1991-1)")
    parser.add_argument("--width", type=int, default=512, help="Image width (default: 512)")
    parser.add_argument("--height", type=int, default=512, help="Image height (default: 512)")
    parser.add_argument("--force", action="store_true", help="Re-render even if image already exists")
    args = parser.parse_args()

    if not os.path.exists(INDEX_FILE):
        print(f"Error: {INDEX_FILE} not found.")
        sys.exit(1)

    with open(INDEX_FILE, "r", encoding="utf-8") as f:
        models = json.load(f)

    if args.set:
        models = [m for m in models if m["set_number"] == args.set]
        if not models:
            print(f"Set {args.set} not found in index.")
            sys.exit(1)

    if args.limit:
        models = models[:args.limit]

    print(f"Starting multi-view rendering for {len(models)} models ({len(VIEWS)} views each)...")

    rendered_count = 0
    for idx, m in enumerate(models):
        set_num = m["set_number"]
        name = m["name"]
        ldr_file = os.path.join(NORM_DIR, m["file_name"])

        if not os.path.exists(ldr_file):
            print(f"  [!] Missing file {ldr_file}")
            continue

        model_dir = os.path.join(DATASET_DIR, set_num, "renders")
        views_dict = render_model(set_num, ldr_file, model_dir, args.width, args.height, args.force)
        rendered_count += len(views_dict)

        print(f"  [{idx+1:03d}/{len(models):03d}] {set_num} ({name}) -> {len(views_dict)}/{len(VIEWS)} views")

    print("\n==========================================")
    print(f"Rendering Complete!")
    print(f"Models processed: {len(models)}")
    print(f"Total images saved: {rendered_count}")
    print(f"Output directory: {DATASET_DIR}")
    print("==========================================")

if __name__ == "__main__":
    main()
