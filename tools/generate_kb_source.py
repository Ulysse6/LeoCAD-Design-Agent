#!/usr/bin/env python3
"""
Generates C++ source and header files for LeoCAD's built-in Knowledge Base Catalog.
Converts index.json and normalized LDR files into common/lc_kb_catalog.h and common/lc_kb_catalog.cpp.
Includes visual difference hashes (dHash), primary/secondary colors, and piece-count conditioning.
"""

import json
import os
import re
from PIL import Image
from collections import Counter

INDEX_FILE = os.path.abspath("knowledge_base/index.json")
NORM_DIR = os.path.abspath("knowledge_base/normalized")
DATASET_DIR = os.path.abspath("dataset/models")
HEADER_PATH = os.path.abspath("common/lc_kb_catalog.h")
SOURCE_PATH = os.path.abspath("common/lc_kb_catalog.cpp")


def compute_dhash(img_path):
    if not os.path.exists(img_path):
        return 0
    try:
        img = Image.open(img_path).convert("L").resize((9, 8), Image.Resampling.LANCZOS)
        pixels = list(img.getdata())
        diff = []
        for row in range(8):
            for col in range(8):
                diff.append(pixels[row * 9 + col] > pixels[row * 9 + col + 1])
        h = 0
        for bit in diff:
            h = (h << 1) | (1 if bit else 0)
        return h
    except Exception:
        return 0


def analyze_ldr_colors(ldr_text, default_dom=16):
    colors = []
    for line in ldr_text.splitlines():
        line = line.strip()
        if line.startswith("1 "):
            parts = line.split()
            if len(parts) >= 2:
                try:
                    c = int(parts[1])
                    if c not in (0, 16):  # Exclude black and main color 16
                        colors.append(c)
                except ValueError:
                    pass
    if not colors:
        return default_dom, 0
    counts = Counter(colors).most_common(2)
    dom = counts[0][0] if len(counts) > 0 else default_dom
    sec = counts[1][0] if len(counts) > 1 else 0
    return dom, sec


def generate():
    if not os.path.exists(INDEX_FILE):
        print(f"Error: {INDEX_FILE} not found.")
        return

    with open(INDEX_FILE, "r", encoding="utf-8") as f:
        models = json.load(f)

    print(f"Loaded {len(models)} models from index.json.")

    # 1. Write lc_kb_catalog.h
    header_content = """#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <cstdint>

struct lcKBModelEntry
{
\tconst char* SetNumber;
\tconst char* Name;
\tconst char* Theme;
\tint Year;
\tint PieceCount;
\tconst char* Categories; // Space-delimited
\tconst char* Tags;       // Space-delimited
\tint DominantColor;
\tint SecondaryColor;
\tuint64_t VisualHash;
\tconst char* LDrawData;
};

class lcKBCatalog
{
public:
\tstatic size_t GetModelCount();
\tstatic const lcKBModelEntry* GetModel(size_t Index);
\tstatic const lcKBModelEntry* FindBestMatch(const QString& Prompt, int TargetPieces = 0, int* OutScore = nullptr);
\tstatic const lcKBModelEntry* FindBestVisualMatch(uint64_t VisualHash, int DominantColor = -1, int TargetPieces = 0, int* OutScore = nullptr);
\tstatic std::vector<const lcKBModelEntry*> Search(const QString& Query, int MaxResults = 5);
};
"""

    with open(HEADER_PATH, "w", encoding="utf-8") as f:
        f.write(header_content)
    print(f"Wrote {HEADER_PATH}")

    # 2. Prepare C++ source
    src_lines = [
        '#include "lc_global.h"',
        '#include "lc_kb_catalog.h"',
        '#include <QRegExp>',
        '#include <QSet>',
        '#include <cmath>',
        '#include <algorithm>',
        '',
        '// Embedded authentic LEGO models from LDraw Official Model Repository',
    ]

    model_entries = []

    for idx, m in enumerate(models):
        ldr_path = os.path.join(NORM_DIR, m["file_name"])
        ldr_text = ""
        if os.path.exists(ldr_path):
            with open(ldr_path, "r", encoding="utf-8", errors="ignore") as lf:
                ldr_text = lf.read().strip()

        # Variable name for LDR data
        var_name = f"gKBModelData_{idx}"
        src_lines.append(f'static const char {var_name}[] = R"ldraw(')
        src_lines.append(ldr_text)
        src_lines.append(')ldraw";')
        src_lines.append('')

        # Visual hash from render
        render_path = os.path.join(DATASET_DIR, m["set_number"], "renders", "iso_front.png")
        vhash = compute_dhash(render_path)

        # Colors
        dom_color, sec_color = analyze_ldr_colors(
            ldr_text,
            m["dominant_colors"][0] if m.get("dominant_colors") else 16,
        )

        set_num = m["set_number"].replace('"', '\\"')
        name = m["name"].replace('"', '\\"')
        theme = m["theme"].replace('"', '\\"')
        year = int(m.get("year", 0)) if str(m.get("year", "")).isdigit() else 0
        pieces = m["piece_count"]
        cats = " ".join(m["categories"]).replace('"', '\\"')
        tags = " ".join(m["tags"]).replace('"', '\\"')

        model_entries.append(
            f'\t{{ "{set_num}", "{name}", "{theme}", {year}, {pieces}, "{cats}", "{tags}", {dom_color}, {sec_color}, UINT64_C(0x{vhash:016x}), {var_name} }},'
        )

    src_lines.append(f'static const lcKBModelEntry gKBModels[{len(models)}] = {{')
    src_lines.extend(model_entries)
    src_lines.append('};')
    src_lines.append('')
    src_lines.append(f'static const size_t gKBModelCount = {len(models)};')
    src_lines.append('')

    src_lines.append("""static inline int HammingDistance(uint64_t a, uint64_t b)
{
#if defined(__GNUC__) || defined(__clang__)
\treturn __builtin_popcountll(a ^ b);
#else
\tuint64_t v = a ^ b;
\tint c = 0;
\twhile (v) { c += (v & 1); v >>= 1; }
\treturn c;
#endif
}

size_t lcKBCatalog::GetModelCount()
{
\treturn gKBModelCount;
}

const lcKBModelEntry* lcKBCatalog::GetModel(size_t Index)
{
\tif (Index < gKBModelCount)
\t\treturn &gKBModels[Index];
\treturn nullptr;
}

const lcKBModelEntry* lcKBCatalog::FindBestMatch(const QString& Prompt, int TargetPieces, int* OutScore)
{
\tQString Lower = Prompt.toLower().trimmed();
\tQStringList Words = Lower.split(QRegExp("[\\\\s,;:.!?-]+"), QString::SkipEmptyParts);
\tQSet<QString> WordSet = Words.toSet();

\tconst lcKBModelEntry* BestModel = nullptr;
\tint BestScore = -1;

\tfor (size_t i = 0; i < gKBModelCount; ++i)
\t{
\t\tconst lcKBModelEntry& Entry = gKBModels[i];
\t\tint Score = 0;

\t\tQString EntryName = QString::fromUtf8(Entry.Name).toLower();
\t\tQStringList NameWords = EntryName.split(QRegExp("[\\\\s,;:.!?-]+"), QString::SkipEmptyParts);
\t\tQString EntryTags = QString::fromUtf8(Entry.Tags).toLower();
\t\tQStringList TagWords = EntryTags.split(' ', QString::SkipEmptyParts);
\t\tQString EntryCats = QString::fromUtf8(Entry.Categories).toLower();
\t\tQStringList CatList = EntryCats.split(' ', QString::SkipEmptyParts);

\t\t// Direct title match
\t\tif (Lower.contains(EntryName))
\t\t\tScore += 160;

\t\t// Name word overlap
\t\tfor (const QString& W : NameWords)
\t\t{
\t\t\tif (W.length() > 2 && WordSet.contains(W))
\t\t\t\tScore += 45;
\t\t}

\t\t// Tag overlap
\t\tfor (const QString& T : TagWords)
\t\t{
\t\t\tif (T.length() > 2 && WordSet.contains(T))
\t\t\t\tScore += 18;
\t\t}

\t\t// Semantic distinctions and priority boosts
\t\tif (WordSet.contains("pickup") || WordSet.contains("pick-up"))
\t\t{
\t\t\tif (TagWords.contains("pickup")) Score += 130;
\t\t}
\t\tif (WordSet.contains("tow") && TagWords.contains("tow"))
\t\t\tScore += 130;
\t\tif (WordSet.contains("tractor") && CatList.contains("heavy_machinery"))
\t\t\tScore += 130;
\t\tif (WordSet.contains("helicopter") && (TagWords.contains("helicopter") || CatList.contains("aircraft")))
\t\t\tScore += 120;
\t\tif (WordSet.contains("dragster") && TagWords.contains("dragster"))
\t\t\tScore += 130;
\t\tif (WordSet.contains("speedboat") && TagWords.contains("speedboat"))
\t\t\tScore += 120;
\t\tif (WordSet.contains("police") && TagWords.contains("police"))
\t\t\tScore += 100;
\t\tif (WordSet.contains("fire") && TagWords.contains("fire"))
\t\t\tScore += 100;
\t\tif (WordSet.contains("x-wing") && TagWords.contains("x-wing"))
\t\t\tScore += 150;
\t\tif (WordSet.contains("destroyer") && TagWords.contains("destroyer"))
\t\t\tScore += 150;
\t\tif (WordSet.contains("lighthouse") && TagWords.contains("lighthouse"))
\t\t\tScore += 130;
\t\tif ((WordSet.contains("empire") || WordSet.contains("state")) && TagWords.contains("empire"))
\t\t\tScore += 130;
\t\tif (WordSet.contains("buggy") && TagWords.contains("buggy"))
\t\t\tScore += 90;
\t\tif (WordSet.contains("ferrari") || WordSet.contains("f40"))
\t\t{
\t\t\tif (EntryName.contains("ferrari") || TagWords.contains("ferrari")) Score += 140;
\t\t}
\t\tif (WordSet.contains("super") && WordSet.contains("chief"))
\t\t{
\t\t\tif (EntryName.contains("super chief")) Score += 160;
\t\t}
\t\tif (WordSet.contains("locomotive") || WordSet.contains("engine"))
\t\t{
\t\t\tif (EntryName.contains("locomotive") || CatList.contains("train_rail")) Score += 100;
\t\t}
\t\tif (WordSet.contains("white") && WordSet.contains("house"))
\t\t{
\t\t\tif (EntryName.contains("white house")) Score += 170;
\t\t}
\t\tif (WordSet.contains("eiffel"))
\t\t{
\t\t\tif (EntryName.contains("eiffel")) Score += 170;
\t\t}
\t\tif (WordSet.contains("big") && WordSet.contains("ben"))
\t\t{
\t\t\tif (EntryName.contains("big ben")) Score += 170;
\t\t}
\t\tif (WordSet.contains("pisa"))
\t\t{
\t\t\tif (EntryName.contains("pisa")) Score += 170;
\t\t}
\t\tif (WordSet.contains("sydney") || WordSet.contains("opera"))
\t\t{
\t\t\tif (EntryName.contains("sydney")) Score += 160;
\t\t}
\t\tif (WordSet.contains("brandenburg"))
\t\t{
\t\t\tif (EntryName.contains("brandenburg")) Score += 170;
\t\t}
\t\tif (WordSet.contains("truck") && CatList.contains("truck_pickup"))
\t\t\tScore += 40;
\t\tif (WordSet.contains("car") && (CatList.contains("car_civilian") || CatList.contains("car_racing")))
\t\t\tScore += 30;
\t\tif (WordSet.contains("train") && CatList.contains("train_rail"))
\t\t\tScore += 50;
\t\tif (WordSet.contains("space") && CatList.contains("spacecraft"))
\t\t\tScore += 40;
\t\tif (WordSet.contains("house") && CatList.contains("building_town"))
\t\t\tScore += 40;

\t\t// Piece Count Conditioning
\t\tif (TargetPieces > 0 && Score > 15)
\t\t{
\t\t\tint Diff = std::abs(Entry.PieceCount - TargetPieces);
\t\t\tif (Diff <= std::max(15, TargetPieces / 5))
\t\t\t\tScore += 55;
\t\t\telse if (Diff <= std::max(30, TargetPieces / 3))
\t\t\t\tScore += 30;
\t\t\telse if (Diff <= std::max(60, TargetPieces / 2))
\t\t\t\tScore += 10;
\t\t\telse if (Diff > TargetPieces)
\t\t\t\tScore -= 25;
\t\t}

\t\tif (Score > BestScore)
\t\t{
\t\t\tBestScore = Score;
\t\t\tBestModel = &Entry;
\t\t}
\t}

\tif (OutScore)
\t\t*OutScore = BestScore;

\t// Threshold for retrieval vs procedural fallback
\treturn (BestScore >= 25) ? BestModel : nullptr;
}

const lcKBModelEntry* lcKBCatalog::FindBestVisualMatch(uint64_t VisualHash, int DominantColor, int TargetPieces, int* OutScore)
{
\tif (VisualHash == 0 && DominantColor < 0)
\t{
\t\tif (OutScore) *OutScore = 0;
\t\treturn nullptr;
\t}

\tconst lcKBModelEntry* BestModel = nullptr;
\tint BestScore = -1;

\tfor (size_t i = 0; i < gKBModelCount; ++i)
\t{
\t\tconst lcKBModelEntry& Entry = gKBModels[i];
\t\tint Score = 0;

\t\tif (VisualHash != 0 && Entry.VisualHash != 0)
\t\t{
\t\t\tint Dist = HammingDistance(VisualHash, Entry.VisualHash);
\t\t\tScore += std::max(0, (32 - Dist) * 4);
\t\t}

\t\tif (DominantColor >= 0)
\t\t{
\t\t\tif (Entry.DominantColor == DominantColor)
\t\t\t\tScore += 35;
\t\t\telse if (Entry.SecondaryColor == DominantColor)
\t\t\t\tScore += 15;
\t\t}

\t\tif (TargetPieces > 0 && Score > 15)
\t\t{
\t\t\tint Diff = std::abs(Entry.PieceCount - TargetPieces);
\t\t\tif (Diff <= std::max(15, TargetPieces / 4))
\t\t\t\tScore += 35;
\t\t\telse if (Diff <= std::max(30, TargetPieces / 2))
\t\t\t\tScore += 15;
\t\t}

\t\tif (Score > BestScore)
\t\t{
\t\t\tBestScore = Score;
\t\t\tBestModel = &Entry;
\t\t}
\t}

\tif (OutScore)
\t\t*OutScore = BestScore;

\treturn (BestScore >= 35) ? BestModel : nullptr;
}

std::vector<const lcKBModelEntry*> lcKBCatalog::Search(const QString& Query, int MaxResults)
{
\tstd::vector<const lcKBModelEntry*> Results;
\tQString Lower = Query.toLower().trimmed();
\tif (Lower.isEmpty()) return Results;

\tfor (size_t i = 0; i < gKBModelCount; ++i)
\t{
\t\tconst lcKBModelEntry& Entry = gKBModels[i];
\t\tQString EntryName = QString::fromUtf8(Entry.Name).toLower();
\t\tQString EntryTags = QString::fromUtf8(Entry.Tags).toLower();

\t\tif (EntryName.contains(Lower) || EntryTags.contains(Lower))
\t\t{
\t\t\tResults.push_back(&Entry);
\t\t\tif (static_cast<int>(Results.size()) >= MaxResults)
\t\t\t\tbreak;
\t\t}
\t}

\treturn Results;
}
""")

    with open(SOURCE_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(src_lines))
    print(f"Wrote {SOURCE_PATH} ({len(models)} models)")


if __name__ == "__main__":
    generate()
