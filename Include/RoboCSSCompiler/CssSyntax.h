// ---------------------------------------------------------------------------
// RoboCSSCompiler - CSS tokenizer.
//
//     characters  ->  CSSTokenizer  ->  CSSToken[]
//
// This is CSS Syntax Level 3's tokenizer, not an approximation of it: numeric
// forms with signs and exponents, dimensions with units, hash tokens that know
// whether they are identifiers, strings and urls with escapes, comments, and the
// CDO/CDC markers a stylesheet can legally contain between rules.
//
// Tokens are a build-time product. Nothing here runs at runtime, and nothing
// here allocates per token beyond the token's own text: a stylesheet is read
// once, by the compiler, and after that it exists only as typed data.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssDiagnostics.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    enum class CSSTokenType : std::uint8_t
    {
        EndOfFile = 0,
        Ident,
        /// `name(` - the parenthesis is part of the token.
        Function,
        AtKeyword,
        /// `#name` - `hashIsId` says whether the name is a valid identifier.
        Hash,
        /// A quoted string, already unescaped.
        QuotedString,
        BadString,
        /// `url(...)`, already unescaped.
        Url,
        BadUrl,
        /// A single character that has no more specific token type.
        Delim,
        Number,
        Percentage,
        Dimension,
        Whitespace,
        Colon,
        Semicolon,
        Comma,
        LeftSquare,
        RightSquare,
        LeftParen,
        RightParen,
        LeftCurly,
        RightCurly,
        /// `<!--`
        Cdo,
        /// `-->`
        Cdc
    };

    const char* ToString(CSSTokenType type);

    struct CSSToken
    {
        CSSTokenType type = CSSTokenType::EndOfFile;
        CSSSourceLocation location;

        /// Decoded text: the name of an ident/function/at-keyword/hash, the value
        /// of a string or url, the character of a delim.
        std::string text;

        /// Numeric value of a Number, Percentage (as written, so 50 for `50%`) or
        /// Dimension token.
        double number = 0.0;
        bool isInteger = false;

        /// Unit of a Dimension token, lower-cased, with escapes decoded.
        std::string unit;

        /// Hash tokens only: true when the name is a valid identifier, which is
        /// what makes `#card` an id selector and `#1a2b3c` a color.
        bool hashIsId = false;

        /// The spelling as it appeared in the source, for diagnostics and for
        /// re-serialisation. Empty for whitespace and end of file.
        std::string raw;

        bool Is(CSSTokenType other) const { return type == other; }
        bool IsDelim(char character) const
        {
            return type == CSSTokenType::Delim && text.size() == 1 && text[0] == character;
        }
        /// True for an Ident token whose name equals `name`, case-insensitively.
        bool IsIdent(const char* name) const;
    };

    /// Tokenizes one stylesheet. The whole text is tokenized up front: it is a
    /// build-time input, and a token stream that cannot seek backwards is what
    /// the parser wants.
    class CSSTokenizer
    {
    public:
        /// `diagnostics` may be null; it receives the unterminated-comment,
        /// unterminated-string and bad-url reports the algorithm calls parse
        /// errors.
        CSSTokenizer(std::string text, std::string fileName, CSSDiagnosticSink* diagnostics = nullptr);

        /// Every token, including whitespace, ending with EndOfFile.
        std::vector<CSSToken> Tokenize();

        const std::string& FileName() const { return fileName; }
        const std::string& Text() const { return source; }

    private:
        bool AtEnd() const { return position >= source.size(); }
        char Peek(std::size_t ahead = 0) const;
        char Advance();
        bool WouldStartIdent(std::size_t offset) const;
        bool WouldStartNumber(std::size_t offset) const;
        static bool IsNameChar(char character);
        static bool IsNonAscii(char character);

        /// Consumes an escape and returns the code point it denotes.
        std::uint32_t ConsumeEscape();
        /// Consumes a name (an ident sequence), decoding escapes.
        std::string ConsumeName();
        /// Consumes a number, percentage or dimension token.
        CSSToken ConsumeNumeric();
        /// Consumes a quoted string starting at the opening quote.
        CSSToken ConsumeString(char quote);
        /// Consumes `url(...)`; the `url(` prefix is already consumed.
        CSSToken ConsumeUrl(const CSSSourceLocation& location, const std::string& rawPrefix);
        /// Consumes an unquoted url body. False when the body cannot be one.
        bool ConsumeUrlBody(std::string& out);
        /// Consumes a comment; the `/*` is already consumed.
        void ConsumeComment();

        void NewLine();
        void SetLocation(CSSSourceLocation& location) const;

        std::string source;
        std::string fileName;
        std::size_t position = 0;
        std::uint32_t line = 1;
        std::uint32_t lineStart = 0;

        CSSDiagnosticSink* diagnostics = nullptr;
    };
}
