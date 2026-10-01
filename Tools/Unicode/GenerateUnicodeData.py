#!/usr/bin/env python3
"""Generate the runtime's compiled-in Unicode tables.

    UCD text sources  ->  Source/RoboRenderer/Generated/UnicodeData.{h,cpp}

The tables are generated, checked in, and then treated as source: the runtime
never reads a UCD file at run time, it does not even know what one is. That is
the same policy the compiler applies to its registries, and for the same reason:
a property lookup is an array search, not a parse.

Usage:
    python Tools/Unicode/GenerateUnicodeData.py --ucd Reference/Runtime/Tools/ucd/source

The default source directory is the one the Roboticela runtime vendored
(Unicode 16.0.0). Only the standard files are read, so any UCD drop works.
"""

import argparse
import os
import re
import sys
from collections import OrderedDict

# ---------------------------------------------------------------------------
# Property vocabularies. The index into each list is the value the table stores,
# so the order is part of the on-disk contract and must not be reordered.
# ---------------------------------------------------------------------------

GENERAL_CATEGORY = [
    "Cc", "Cf", "Cn", "Co", "Cs", "Ll", "Lm", "Lo", "Lt", "Lu",
    "Mc", "Me", "Mn", "Nd", "Nl", "No", "Pc", "Pd", "Pe", "Pf",
    "Pi", "Po", "Ps", "Sc", "Sk", "Sm", "So", "Zl", "Zp", "Zs",
]

BIDI_CLASS = [
    "L", "R", "AL", "EN", "ES", "ET", "AN", "CS", "NSM", "BN",
    "B", "S", "WS", "ON", "LRE", "RLE", "PDF", "LRO", "RLO",
    "LRI", "RLI", "FSI", "PDI",
]

LINE_BREAK = [
    "XX", "BK", "CR", "LF", "CM", "NL", "SG", "WJ", "ZW", "GL",
    "SP", "ZWJ", "B2", "BA", "BB", "HY", "CB", "CL", "CP", "EX",
    "IN", "NS", "OP", "QU", "IS", "NU", "PO", "PR", "SY", "AI",
    "AL", "CJ", "EB", "EM", "H2", "H3", "HL", "ID", "JL", "JV",
    "JT", "RI", "SA", "SG", "VF", "VI", "AP", "AK", "AS",
]

# The list above is the generated values, with duplicates removed below.
LINE_BREAK = list(OrderedDict.fromkeys(LINE_BREAK))

GRAPHEME_BREAK = [
    "XX", "CR", "LF", "Control", "Extend", "ZWJ", "Regional_Indicator",
    "Prepend", "SpacingMark", "L", "V", "T", "LV", "LVT",
]

WORD_BREAK = [
    "XX", "CR", "LF", "Newline", "Extend", "ZWJ", "Regional_Indicator",
    "Format", "Katakana", "Hebrew_Letter", "ALetter", "Single_Quote",
    "Double_Quote", "MidNumLet", "MidLetter", "MidNum", "Numeric",
    "ExtendNumLet", "WSegSpace",
]

SENTENCE_BREAK = [
    "XX", "CR", "LF", "Sep", "Format", "Sp", "Lower", "Upper", "OLetter",
    "Numeric", "ATerm", "SContinue", "STerm", "Close", "Extend",
]

EAST_ASIAN_WIDTH = ["N", "A", "H", "W", "F", "Na"]

# Emoji bit flags.
EMOJI_EMOJI = 1 << 0
EMOJI_PRESENTATION = 1 << 1
EMOJI_MODIFIER = 1 << 2
EMOJI_MODIFIER_BASE = 1 << 3
EMOJI_COMPONENT = 1 << 4
EMOJI_EXTENDED_PICTOGRAPHIC = 1 << 5

RANGE_RE = re.compile(r"^([0-9A-Fa-f]{4,6})(?:\.\.([0-9A-Fa-f]{4,6}))?\s*;\s*([^#;]+)")


def parse_range_lines(path, index_by_name, field=2):
    """Parse `start..end ; Name # comment` lines into (start, end, value)."""
    ranges = []
    if not os.path.exists(path):
        print("  missing: %s" % path, file=sys.stderr)
        return ranges

    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = [part.strip() for part in line.split(";")]
            if len(parts) < 2:
                continue
            span = parts[0]
            name = parts[1]
            if ".." in span:
                low, high = span.split("..")
            else:
                low = high = span
            value = index_by_name.get(name)
            if value is None:
                continue
            ranges.append((int(low, 16), int(high, 16), value))

    return ranges


def combine_bits(entries):
    """Union overlapping bit-flag ranges into disjoint, sorted ranges."""
    events = {}
    for start, end, bits in entries:
        events.setdefault(start, [0, 0])
        events[start][0] |= bits
        events.setdefault(end + 1, [0, 0])
        events[end + 1][1] |= bits

    result = []
    active = 0
    current_start = None

    for point in sorted(events.keys()):
        add, remove = events[point]
        if active != 0 and current_start is not None:
            result.append((current_start, point - 1, active))
        active = (active & ~remove) | add
        current_start = point

    merged = []
    for start, end, bits in result:
        if merged and merged[-1][2] == bits and merged[-1][1] + 1 == start:
            merged[-1] = (merged[-1][0], end, bits)
        else:
            merged.append((start, end, bits))
    return merged


def merge(ranges):
    """Sort and merge adjacent or overlapping ranges that share a value."""
    ranges = sorted(ranges, key=lambda item: (item[0], item[1]))
    merged = []
    for start, end, value in ranges:
        if merged and merged[-1][2] == value and start <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], end), value)
        else:
            merged.append((start, end, value))
    return merged


def parse_unicode_data(path):
    """general category, combining class, decompositions, simple case, mirror."""
    category = []
    combining = []
    decomposition = {}
    simple_case = {}
    mirrored = {}

    pending_first = None

    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            fields = line.rstrip("\n").split(";")
            if len(fields) < 15:
                continue

            cp = int(fields[0], 16)
            name = fields[1]
            gc = fields[2]
            ccc = int(fields[3])
            decomp = fields[5].strip()
            upper = fields[12].strip()
            lower = fields[13].strip()
            title = fields[14].strip()

            # UnicodeData encodes ranges as `NAME, First` / `NAME, Last` pairs.
            if name.endswith(", First>"):
                pending_first = (cp, gc, ccc, upper, lower, title)
                continue
            if name.endswith(", Last>"):
                if pending_first is not None:
                    start, gc_range, ccc_range, upper_range, lower_range, title_range = pending_first
                    category.append((start, cp, index_of(GENERAL_CATEGORY, gc_range)))
                    if ccc_range:
                        combining.append((start, cp, ccc_range))
                    if upper_range or lower_range or title_range:
                        for value in range(start, cp + 1):
                            simple_case[value] = (upper_range, lower_range, title_range)
                    pending_first = None
                continue

            category.append((cp, cp, index_of(GENERAL_CATEGORY, gc)))
            if ccc:
                combining.append((cp, cp, ccc))

            if decomp:
                canonical = not decomp.startswith("<")
                body = decomp if canonical else decomp.split(">", 1)[1].strip()
                cps = [int(token, 16) for token in body.split()]
                decomposition[cp] = (canonical, cps)

            if upper or lower or title:
                simple_case[cp] = (upper, lower, title)

            # Field 9 is the mirrored glyph flag.
            if fields[9].strip() == "Y":
                mirrored[cp] = True

    return category, combining, decomposition, simple_case, mirrored


def index_of(values, name):
    try:
        return values.index(name)
    except ValueError:
        return 0


def to_u32(value):
    return "%uu" % value


def emit_range8(name, ranges):
    lines = ["const Range8 %s[] = {" % name]
    for start, end, value in ranges:
        lines.append("    { %su, %su, %du }," % (start, end, value))
    lines.append("};")
    lines.append("const std::uint32_t %sCount = %du;" % (name, len(ranges)))
    return "\n".join(lines)


def emit_range16(name, ranges):
    lines = ["const Range16 %s[] = {" % name]
    for start, end, value in ranges:
        lines.append("    { %su, %su, %du }," % (start, end, value))
    lines.append("};")
    lines.append("const std::uint32_t %sCount = %du;" % (name, len(ranges)))
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ucd", default="Reference/Runtime/Tools/ucd/source")
    parser.add_argument("--out", default="Source/RoboRenderer/Generated")
    args = parser.parse_args()

    ucd = args.ucd
    out = args.out
    os.makedirs(out, exist_ok=True)

    print("reading %s" % ucd)

    general_category, combining, decomposition, simple_case, mirrored_flag = parse_unicode_data(
        os.path.join(ucd, "UnicodeData.txt")
    )
    general_category = merge(general_category)
    combining = merge(combining)

    bidi = merge(parse_range_lines(os.path.join(ucd, "DerivedBidiClass.txt"), {v: i for i, v in enumerate(BIDI_CLASS)}))
    line_break = merge(parse_range_lines(os.path.join(ucd, "LineBreak.txt"), {v: i for i, v in enumerate(LINE_BREAK)}))
    grapheme = merge(parse_range_lines(os.path.join(ucd, "GraphemeBreakProperty.txt"), {v: i for i, v in enumerate(GRAPHEME_BREAK)}))
    word = merge(parse_range_lines(os.path.join(ucd, "WordBreakProperty.txt"), {v: i for i, v in enumerate(WORD_BREAK)}))
    sentence = merge(parse_range_lines(os.path.join(ucd, "SentenceBreakProperty.txt"), {v: i for i, v in enumerate(SENTENCE_BREAK)}))
    width = merge(parse_range_lines(os.path.join(ucd, "EastAsianWidth.txt"), {v: i for i, v in enumerate(EAST_ASIAN_WIDTH)}))

    # Emoji properties overlap, so the flags are combined rather than indexed:
    # a code point can be Emoji, Emoji_Presentation and Extended_Pictographic at
    # once, and one lookup has to answer all of them.
    emoji_entries = []
    emoji_names = {
        "Emoji": EMOJI_EMOJI,
        "Emoji_Presentation": EMOJI_PRESENTATION,
        "Emoji_Modifier": EMOJI_MODIFIER,
        "Emoji_Modifier_Base": EMOJI_MODIFIER_BASE,
        "Emoji_Component": EMOJI_COMPONENT,
        "Extended_Pictographic": EMOJI_EXTENDED_PICTOGRAPHIC,
    }
    emoji_path = os.path.join(ucd, "emoji-data.txt")
    if os.path.exists(emoji_path):
        with open(emoji_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = [part.strip() for part in line.split(";")]
                if len(parts) < 2 or parts[1] not in emoji_names:
                    continue
                span = parts[0]
                if ".." in span:
                    low, high = span.split("..")
                else:
                    low = high = span
                emoji_entries.append((int(low, 16), int(high, 16), emoji_names[parts[1]]))
    emoji = combine_bits(emoji_entries)

    # Indic_Conjunct_Break: `Consonant`, `Extend`, `Linker`. Grapheme cluster
    # breaking (GB9c) joins a consonant to a following consonant across them.
    incb_values = {"Consonant": 1, "Extend": 2, "Linker": 3}
    incb = []
    core_path = os.path.join(ucd, "DerivedCoreProperties.txt")
    if os.path.exists(core_path):
        with open(core_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = [part.strip() for part in line.split(";")]
                if len(parts) < 2:
                    continue
                if parts[1] != "InCB" or len(parts) < 3:
                    continue
                kind = parts[2]
                if kind not in incb_values:
                    continue
                span = parts[0]
                if ".." in span:
                    low, high = span.split("..")
                else:
                    low = high = span
                incb.append((int(low, 16), int(high, 16), incb_values[kind]))
    incb = merge(incb)

    # Scripts are named, not enumerated by hand: the generator reads the names.
    scripts = []
    script_index = {}

    def script_value(name):
        if name not in script_index:
            script_index[name] = len(scripts)
            scripts.append(name)
        return script_index[name]

    script_ranges = []
    with open(os.path.join(ucd, "Scripts.txt"), "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = [part.strip() for part in line.split(";")]
            if len(parts) < 2:
                continue
            span = parts[0]
            if ".." in span:
                low, high = span.split("..")
            else:
                low = high = span
            script_ranges.append((int(low, 16), int(high, 16), script_value(parts[1])))
    script_ranges = merge(script_ranges)

    # Bidi paired brackets and mirroring.
    brackets = []
    bracket_path = os.path.join(ucd, "BidiBrackets.txt")
    if os.path.exists(bracket_path):
        with open(bracket_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = [part.strip() for part in line.split(";")]
                if len(parts) < 3:
                    continue
                cp = int(parts[0], 16)
                pair = int(parts[1], 16)
                is_open = 1 if parts[2] == "o" else 0
                brackets.append((cp, pair, is_open))

    mirrors = []
    mirror_path = os.path.join(ucd, "BidiMirroring.txt")
    if os.path.exists(mirror_path):
        with open(mirror_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = [part.strip() for part in line.split(";")]
                if len(parts) < 2:
                    continue
                mirrors.append((int(parts[0], 16), int(parts[1], 16)))
    mirrors.extend((cp, cp) for cp in mirrored_flag if cp not in {m[0] for m in mirrors})
    mirrors = sorted(set(mirrors))

    # Full case mappings (SpecialCasing), used where the simple mapping cannot
    # express the answer: the German sharp s uppercases to "SS", for instance.
    full_case = []
    special_path = os.path.join(ucd, "SpecialCasing.txt")
    if os.path.exists(special_path):
        with open(special_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = [part.strip() for part in line.split(";")]
                if len(parts) < 5:
                    continue
                # Skip unconditional entries only if they are context free.
                if parts[4]:
                    continue
                cp = int(parts[0], 16)
                lower = [int(token, 16) for token in parts[1].split()] if parts[1] else []
                title = [int(token, 16) for token in parts[2].split()] if parts[2] else []
                upper = [int(token, 16) for token in parts[3].split()] if parts[3] else []
                full_case.append((cp, lower, title, upper))

    # Canonical compositions: two-element canonical decompositions that are not
    # excluded. A hash of the pair is cheaper than a search at run time.
    # Characters that must never be produced by composition. The UCD states this
    # as `Full_Composition_Exclusion`, which is exactly the set the algorithm
    # needs, so the generator reads the property rather than re-deriving it.
    exclusions = set()
    normalization_path = os.path.join(ucd, "DerivedNormalizationProps.txt")
    if os.path.exists(normalization_path):
        with open(normalization_path, "r", encoding="utf-8") as handle:
            for line in handle:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = [part.strip() for part in line.split(";")]
                if len(parts) < 2 or parts[1] != "Full_Composition_Exclusion":
                    continue
                span = parts[0]
                if ".." in span:
                    low, high = span.split("..")
                else:
                    low = high = span
                for value in range(int(low, 16), int(high, 16) + 1):
                    exclusions.add(value)

    compositions = []
    for cp, (canonical, cps) in sorted(decomposition.items()):
        if not canonical or len(cps) != 2 or cp in exclusions:
            continue
        first, second = cps
        compositions.append((first, second, cp))
    compositions.sort()

    # Decompositions go into a flat pool: compatibility mappings can be long
    # (a ligature or an Arabic phrase form), so a fixed record cannot hold them.
    # Hangul decomposes algorithmically and is handled in code.
    decomp_data = []
    decomp_records = []
    for cp in sorted(decomposition.keys()):
        canonical, cps = decomposition[cp]
        offset = len(decomp_data)
        decomp_data.extend(cps)
        decomp_records.append((cp, canonical, offset, len(cps)))

    # ---------------------------------------------------------------------
    # Emit
    # ---------------------------------------------------------------------
    header = os.path.join(out, "UnicodeData.h")
    source = os.path.join(out, "UnicodeData.cpp")

    with open(header, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(
            "// Generated by Tools/Unicode/GenerateUnicodeData.py from the Unicode Character\n"
            "// Database. Do not edit: rerun the generator instead.\n"
            "#pragma once\n\n"
            "#include <cstdint>\n\n"
            "namespace RoboRenderer\n{\n"
            "namespace UnicodeData\n{\n"
            "    struct Range8 { std::uint32_t start; std::uint32_t end; std::uint8_t value; };\n"
            "    struct Range16 { std::uint32_t start; std::uint32_t end; std::uint16_t value; };\n"
            "    struct Decomposition { std::uint32_t codepoint; std::uint32_t offset; std::uint8_t compat; std::uint8_t length; };\n"
            "    struct CaseMap { std::uint32_t codepoint; std::uint32_t lower; std::uint32_t upper; std::uint32_t title; };\n"
            "    struct FullCaseMap { std::uint32_t codepoint; std::uint8_t lowerLength; std::uint8_t titleLength; std::uint8_t upperLength; std::uint32_t offset; };\n"
            "    struct MirrorPair { std::uint32_t codepoint; std::uint32_t mirror; };\n"
            "    struct Bracket { std::uint32_t codepoint; std::uint32_t pair; std::uint8_t open; };\n"
            "    struct Composition { std::uint32_t first; std::uint32_t second; std::uint32_t composed; };\n\n"
        )

        for name, count in [
            ("GeneralCategoryRanges", len(general_category)),
            ("CombiningClassRanges", len(combining)),
            ("BidiClassRanges", len(bidi)),
            ("ScriptRanges", len(script_ranges)),
            ("LineBreakRanges", len(line_break)),
            ("GraphemeBreakRanges", len(grapheme)),
            ("WordBreakRanges", len(word)),
            ("SentenceBreakRanges", len(sentence)),
            ("EastAsianWidthRanges", len(width)),
            ("EmojiRanges", len(emoji)),
            ("IndicConjunctRanges", len(incb)),
        ]:
            kind = "Range16" if name == "ScriptRanges" else "Range8"
            handle.write("    extern const %s %s[];\n    extern const std::uint32_t %sCount;\n" % (kind, name, name))

            handle.write("    extern const Decomposition Decompositions[];\n    extern const std::uint32_t DecompositionCount;\n")
        handle.write("    extern const std::uint32_t DecompositionData[];\n    extern const std::uint32_t DecompositionDataCount;\n")
        handle.write("    extern const CaseMap SimpleCaseMaps[];\n    extern const std::uint32_t SimpleCaseCount;\n")
        handle.write("    extern const FullCaseMap FullCaseMaps[];\n    extern const std::uint32_t FullCaseCount;\n")
        handle.write("    extern const std::uint32_t FullCaseData[];\n    extern const std::uint32_t FullCaseDataCount;\n")
        handle.write("    extern const MirrorPair Mirrors[];\n    extern const std::uint32_t MirrorCount;\n")
        handle.write("    extern const Bracket Brackets[];\n    extern const std::uint32_t BracketCount;\n")
        handle.write("    extern const Composition Compositions[];\n    extern const std::uint32_t CompositionCount;\n")
        handle.write("    extern const char* const ScriptNames[];\n    extern const std::uint32_t ScriptNameCount;\n")
        handle.write("    extern const char* const GeneralCategoryNames[];\n    extern const std::uint32_t GeneralCategoryNameCount;\n")
        handle.write("    extern const char* const BidiClassNames[];\n    extern const std::uint32_t BidiClassNameCount;\n")
        handle.write("    extern const char* const LineBreakNames[];\n    extern const std::uint32_t LineBreakNameCount;\n")
        handle.write("}\n}\n")

    with open(source, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(
            "// Generated by Tools/Unicode/GenerateUnicodeData.py from the Unicode Character\n"
            "// Database (Unicode 16.0.0). Do not edit: rerun the generator instead.\n"
            '#include "Generated/UnicodeData.h"\n\n'
            "namespace RoboRenderer\n{\n"
            "namespace UnicodeData\n{\n"
        )

        handle.write(emit_range8("GeneralCategoryRanges", general_category) + "\n\n")
        handle.write(emit_range8("CombiningClassRanges", combining) + "\n\n")
        handle.write(emit_range8("BidiClassRanges", bidi) + "\n\n")
        handle.write(emit_range16("ScriptRanges", script_ranges) + "\n\n")
        handle.write(emit_range8("LineBreakRanges", line_break) + "\n\n")
        handle.write(emit_range8("GraphemeBreakRanges", grapheme) + "\n\n")
        handle.write(emit_range8("WordBreakRanges", word) + "\n\n")
        handle.write(emit_range8("SentenceBreakRanges", sentence) + "\n\n")
        handle.write(emit_range8("EastAsianWidthRanges", width) + "\n\n")
        handle.write(emit_range8("EmojiRanges", emoji) + "\n\n")
        handle.write(emit_range8("IndicConjunctRanges", incb) + "\n\n")

        handle.write("const std::uint32_t DecompositionData[] = {\n")
        for index in range(0, len(decomp_data), 12):
            handle.write("    " + ", ".join("%su" % v for v in decomp_data[index:index + 12]) + ",\n")
        handle.write("};\nconst std::uint32_t DecompositionDataCount = %du;\n\n" % len(decomp_data))

        handle.write("const Decomposition Decompositions[] = {\n")
        for cp, canonical, offset, length in decomp_records:
            handle.write("    { %su, %du, %du, %du },\n" % (cp, offset, 0 if canonical else 1, length))
        handle.write("};\nconst std::uint32_t DecompositionCount = %du;\n\n" % len(decomp_records))

        handle.write("const CaseMap SimpleCaseMaps[] = {\n")
        for cp in sorted(simple_case.keys()):
            upper, lower, title = simple_case[cp]
            handle.write("    { %su, %su, %su, %su },\n" % (
                cp,
                int(lower, 16) if lower else 0,
                int(upper, 16) if upper else 0,
                int(title, 16) if title else 0,
            ))
        handle.write("};\nconst std::uint32_t SimpleCaseCount = %du;\n\n" % len(simple_case))

        full_case_data = []
        full_case_records = []
        for cp, lower, title, upper in full_case:
            offset = len(full_case_data)
            full_case_data.extend(lower + title + upper)
            full_case_records.append((cp, len(lower), len(title), len(upper), offset))

        handle.write("const std::uint32_t FullCaseData[] = {\n")
        for index in range(0, len(full_case_data), 12):
            handle.write("    " + ", ".join("%su" % v for v in full_case_data[index:index + 12]) + ",\n")
        handle.write("};\nconst std::uint32_t FullCaseDataCount = %du;\n\n" % len(full_case_data))

        handle.write("const FullCaseMap FullCaseMaps[] = {\n")
        for cp, lower_length, title_length, upper_length, offset in full_case_records:
            handle.write("    { %su, %du, %du, %du, %du },\n" % (cp, lower_length, title_length, upper_length, offset))
        handle.write("};\nconst std::uint32_t FullCaseCount = %du;\n\n" % len(full_case_records))

        handle.write("const MirrorPair Mirrors[] = {\n")
        for cp, mirror in mirrors:
            handle.write("    { %su, %su },\n" % (cp, mirror))
        handle.write("};\nconst std::uint32_t MirrorCount = %du;\n\n" % len(mirrors))

        handle.write("const Bracket Brackets[] = {\n")
        for cp, pair, is_open in sorted(brackets):
            handle.write("    { %su, %su, %du },\n" % (cp, pair, is_open))
        handle.write("};\nconst std::uint32_t BracketCount = %du;\n\n" % len(brackets))

        handle.write("const Composition Compositions[] = {\n")
        for first, second, composed in compositions:
            handle.write("    { %su, %su, %su },\n" % (first, second, composed))
        handle.write("};\nconst std::uint32_t CompositionCount = %du;\n\n" % len(compositions))

        handle.write("const char* const ScriptNames[] = {\n")
        for name in scripts:
            handle.write('    "%s",\n' % name)
        handle.write("};\nconst std::uint32_t ScriptNameCount = %du;\n\n" % len(scripts))

        for array_name, count_name, values in [
            ("GeneralCategoryNames", "GeneralCategoryNameCount", GENERAL_CATEGORY),
            ("BidiClassNames", "BidiClassNameCount", BIDI_CLASS),
            ("LineBreakNames", "LineBreakNameCount", LINE_BREAK),
        ]:
            handle.write("const char* const %s[] = {\n" % array_name)
            for name in values:
                handle.write('    "%s",\n' % name)
            handle.write("};\nconst std::uint32_t %s = %du;\n\n" % (count_name, len(values)))

        handle.write("}\n}\n")

    print("scripts:      %d" % len(scripts))
    print("ranges:       gc=%d ccc=%d bidi=%d script=%d lb=%d gcb=%d wb=%d sb=%d eaw=%d emoji=%d" % (
        len(general_category), len(combining), len(bidi), len(script_ranges), len(line_break),
        len(grapheme), len(word), len(sentence), len(width), len(emoji)))
    print("incb:         %d" % len(incb))
    print("decomps:      %d" % len(decomp_records))
    print("compositions: %d" % len(compositions))
    print("case:         simple=%d full=%d" % (len(simple_case), len(full_case_records)))
    print("wrote %s and %s" % (header, source))


if __name__ == "__main__":
    main()
