#!/usr/bin/env python3
"""
Generates C++ source and header files for LeoCAD's built-in Knowledge Base Catalog.
Converts index.json and normalized LDR files into common/lc_kb_catalog.h and common/lc_kb_catalog.cpp.
"""

import json
import os
import re

INDEX_FILE = os.path.abspath("knowledge_base/index.json")
NORM_DIR = os.path.abspath("knowledge_base/normalized")
HEADER_PATH = os.path.abspath("common/lc_kb_catalog.h")
SOURCE_PATH = os.path.abspath("common/lc_kb_catalog.cpp")

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
\tconst char* LDrawData;
};

class lcKBCatalog
{
public:
\tstatic size_t GetModelCount();
\tstatic const lcKBModelEntry* GetModel(size_t Index);
\tstatic const lcKBModelEntry* FindBestMatch(const QString& Prompt, int* OutScore = nullptr);
\tstatic std::vector<const lcKBModelEntry*> Search(const QString& Query, int MaxResults = 5);
};
"""

    with open(HEADER_PATH, "w", encoding="utf-8") as f:
        f.write(header_content)
    print(f"Wrote {HEADER_PATH}")

    # 2. Write lc_kb_catalog.cpp
    src_lines = [
        '#include "lc_global.h"',
        '#include "lc_kb_catalog.h"',
        '#include <QRegExp>',
        '#include <QSet>',
        '',
        '// Embedded authentic LEGO models from LDraw Official Model Repository',
    ]

    # Model definitions
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

    src_lines.append(f'static const lcKBModelEntry gKBModels[{len(models)}] = {{')

    for idx, m in enumerate(models):
        var_name = f"gKBModelData_{idx}"
        set_num = m["set_number"].replace('"', '\\"')
        name = m["name"].replace('"', '\\"')
        theme = m["theme"].replace('"', '\\"')
        year = int(m.get("year", 0)) if str(m.get("year", "")).isdigit() else 0
        pieces = m["piece_count"]
        cats = " ".join(m["categories"]).replace('"', '\\"')
        tags = " ".join(m["tags"]).replace('"', '\\"')
        dom_color = m["dominant_colors"][0] if m.get("dominant_colors") else 16

        src_lines.append(
            f'\t{{ "{set_num}", "{name}", "{theme}", {year}, {pieces}, "{cats}", "{tags}", {dom_color}, {var_name} }},'
        )

    src_lines.append('};')
    src_lines.append('')
    src_lines.append(f'static const size_t gKBModelCount = {len(models)};')
    src_lines.append('')
    src_lines.append("""size_t lcKBCatalog::GetModelCount()
{
\treturn gKBModelCount;
}

const lcKBModelEntry* lcKBCatalog::GetModel(size_t Index)
{
\tif (Index < gKBModelCount)
\t\treturn &gKBModels[Index];
\treturn nullptr;
}

const lcKBModelEntry* lcKBCatalog::FindBestMatch(const QString& Prompt, int* OutScore)
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
    print(f"Wrote {SOURCE_PATH}")

if __name__ == "__main__":
    generate()
