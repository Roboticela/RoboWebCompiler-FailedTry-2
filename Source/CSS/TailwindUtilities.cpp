// ---------------------------------------------------------------------------
// RoboHTMLCompiler - Tailwind utility expansion.
//
// Utility classes are expanded at build time into typed declarations. The
// native runtime therefore never interprets a class name: it only ever sees a
// resolved StyleId.
//
// The palette below is the Tailwind default palette for the colour families the
// compiler knows about. Extending it means adding rows here; the expansion
// algorithm itself is family agnostic.
// ---------------------------------------------------------------------------
#include "RoboHTMLCompiler/StyleResolver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <vector>

namespace RoboHTMLCompiler
{
    namespace
    {
        const char* const PaletteFamilies[] = {
            "slate", "gray", "red", "orange", "amber", "yellow", "lime", "green",
            "emerald", "teal", "cyan", "sky", "blue", "indigo", "violet", "purple",
            "fuchsia", "pink", "rose"
        };

        const char* const PaletteShades[] = {
            "50", "100", "200", "300", "400", "500", "600", "700", "800", "900", "950"
        };

        /// Default Tailwind colour ramp. Rows follow PaletteFamilies and columns
        /// follow PaletteShades.
        const char* const PaletteValues[][11] = {
            { "#f8fafc", "#f1f5f9", "#e2e8f0", "#cbd5e1", "#94a3b8", "#64748b", "#475569", "#334155", "#1e293b", "#0f172a", "#020617" },
            { "#f9fafb", "#f3f4f6", "#e5e7eb", "#d1d5db", "#9ca3af", "#6b7280", "#4b5563", "#374151", "#1f2937", "#111827", "#030712" },
            { "#fef2f2", "#fee2e2", "#fecaca", "#fca5a5", "#f87171", "#ef4444", "#dc2626", "#b91c1c", "#991b1b", "#7f1d1d", "#450a0a" },
            { "#fff7ed", "#ffedd5", "#fed7aa", "#fdba74", "#fb923c", "#f97316", "#ea580c", "#c2410c", "#9a3412", "#7c2d12", "#431407" },
            { "#fffbeb", "#fef3c7", "#fde68a", "#fcd34d", "#fbbf24", "#f59e0b", "#d97706", "#b45309", "#92400e", "#78350f", "#451a03" },
            { "#fefce8", "#fef9c3", "#fef08a", "#fde047", "#facc15", "#eab308", "#ca8a04", "#a16207", "#854d0e", "#713f12", "#422006" },
            { "#f7fee7", "#ecfccb", "#d9f99d", "#bef264", "#a3e635", "#84cc16", "#65a30d", "#4d7c0f", "#3f6212", "#365314", "#1a2e05" },
            { "#f0fdf4", "#dcfce7", "#bbf7d0", "#86efac", "#4ade80", "#22c55e", "#16a34a", "#15803d", "#166534", "#14532d", "#052e16" },
            { "#ecfdf5", "#d1fae5", "#a7f3d0", "#6ee7b7", "#34d399", "#10b981", "#059669", "#047857", "#065f46", "#064e3b", "#022c22" },
            { "#f0fdfa", "#ccfbf1", "#99f6e4", "#5eead4", "#2dd4bf", "#14b8a6", "#0d9488", "#0f766e", "#115e59", "#134e4a", "#042f2e" },
            { "#ecfeff", "#cffafe", "#a5f3fc", "#67e8f9", "#22d3ee", "#06b6d4", "#0891b2", "#0e7490", "#155e75", "#164e63", "#083344" },
            { "#f0f9ff", "#e0f2fe", "#bae6fd", "#7dd3fc", "#38bdf8", "#0ea5e9", "#0284c7", "#0369a1", "#075985", "#0c4a6e", "#082f49" },
            { "#eff6ff", "#dbeafe", "#bfdbfe", "#93c5fd", "#60a5fa", "#3b82f6", "#2563eb", "#1d4ed8", "#1e40af", "#1e3a8a", "#172554" },
            { "#eef2ff", "#e0e7ff", "#c7d2fe", "#a5b4fc", "#818cf8", "#6366f1", "#4f46e5", "#4338ca", "#3730a3", "#312e81", "#1e1b4b" },
            { "#f5f3ff", "#ede9fe", "#ddd6fe", "#c4b5fd", "#a78bfa", "#8b5cf6", "#7c3aed", "#6d28d9", "#5b21b6", "#4c1d95", "#2e1065" },
            { "#faf5ff", "#f3e8ff", "#e9d5ff", "#d8b4fe", "#c084fc", "#a855f7", "#9333ea", "#7e22ce", "#6b21a8", "#581c87", "#3b0764" },
            { "#fdf4ff", "#fae8ff", "#f5d0fe", "#f0abfc", "#e879f9", "#d946ef", "#c026d3", "#a21caf", "#86198f", "#701a75", "#4a044e" },
            { "#fdf2f8", "#fce7f3", "#fbcfe8", "#f9a8d4", "#f472b6", "#ec4899", "#db2777", "#be185d", "#9d174d", "#831843", "#500724" },
            { "#fff1f2", "#ffe4e6", "#fecdd3", "#fda4af", "#fb7185", "#f43f5e", "#e11d48", "#be123c", "#9f1239", "#881337", "#4c0519" }
        };

        const std::map<std::string, std::string>& SpacingScale()
        {
            static const std::map<std::string, std::string> scale = {
                { "0", "0px" },        { "px", "1px" },      { "0.5", "0.125rem" },
                { "1", "0.25rem" },    { "1.5", "0.375rem" },{ "2", "0.5rem" },
                { "2.5", "0.625rem" }, { "3", "0.75rem" },   { "3.5", "0.875rem" },
                { "4", "1rem" },       { "5", "1.25rem" },   { "6", "1.5rem" },
                { "7", "1.75rem" },    { "8", "2rem" },      { "9", "2.25rem" },
                { "10", "2.5rem" },    { "11", "2.75rem" },  { "12", "3rem" },
                { "14", "3.5rem" },    { "16", "4rem" },     { "20", "5rem" },
                { "24", "6rem" },      { "28", "7rem" },     { "32", "8rem" },
                { "36", "9rem" },      { "40", "10rem" },    { "44", "11rem" },
                { "48", "12rem" },     { "52", "13rem" },    { "56", "14rem" },
                { "60", "15rem" },     { "64", "16rem" },    { "72", "18rem" },
                { "80", "20rem" },     { "96", "24rem" }
            };
            return scale;
        }

        const std::map<std::string, std::string>& FontSizeScale()
        {
            static const std::map<std::string, std::string> scale = {
                { "xs", "0.75rem" },   { "sm", "0.875rem" },  { "base", "1rem" },
                { "lg", "1.125rem" },  { "xl", "1.25rem" },   { "2xl", "1.5rem" },
                { "3xl", "1.875rem" }, { "4xl", "2.25rem" },  { "5xl", "3rem" },
                { "6xl", "3.75rem" },  { "7xl", "4.5rem" },   { "8xl", "6rem" },
                { "9xl", "8rem" }
            };
            return scale;
        }

        const std::map<std::string, std::string>& FontWeightScale()
        {
            static const std::map<std::string, std::string> scale = {
                { "thin", "100" },       { "extralight", "200" }, { "light", "300" },
                { "normal", "400" },     { "medium", "500" },     { "semibold", "600" },
                { "bold", "700" },       { "extrabold", "800" },  { "black", "900" }
            };
            return scale;
        }

        const std::map<std::string, std::string>& RadiusScale()
        {
            static const std::map<std::string, std::string> scale = {
                { "none", "0px" },    { "sm", "0.125rem" }, { "DEFAULT", "0.25rem" },
                { "md", "0.375rem" }, { "lg", "0.5rem" },   { "xl", "0.75rem" },
                { "2xl", "1rem" },    { "3xl", "1.5rem" },  { "full", "9999px" }
            };
            return scale;
        }

        bool ResolveColor(const std::string& token, std::string& out)
        {
            if (token == "white") { out = "#ffffff"; return true; }
            if (token == "black") { out = "#000000"; return true; }

            const std::size_t dash = token.rfind('-');
            if (dash == std::string::npos)
            {
                return false;
            }

            const std::string family = token.substr(0, dash);
            const std::string shade = token.substr(dash + 1);

            for (std::size_t familyIndex = 0;
                 familyIndex < sizeof(PaletteFamilies) / sizeof(PaletteFamilies[0]);
                 ++familyIndex)
            {
                if (family != PaletteFamilies[familyIndex])
                {
                    continue;
                }

                for (std::size_t shadeIndex = 0;
                     shadeIndex < sizeof(PaletteShades) / sizeof(PaletteShades[0]);
                     ++shadeIndex)
                {
                    if (shade == PaletteShades[shadeIndex])
                    {
                        out = PaletteValues[familyIndex][shadeIndex];
                        return true;
                    }
                }
            }

            return false;
        }

        bool ResolveSpacing(const std::string& token, std::string& out)
        {
            if (token == "auto") { out = "auto"; return true; }
            if (token == "full") { out = "100%"; return true; }
            if (token == "screen") { out = "100vh"; return true; }
            if (token == "min") { out = "min-content"; return true; }
            if (token == "max") { out = "max-content"; return true; }
            if (token == "fit") { out = "fit-content"; return true; }
            if (token == "1/2") { out = "50%"; return true; }
            if (token == "1/3") { out = "33.333333%"; return true; }
            if (token == "2/3") { out = "66.666667%"; return true; }
            if (token == "1/4") { out = "25%"; return true; }
            if (token == "3/4") { out = "75%"; return true; }

            const auto iterator = SpacingScale().find(token);
            if (iterator == SpacingScale().end())
            {
                return false;
            }

            out = iterator->second;
            return true;
        }

        void Add(std::vector<StyleDeclaration>& out, const std::string& property, const std::string& value)
        {
            out.push_back(StyleDeclaration{ property, value });
        }

        bool ExpandSpacingFamily(const std::string& className,
                                 const std::string& prefix,
                                 const std::vector<std::string>& properties,
                                 std::vector<StyleDeclaration>& out)
        {
            if (className.compare(0, prefix.size(), prefix) != 0)
            {
                return false;
            }

            const std::string token = className.substr(prefix.size());
            std::string value;
            if (!ResolveSpacing(token, value))
            {
                return false;
            }

            for (const std::string& property : properties)
            {
                Add(out, property, value);
            }
            return true;
        }

        bool ExpandColorFamily(const std::string& className,
                               const std::string& prefix,
                               const std::string& property,
                               std::vector<StyleDeclaration>& out)
        {
            if (className.compare(0, prefix.size(), prefix) != 0)
            {
                return false;
            }

            const std::string token = className.substr(prefix.size());
            std::string value;
            if (!ResolveColor(token, value))
            {
                return false;
            }

            Add(out, property, value);
            return true;
        }
    }

    bool StyleSheetCompiler::ExpandTailwindUtility(const std::string& className,
                                                   std::vector<StyleDeclaration>& out)
    {
        if (className.empty())
        {
            return false;
        }

        out.clear();

        // --- Display -------------------------------------------------------
        static const std::map<std::string, std::string> displayUtilities = {
            { "block", "block" },           { "inline-block", "inline-block" },
            { "inline", "inline" },         { "flex", "flex" },
            { "inline-flex", "inline-flex" },{ "grid", "grid" },
            { "inline-grid", "inline-grid" },{ "table", "table" },
            { "contents", "contents" },     { "hidden", "none" }
        };
        if (const auto iterator = displayUtilities.find(className); iterator != displayUtilities.end())
        {
            Add(out, "display", iterator->second);
            return true;
        }

        // --- Position ------------------------------------------------------
        static const std::map<std::string, std::string> positionUtilities = {
            { "static", "static" },     { "fixed", "fixed" },       { "absolute", "absolute" },
            { "relative", "relative" }, { "sticky", "sticky" }
        };
        if (const auto iterator = positionUtilities.find(className); iterator != positionUtilities.end())
        {
            Add(out, "position", iterator->second);
            return true;
        }

        // --- Flexbox and grid ----------------------------------------------
        static const std::map<std::string, std::vector<StyleDeclaration>> keywordUtilities = {
            { "flex-row", { { "flex-direction", "row" } } },
            { "flex-row-reverse", { { "flex-direction", "row-reverse" } } },
            { "flex-col", { { "flex-direction", "column" } } },
            { "flex-col-reverse", { { "flex-direction", "column-reverse" } } },
            { "flex-wrap", { { "flex-wrap", "wrap" } } },
            { "flex-wrap-reverse", { { "flex-wrap", "wrap-reverse" } } },
            { "flex-nowrap", { { "flex-wrap", "nowrap" } } },
            { "flex-1", { { "flex", "1 1 0%" } } },
            { "flex-auto", { { "flex", "1 1 auto" } } },
            { "flex-initial", { { "flex", "0 1 auto" } } },
            { "flex-none", { { "flex", "none" } } },
            { "grow", { { "flex-grow", "1" } } },
            { "grow-0", { { "flex-grow", "0" } } },
            { "shrink", { { "flex-shrink", "1" } } },
            { "shrink-0", { { "flex-shrink", "0" } } },
            { "items-start", { { "align-items", "flex-start" } } },
            { "items-end", { { "align-items", "flex-end" } } },
            { "items-center", { { "align-items", "center" } } },
            { "items-baseline", { { "align-items", "baseline" } } },
            { "items-stretch", { { "align-items", "stretch" } } },
            { "justify-start", { { "justify-content", "flex-start" } } },
            { "justify-end", { { "justify-content", "flex-end" } } },
            { "justify-center", { { "justify-content", "center" } } },
            { "justify-between", { { "justify-content", "space-between" } } },
            { "justify-around", { { "justify-content", "space-around" } } },
            { "justify-evenly", { { "justify-content", "space-evenly" } } },
            { "self-auto", { { "align-self", "auto" } } },
            { "self-center", { { "align-self", "center" } } },
            { "self-stretch", { { "align-self", "stretch" } } },
            { "grid-cols-1", { { "grid-template-columns", "repeat(1, minmax(0, 1fr))" } } },
            { "grid-cols-2", { { "grid-template-columns", "repeat(2, minmax(0, 1fr))" } } },
            { "grid-cols-3", { { "grid-template-columns", "repeat(3, minmax(0, 1fr))" } } },
            { "grid-cols-4", { { "grid-template-columns", "repeat(4, minmax(0, 1fr))" } } },
            { "grid-cols-6", { { "grid-template-columns", "repeat(6, minmax(0, 1fr))" } } },
            { "grid-cols-12", { { "grid-template-columns", "repeat(12, minmax(0, 1fr))" } } },
            { "col-span-2", { { "grid-column", "span 2 / span 2" } } },
            { "col-span-3", { { "grid-column", "span 3 / span 3" } } },
            { "relative-position", { { "position", "relative" } } },
            { "overflow-hidden", { { "overflow", "hidden" } } },
            { "overflow-auto", { { "overflow", "auto" } } },
            { "overflow-scroll", { { "overflow", "scroll" } } },
            { "overflow-visible", { { "overflow", "visible" } } },
            { "truncate", { { "overflow", "hidden" }, { "text-overflow", "ellipsis" }, { "white-space", "nowrap" } } },
            { "uppercase", { { "text-transform", "uppercase" } } },
            { "lowercase", { { "text-transform", "lowercase" } } },
            { "capitalize", { { "text-transform", "capitalize" } } },
            { "underline", { { "text-decoration-line", "underline" } } },
            { "italic", { { "font-style", "italic" } } },
            { "not-italic", { { "font-style", "normal" } } },
            { "text-left", { { "text-align", "left" } } },
            { "text-center", { { "text-align", "center" } } },
            { "text-right", { { "text-align", "right" } } },
            { "text-justify", { { "text-align", "justify" } } },
            { "select-none", { { "user-select", "none" } } },
            { "select-text", { { "user-select", "text" } } },
            { "pointer-events-none", { { "pointer-events", "none" } } },
            { "cursor-pointer", { { "cursor", "pointer" } } },
            { "cursor-default", { { "cursor", "default" } } },
            { "border", { { "border-width", "1px" }, { "border-style", "solid" } } },
            { "border-0", { { "border-width", "0px" } } },
            { "border-2", { { "border-width", "2px" }, { "border-style", "solid" } } },
            { "border-4", { { "border-width", "4px" }, { "border-style", "solid" } } },
            { "border-8", { { "border-width", "8px" }, { "border-style", "solid" } } },
            { "shadow", { { "box-shadow", "0 1px 3px 0 rgb(0 0 0 / 0.1), 0 1px 2px -1px rgb(0 0 0 / 0.1)" } } },
            { "shadow-sm", { { "box-shadow", "0 1px 2px 0 rgb(0 0 0 / 0.05)" } } },
            { "shadow-md", { { "box-shadow", "0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1)" } } },
            { "shadow-lg", { { "box-shadow", "0 10px 15px -3px rgb(0 0 0 / 0.1), 0 4px 6px -4px rgb(0 0 0 / 0.1)" } } },
            { "shadow-none", { { "box-shadow", "none" } } },
            { "font-sans", { { "font-family", "ui-sans-serif, system-ui, sans-serif" } } },
            { "font-serif", { { "font-family", "ui-serif, Georgia, serif" } } },
            { "font-mono", { { "font-family", "ui-monospace, SFMono-Regular, monospace" } } },
            { "transition", { { "transition-property", "color, background-color, border-color, opacity, box-shadow, transform" },
                              { "transition-duration", "150ms" },
                              { "transition-timing-function", "cubic-bezier(0.4, 0, 0.2, 1)" } } },
            { "transition-colors", { { "transition-property", "color, background-color, border-color" },
                                     { "transition-duration", "150ms" } } },
            { "ease-in-out", { { "transition-timing-function", "cubic-bezier(0.4, 0, 0.2, 1)" } } },
            { "ease-in", { { "transition-timing-function", "cubic-bezier(0.4, 0, 1, 1)" } } },
            { "ease-out", { { "transition-timing-function", "cubic-bezier(0, 0, 0.2, 1)" } } },
            { "ease-linear", { { "transition-timing-function", "linear" } } },
            { "sr-only", { { "position", "absolute" },
                           { "width", "1px" }, { "height", "1px" },
                           { "padding", "0" }, { "margin", "-1px" },
                           { "overflow", "hidden" }, { "clip", "rect(0, 0, 0, 0)" },
                           { "white-space", "nowrap" }, { "border-width", "0" } } },
            { "antialiased", { { "-webkit-font-smoothing", "antialiased" },
                               { "-moz-osx-font-smoothing", "grayscale" } } }
        };
        if (const auto iterator = keywordUtilities.find(className); iterator != keywordUtilities.end())
        {
            out = iterator->second;
            return true;
        }

        // --- Spacing families ----------------------------------------------
        // Padding and margin shorthands expand to their four longhands so that
        // an authored shorthand rule can still be layered over the utility
        // without silently discarding the other sides.
        if (ExpandSpacingFamily(className, "p-",
                                { "padding-top", "padding-right", "padding-bottom", "padding-left" }, out)) return true;
        if (ExpandSpacingFamily(className, "px-", { "padding-left", "padding-right" }, out)) return true;
        if (ExpandSpacingFamily(className, "py-", { "padding-top", "padding-bottom" }, out)) return true;
        if (ExpandSpacingFamily(className, "pt-", { "padding-top" }, out)) return true;
        if (ExpandSpacingFamily(className, "pr-", { "padding-right" }, out)) return true;
        if (ExpandSpacingFamily(className, "pb-", { "padding-bottom" }, out)) return true;
        if (ExpandSpacingFamily(className, "pl-", { "padding-left" }, out)) return true;
        if (ExpandSpacingFamily(className, "m-",
                                { "margin-top", "margin-right", "margin-bottom", "margin-left" }, out)) return true;
        if (ExpandSpacingFamily(className, "mx-", { "margin-left", "margin-right" }, out)) return true;
        if (ExpandSpacingFamily(className, "my-", { "margin-top", "margin-bottom" }, out)) return true;
        if (ExpandSpacingFamily(className, "mt-", { "margin-top" }, out)) return true;
        if (ExpandSpacingFamily(className, "mr-", { "margin-right" }, out)) return true;
        if (ExpandSpacingFamily(className, "mb-", { "margin-bottom" }, out)) return true;
        if (ExpandSpacingFamily(className, "ml-", { "margin-left" }, out)) return true;
        if (ExpandSpacingFamily(className, "gap-", { "gap" }, out)) return true;
        if (ExpandSpacingFamily(className, "gap-x-", { "column-gap" }, out)) return true;
        if (ExpandSpacingFamily(className, "gap-y-", { "row-gap" }, out)) return true;

        // Width and height use the spacing ramp plus sizing keywords.
        if (className.compare(0, 2, "w-") == 0)
        {
            std::string value;
            const std::string token = className.substr(2);
            if (ResolveSpacing(token, value))
            {
                Add(out, "width", token == "screen" ? "100vw" : value);
                return true;
            }
            if (const auto iterator = SpacingScale().find(token); iterator != SpacingScale().end())
            {
                Add(out, "width", iterator->second);
                return true;
            }
        }
        if (className.compare(0, 2, "h-") == 0)
        {
            std::string value;
            const std::string token = className.substr(2);
            if (ResolveSpacing(token, value))
            {
                Add(out, "height", value);
                return true;
            }
            if (const auto iterator = SpacingScale().find(token); iterator != SpacingScale().end())
            {
                Add(out, "height", iterator->second);
                return true;
            }
        }
        if (className == "min-w-full") { Add(out, "min-width", "100%"); return true; }
        if (className == "max-w-full") { Add(out, "max-width", "100%"); return true; }

        // --- Colours -------------------------------------------------------
        if (ExpandColorFamily(className, "text-", "color", out)) return true;
        if (ExpandColorFamily(className, "bg-", "background-color", out)) return true;
        if (ExpandColorFamily(className, "border-", "border-color", out)) return true;

        // --- Typography ----------------------------------------------------
        if (className.compare(0, 5, "text-") == 0)
        {
            const std::string token = className.substr(5);
            if (const auto iterator = FontSizeScale().find(token); iterator != FontSizeScale().end())
            {
                Add(out, "font-size", iterator->second);
                return true;
            }
        }
        if (className.compare(0, 5, "font-") == 0)
        {
            const std::string token = className.substr(5);
            if (const auto iterator = FontWeightScale().find(token); iterator != FontWeightScale().end())
            {
                Add(out, "font-weight", iterator->second);
                return true;
            }
        }
        if (className.compare(0, 8, "leading-") == 0)
        {
            std::string value;
            if (ResolveSpacing(className.substr(8), value))
            {
                Add(out, "line-height", value);
                return true;
            }
        }
        if (className.compare(0, 9, "tracking-") == 0)
        {
            static const std::map<std::string, std::string> tracking = {
                { "tighter", "-0.05em" }, { "tight", "-0.025em" }, { "normal", "0em" },
                { "wide", "0.025em" },    { "wider", "0.05em" },   { "widest", "0.1em" }
            };
            const auto iterator = tracking.find(className.substr(9));
            if (iterator != tracking.end())
            {
                Add(out, "letter-spacing", iterator->second);
                return true;
            }
        }

        // --- Rounded corners ------------------------------------------------
        if (className == "rounded")
        {
            Add(out, "border-radius", RadiusScale().at("DEFAULT"));
            return true;
        }
        if (className.compare(0, 8, "rounded-") == 0)
        {
            const std::string token = className.substr(8);

            static const std::map<std::string, std::string> sides = {
                { "t", "border-top-left-radius|border-top-right-radius" },
                { "r", "border-top-right-radius|border-bottom-right-radius" },
                { "b", "border-bottom-left-radius|border-bottom-right-radius" },
                { "l", "border-top-left-radius|border-bottom-left-radius" },
                { "tl", "border-top-left-radius" },
                { "tr", "border-top-right-radius" },
                { "bl", "border-bottom-left-radius" },
                { "br", "border-bottom-right-radius" }
            };

            for (const auto& side : sides)
            {
                if (token.compare(0, side.first.size(), side.first) != 0)
                {
                    continue;
                }

                std::string remainder = token.substr(side.first.size());

                // 'rounded-lg' must not be read as the 'l' (left) side followed
                // by the scale key 'g': a side token is only a match when the
                // remainder is empty or introduces a scale key of its own.
                if (!remainder.empty() && remainder[0] != '-')
                {
                    continue;
                }

                if (!remainder.empty())
                {
                    remainder = remainder.substr(1);
                }

                const std::string radiusKey = remainder.empty() ? "DEFAULT" : remainder;
                const auto radius = RadiusScale().find(radiusKey);
                if (radius == RadiusScale().end())
                {
                    return false;
                }

                std::size_t position = 0;
                while (position <= side.second.size())
                {
                    const std::size_t terminator = side.second.find('|', position);
                    const std::string property =
                        side.second.substr(position, (terminator == std::string::npos ? side.second.size()
                                                                                      : terminator) - position);
                    if (!property.empty())
                    {
                        Add(out, property, radius->second);
                    }
                    if (terminator == std::string::npos)
                    {
                        break;
                    }
                    position = terminator + 1;
                }
                return true;
            }

            const auto radius = RadiusScale().find(token);
            if (radius == RadiusScale().end())
            {
                return false;
            }
            Add(out, "border-radius", radius->second);
            return true;
        }

        // --- Numeric families ------------------------------------------------
        if (className.compare(0, 8, "opacity-") == 0)
        {
            const std::string token = className.substr(8);
            char* end = nullptr;
            const long percent = std::strtol(token.c_str(), &end, 10);
            if (end != nullptr && *end == '\0' && percent >= 0 && percent <= 100)
            {
                std::ostringstream value;
                value << (static_cast<double>(percent) / 100.0);
                Add(out, "opacity", value.str());
                return true;
            }
        }
        if (className.compare(0, 9, "duration-") == 0)
        {
            const std::string token = className.substr(9);
            char* end = nullptr;
            const long milliseconds = std::strtol(token.c_str(), &end, 10);
            if (end != nullptr && *end == '\0' && milliseconds >= 0)
            {
                Add(out, "transition-duration", std::to_string(milliseconds) + "ms");
                return true;
            }
        }
        if (className.compare(0, 2, "z-") == 0)
        {
            const std::string token = className.substr(2);
            static const std::map<std::string, std::string> zIndex = {
                { "0", "0" }, { "10", "10" }, { "20", "20" }, { "30", "30" },
                { "40", "40" }, { "50", "50" }, { "auto", "auto" }
            };
            const auto iterator = zIndex.find(token);
            if (iterator != zIndex.end())
            {
                Add(out, "z-index", iterator->second);
                return true;
            }
        }

        out.clear();
        return false;
    }
}
