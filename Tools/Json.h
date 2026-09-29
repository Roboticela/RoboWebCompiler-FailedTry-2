// ---------------------------------------------------------------------------
// RoboHTMLCompiler - minimal, dependency-free JSON support for the tooling.
//
// The compiler itself never parses JSON at run time: registry JSON is converted
// into strongly typed C++ tables by the RoboHTMLCompilerTools targets. This
// header exists only for that offline tooling.
// ---------------------------------------------------------------------------
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace RoboHTMLCompiler::Tools
{
    /// A JSON value. Objects preserve member order so that generated registry
    /// files stay stable and diffable.
    class JsonValue
    {
    public:
        enum class Type
        {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        };

        JsonValue() = default;

        static JsonValue MakeNull();
        static JsonValue MakeBoolean(bool value);
        static JsonValue MakeNumber(double value);
        static JsonValue MakeInteger(long long value);
        static JsonValue MakeString(std::string value);
        static JsonValue MakeArray();
        static JsonValue MakeObject();

        Type GetType() const { return type; }

        bool IsNull() const { return type == Type::Null; }
        bool IsArray() const { return type == Type::Array; }
        bool IsObject() const { return type == Type::Object; }
        bool IsString() const { return type == Type::String; }

        bool AsBoolean(bool fallback = false) const;
        double AsNumber(double fallback = 0.0) const;
        long long AsInteger(long long fallback = 0) const;
        const std::string& AsString() const;

        const std::vector<JsonValue>& GetElements() const { return elements; }

        std::size_t GetMemberCount() const { return memberKeys.size(); }
        const std::string& GetMemberKey(std::size_t index) const { return memberKeys[index]; }
        const JsonValue& GetMemberValue(std::size_t index) const { return memberValues[index]; }

        /// Appends a value to an array-typed JSON value.
        void Append(JsonValue value);

        /// Inserts or replaces a member of an object-typed JSON value.
        void Set(const std::string& key, JsonValue value);

        /// Returns a pointer to the named member, or nullptr when absent.
        const JsonValue* Find(const std::string& key) const;

        // Convenience accessors used throughout the tooling.
        std::string GetString(const std::string& key, const std::string& fallback = std::string()) const;
        bool GetBoolean(const std::string& key, bool fallback = false) const;
        long long GetInteger(const std::string& key, long long fallback = 0) const;
        std::vector<std::string> GetStringArray(const std::string& key) const;

        /// Serialises this value. When pretty is true the output is indented
        /// with two spaces and terminated with a trailing newline.
        std::string Serialize(bool pretty = true) const;

        /// Parses JSON text. On failure an error description is returned.
        static std::optional<JsonValue> Parse(const std::string& text, std::string* error);

    private:
        void SerializeInto(std::string& output, bool pretty, int depth) const;

        Type type = Type::Null;
        bool boolean = false;
        double number = 0.0;
        std::string text;
        std::vector<JsonValue> elements;
        std::vector<std::string> memberKeys;
        std::vector<JsonValue> memberValues;
    };

    /// Escapes a string for embedding in JSON output.
    std::string EscapeJsonString(const std::string& value);
}
