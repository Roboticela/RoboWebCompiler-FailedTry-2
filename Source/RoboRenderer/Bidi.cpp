#include "RoboRenderer/Bidi.h"

namespace RoboRenderer
{
    namespace
    {
        bool InRange(std::uint32_t codepoint, std::uint32_t low, std::uint32_t high)
        {
            return codepoint >= low && codepoint <= high;
        }

        /// Arabic joining type, the property the shaping decision reads.
        enum class Joining
        {
            None,        // non-joining
            Right,       // joins only to the letter on its right (the previous)
            Dual,        // joins on both sides
            Transparent, // combining marks: skipped when looking for a neighbour
        };

        struct ArabicForm
        {
            std::uint32_t base;
            std::uint32_t isolated;
            std::uint32_t final;
            std::uint32_t initial;
            std::uint32_t medial;
            Joining joining;
        };

        /// The Arabic block's letters and their presentation forms. The order of
        /// the four forms is the order Unicode stores them in: isolated, final,
        /// initial, medial.
        const ArabicForm ArabicForms[] = {
            { 0x0621, 0xFE80, 0xFE80, 0x0000, 0x0000, Joining::None },
            { 0x0622, 0xFE81, 0xFE82, 0x0000, 0x0000, Joining::Right },
            { 0x0623, 0xFE83, 0xFE84, 0x0000, 0x0000, Joining::Right },
            { 0x0624, 0xFE85, 0xFE86, 0x0000, 0x0000, Joining::Right },
            { 0x0625, 0xFE87, 0xFE88, 0x0000, 0x0000, Joining::Right },
            { 0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C, Joining::Dual },
            { 0x0627, 0xFE8D, 0xFE8E, 0x0000, 0x0000, Joining::Right },
            { 0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92, Joining::Dual },
            { 0x0629, 0xFE93, 0xFE94, 0x0000, 0x0000, Joining::Right },
            { 0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98, Joining::Dual },
            { 0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C, Joining::Dual },
            { 0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0, Joining::Dual },
            { 0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4, Joining::Dual },
            { 0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8, Joining::Dual },
            { 0x062F, 0xFEA9, 0xFEAA, 0x0000, 0x0000, Joining::Right },
            { 0x0630, 0xFEAB, 0xFEAC, 0x0000, 0x0000, Joining::Right },
            { 0x0631, 0xFEAD, 0xFEAE, 0x0000, 0x0000, Joining::Right },
            { 0x0632, 0xFEAF, 0xFEB0, 0x0000, 0x0000, Joining::Right },
            { 0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4, Joining::Dual },
            { 0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8, Joining::Dual },
            { 0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC, Joining::Dual },
            { 0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0, Joining::Dual },
            { 0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4, Joining::Dual },
            { 0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8, Joining::Dual },
            { 0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC, Joining::Dual },
            { 0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0, Joining::Dual },
            { 0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4, Joining::Dual },
            { 0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8, Joining::Dual },
            { 0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC, Joining::Dual },
            { 0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0, Joining::Dual },
            { 0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4, Joining::Dual },
            { 0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8, Joining::Dual },
            { 0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC, Joining::Dual },
            { 0x0648, 0xFEED, 0xFEEE, 0x0000, 0x0000, Joining::Right },
            { 0x0649, 0xFEEF, 0xFEF0, 0x0000, 0x0000, Joining::Right },
            { 0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4, Joining::Dual },
        };

        bool IsArabicTransparent(std::uint32_t codepoint)
        {
            return InRange(codepoint, 0x0610, 0x061A) || InRange(codepoint, 0x064B, 0x065F) ||
                   codepoint == 0x0670 || InRange(codepoint, 0x06D6, 0x06DC) ||
                   InRange(codepoint, 0x06DF, 0x06E4) || InRange(codepoint, 0x06E7, 0x06E8) ||
                   InRange(codepoint, 0x06EA, 0x06ED);
        }

        const ArabicForm* FindArabicForm(std::uint32_t codepoint)
        {
            for (const ArabicForm& form : ArabicForms)
            {
                if (form.base == codepoint)
                {
                    return &form;
                }
            }
            return nullptr;
        }

        Joining JoiningOf(std::uint32_t codepoint)
        {
            if (IsArabicTransparent(codepoint))
            {
                return Joining::Transparent;
            }

            const ArabicForm* form = FindArabicForm(codepoint);
            return form == nullptr ? Joining::None : form->joining;
        }

        bool JoinsToLeft(Joining joining)
        {
            return joining == Joining::Dual;
        }

        bool JoinsToRight(Joining joining)
        {
            return joining == Joining::Dual || joining == Joining::Right;
        }
    }

    bool IsRtlCodepoint(std::uint32_t codepoint)
    {
        return InRange(codepoint, 0x0590, 0x05FF) || InRange(codepoint, 0x0600, 0x06FF) ||
               InRange(codepoint, 0x0700, 0x074F) || InRange(codepoint, 0x0750, 0x077F) ||
               InRange(codepoint, 0x0780, 0x07BF) || InRange(codepoint, 0x07C0, 0x07FF) ||
               InRange(codepoint, 0x0800, 0x083F) || InRange(codepoint, 0x0840, 0x085F) ||
               InRange(codepoint, 0x0860, 0x086F) || InRange(codepoint, 0x08A0, 0x08FF) ||
               InRange(codepoint, 0xFB1D, 0xFB4F) || InRange(codepoint, 0xFD50, 0xFDFF) ||
               InRange(codepoint, 0xFE70, 0xFEFF) || InRange(codepoint, 0x10800, 0x10FFF) ||
               InRange(codepoint, 0x1E800, 0x1EFFF);
    }

    bool IsLtrCodepoint(std::uint32_t codepoint)
    {
        if (InRange(codepoint, '0', '9') || InRange(codepoint, 'A', 'Z') || InRange(codepoint, 'a', 'z'))
        {
            return true;
        }

        // Latin-1 letters, Greek, Cyrillic, Armenian, Georgian and the general
        // Latin and Greek extended blocks.
        return InRange(codepoint, 0x00C0, 0x024F) || InRange(codepoint, 0x0370, 0x03FF) ||
               InRange(codepoint, 0x0400, 0x04FF) || InRange(codepoint, 0x0530, 0x058F) ||
               InRange(codepoint, 0x10A0, 0x10FF) || InRange(codepoint, 0x1E00, 0x1EFF);
    }

    bool IsCjkCodepoint(std::uint32_t codepoint)
    {
        return InRange(codepoint, 0x2E80, 0x2EFF) || InRange(codepoint, 0x3000, 0x303F) ||
               InRange(codepoint, 0x3040, 0x309F) || InRange(codepoint, 0x30A0, 0x30FF) ||
               InRange(codepoint, 0x3400, 0x4DBF) || InRange(codepoint, 0x4E00, 0x9FFF) ||
               InRange(codepoint, 0xAC00, 0xD7AF) || InRange(codepoint, 0xF900, 0xFAFF) ||
               InRange(codepoint, 0x20000, 0x2FA1F);
    }

    bool IsRegionalIndicator(std::uint32_t codepoint)
    {
        return InRange(codepoint, 0x1F1E6, 0x1F1FF);
    }

    bool IsClusterExtender(std::uint32_t codepoint)
    {
        if (codepoint == 0x200D || codepoint == 0x20E3)
        {
            return true; // ZWJ, keycap
        }

        if (InRange(codepoint, 0xFE00, 0xFE0F) || InRange(codepoint, 0xE0020, 0xE007F))
        {
            return true; // variation selectors, tag characters
        }

        if (InRange(codepoint, 0x1F3FB, 0x1F3FF))
        {
            return true; // skin-tone modifiers
        }

        // Combining marks across the scripts the showcase covers. A miss here
        // costs a line break inside a cluster, not a wrong glyph.
        return InRange(codepoint, 0x0300, 0x036F) || InRange(codepoint, 0x0483, 0x0489) ||
               InRange(codepoint, 0x0591, 0x05C7) || InRange(codepoint, 0x0610, 0x061A) ||
               InRange(codepoint, 0x064B, 0x065F) || codepoint == 0x0670 ||
               InRange(codepoint, 0x06D6, 0x06ED) || codepoint == 0x0711 ||
               InRange(codepoint, 0x0730, 0x074A) || InRange(codepoint, 0x07A6, 0x07B0) ||
               InRange(codepoint, 0x0900, 0x0903) || InRange(codepoint, 0x093A, 0x094F) ||
               InRange(codepoint, 0x0951, 0x0957) || InRange(codepoint, 0x0962, 0x0963) ||
               InRange(codepoint, 0x0981, 0x0983) || InRange(codepoint, 0x09BC, 0x09CD) ||
               InRange(codepoint, 0x0A01, 0x0A03) || InRange(codepoint, 0x0A3C, 0x0A4D) ||
               InRange(codepoint, 0x0B01, 0x0B03) || InRange(codepoint, 0x0B3C, 0x0B4D) ||
               InRange(codepoint, 0x0C00, 0x0C04) || InRange(codepoint, 0x0C3E, 0x0C56) ||
               InRange(codepoint, 0x0D00, 0x0D03) || InRange(codepoint, 0x0D3B, 0x0D4D) ||
               codepoint == 0x0E31 || InRange(codepoint, 0x0E34, 0x0E3A) ||
               InRange(codepoint, 0x0E47, 0x0E4E) || codepoint == 0x0EB1 ||
               InRange(codepoint, 0x0EB4, 0x0EBC) || InRange(codepoint, 0x0EC8, 0x0ECD) ||
               InRange(codepoint, 0x0F18, 0x0F19) || InRange(codepoint, 0x0F35, 0x0F39) ||
               InRange(codepoint, 0x1AB0, 0x1AFF) || InRange(codepoint, 0x1DC0, 0x1DFF) ||
               InRange(codepoint, 0x20D0, 0x20FF) || InRange(codepoint, 0xFE20, 0xFE2F);
    }

    std::uint32_t ArabicShapedCodepoint(std::uint32_t previous,
                                        std::uint32_t codepoint,
                                        std::uint32_t next)
    {
        const ArabicForm* form = FindArabicForm(codepoint);
        if (form == nullptr || form->joining == Joining::None)
        {
            return codepoint;
        }

        const Joining previousJoining = JoiningOf(previous);
        const Joining nextJoining = JoiningOf(next);

        const bool joinsRight = JoinsToRight(form->joining) && JoinsToLeft(previousJoining);
        const bool joinsLeft = JoinsToLeft(form->joining) && JoinsToRight(nextJoining);

        std::uint32_t shaped = form->isolated;

        if (joinsRight && joinsLeft && form->medial != 0)
        {
            shaped = form->medial;
        }
        else if (joinsRight && form->final != 0)
        {
            shaped = form->final;
        }
        else if (joinsLeft && form->initial != 0)
        {
            shaped = form->initial;
        }

        return shaped == 0 ? codepoint : shaped;
    }
}
