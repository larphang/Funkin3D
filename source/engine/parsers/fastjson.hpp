
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <stdexcept>

namespace fastjson {

struct Value {
    enum class Type : uint8_t {
        Null, Boolean, Number, String, Array, Object
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;

    bool isNull() const noexcept { return type == Type::Null; }
    bool isBool() const noexcept { return type == Type::Boolean; }
    bool isNumber() const noexcept { return type == Type::Number; }
    bool isString() const noexcept { return type == Type::String; }
    bool isArray() const noexcept { return type == Type::Array; }
    bool isObject() const noexcept { return type == Type::Object; }

    bool asBool(bool fallback = false) const noexcept {
        return isBool() ? boolean : fallback;
    }

    double asNumber(double fallback = 0.0) const noexcept {
        return isNumber() ? number : fallback;
    }

    std::string_view asString(std::string_view fallback = {}) const noexcept {
        return isString() ? std::string_view(string) : fallback;
    }

    const Value* get(std::string_view key) const noexcept {
        if (!isObject()) return nullptr;

        for (const auto& item : object)
            if (item.first == key)
                return &item.second;

        return nullptr;
    }

    Value* get(std::string_view key) noexcept {
        if (!isObject()) return nullptr;

        for (auto& item : object)
            if (item.first == key)
                return &item.second;

        return nullptr;
    }

    const Value& operator[](std::string_view key) const noexcept {
        static const Value missing{};
        const Value* value = get(key);
        return value ? *value : missing;
    }

    Value& operator[](std::string_view key) noexcept {
        static Value missing{};
        Value* value = get(key);
        return value ? *value : missing;
    }

    const Value* at(size_t index) const noexcept {
        return isArray() && index < array.size() ? &array[index] : nullptr;
    }

    Value* at(size_t index) noexcept {
        return isArray() && index < array.size() ? &array[index] : nullptr;
    }

    size_t size() const noexcept {
        if (isArray()) return array.size();
        if (isObject()) return object.size();
        return 0;
    }
};

struct ParseError : std::runtime_error {
    size_t offset;

    ParseError(const char* message, size_t position) : std::runtime_error(message), offset(position) {}
};

class Parser {
public:
    explicit Parser(std::string_view input) : begin_(input.data()), cur_(input.data()), end_(input.data() + input.size()) {}

    Value parse() {
        whitespace();
        Value result = value();
        whitespace();

        if (cur_ != end_)
            fail("unexpected trailing characters");

        return result;
    }

private:
    const char* begin_;
    const char* cur_;
    const char* end_;

    [[noreturn]] void fail(const char* message) const {
        throw ParseError(message, static_cast<size_t>(cur_ - begin_));
    }

    void whitespace() noexcept {
        while (cur_ != end_) {
            char c = *cur_;
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                break;
            ++cur_;
        }
    }

    bool consume(char c) noexcept {
        if (cur_ != end_ && *cur_ == c) {
            ++cur_;
            return true;
        }
        return false;
    }

    void expect(char c) {
        if (!consume(c))
            fail("unexpected character");
    }

    bool starts(std::string_view s) const noexcept {
        return static_cast<size_t>(end_ - cur_) >= s.size() && std::string_view(cur_, s.size()) == s;
    }

    Value literal(std::string_view expected, Value result) {
        if (!starts(expected))
            fail("invalid literal");

        cur_ += expected.size();
        return result;
    }

    Value value() {
        if (cur_ == end_)
            fail("expected value");

        switch (*cur_) {
            case 'n':
                return literal("null", Value{});
            case 't': {
                Value v;
                v.type = Value::Type::Boolean;
                v.boolean = true;
                return literal("true", std::move(v));
            }
            case 'f': {
                Value v;
                v.type = Value::Type::Boolean;
                v.boolean = false;
                return literal("false", std::move(v));
            }
            case '"': {
                Value v;
                v.type = Value::Type::String;
                v.string = string();
                return v;
            }
            case '[':
                return array();
            case '{':
                return object();
            default:
                if (*cur_ == '-' ||
                    (*cur_ >= '0' && *cur_ <= '9'))
                    return numeric();

                fail("invalid value");
        }
    }

    static void utf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    uint32_t hex4() {
        uint32_t result = 0;

        for (int i = 0; i < 4; ++i) {
            if (cur_ == end_)
                fail("incomplete Unicode escape");

            char c = *cur_++;
            result <<= 4;

            if (c >= '0' && c <= '9')
                result |= c - '0';
            else if (c >= 'a' && c <= 'f')
                result |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                result |= c - 'A' + 10;
            else
                fail("invalid Unicode escape");
        }

        return result;
    }

    std::string string() {
        expect('"');

        std::string out;

        while (cur_ != end_) {
            unsigned char c = static_cast<unsigned char>(*cur_++);

            if (c == '"')
                return out;

            if (c < 0x20)
                fail("unescaped control character");

            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }

            if (cur_ == end_)
                fail("incomplete escape sequence");

            char e = *cur_++;

            switch (e) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    uint32_t cp = hex4();

                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (end_ - cur_ < 6 ||
                            cur_[0] != '\\' ||
                            cur_[1] != 'u')
                            fail("missing low surrogate");
                        cur_ += 2;
                        uint32_t low = hex4();

                        if (low < 0xDC00 || low > 0xDFFF)
                            fail("invalid low surrogate");

                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail("unexpected low surrogate");
                    }

                    utf8(out, cp);
                    break;
                }

                default:
                    fail("invalid escape sequence");
            }
        }

        fail("unterminated string");
    }

    Value numeric() {
        const char* start = cur_;

        consume('-');

        if (consume('0')) {
            if (cur_ != end_ && *cur_ >= '0' && *cur_ <= '9')
                fail("leading zero in number");
        } else {
            if (cur_ == end_ || *cur_ < '1' || *cur_ > '9')
                fail("invalid number");
            while (cur_ != end_ && *cur_ >= '0' && *cur_ <= '9')
                ++cur_;
        }

        if (consume('.')) {
            if (cur_ == end_ || *cur_ < '0' || *cur_ > '9')
                fail("missing fractional digits");

            while (cur_ != end_ && *cur_ >= '0' && *cur_ <= '9')
                ++cur_;
        }

        if (consume('e') || consume('E')) {
            if (!consume('+'))
                consume('-');
            if (cur_ == end_ || *cur_ < '0' || *cur_ > '9')
                fail("missing exponent digits");
            while (cur_ != end_ && *cur_ >= '0' && *cur_ <= '9')
                ++cur_;
        }

        std::string token(start, static_cast<size_t>(cur_ - start));

        char* tail = nullptr;
        errno = 0;

        double n = std::strtod(token.c_str(), &tail);

        if (!tail || *tail != '\0' || errno == ERANGE || !std::isfinite(n))
            fail("number out of range");

        Value v;
        v.type = Value::Type::Number;
        v.number = n;
        return v;
    }

    Value array() {
        expect('[');

        Value v;
        v.type = Value::Type::Array;

        whitespace();

        if (consume(']'))
            return v;
        for (;;) {
            whitespace();
            v.array.push_back(value());
            whitespace();

            if (consume(']'))
                return v;

            expect(',');
            whitespace();
        }
    }

    Value object() {
        expect('{');

        Value v;
        v.type = Value::Type::Object;

        whitespace();

        if (consume('}'))
            return v;

        for (;;) {
            whitespace();

            if (cur_ == end_ || *cur_ != '"')
                fail("expected object key");

            std::string key = string();

            whitespace();
            expect(':');
            whitespace();

            Value child = value();

            v.object.emplace_back(std::move(key), std::move(child));

            whitespace();

            if (consume('}'))
                return v;

            expect(',');
            whitespace();
        }
    }
};

inline Value parse(std::string_view json) {
    return Parser(json).parse();
}

} // namespace fastjson
