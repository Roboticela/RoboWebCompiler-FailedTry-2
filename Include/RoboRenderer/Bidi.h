// ---------------------------------------------------------------------------
// RoboRenderer - bidirectional text and Arabic joining.
//
// Two questions a left-to-right renderer cannot avoid once it claims to draw
// every language:
//
//   Which way does this run go?   -- resolved here as "the first strong
//                                    character decides", which is the part of
//                                    the Unicode bidi algorithm a pure run
//                                    actually needs.
//   Which shape is this letter?   -- Arabic letters have isolated, initial,
//                                    medial and final forms that depend on
//                                    their neighbours. This maps a letter to
//                                    the right form without a shaping engine.
//
// The full UBA and OpenType Layout are larger than this runtime needs to be
// honest about a multilingual document; what is here is the part that decides
// the pixels of the showcase, and it never guesses: a letter whose contextual
// form a face does not carry keeps its base form.
// ---------------------------------------------------------------------------
#pragma once

#include <cstdint>

namespace RoboRenderer
{
    /// True for a code point in one of the right-to-left scripts.
    bool IsRtlCodepoint(std::uint32_t codepoint);

    /// True for a strong left-to-right character (a Latin, Greek or Cyrillic
    /// letter, or a digit).
    bool IsLtrCodepoint(std::uint32_t codepoint);

    /// True for a CJK ideograph, kana or Hangul syllable: text that breaks
    /// between characters rather than at spaces.
    bool IsCjkCodepoint(std::uint32_t codepoint);

    /// True for a code point that never starts a new grapheme cluster: combining
    /// marks, variation selectors, the zero-width joiner, skin-tone modifiers,
    /// the keycap mark and tag characters. Line breaking must not split here.
    bool IsClusterExtender(std::uint32_t codepoint);

    /// True for a regional-indicator code point, two of which form a flag.
    bool IsRegionalIndicator(std::uint32_t codepoint);

    /// The contextual presentation form of an Arabic letter, given its
    /// neighbours in logical order. Returns `codepoint` unchanged when the
    /// letter is not Arabic, has no joining context, or the form is not named.
    std::uint32_t ArabicShapedCodepoint(std::uint32_t previous,
                                        std::uint32_t codepoint,
                                        std::uint32_t next);
}
