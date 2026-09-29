#include "Json.h"

#include <cmath>
#include <cstdio>

namespace RoboHTMLCompiler::Tools
{
    namespace
    {
        const std::string EmptyString;

        void AppendIndent(std::string& output, int depth)
        {
            for (int index = 0; index < depth; ++index)
            {
                output += "  ";
            }
        }

        void AppendNumber(std::string& output, double value)
        {
            if (std::isfinite(value) && value == static_cast<double>(static_cast<long long>(value)))
            {
                output += std::to_string(static_cast<long long>(value));
                return;
            }

            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.10g", value);
            output += buffer;
        }

        void SkipWhitespace(const std::string& text, std::size_t& position)
        {
            while (position < text.size())
            {
                const char character = text[position];
                if (character == ' ' || character == '\t' || character == '\n' || character == '\r')
                {
                    ++position;
                    continue;
                }
                break;
            }
        }

        bool ParseString(const std::string& text, std::size_t& position, std::string& out, std::string& error)
        {
            if (position >= text.size() || text[position] != '"')
            {
                error = "expected a JSON string";
                return false;
            }

            ++position;
            out.clear();

            while (position < text.size())
            {
                const char character = text[position++];
                if (character == '"')
                {
                    return true;
                }

                if (character != '\\')
                {
                    out.push_back(character);
                    continue;
                }

                if (position >= text.size())
                {
                    error = "unterminated escape sequence";
                    return false;
                }

                const char escaped = text[position++];
                switch (escaped)
                {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                {
                    if (position + 4 > text.size())
                    {
                        error = "truncated unicode escape";
                        return false;
                    }

                    unsigned int codePoint = 0;
                    for (int digit = 0; digit < 4; ++digit)
                    {
                        const char hex = text[position++];
                        codePoint <<= 4;
                        if (hex >= '0' && hex <= '9') codePoint |= static_cast<unsigned int>(hex - '0');
                        else if (hex >= 'a' && hex <= 'f') codePoint |= static_cast<unsigned int>(hex - 'a' + 10);
                        else if (hex >= 'A' && hex <= 'F') codePoint |= static_cast<unsigned int>(hex - 'A' + 10);
                        else
                        {
                            error = "invalid unicode escape";
                            return false;
                        }
                    }

                    if (codePoint < 0x80)
                    {
                        out.push_back(static_cast<char>(codePoint));
                    }
                    else if (codePoint < 0x800)
                    {
                        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                    }
                    else
                    {
                        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
                    }
                    break;
                }
                default:
                    error = "unknown escape sequence";
                    return false;
                }
            }

            error = "unterminated string";
            return false;
        }

        bool ParseValue(const std::string& text, std::size_t& position, JsonValue& out, std::string& error)
        {
            SkipWhitespace(text, position);

            if (position >= text.size())
            {
                error = "unexpected end of input";
                return false;
            }

            const char character = text[position];

            if (character == '{')
            {
                ++position;
                out = JsonValue::MakeObject();
                SkipWhitespace(text, position);

                if (position < text.size() && text[position] == '}')
                {
                    ++position;
                    return true;
                }

                while (true)
                {
                    SkipWhitespace(text, position);

                    std::string key;
                    if (!ParseString(text, position, key, error))
                    {
                        return false;
                    }

                    SkipWhitespace(text, position);
                    if (position >= text.size() || text[position] != ':')
                    {
                        error = "expected ':' after object key";
                        return false;
                    }
                    ++position;

                    JsonValue memberValue;
                    if (!ParseValue(text, position, memberValue, error))
                    {
                        return false;
                    }
                    out.Set(key, std::move(memberValue));

                    SkipWhitespace(text, position);
                    if (position >= text.size())
                    {
                        error = "unterminated object";
                        return false;
                    }

                    if (text[position] == ',')
                    {
                        ++position;
                        continue;
                    }

                    if (text[position] == '}')
                    {
                        ++position;
                        return true;
                    }

                    error = "expected ',' or '}' in object";
                    return false;
                }
            }

            if (character == '[')
            {
                ++position;
                out = JsonValue::MakeArray();
                SkipWhitespace(text, position);

                if (position < text.size() && text[position] == ']')
                {
                    ++position;
                    return true;
                }

                while (true)
                {
                    JsonValue element;
                    if (!ParseValue(text, position, element, error))
                    {
                        return false;
                    }
                    out.Append(std::move(element));

                    SkipWhitespace(text, position);
                    if (position >= text.size())
                    {
                        error = "unterminated array";
                        return false;
                    }

                    if (text[position] == ',')
                    {
                        ++position;
                        continue;
                    }

                    if (text[position] == ']')
                    {
                        ++position;
                        return true;
                    }

                    error = "expected ',' or ']' in array";
                    return false;
                }
            }

            if (character == '"')
            {
                std::string value;
                if (!ParseString(text, position, value, error))
                {
                    return false;
                }
                out = JsonValue::MakeString(std::move(value));
                return true;
            }

            if (text.compare(position, 4, "true") == 0)
            {
                position += 4;
                out = JsonValue::MakeBoolean(true);
                return true;
            }

            if (text.compare(position, 5, "false") == 0)
            {
                position += 5;
                out = JsonValue::MakeBoolean(false);
                return true;
            }

            if (text.compare(position, 4, "null") == 0)
            {
                position += 4;
                out = JsonValue::MakeNull();
                return true;
            }

            const std::size_t start = position;
            if (position < text.size() && (text[position] == '-' || text[position] == '+'))
            {
                ++position;
            }
            while (position < text.size())
            {
                const char digit = text[position];
                if ((digit >= '0' && digit <= '9') || digit == '.' || digit == 'e' || digit == 'E' ||
                    digit == '+' || digit == '-')
                {
                    ++position;
                    continue;
                }
                break;
            }

            if (start == position)
            {
                error = "unexpected character";
                return false;
            }

            const std::string numberText = text.substr(start, position - start);
            try
            {
                out = JsonValue::MakeNumber(std::stod(numberText));
            }
            catch (...)
            {
                error = "invalid number '" + numberText + "'";
                return false;
            }
            return true;
        }
    }

    std::string EscapeJsonString(const std::string& value)
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
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
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

    JsonValue JsonValue::MakeNull()
    {
        JsonValue value;
        value.type = Type::Null;
        return value;
    }

    JsonValue JsonValue::MakeBoolean(bool value)
    {
        JsonValue result;
        result.type = Type::Boolean;
        result.boolean = value;
        return result;
    }

    JsonValue JsonValue::MakeNumber(double value)
    {
        JsonValue result;
        result.type = Type::Number;
        result.number = value;
        return result;
    }

    JsonValue JsonValue::MakeInteger(long long value)
    {
        JsonValue result;
        result.type = Type::Number;
        result.number = static_cast<double>(value);
        return result;
    }

    JsonValue JsonValue::MakeString(std::string value)
    {
        JsonValue result;
        result.type = Type::String;
        result.text = std::move(value);
        return result;
    }

    JsonValue JsonValue::MakeArray()
    {
        JsonValue result;
        result.type = Type::Array;
        return result;
    }

    JsonValue JsonValue::MakeObject()
    {
        JsonValue result;
        result.type = Type::Object;
        return result;
    }

    bool JsonValue::AsBoolean(bool fallback) const
    {
        if (type == Type::Boolean)
        {
            return boolean;
        }
        return fallback;
    }

    double JsonValue::AsNumber(double fallback) const
    {
        if (type == Type::Number)
        {
            return number;
        }
        return fallback;
    }

    long long JsonValue::AsInteger(long long fallback) const
    {
        if (type == Type::Number)
        {
            return static_cast<long long>(number);
        }
        return fallback;
    }

    const std::string& JsonValue::AsString() const
    {
        if (type == Type::String)
        {
            return text;
        }
        return EmptyString;
    }

    void JsonValue::Append(JsonValue value)
    {
        type = Type::Array;
        elements.push_back(std::move(value));
    }

    void JsonValue::Set(const std::string& key, JsonValue value)
    {
        type = Type::Object;

        for (std::size_t index = 0; index < memberKeys.size(); ++index)
        {
            if (memberKeys[index] == key)
            {
                memberValues[index] = std::move(value);
                return;
            }
        }

        memberKeys.push_back(key);
        memberValues.push_back(std::move(value));
    }

    const JsonValue* JsonValue::Find(const std::string& key) const
    {
        if (type != Type::Object)
        {
            return nullptr;
        }

        for (std::size_t index = 0; index < memberKeys.size(); ++index)
        {
            if (memberKeys[index] == key)
            {
                return &memberValues[index];
            }
        }
        return nullptr;
    }

    std::string JsonValue::GetString(const std::string& key, const std::string& fallback) const
    {
        const JsonValue* member = Find(key);
        if (member == nullptr || !member->IsString())
        {
            return fallback;
        }
        return member->AsString();
    }

    bool JsonValue::GetBoolean(const std::string& key, bool fallback) const
    {
        const JsonValue* member = Find(key);
        if (member == nullptr || member->GetType() != Type::Boolean)
        {
            return fallback;
        }
        return member->AsBoolean();
    }

    long long JsonValue::GetInteger(const std::string& key, long long fallback) const
    {
        const JsonValue* member = Find(key);
        if (member == nullptr || member->GetType() != Type::Number)
        {
            return fallback;
        }
        return member->AsInteger();
    }

    std::vector<std::string> JsonValue::GetStringArray(const std::string& key) const
    {
        std::vector<std::string> result;

        const JsonValue* member = Find(key);
        if (member == nullptr || !member->IsArray())
        {
            return result;
        }

        for (const JsonValue& element : member->GetElements())
        {
            result.push_back(element.AsString());
        }
        return result;
    }

    void JsonValue::SerializeInto(std::string& output, bool pretty, int depth) const
    {
        switch (type)
        {
        case Type::Null:
            output += "null";
            return;

        case Type::Boolean:
            output += boolean ? "true" : "false";
            return;

        case Type::Number:
            AppendNumber(output, number);
            return;

        case Type::String:
            output += '"';
            output += EscapeJsonString(text);
            output += '"';
            return;

        case Type::Array:
        {
            if (elements.empty())
            {
                output += "[]";
                return;
            }

            output += '[';
            for (std::size_t index = 0; index < elements.size(); ++index)
            {
                if (index > 0)
                {
                    output += ',';
                }
                if (pretty)
                {
                    output += '\n';
                    AppendIndent(output, depth + 1);
                }
                elements[index].SerializeInto(output, pretty, depth + 1);
            }
            if (pretty)
            {
                output += '\n';
                AppendIndent(output, depth);
            }
            output += ']';
            return;
        }

        case Type::Object:
        {
            if (memberKeys.empty())
            {
                output += "{}";
                return;
            }

            output += '{';
            for (std::size_t index = 0; index < memberKeys.size(); ++index)
            {
                if (index > 0)
                {
                    output += ',';
                }
                if (pretty)
                {
                    output += '\n';
                    AppendIndent(output, depth + 1);
                }
                output += '"';
                output += EscapeJsonString(memberKeys[index]);
                output += "\":";
                if (pretty)
                {
                    output += ' ';
                }
                memberValues[index].SerializeInto(output, pretty, depth + 1);
            }
            if (pretty)
            {
                output += '\n';
                AppendIndent(output, depth);
            }
            output += '}';
            return;
        }
        }
    }

    std::string JsonValue::Serialize(bool pretty) const
    {
        std::string output;
        SerializeInto(output, pretty, 0);
        if (pretty)
        {
            output += '\n';
        }
        return output;
    }

    std::optional<JsonValue> JsonValue::Parse(const std::string& text, std::string* error)
    {
        std::size_t position = 0;
        JsonValue value;
        std::string localError;

        if (!ParseValue(text, position, value, localError))
        {
            if (error != nullptr)
            {
                *error = localError;
            }
            return std::nullopt;
        }

        SkipWhitespace(text, position);
        if (position != text.size())
        {
            if (error != nullptr)
            {
                *error = "trailing content after JSON value";
            }
            return std::nullopt;
        }

        return value;
    }
}
