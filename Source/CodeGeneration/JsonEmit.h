// ---------------------------------------------------------------------------
// RoboHTMLCompiler - JSON emission helpers.
//
// The compiler only ever writes JSON (reports, IR dumps); it never parses it,
// so this is a small set of streaming helpers rather than a document model.
// ---------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace RoboHTMLCompiler::CodeGeneration
{
    inline std::string EscapeJson(const std::string& value)
    {
        std::string escaped;
        escaped.reserve(value.size() + 8);

        for (const char character : value)
        {
            switch (character)
            {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(character));
                    escaped += buffer;
                }
                else
                {
                    escaped.push_back(character);
                }
                break;
            }
        }

        return escaped;
    }

    inline std::string Quote(const std::string& value)
    {
        return "\"" + EscapeJson(value) + "\"";
    }

    inline std::string Quote(bool value)
    {
        return value ? "true" : "false";
    }

    inline std::string Quote(long long value)
    {
        return std::to_string(value);
    }

    inline std::string Quote(unsigned long long value)
    {
        return std::to_string(value);
    }

    inline std::string Quote(int value)
    {
        return std::to_string(value);
    }

    inline std::string Quote(double value)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.6g", value);
        return buffer;
    }

    inline std::string JsonArray(const std::vector<std::string>& values)
    {
        std::string result = "[";
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0)
            {
                result += ", ";
            }
            result += values[index];
        }
        result += "]";
        return result;
    }
}
