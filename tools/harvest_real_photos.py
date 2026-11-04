#!/usr/bin/env python3
"""
Real-World Photo Harvester for LeoCAD Multimodal AI Dataset.
Pairs scraped/normalized LEGO models with authentic real-world photography
from Wikimedia Commons based on semantic tags and cleaned entity queries.
"""

import os
import re
import sys
import json
import time
import urllib.request
import urllib.parse
from PIL import Image
import io

INDEX_FILE = os.path.abspath("knowledge_base/index.json")
DATASET_DIR = os.path.abspath("dataset/models")

USER_AGENT = "LeoCAD-AI-Dataset-Harvester/1.0 (https://github.com/Ulysse6/LeoCAD-Design-Agent; multimodal research)"

CATEGORY_CONTEXT = {
    "truck_pickup": "truck",
    "car_civilian": "car",
    "car_racing": "racing car",
    "service_emergency": "vehicle",
    "heavy_machinery": "construction machinery",
    "spacecraft": "spacecraft",
    "aircraft": "aircraft",
    "watercraft": "boat",
    "train_rail": "train",
    "building_town": "building",
    "castle_historical": "historical",
    "creature_animal": "animal",
    "figure_character": "character",
    "furniture_props": "furniture",
    "seasonal_holiday": "holiday"
}

def clean_query(name, category, tags):
    """Constructs a clean, photographic search query from model metadata."""
    clean = re.sub(r"[\{\}\(\)]", "", name)
    clean = re.sub(r"\b(set|mini|series|edition|pack|lego)\b", "", clean, flags=re.I)
    clean = re.sub(r"#[0-9]+", "", clean)
    clean = re.sub(r"\b\d+\s*x\s*\d+\b", "", clean)  # remove "4 x 4"
    clean = " ".join(clean.split()).strip()

    clean_lower = clean.lower()

    # Specialized queries for vehicles & machines
    if "pickup" in clean_lower: return "pickup truck"
    if "tractor" in clean_lower: return "front loader tractor"
    if "tow truck" in clean_lower: return "tow truck wrecker"
    if "dragster" in clean_lower: return "top fuel dragster racing"
    if "helicopter" in clean_lower: return "rescue helicopter"
    if "mail truck" in clean_lower or "postal" in clean_lower: return "postal delivery mail truck"
    if "milk" in clean_lower and "truck" in clean_lower: return "vintage milk delivery truck"
    if "garbage" in clean_lower and "truck" in clean_lower: return "garbage collection truck"
    if "container" in clean_lower and "lorry" in clean_lower: return "container semi truck"
    if "crane" in clean_lower: return "mobile crane truck"
    if "speedboat" in clean_lower or ("motor" in clean_lower and "boat" in clean_lower): return "motor speedboat"
    if "rally" in clean_lower: return "rally race car"
    if "buggy" in clean_lower: return "off road racing buggy"
    if "locomotive" in clean_lower or "train" in clean_lower: return "steam locomotive" if "steam" in clean_lower else "diesel electric locomotive"
    if "tanker" in clean_lower: return "railroad tank car tanker"
    if "freight" in clean_lower or "wagon" in clean_lower: return "railroad freight car"
    if "f40" in clean_lower: return "Ferrari F40"
    if "458" in clean_lower: return "Ferrari 458 Italia"
    if "250 gt" in clean_lower: return "Ferrari 250 GT Berlinetta"
    if "ferrari" in clean_lower: return "Ferrari racing car"
    if "batmobile" in clean_lower or "tumbler" in clean_lower: return "Batmobile vehicle"
    if "tug" in clean_lower and "boat" in category: return "tugboat vessel"
    if "plane" in clean_lower or "aeroplane" in clean_lower or "jet" in clean_lower: return "passenger airplane"

    # World Landmarks & Architecture
    if "empire state" in clean_lower: return "Empire State Building New York"
    if "burj khalifa" in clean_lower: return "Burj Khalifa Dubai"
    if "hancock" in clean_lower: return "John Hancock Center Chicago"
    if "eiffel tower" in clean_lower: return "Eiffel Tower Paris"
    if "big ben" in clean_lower: return "Big Ben London"
    if "pisa" in clean_lower: return "Leaning Tower of Pisa"
    if "white house" in clean_lower: return "White House Washington DC"
    if "brandenburg" in clean_lower: return "Brandenburg Gate Berlin"
    if "sydney opera" in clean_lower: return "Sydney Opera House"
    if "arc de triomphe" in clean_lower: return "Arc de Triomphe Paris"
    if "flatiron" in clean_lower: return "Flatiron Building Manhattan"
    if "lincoln memorial" in clean_lower: return "Lincoln Memorial Washington"
    if "rockefeller" in clean_lower: return "Rockefeller Center New York"
    if "guggenheim" in clean_lower: return "Guggenheim Museum New York"
    if "lighthouse" in clean_lower: return "coastal lighthouse"

    # Animals & Nature
    if "panda" in clean_lower or "bear" in clean_lower: return "giant panda bear"
    if "alligator" in clean_lower or "crocodile" in clean_lower: return "alligator"
    if "tiger" in clean_lower: return "Bengal tiger"
    if "parrot" in clean_lower: return "colorful parrot"
    if "pug" in clean_lower: return "pug dog"
    if "pelican" in clean_lower: return "pelican bird"
    if "reindeer" in clean_lower: return "reindeer in snow"
    if "bee" in clean_lower: return "honey bee on flower"
    if "clown fish" in clean_lower or "fish" in clean_lower: return "clownfish anemone"
    if "wreath" in clean_lower: return "Christmas wreath"
    if "flower" in clean_lower or "plant" in clean_lower: return "flower bouquet colorful"
    if "desk" in clean_lower: return "wooden study desk"

    # Sci-Fi & Pop Culture
    if "x-wing" in clean_lower: return "X-wing starfighter"
    if "star destroyer" in clean_lower: return "Star Destroyer"
    if "slave i" in clean_lower: return "Slave I spaceship"

    # Generic clean fallback
    if clean.lower() == "truck": return "commercial transport truck"
    if clean.lower() == "car": return "modern passenger car"
    context = CATEGORY_CONTEXT.get(category, "")
    if context and context not in clean_lower:
        return f"{clean} {context}"
    return clean

def search_wikimedia(query, max_results=3):
    """Searches Wikimedia Commons for high-quality photos matching query."""
    base_url = "https://commons.wikimedia.org/w/api.php"
    headers = {"User-Agent": USER_AGENT}

    # Try first with negative keywords to avoid toy photos
    for attempt_query in [f"{query} -lego -toy -miniature", query]:
        params = {
            "action": "query",
            "format": "json",
            "list": "search",
            "srsearch": attempt_query,
            "srnamespace": "6",  # File namespace
            "srlimit": str(max_results * 2)
        }
        url = f"{base_url}?{urllib.parse.urlencode(params)}"

        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=12) as response:
                data = json.loads(response.read().decode("utf-8"))
                results = data.get("query", {}).get("search", [])

            photo_titles = []
            for r in results:
                t = r.get("title", "")
                ext = os.path.splitext(t)[1].lower()
                if ext in [".jpg", ".jpeg", ".png", ".webp"] and not any(k in t.lower() for k in ["diagram", "map", "logo", "icon", "drawing", "scheme"]):
                    photo_titles.append(t)
                if len(photo_titles) >= max_results:
                    break

            if photo_titles:
                return photo_titles
        except Exception as e:
            print(f"      [-] Search error for '{query}': {e}")
            break

    return []

def get_image_info(title, target_width=800):
    """Gets direct image/thumbnail URL and dimensions for a Wikimedia Commons title."""
    base_url = "https://commons.wikimedia.org/w/api.php"
    params = {
        "action": "query",
        "format": "json",
        "titles": title,
        "prop": "imageinfo",
        "iiprop": "url|size|mime",
        "iiurlwidth": str(target_width)
    }
    url = f"{base_url}?{urllib.parse.urlencode(params)}"
    headers = {"User-Agent": USER_AGENT}

    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=12) as response:
            data = json.loads(response.read().decode("utf-8"))
            pages = data.get("query", {}).get("pages", {})
            for pid, p in pages.items():
                if "imageinfo" in p and len(p["imageinfo"]) > 0:
                    info = p["imageinfo"][0]
                    thumb = info.get("thumburl", info.get("url"))
                    return {
                        "url": thumb,
                        "orig_url": info.get("url"),
                        "width": info.get("thumbwidth", info.get("width")),
                        "height": info.get("thumbheight", info.get("height")),
                        "mime": info.get("mime", "")
                    }
    except Exception as e:
        print(f"      [-] Image info error for '{title}': {e}")
    return None

def download_and_process_image(image_url, dest_path, max_dim=800):
    """Downloads an image, validates with PIL, resizes to max_dim, and saves as clean JPEG."""
    headers = {"User-Agent": USER_AGENT}
    try:
        req = urllib.request.Request(image_url, headers=headers)
        with urllib.request.urlopen(req, timeout=15) as res:
            img_bytes = res.read()

        if len(img_bytes) < 3000:
            return False

        img = Image.open(io.BytesIO(img_bytes))
        img = img.convert("RGB")

        # Resize if larger than max_dim
        w, h = img.size
        if max(w, h) > max_dim:
            scale = max_dim / float(max(w, h))
            new_size = (int(w * scale), int(h * scale))
            img = img.resize(new_size, Image.Resampling.LANCZOS)

        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        img.save(dest_path, "JPEG", quality=85)
        return True
    except Exception as e:
        print(f"      [-] Download/processing error from {image_url}: {e}")
        return False

def harvest_for_model(model_entry, photos_per_model=2, force=False):
    """Harvests real-world reference photos for a single model."""
    set_num = model_entry["set_number"]
    name = model_entry["name"]
    categories = model_entry.get("categories", ["object"])
    category = categories[0] if categories else "object"
    tags = model_entry.get("tags", [])

    model_dir = os.path.join(DATASET_DIR, set_num, "real_world")
    meta_path = os.path.join(model_dir, "metadata.json")

    # Check if already harvested
    if not force and os.path.exists(meta_path):
        try:
            with open(meta_path, "r", encoding="utf-8") as f:
                existing = json.load(f)
                if len(existing.get("photos", [])) >= photos_per_model:
                    return existing.get("photos", [])
        except:
            pass

    query = clean_query(name, category, tags)
    print(f"  [>] Harvesting {set_num} ({name}) -> Query: '{query}'")

    titles = search_wikimedia(query, max_results=photos_per_model + 1)
    time.sleep(1.2)  # Polite API spacing

    saved_photos = []
    for idx, t in enumerate(titles):
        if len(saved_photos) >= photos_per_model:
            break

        info = get_image_info(t)
        time.sleep(1.0)
        if not info:
            continue

        filename = f"photo_{len(saved_photos)+1:02d}.jpg"
        dest_path = os.path.join(model_dir, filename)

        success = download_and_process_image(info["url"], dest_path)
        if success:
            saved_photos.append({
                "file": filename,
                "relative_path": os.path.relpath(dest_path, os.getcwd()),
                "source_title": t,
                "source_url": info["orig_url"],
                "query": query
            })

    # Save metadata
    os.makedirs(model_dir, exist_ok=True)
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump({
            "set_number": set_num,
            "name": name,
            "query": query,
            "photos": saved_photos
        }, f, indent=2)

    return saved_photos

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Harvest real-world photography for LEGO models.")
    parser.add_argument("--limit", type=int, default=None, help="Limit number of models to harvest")
    parser.add_argument("--set", type=str, default=None, help="Harvest for specific set (e.g. 1991-1)")
    parser.add_argument("--photos", type=int, default=2, help="Number of real-world photos per model (default: 2)")
    parser.add_argument("--force", action="store_true", help="Re-harvest even if photos already exist")
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

    print(f"Starting real-world photo harvesting for {len(models)} models ({args.photos} photos each)...")

    total_photos = 0
    for idx, m in enumerate(models):
        photos = harvest_for_model(m, args.photos, args.force)
        total_photos += len(photos)
        print(f"    [OK] #{idx+1:03d} Saved {len(photos)}/{args.photos} photos for {m['set_number']}")

    print("\n==========================================")
    print("Photo Harvesting Complete!")
    print(f"Models processed: {len(models)}")
    print(f"Total real-world photos collected: {total_photos}")
    print(f"Output directory: {DATASET_DIR}")
    print("==========================================")

if __name__ == "__main__":
    main()
