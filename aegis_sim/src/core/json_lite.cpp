#include "core/json_lite.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace jl {

static const Value kNull;

const Value* Value::find(const std::string& key) const {
    if (type != Object) return nullptr;
    for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == key) return &vals[i];
    return nullptr;
}
const Value& Value::at(const std::string& key) const {
    const Value* v = find(key);
    return v ? *v : kNull;
}
std::string Value::asString(const std::string& def) const { return type == String ? str : def; }
double Value::asNumber(double def) const { return type == Number ? num : def; }
bool Value::asBool(bool def) const { return type == Bool ? b : def; }

namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string err;
    int depth = 0;
    explicit Parser(const std::string& text) : s(text) {}

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }
    bool fail(const std::string& m) {
        if (err.empty()) err = m + " at offset " + std::to_string(i);
        return false;
    }
    bool lit(const char* word) {
        size_t n = std::char_traits<char>::length(word);
        if (s.compare(i, n, word) == 0) {
            i += n;
            return true;
        }
        return false;
    }
    static void appendUtf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o += static_cast<char>(cp);
        else if (cp < 0x800) {
            o += static_cast<char>(0xC0 | (cp >> 6));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            o += static_cast<char>(0xE0 | (cp >> 12));
            o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            o += static_cast<char>(0xF0 | (cp >> 18));
            o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    bool parseString(std::string& out) {
        if (i >= s.size() || s[i] != '"') return fail("expected string");
        ++i;
        out.clear();
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= s.size()) return fail("bad escape");
                char e = s[i++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (i + 4 > s.size()) return fail("bad unicode escape");
                        unsigned cp = static_cast<unsigned>(std::strtoul(s.substr(i, 4).c_str(), nullptr, 16));
                        i += 4;
                        appendUtf8(out, cp);
                        break;
                    }
                    default: return fail("bad escape");
                }
            } else {
                out += c;
            }
        }
        return fail("unterminated string");
    }
    bool parseValue(Value& v) {
        if (++depth > 64) return fail("nesting too deep");
        ws();
        if (i >= s.size()) return fail("unexpected end");
        char c = s[i];
        bool ok = false;
        if (c == '{') {
            ++i;
            v.type = Value::Object;
            ws();
            if (i < s.size() && s[i] == '}') {
                ++i;
                ok = true;
            } else {
                ok = true;
                while (ok) {
                    ws();
                    std::string key;
                    if (!parseString(key)) { ok = false; break; }
                    ws();
                    if (i >= s.size() || s[i] != ':') { ok = fail("expected ':'"); break; }
                    ++i;
                    Value child;
                    if (!parseValue(child)) { ok = false; break; }
                    v.keys.push_back(std::move(key));
                    v.vals.push_back(std::move(child));
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == '}') { ++i; break; }
                    ok = fail("expected ',' or '}'");
                }
            }
        } else if (c == '[') {
            ++i;
            v.type = Value::Array;
            ws();
            if (i < s.size() && s[i] == ']') {
                ++i;
                ok = true;
            } else {
                ok = true;
                while (ok) {
                    Value child;
                    if (!parseValue(child)) { ok = false; break; }
                    v.arr.push_back(std::move(child));
                    ws();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == ']') { ++i; break; }
                    ok = fail("expected ',' or ']'");
                }
            }
        } else if (c == '"') {
            v.type = Value::String;
            ok = parseString(v.str);
        } else if (lit("true")) {
            v.type = Value::Bool; v.b = true; ok = true;
        } else if (lit("false")) {
            v.type = Value::Bool; v.b = false; ok = true;
        } else if (lit("null")) {
            v.type = Value::Null; ok = true;
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            char* end = nullptr;
            double d = std::strtod(s.c_str() + i, &end);
            if (end == s.c_str() + i) {
                ok = fail("bad number");
            } else {
                i = static_cast<size_t>(end - s.c_str());
                v.type = Value::Number;
                v.num = d;
                ok = true;
            }
        } else {
            ok = fail("unexpected character");
        }
        --depth;
        return ok;
    }
};

}  // namespace

bool parse(const std::string& text, Value& out, std::string& error) {
    Parser p(text);
    out = Value();
    if (!p.parseValue(out)) {
        error = p.err;
        return false;
    }
    p.ws();
    if (p.i != text.size()) {
        error = "trailing characters at offset " + std::to_string(p.i);
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
std::string Writer::escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof(b), "\\u%04x", c);
                    o += b;
                } else {
                    o += c;
                }
        }
    }
    return o;
}

void Writer::prefix(const std::string& key) {
    if (!first_.empty()) {
        if (!first_.back()) out_ += ",";
        first_.back() = false;
        out_ += "\n";
        out_ += std::string(first_.size() * 2, ' ');
    }
    if (!key.empty()) out_ += "\"" + escape(key) + "\": ";
}

Writer& Writer::beginObject(const std::string& key) {
    prefix(key);
    out_ += "{";
    first_.push_back(true);
    return *this;
}
Writer& Writer::endObject() {
    bool empty = first_.back();
    first_.pop_back();
    if (!empty) {
        out_ += "\n";
        out_ += std::string(first_.size() * 2, ' ');
    }
    out_ += "}";
    return *this;
}
Writer& Writer::beginArray(const std::string& key) {
    prefix(key);
    out_ += "[";
    first_.push_back(true);
    return *this;
}
Writer& Writer::endArray() {
    bool empty = first_.back();
    first_.pop_back();
    if (!empty) {
        out_ += "\n";
        out_ += std::string(first_.size() * 2, ' ');
    }
    out_ += "]";
    return *this;
}
Writer& Writer::kv(const std::string& key, const std::string& v) {
    prefix(key);
    out_ += "\"" + escape(v) + "\"";
    return *this;
}
Writer& Writer::kv(const std::string& key, const char* v) { return kv(key, std::string(v)); }
Writer& Writer::kv(const std::string& key, double v) {
    prefix(key);
    char b[64];
    if (!std::isfinite(v)) v = 0.0;
    std::snprintf(b, sizeof(b), "%.4f", v);
    out_ += b;
    return *this;
}
Writer& Writer::kv(const std::string& key, int v) {
    prefix(key);
    out_ += std::to_string(v);
    return *this;
}
Writer& Writer::kv(const std::string& key, bool v) {
    prefix(key);
    out_ += v ? "true" : "false";
    return *this;
}

}  // namespace jl
