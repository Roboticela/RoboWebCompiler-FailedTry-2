// ---------------------------------------------------------------------------
// RoboHTMLCompiler - small helpers shared by the source code generators.
//
// Both targets emit ordinary C++ that a normal compiler optimises: nothing here
// is a template metaprogram, a code generator framework or a runtime library.
// ---------------------------------------------------------------------------
#pragma once

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

namespace RoboHTMLCompiler
{
    namespace CodeGeneration
    {
        /// "BasicApp" -> "BasicAppGenerated": a valid C++ namespace that cannot
        /// collide with the application's own names.
        inline std::string NamespaceNameFor(const std::string& applicationName)
        {
            std::string name = applicationName.empty() ? std::string("Application") : applicationName;

            std::string sanitized;
            sanitized.reserve(name.size());
            for (const char character : name)
            {
                if (std::isalnum(static_cast<unsigned char>(character)) != 0)
                {
                    sanitized.push_back(character);
                }
            }

            if (sanitized.empty())
            {
                sanitized = "Application";
            }

            if (std::isdigit(static_cast<unsigned char>(sanitized[0])) != 0)
            {
                sanitized.insert(sanitized.begin(), 'A');
            }

            return sanitized + "Generated";
        }

        /// A C++ string literal. Bytes above ASCII pass through unchanged: the
        /// generated files are UTF-8, exactly like the source documents.
        inline std::string StringLiteral(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');

            for (const char character : value)
            {
                switch (character)
                {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped.push_back(character); break;
                }
            }

            escaped.push_back('"');
            return escaped;
        }

        /// A float literal that always reads as a float, with enough digits to
        /// round-trip exactly.
        inline std::string FloatLiteral(float value)
        {
            std::ostringstream output;
            output << std::setprecision(9) << value;

            std::string text = output.str();
            if (text.find('.') == std::string::npos && text.find('e') == std::string::npos &&
                text.find("inf") == std::string::npos && text.find("nan") == std::string::npos)
            {
                text += ".0";
            }

            text += "f";
            return text;
        }
    }
}
