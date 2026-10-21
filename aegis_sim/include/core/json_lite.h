#pragma once
// Tiny JSON reader/writer.  The simulator only needs to read one small file
// (scene_definition.json) and write one small file (player_performance.json),
// so it carries its own parser instead of a second copy of nlohmann::json
// (tinygltf already bundles one and mixing two versions in one binary is a
// classic ODR trap).

#include <string>
#include <vector>

namespace jl {

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<Value> arr;
    std::vector<std::string> keys;  // object keys (parallel to vals)
    std::vector<Value> vals;        // object values

    bool isNull() const { return type == Null; }
    bool isObject() const { return type == Object; }
    bool isArray() const { return type == Array; }
    bool isString() const { return type == String; }
    bool isNumber() const { return type == Number; }
    bool isBool() const { return type == Bool; }

    const Value* find(const std::string& key) const;
    const Value& at(const std::string& key) const;  // returns a Null value when missing
    std::string asString(const std::string& def = "") const;
    double asNumber(double def = 0.0) const;
    int asInt(int def = 0) const { return static_cast<int>(asNumber(def)); }
    bool asBool(bool def = false) const;
};

bool parse(const std::string& text, Value& out, std::string& error);

// Minimal pretty-printing writer.
class Writer {
public:
    Writer& beginObject(const std::string& key = "");
    Writer& endObject();
    Writer& beginArray(const std::string& key = "");
    Writer& endArray();
    Writer& kv(const std::string& key, const std::string& v);
    Writer& kv(const std::string& key, const char* v);
    Writer& kv(const std::string& key, double v);
    Writer& kv(const std::string& key, int v);
    Writer& kv(const std::string& key, bool v);
    std::string str() const { return out_ + "\n"; }

private:
    void prefix(const std::string& key);
    static std::string escape(const std::string& s);
    std::string out_;
    std::vector<bool> first_;
};

}  // namespace jl
