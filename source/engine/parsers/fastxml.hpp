#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <stdexcept>

namespace fastxml {

struct Attribute {
    std::string name;
    std::string value;
};

struct Node {
    std::string name;
    std::string text;
    std::vector<Attribute> attributes;
    std::vector<Node> children;

    const Attribute* findAttribute(std::string_view key) const noexcept {
        for (const auto& a : attributes)
            if (a.name == key) return &a;
        return nullptr;
    }

    std::string_view attribute(std::string_view key, std::string_view fallback = {}) const noexcept {
        const auto* a = findAttribute(key);
        return a ? std::string_view(a->value) : fallback;
    }

    const Node* firstChild(std::string_view key) const noexcept {
        for (const auto& child : children)
            if (child.name == key) return &child;
        return nullptr;
    }

    Node* firstChild(std::string_view key) noexcept {
        for (auto& child : children)
            if (child.name == key) return &child;
        return nullptr;
    }

    std::vector<const Node*> childrenNamed(std::string_view key) const {
        std::vector<const Node*> result;
        for (const auto& child : children)
            if (child.name == key) result.push_back(&child);
        return result;
    }
};

struct ParseError : std::runtime_error {
    std::size_t offset;
    ParseError(const char* message, std::size_t at) : std::runtime_error(message), offset(at) {}
};

class Parser {
public:
    explicit Parser(std::string_view input) : begin_(input.data()), 
        cur_(input.data()), end_(input.data() + input.size()) 
    {
        if (input.size() >= 3 &&
            static_cast<unsigned char>(input[0]) == 0xEF &&
            static_cast<unsigned char>(input[1]) == 0xBB &&
            static_cast<unsigned char>(input[2]) == 0xBF) {
            cur_ += 3;
        }
    }

    Node parse() {
        skipMisc();
        if (cur_ == end_) fail("expected root element");
        Node root = parseElement();
        skipMisc();
        if (cur_ != end_) fail("unexpected content after root element");
        return root;
    }

private:
    const char* begin_;
    const char* cur_;
    const char* end_;

    [[noreturn]] void fail(const char* message) const {
        throw ParseError(message, static_cast<std::size_t>(cur_ - begin_));
    }

    bool starts(std::string_view s) const noexcept {
        return static_cast<std::size_t>(end_ - cur_) >= s.size() && std::memcmp(cur_, s.data(), s.size()) == 0;
    }

    static bool ws(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    void skipWhitespace() noexcept {
        while (cur_ != end_ && ws(*cur_)) ++cur_;
    }

    void expect(char c) {
        if (cur_ == end_ || *cur_ != c) fail("unexpected character");
        ++cur_;
    }

    void skipUntil(std::string_view terminator) {
        while (cur_ != end_ && !starts(terminator)) ++cur_;
        if (cur_ == end_) fail("unterminated declaration or comment");
        cur_ += terminator.size();
    }

    void skipMisc() {
        for (;;) {
            skipWhitespace();
            if (starts("<!--")) {
                cur_ += 4;
                skipUntil("-->");
            } else if (starts("<?")) {
                cur_ += 2;
                skipUntil("?>");
            } else if (starts("<!DOCTYPE")) {
                cur_ += 9;
                int bracketDepth = 0;
                char quote = 0;
                while (cur_ != end_) {
                    char c = *cur_++;
                    if (quote) {
                        if (c == quote) quote = 0;
                    } else if (c == '\'' || c == '"') {
                        quote = c;
                    } else if (c == '[') {
                        ++bracketDepth;
                    } else if (c == ']') {
                        if (bracketDepth) --bracketDepth;
                    } else if (c == '>' && bracketDepth == 0) {
                        break;
                    }
                }
                if (cur_ == end_ && (end_ == begin_ || end_[-1] != '>'))
                    fail("unterminated DOCTYPE");
            } else {
                return;
            }
        }
    }

    static bool nameStart(char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               c == '_' || c == ':' || static_cast<unsigned char>(c) >= 0x80;
    }

    static bool nameChar(char c) noexcept {
        return nameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
    }

    std::string parseName() {
        if (cur_ == end_ || !nameStart(*cur_)) fail("expected XML name");
        const char* start = cur_++;
        while (cur_ != end_ && nameChar(*cur_)) ++cur_;
        return std::string(start, static_cast<std::size_t>(cur_ - start));
    }

    static void appendCodepoint(std::string& out, unsigned long cp) {
        if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF && !(cp >= 0xD800 && cp <= 0xDFFF)) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::string decode(std::string_view raw) {
        std::string out;
        out.reserve(raw.size());
        for (std::size_t i = 0; i < raw.size();) {
            if (raw[i] != '&') {
                out.push_back(raw[i++]);
                continue;
            }
            const std::size_t semi = raw.find(';', i + 1);
            if (semi == std::string_view::npos) fail("unterminated entity reference");
            const auto ent = raw.substr(i + 1, semi - i - 1);
            if (ent == "amp") out.push_back('&');
            else if (ent == "lt") out.push_back('<');
            else if (ent == "gt") out.push_back('>');
            else if (ent == "quot") out.push_back('"');
            else if (ent == "apos") out.push_back('\'');
            else if (!ent.empty() && ent[0] == '#') {
                char* tail = nullptr;
                std::string number(ent.substr((ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) ? 2 : 1));
                const int base = (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) ? 16 : 10;
                unsigned long cp = std::strtoul(number.c_str(), &tail, base);
                if (number.empty() || !tail || *tail != '\0' || cp > 0x10FFFF ||
                    (cp >= 0xD800 && cp <= 0xDFFF))
                    fail("invalid numeric character reference");
                appendCodepoint(out, cp);
            } else {
                fail("unknown entity reference");
            }
            i = semi + 1;
        }
        return out;
    }

    std::string parseQuotedValue() {
        if (cur_ == end_ || (*cur_ != '\'' && *cur_ != '"'))
            fail("expected quoted attribute value");
        const char quote = *cur_++;
        const char* start = cur_;
        while (cur_ != end_ && *cur_ != quote) ++cur_;
        if (cur_ == end_) fail("unterminated attribute value");
        std::string result = decode(std::string_view(start, static_cast<std::size_t>(cur_ - start)));
        ++cur_;
        return result;
    }

    Node parseElement() {
        expect('<');
        if (cur_ == end_ || *cur_ == '/' || *cur_ == '!' || *cur_ == '?')
            fail("expected element");
        Node node;
        node.name = parseName();

        for (;;) {
            skipWhitespace();
            if (starts("/>")) {
                cur_ += 2;
                return node;
            }
            if (cur_ != end_ && *cur_ == '>') {
                ++cur_;
                break;
            }
            if (cur_ == end_) fail("unterminated start tag");
            Attribute attr;
            attr.name = parseName();
            skipWhitespace();
            expect('=');
            skipWhitespace();
            attr.value = parseQuotedValue();
            node.attributes.push_back(std::move(attr));
        }

        for (;;) {
            if (cur_ == end_) fail("unterminated element");
            if (starts("</")) {
                cur_ += 2;
                std::string closeName = parseName();
                skipWhitespace();
                expect('>');
                if (closeName != node.name) fail("mismatched closing tag");
                return node;
            }
            if (starts("<!--")) {
                cur_ += 4;
                skipUntil("-->");
            } else if (starts("<![CDATA[")) {
                cur_ += 9;
                const char* start = cur_;
                while (cur_ != end_ && !starts("]]>")) ++cur_;
                if (cur_ == end_) fail("unterminated CDATA section");
                node.text.append(start, static_cast<std::size_t>(cur_ - start));
                cur_ += 3;
            } else if (starts("<?")) {
                cur_ += 2;
                skipUntil("?>");
            } else if (starts("<!")) {
                fail("unsupported markup declaration inside element");
            } else if (*cur_ == '<') {
                node.children.push_back(parseElement());
            } else {
                const char* start = cur_;
                while (cur_ != end_ && *cur_ != '<') ++cur_;
                node.text += decode(std::string_view(start, static_cast<std::size_t>(cur_ - start)));
            }
        }
    }
};

inline Node parse(std::string_view xml) {
    return Parser(xml).parse();
}

} // namespace fastxml
