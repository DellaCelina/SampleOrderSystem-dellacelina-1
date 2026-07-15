#include "JsonParser.h"

#include <charconv>
#include <cctype>

JsonParseException::JsonParseException(const std::string& message, size_t line, size_t column)
    : std::runtime_error(message), m_line(line), m_column(column) {}

namespace {

struct Cursor {
    const std::string& text;
    size_t pos = 0;
    size_t line = 1;
    size_t column = 1;

    explicit Cursor(const std::string& source) : text(source) {}

    bool AtEnd() const { return pos >= text.size(); }

    char Peek() const { return text[pos]; }

    char Advance() {
        char c = text[pos];
        ++pos;
        if (c == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        return c;
    }

    [[noreturn]] void Fail(const std::string& message) const {
        throw JsonParseException(message, line, column);
    }
};

void SkipWhitespace(Cursor& cursor) {
    while (!cursor.AtEnd()) {
        char c = cursor.Peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            cursor.Advance();
        } else {
            break;
        }
    }
}

JsonValue ParseValue(Cursor& cursor);

void ExpectChar(Cursor& cursor, char expected, const std::string& context) {
    if (cursor.AtEnd() || cursor.Peek() != expected) {
        cursor.Fail("Expected '" + std::string(1, expected) + "' " + context);
    }
    cursor.Advance();
}

void ParseLiteral(Cursor& cursor, const char* literal) {
    size_t len = 0;
    while (literal[len] != '\0') ++len;
    for (size_t i = 0; i < len; ++i) {
        if (cursor.AtEnd() || cursor.Peek() != literal[i]) {
            cursor.Fail(std::string("Invalid literal, expected '") + literal + "'");
        }
        cursor.Advance();
    }
}

bool IsHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return 10 + (c - 'A');
}

void AppendUtf8(std::string& out, unsigned int codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

unsigned int ParseHex4(Cursor& cursor) {
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
        if (cursor.AtEnd() || !IsHexDigit(cursor.Peek())) {
            cursor.Fail("Invalid \\u escape: expected 4 hex digits");
        }
        value = (value << 4) | static_cast<unsigned int>(HexValue(cursor.Peek()));
        cursor.Advance();
    }
    return value;
}

std::string ParseStringRaw(Cursor& cursor) {
    ExpectChar(cursor, '"', "to start a string");
    std::string result;
    while (true) {
        if (cursor.AtEnd()) {
            cursor.Fail("Unterminated string");
        }
        char c = cursor.Peek();
        if (c == '"') {
            cursor.Advance();
            break;
        }
        if (static_cast<unsigned char>(c) <= 0x1F) {
            cursor.Fail("Unescaped control character in string");
        }
        if (c == '\\') {
            cursor.Advance();
            if (cursor.AtEnd()) {
                cursor.Fail("Unterminated escape sequence");
            }
            char esc = cursor.Peek();
            switch (esc) {
                case '"': result.push_back('"'); cursor.Advance(); break;
                case '\\': result.push_back('\\'); cursor.Advance(); break;
                case '/': result.push_back('/'); cursor.Advance(); break;
                case 'b': result.push_back('\b'); cursor.Advance(); break;
                case 'f': result.push_back('\f'); cursor.Advance(); break;
                case 'n': result.push_back('\n'); cursor.Advance(); break;
                case 'r': result.push_back('\r'); cursor.Advance(); break;
                case 't': result.push_back('\t'); cursor.Advance(); break;
                case 'u': {
                    cursor.Advance();
                    unsigned int codepoint = ParseHex4(cursor);
                    AppendUtf8(result, codepoint);
                    break;
                }
                default:
                    cursor.Fail("Invalid escape sequence '\\" + std::string(1, esc) + "'");
            }
        } else {
            result.push_back(c);
            cursor.Advance();
        }
    }
    return result;
}

JsonValue ParseString(Cursor& cursor) {
    return JsonValue(ParseStringRaw(cursor));
}

JsonValue ParseNumber(Cursor& cursor) {
    size_t start = cursor.pos;

    if (!cursor.AtEnd() && cursor.Peek() == '-') {
        cursor.Advance();
    }

    if (cursor.AtEnd() || !std::isdigit(static_cast<unsigned char>(cursor.Peek()))) {
        cursor.Fail("Invalid number: expected digit");
    }

    if (cursor.Peek() == '0') {
        cursor.Advance();
        if (!cursor.AtEnd() && std::isdigit(static_cast<unsigned char>(cursor.Peek()))) {
            cursor.Fail("Invalid number: leading zero followed by more digits");
        }
    } else {
        while (!cursor.AtEnd() && std::isdigit(static_cast<unsigned char>(cursor.Peek()))) {
            cursor.Advance();
        }
    }

    if (!cursor.AtEnd() && cursor.Peek() == '.') {
        cursor.Advance();
        if (cursor.AtEnd() || !std::isdigit(static_cast<unsigned char>(cursor.Peek()))) {
            cursor.Fail("Invalid number: expected digit after decimal point");
        }
        while (!cursor.AtEnd() && std::isdigit(static_cast<unsigned char>(cursor.Peek()))) {
            cursor.Advance();
        }
    }

    if (!cursor.AtEnd() && (cursor.Peek() == 'e' || cursor.Peek() == 'E')) {
        cursor.Advance();
        if (!cursor.AtEnd() && (cursor.Peek() == '+' || cursor.Peek() == '-')) {
            cursor.Advance();
        }
        if (cursor.AtEnd() || !std::isdigit(static_cast<unsigned char>(cursor.Peek()))) {
            cursor.Fail("Invalid number: expected digit in exponent");
        }
        while (!cursor.AtEnd() && std::isdigit(static_cast<unsigned char>(cursor.Peek()))) {
            cursor.Advance();
        }
    }

    std::string token = cursor.text.substr(start, cursor.pos - start);
    double value = 0.0;
    auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc()) {
        cursor.Fail("Invalid number literal: " + token);
    }
    return JsonValue(value);
}

JsonValue ParseArray(Cursor& cursor) {
    ExpectChar(cursor, '[', "to start an array");
    JsonValue array = JsonValue::MakeArray();

    SkipWhitespace(cursor);
    if (!cursor.AtEnd() && cursor.Peek() == ']') {
        cursor.Advance();
        return array;
    }

    while (true) {
        SkipWhitespace(cursor);
        array.Push(ParseValue(cursor));
        SkipWhitespace(cursor);

        if (cursor.AtEnd()) {
            cursor.Fail("Unterminated array");
        }
        char c = cursor.Peek();
        if (c == ',') {
            cursor.Advance();
            SkipWhitespace(cursor);
            if (!cursor.AtEnd() && cursor.Peek() == ']') {
                cursor.Fail("Trailing comma in array");
            }
            continue;
        }
        if (c == ']') {
            cursor.Advance();
            break;
        }
        cursor.Fail("Expected ',' or ']' in array");
    }
    return array;
}

JsonValue ParseObject(Cursor& cursor) {
    ExpectChar(cursor, '{', "to start an object");
    JsonValue object = JsonValue::MakeObject();

    SkipWhitespace(cursor);
    if (!cursor.AtEnd() && cursor.Peek() == '}') {
        cursor.Advance();
        return object;
    }

    while (true) {
        SkipWhitespace(cursor);
        if (cursor.AtEnd() || cursor.Peek() != '"') {
            cursor.Fail("Expected string key in object");
        }
        std::string key = ParseStringRaw(cursor);
        SkipWhitespace(cursor);
        ExpectChar(cursor, ':', "after object key");
        SkipWhitespace(cursor);
        JsonValue value = ParseValue(cursor);
        object.Set(key, std::move(value));
        SkipWhitespace(cursor);

        if (cursor.AtEnd()) {
            cursor.Fail("Unterminated object");
        }
        char c = cursor.Peek();
        if (c == ',') {
            cursor.Advance();
            SkipWhitespace(cursor);
            if (!cursor.AtEnd() && cursor.Peek() == '}') {
                cursor.Fail("Trailing comma in object");
            }
            continue;
        }
        if (c == '}') {
            cursor.Advance();
            break;
        }
        cursor.Fail("Expected ',' or '}' in object");
    }
    return object;
}

JsonValue ParseValue(Cursor& cursor) {
    SkipWhitespace(cursor);
    if (cursor.AtEnd()) {
        cursor.Fail("Unexpected end of input, expected a value");
    }
    char c = cursor.Peek();
    switch (c) {
        case '{': return ParseObject(cursor);
        case '[': return ParseArray(cursor);
        case '"': return ParseString(cursor);
        case 't': ParseLiteral(cursor, "true"); return JsonValue(true);
        case 'f': ParseLiteral(cursor, "false"); return JsonValue(false);
        case 'n': ParseLiteral(cursor, "null"); return JsonValue();
        default:
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                return ParseNumber(cursor);
            }
            cursor.Fail("Unexpected character, expected a value");
    }
}

} // namespace

JsonValue JsonParser::Parse(const std::string& text) {
    Cursor cursor(text);
    SkipWhitespace(cursor);
    if (cursor.AtEnd()) {
        cursor.Fail("Empty input, expected a JSON value");
    }
    JsonValue value = ParseValue(cursor);
    SkipWhitespace(cursor);
    if (!cursor.AtEnd()) {
        cursor.Fail("Trailing garbage after JSON value");
    }
    return value;
}
