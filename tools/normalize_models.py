#!/usr/bin/env python3
"""
LDraw Model Normalization & Indexing Pipeline for LeoCAD AI Assistant.
Extracts models from knowledge_base/raw/, flattens submodels into world coordinates,
centers geometry on (X=0, Z=0, ground Y=0), sorts pieces bottom-to-top (assembly layer),
inserts progressive '0 STEP' instructions, tags semantically, and generates index.json.
"""

import os
import re
import json
import html
import subprocess
from collections import Counter

RAW_DIR = os.path.abspath("knowledge_base/raw")
NORM_DIR = os.path.abspath("knowledge_base/normalized")
METADATA_FILE = os.path.join(RAW_DIR, "metadata.json")
INDEX_FILE = os.path.abspath("knowledge_base/index.json")

CATEGORY_KEYWORDS = {
    "truck_pickup": [
        "pickup", "pick-up", "truck", "lorry", "utility", "flatbed", "tow truck",
        "dump", "freight", "transport", "van", "delivery", "carrier", "trailer", "hauler"
    ],
    "car_civilian": [
        "car", "roadster", "coupe", "sedan", "automobile", "buggy", "auto",
        "convertible", "jeep", "4x4", "suv", "cruiser", "rover", "dynamo", "vehicle"
    ],
    "car_racing": [
        "racer", "racing", "speed", "dragster", "formula", "sprint", "kart",
        "hot rod", "sport", "grand prix", "rally"
    ],
    "service_emergency": [
        "police", "patrol", "fire", "firetruck", "rescue", "res-q", "ambulance",
        "medic", "emergency"
    ],
    "heavy_machinery": [
        "tractor", "loader", "digger", "excavator", "crane", "forklift",
        "bulldozer", "roller"
    ],
    "spacecraft": [
        "space", "craft", "spaceship", "starfighter", "shuttle", "interceptor",
        "rocket", "lunar", "moon", "orbit", "satellite", "futuron", "blacktron",
        "m:tron", "star wars", "droid", "fighter", "destroyer", "gunship", "walker",
        "aat", "at-st", "at-te", "slave i", "x-wing"
    ],
    "aircraft": [
        "plane", "airplane", "aeroplane", "jet", "helicopter", "chopper",
        "biplane", "glider", "propeller", "flight", "aircraft", "dart", "aviation"
    ],
    "watercraft": [
        "boat", "ship", "speedboat", "canoe", "raft", "sea", "yacht", "vessel",
        "sail", "coastal", "kayak", "pirate", "cruiser"
    ],
    "train_rail": [
        "train", "wagon", "locomotive", "railway", "railroad", "car transport",
        "freight wagon", "tanker"
    ],
    "building_town": [
        "house", "building", "station", "garage", "store", "shop", "depot",
        "cottage", "post office", "shed", "center", "tower", "lighthouse",
        "architecture", "khalifa", "hancock", "empire state"
    ],
    "castle_historical": [
        "castle", "knight", "guard", "cart", "wagon", "horse", "hay cart",
        "fortress", "smuggler", "tower", "dungeon", "western", "pirates"
    ],
    "creature_animal": [
        "animal", "creature", "bee", "alligator", "fish", "panda", "reindeer",
        "bear", "dog", "cat", "bird", "dinosaur", "monster"
    ],
    "figure_character": [
        "minifig", "figure", "character", "biker", "skater", "chief",
        "pirate", "soldier", "friend", "friends", "beach", "andrea", "puzzle"
    ],
    "furniture_props": [
        "desk", "table", "chair", "bed", "furniture", "room", "interior", "lamp"
    ],
    "seasonal_holiday": [
        "wreath", "christmas", "holiday", "seasonal", "santa", "tree", "snow", "easter", "halloween"
    ]
}

def parse_mpd(filepath):
    """Parses MPD or LDR file into a dictionary of submodels."""
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        text = f.read()

    submodels = {}
    current = "main"
    first_name = None
    lines = text.splitlines()

    for line in lines:
        s = line.strip()
        if s.startswith("0 FILE"):
            current = s[6:].strip().lower()
            if first_name is None:
                first_name = current
            submodels[current] = []
        elif s.startswith("1 "):
            if current not in submodels:
                submodels[current] = []
                if first_name is None:
                    first_name = current
            parts = s.split()
            if len(parts) >= 15:
                try:
                    submodels[current].append({
                        "color": int(parts[1]),
                        "pos": [float(parts[2]), float(parts[3]), float(parts[4])],
                        "rot": [float(x) for x in parts[5:14]],
                        "part": " ".join(parts[14:])
                    })
                except ValueError:
                    pass

    return submodels, first_name

def mult_rot(r1, r2):
    """Multiplies two 3x3 matrices represented as 9-element arrays."""
    return [
        r1[0]*r2[0] + r1[1]*r2[3] + r1[2]*r2[6],
        r1[0]*r2[1] + r1[1]*r2[4] + r1[2]*r2[7],
        r1[0]*r2[2] + r1[1]*r2[5] + r1[2]*r2[8],
        r1[3]*r2[0] + r1[4]*r2[3] + r1[5]*r2[6],
        r1[3]*r2[1] + r1[4]*r2[4] + r1[5]*r2[7],
        r1[3]*r2[2] + r1[4]*r2[5] + r1[5]*r2[8],
        r1[6]*r2[0] + r1[7]*r2[3] + r1[8]*r2[6],
        r1[6]*r2[1] + r1[7]*r2[4] + r1[8]*r2[7],
        r1[6]*r2[2] + r1[7]*r2[5] + r1[8]*r2[8],
    ]

def mult_vec(r, v):
    """Multiplies a 3x3 matrix by a 3-element vector."""
    return [
        r[0]*v[0] + r[1]*v[1] + r[2]*v[2],
        r[3]*v[0] + r[4]*v[1] + r[5]*v[2],
        r[6]*v[0] + r[7]*v[1] + r[8]*v[2]
    ]

def flatten_submodels(submodels, model_name, cur_color=16, cur_pos=None, cur_rot=None, depth=0):
    """Recursively resolves submodels into world coordinates."""
    if cur_pos is None:
        cur_pos = [0.0, 0.0, 0.0]
    if cur_rot is None:
        cur_rot = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]

    if depth > 12 or model_name not in submodels:
        return []

    flat = []
    for elem in submodels[model_name]:
        part_name = elem["part"].lower()
        elem_color = cur_color if elem["color"] == 16 else elem["color"]
        elem_rot = mult_rot(cur_rot, elem["rot"])
        p_rot = mult_vec(cur_rot, elem["pos"])
        elem_pos = [cur_pos[0] + p_rot[0], cur_pos[1] + p_rot[1], cur_pos[2] + p_rot[2]]

        if part_name in submodels:
            flat.extend(flatten_submodels(submodels, part_name, elem_color, elem_pos, elem_rot, depth + 1))
        else:
            flat.append({
                "color": elem_color,
                "pos": elem_pos,
                "rot": elem_rot,
                "part": elem["part"]
            })

    return flat

def generate_tags_and_categories(name, theme, set_number):
    """Generates clean search tags and categorizes model."""
    clean_name = html.unescape(name or "")
    clean_theme = html.unescape(theme or "")
    text = f"{clean_name} {clean_theme} {set_number}".lower()

    words = re.findall(r"\b[a-z0-9\-]+\b", text)
    tags = set(words)
    # Ignore pure short numeric junk
    tags = {t for t in tags if not (t.isdigit() and len(t) <= 2) and t != "gt"}

    matched_categories = []
    for cat, kw_list in CATEGORY_KEYWORDS.items():
        for kw in kw_list:
            if re.search(r"\b" + re.escape(kw) + r"\b", text):
                matched_categories.append(cat)
                tags.add(kw)
                for w in kw.split():
                    tags.add(w)

    return list(tags), list(set(matched_categories))

def run_normalization():
    os.makedirs(NORM_DIR, exist_ok=True)
    if not os.path.exists(METADATA_FILE):
        print(f"Error: {METADATA_FILE} not found.")
        return

    with open(METADATA_FILE, "r", encoding="utf-8") as f:
        metadata = json.load(f)

    print(f"Loaded {len(metadata)} raw models. Running normalization...")

    index_entries = []
    skipped = 0

    for set_num, meta in metadata.items():
        raw_file = os.path.join(RAW_DIR, meta["file_name"])
        if not os.path.exists(raw_file):
            skipped += 1
            continue

        name = html.unescape(meta.get("name", ""))
        theme = html.unescape(meta.get("theme", ""))
        year = meta.get("year", "")

        submodels, root = parse_mpd(raw_file)
        if not root or root not in submodels:
            skipped += 1
            continue

        pieces = flatten_submodels(submodels, root)
        if len(pieces) < 8 or len(pieces) > 260:
            skipped += 1
            continue

        # Compute Bounding Box
        xs = [p["pos"][0] for p in pieces]
        ys = [p["pos"][1] for p in pieces]
        zs = [p["pos"][2] for p in pieces]

        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        min_z, max_z = min(zs), max(zs)

        # In LDraw, +Y is DOWN. The ground is max_y.
        # Shift to center X=0, Z=0, and ground Y=0.
        center_x = (min_x + max_x) / 2.0
        center_z = (min_z + max_z) / 2.0
        shift_y = -max_y

        grid_shift_x = round(-center_x / 10.0) * 10.0
        grid_shift_z = round(-center_z / 10.0) * 10.0
        grid_shift_y = round(shift_y / 8.0) * 8.0

        centered_pieces = []
        for p in pieces:
            new_p = {
                "color": p["color"],
                "pos": [
                    p["pos"][0] + grid_shift_x,
                    p["pos"][1] + grid_shift_y,
                    p["pos"][2] + grid_shift_z
                ],
                "rot": p["rot"],
                "part": p["part"]
            }
            centered_pieces.append(new_p)

        # Sort bottom-to-top (descending Y: highest Y to lowest Y)
        centered_pieces.sort(key=lambda p: -p["pos"][1])

        # Step division: ~6-10 steps per model
        step_size = max(4, len(centered_pieces) // 8)

        # Format LDraw Content
        norm_lines = [
            f"0 {name}",
            f"0 Name: {set_num}.ldr",
            f"0 Author: LEGO & LDraw Community",
            f"0 !LEOCAD MODEL"
        ]

        for idx, p in enumerate(centered_pieces):
            if idx > 0 and (idx % step_size == 0):
                norm_lines.append("0 STEP")

            r_str = " ".join(f"{v:.4f}".rstrip("0").rstrip(".") if v != 0 else "0" for v in p["rot"])
            norm_lines.append(
                f"1 {p['color']} {p['pos'][0]:.2f} {p['pos'][1]:.2f} {p['pos'][2]:.2f} {r_str} {p['part']}"
            )

        norm_text = "\n".join(norm_lines) + "\n"
        norm_filename = f"{set_num}.ldr"
        norm_filepath = os.path.join(NORM_DIR, norm_filename)

        with open(norm_filepath, "w", encoding="utf-8") as f:
            f.write(norm_text)

        tags, categories = generate_tags_and_categories(name, theme, set_num)
        parts_counter = Counter(p["part"].lower() for p in centered_pieces)
        colors_counter = Counter(p["color"] for p in centered_pieces)

        entry = {
            "set_number": set_num,
            "name": name,
            "theme": theme,
            "year": year,
            "piece_count": len(centered_pieces),
            "file_name": norm_filename,
            "relative_path": os.path.relpath(norm_filepath, os.getcwd()),
            "categories": categories,
            "tags": sorted(tags),
            "dominant_colors": [c[0] for c in colors_counter.most_common(3)],
            "bounding_box": {
                "min_x": round(min_x + grid_shift_x, 2),
                "max_x": round(max_x + grid_shift_x, 2),
                "min_y": round(min_y + grid_shift_y, 2),
                "max_y": round(max_y + grid_shift_y, 2),
                "min_z": round(min_z + grid_shift_z, 2),
                "max_z": round(max_z + grid_shift_z, 2)
            },
            "inventory": dict(parts_counter.most_common(8))
        }

        index_entries.append(entry)

    # Save index.json
    with open(INDEX_FILE, "w", encoding="utf-8") as f:
        json.dump(index_entries, f, indent=2)

    print("==========================================")
    print(f"Normalization Complete!")
    print(f"Successfully processed {len(index_entries)} models.")
    print(f"Skipped: {skipped}")
    print(f"Index written to {INDEX_FILE}")
    print("==========================================")

if __name__ == "__main__":
    run_normalization()
