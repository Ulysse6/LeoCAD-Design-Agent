#!/usr/bin/env python3
"""
Multimodal Dataset Builder for LeoCAD AI.
Assembles normalized LDraw files, real-world photos, multi-view CAD renders,
and natural language prompt variations into a unified training dataset.
Exports a master manifest (manifest.json) and training pairs (training_pairs.jsonl).
"""

import os
import sys
import json
import glob
import re

INDEX_FILE = os.path.abspath("knowledge_base/index.json")
NORM_DIR = os.path.abspath("knowledge_base/normalized")
DATASET_DIR = os.path.abspath("dataset")
MODELS_DIR = os.path.join(DATASET_DIR, "models")
MANIFEST_FILE = os.path.join(DATASET_DIR, "manifest.json")
TRAIN_FILE = os.path.join(DATASET_DIR, "training_pairs.jsonl")

COLOR_NAMES = {
    0: "black", 1: "blue", 2: "green", 3: "teal", 4: "red",
    5: "magenta", 6: "brown", 7: "gray", 14: "yellow", 15: "white",
    19: "tan", 25: "orange", 27: "lime", 71: "light bluish gray", 72: "dark bluish gray"
}

def generate_prompts(name, category, dom_color, piece_count):
    """Generates varied human prompt expressions explicitly conditioning on piece counts."""
    color_name = COLOR_NAMES.get(dom_color, "")
    c_prefix = f"{color_name} " if color_name else ""

    clean_name = re.sub(r"[\{\}\(\)]", "", name)
    clean_name = re.sub(r"\b(set|mini|series|edition)\b", "", clean_name, flags=re.I).strip()

    prompts = [
        f"Build a {c_prefix}{clean_name.lower()} using approximately {piece_count} pieces",
        f"Create a {piece_count}-piece 3D LEGO model of this {clean_name.lower()}",
        f"Construct a LEGO {clean_name.lower()} with about {piece_count} bricks",
        f"Build this {clean_name.lower()} in LEGO matching the reference image, using around {piece_count} parts",
        f"Generate a {c_prefix}{clean_name.lower()} model with an exact budget of {piece_count} bricks",
        f"I want a {piece_count}-piece LEGO build of this {clean_name.lower()}"
    ]
    return prompts

def build_dataset():
    if not os.path.exists(INDEX_FILE):
        print(f"Error: {INDEX_FILE} not found.")
        sys.exit(1)

    with open(INDEX_FILE, "r", encoding="utf-8") as f:
        catalog = json.load(f)

    print(f"Building multimodal dataset from {len(catalog)} catalog entries...")

    dataset_entries = []
    training_pairs = []

    total_real_photos = 0
    total_renders = 0

    for m in catalog:
        set_num = m["set_number"]
        name = m["name"]
        piece_count = m["piece_count"]
        categories = m.get("categories", ["object"])
        cat = categories[0] if categories else "object"
        dom_color = m["dominant_colors"][0] if m.get("dominant_colors") else 16

        ldr_path = os.path.join(NORM_DIR, m["file_name"])
        if not os.path.exists(ldr_path):
            continue

        with open(ldr_path, "r", encoding="utf-8", errors="ignore") as f:
            ldr_content = f.read().strip()

        step_count = ldr_content.count("0 STEP") + 1

        # Check real-world photos
        real_dir = os.path.join(MODELS_DIR, set_num, "real_world")
        real_photos = sorted(glob.glob(os.path.join(real_dir, "photo_*.jpg")))
        total_real_photos += len(real_photos)

        # Check synthetic renders
        render_dir = os.path.join(MODELS_DIR, set_num, "renders")
        renders = sorted(glob.glob(os.path.join(render_dir, "*.png")))
        total_renders += len(renders)

        prompts = generate_prompts(name, cat, dom_color, piece_count)

        entry = {
            "set_number": set_num,
            "name": name,
            "theme": m.get("theme", ""),
            "piece_count": piece_count,
            "categories": categories,
            "tags": m.get("tags", []),
            "bounding_box": m.get("bounding_box", {}),
            "ldr_file": os.path.relpath(ldr_path, DATASET_DIR),
            "real_world_photos": [os.path.relpath(p, DATASET_DIR) for p in real_photos],
            "synthetic_renders": [os.path.relpath(r, DATASET_DIR) for r in renders],
            "prompts": prompts
        }
        dataset_entries.append(entry)

        # Generate vision-language conversational pairs for training
        # 1. Real photo -> LDraw assembly
        for p_idx, r_photo in enumerate(real_photos):
            prompt = prompts[p_idx % len(prompts)]
            pair = {
                "id": f"{set_num}_real_{p_idx+1:02d}",
                "set_number": set_num,
                "type": "real_world",
                "image": os.path.relpath(r_photo, os.getcwd()),
                "conversations": [
                    {
                        "from": "human",
                        "value": f"<image>\n{prompt}"
                    },
                    {
                        "from": "gpt",
                        "value": f"Here is the authentic LEGO 3D model for {name} ({piece_count} pieces):\n\n```ldraw\n{ldr_content}\n```"
                    }
                ]
            }
            training_pairs.append(pair)

        # 2. Synthetic LEGO render -> LDraw assembly (reconstruction / fine detail matching)
        for r_idx, s_render in enumerate(renders):
            view_name = os.path.splitext(os.path.basename(s_render))[0]
            clean_name = re.sub(r"[\{\}\(\)]", "", name)
            clean_name = re.sub(r"\b(set|mini|series|edition)\b", "", clean_name, flags=re.I).strip()
            pair = {
                "id": f"{set_num}_cad_{view_name}",
                "set_number": set_num,
                "type": "synthetic_cad",
                "image": os.path.relpath(s_render, os.getcwd()),
                "conversations": [
                    {
                        "from": "human",
                        "value": f"<image>\nRecreate this LEGO {clean_name.lower()} in 3D LDraw format using {piece_count} pieces."
                    },
                    {
                        "from": "gpt",
                        "value": f"Here is the 3D LEGO model for {name} ({piece_count} pieces across {step_count} building steps):\n\n```ldraw\n{ldr_content}\n```"
                    }
                ]
            }
            training_pairs.append(pair)

    # Save manifest
    with open(MANIFEST_FILE, "w", encoding="utf-8") as f:
        json.dump({
            "version": "1.0",
            "model_count": len(dataset_entries),
            "total_real_photos": total_real_photos,
            "total_synthetic_renders": total_renders,
            "total_training_pairs": len(training_pairs),
            "models": dataset_entries
        }, f, indent=2)

    # Save training pairs JSONL
    with open(TRAIN_FILE, "w", encoding="utf-8") as f:
        for tp in training_pairs:
            f.write(json.dumps(tp) + "\n")

    print("\n==========================================")
    print("Multimodal Dataset Assembly Complete!")
    print(f"Total LEGO models indexed: {len(dataset_entries)}")
    print(f"Real-world reference photos: {total_real_photos}")
    print(f"Synthetic multi-view renders: {total_renders}")
    print(f"Total multimodal training pairs: {len(training_pairs)}")
    print(f"Manifest written to: {MANIFEST_FILE}")
    print(f"Training JSONL written to: {TRAIN_FILE}")
    print("==========================================")

if __name__ == "__main__":
    build_dataset()
