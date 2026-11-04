#!/usr/bin/env python3
"""
LDraw OMR Scraper for LeoCAD AI Knowledge Base
Fetches ~100 official LEGO models from the LDraw Official Model Repository (OMR)
strictly with < 150 pieces (small to medium builds).
"""

import os
import re
import time
import json
import urllib.request
import urllib.error

BASE_URL = "https://library.ldraw.org"
OMR_SETS_URL = f"{BASE_URL}/omr/sets?page="
OUTPUT_DIR = os.path.abspath("knowledge_base/raw")
METADATA_FILE = os.path.join(OUTPUT_DIR, "metadata.json")
MAX_MODELS = 105
MIN_PIECES = 12
MAX_PIECES = 150

HEADERS = {
    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0"
}

def fetch_html(url, retries=3, delay=1.0):
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers=HEADERS)
            with urllib.request.urlopen(req, timeout=15) as resp:
                return resp.read().decode("utf-8", errors="ignore")
        except Exception as e:
            if attempt == retries - 1:
                print(f"Error fetching {url}: {e}")
                return ""
            time.sleep(delay)
    return ""

def download_bytes(url, retries=3, delay=1.0):
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers=HEADERS)
            with urllib.request.urlopen(req, timeout=20) as resp:
                return resp.read()
        except Exception as e:
            if attempt == retries - 1:
                print(f"Error downloading {url}: {e}")
                return None
            time.sleep(delay)
    return None

def count_pieces(ldraw_bytes):
    """Counts total piece placement commands (lines starting with '1 ')."""
    text = ldraw_bytes.decode("utf-8", errors="ignore")
    count = 0
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("1 "):
            tokens = line.split()
            if len(tokens) >= 15:
                # tokens[14] is part filename
                part_name = tokens[14].lower()
                # Exclude submodel calls if they are just grouping wrappers
                count += 1
    return count

def scrape_omr():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scraped_models = {}

    # Load existing if available
    if os.path.exists(METADATA_FILE):
        try:
            with open(METADATA_FILE, "r", encoding="utf-8") as f:
                scraped_models = json.load(f)
            print(f"Loaded {len(scraped_models)} existing models from {METADATA_FILE}")
        except Exception:
            scraped_models = {}

    print(f"Starting OMR Scraping (Target: {MAX_MODELS} models with {MIN_PIECES}..{MAX_PIECES} pieces)...")

    page = 1
    max_pages = 59

    while len(scraped_models) < MAX_MODELS and page <= max_pages:
        print(f"\nScanning OMR page {page}/{max_pages} (Current models collected: {len(scraped_models)})...")
        html = fetch_html(f"{OMR_SETS_URL}{page}")
        if not html:
            page += 1
            continue

        # Extract set links and data rows
        # Format in filament table:
        # href="https://library.ldraw.org/omr/sets/(\d+)"
        records = re.findall(r"href=\"https://library\.ldraw\.org/omr/sets/(\d+)\"[^>]*>.*?<div class=\"[^\"]*fi-ta-text[^\"]*\">([^<]+)</div>", html, re.DOTALL)
        
        sets_on_page = {}
        for sid, text in records:
            if sid not in sets_on_page:
                sets_on_page[sid] = []
            sets_on_page[sid].append(text.strip())

        for sid, fields in sets_on_page.items():
            if len(scraped_models) >= MAX_MODELS:
                break

            if len(fields) < 2:
                continue

            set_number = fields[0]
            set_name = fields[1]
            set_theme = fields[2] if len(fields) > 2 else "General"
            set_year = fields[3] if len(fields) > 3 else "Unknown"

            # Check if already processed
            clean_num = set_number.replace("/", "_").replace("\\", "_")
            if clean_num in scraped_models:
                continue

            # Visit set page to get download URL
            set_page_url = f"{BASE_URL}/omr/sets/{sid}"
            set_html = fetch_html(set_page_url)
            if not set_html:
                continue

            # Find download links (.mpd or .ldr)
            dl_links = re.findall(r"href=\"([^\"]*media/omr_models/[^\"]*\.(?:mpd|ldr)[^\"]*)\"", set_html)
            if not dl_links:
                continue

            dl_url = dl_links[0]
            if not dl_url.startswith("http"):
                dl_url = BASE_URL + dl_url

            model_bytes = download_bytes(dl_url)
            if not model_bytes:
                continue

            pieces = count_pieces(model_bytes)
            if pieces < MIN_PIECES or pieces > MAX_PIECES:
                # Outside piece range
                continue

            ext = ".mpd" if ".mpd" in dl_url else ".ldr"
            file_name = f"{clean_num}{ext}"
            file_path = os.path.join(OUTPUT_DIR, file_name)

            with open(file_path, "wb") as f:
                f.write(model_bytes)

            model_entry = {
                "set_number": set_number,
                "name": set_name,
                "theme": set_theme,
                "year": set_year,
                "pieces": pieces,
                "file_name": file_name,
                "omr_id": sid,
                "source_url": dl_url
            }

            scraped_models[clean_num] = model_entry
            print(f"  [+] #{len(scraped_models):03d} Saved: {set_number} - {set_name} ({set_theme}, {set_year}) | {pieces} pieces")

            # Save progress periodically
            if len(scraped_models) % 10 == 0:
                with open(METADATA_FILE, "w", encoding="utf-8") as f:
                    json.dump(scraped_models, f, indent=2)

            time.sleep(0.3)  # Polite crawling delay

        page += 1

    # Final save
    with open(METADATA_FILE, "w", encoding="utf-8") as f:
        json.dump(scraped_models, f, indent=2)

    print(f"\nOMR Scraping Completed! Total models collected: {len(scraped_models)}")
    print(f"Metadata saved to: {METADATA_FILE}")

if __name__ == "__main__":
    scrape_omr()
