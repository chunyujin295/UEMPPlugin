#include "test_common.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

using json = nlohmann::json;

// ---- nlohmann/json tests ----
static void test_json_basic_types(TestRunner& t) {
    printf("\n[nlohmann/json] basic types...\n");

    json j;
    j["str"]    = "hello";
    j["int"]    = 42;
    j["float"]  = 3.14159;
    j["bool_t"] = true;
    j["bool_f"] = false;

    t.check(j["str"].is_string(),        "string type");
    t.check(j["int"].is_number_integer(),"integer type");
    t.check(j["float"].is_number(),      "float type");
    t.check(j["bool_t"].is_boolean(),    "boolean type");

    t.check(j["str"].get<std::string>() == "hello", "string value");
    t.check(j["int"].get<int>() == 42,              "integer value");
    t.check(j["float"].get<double>() > 3.14,        "float value");
    t.check(j["bool_t"].get<bool>() == true,         "bool true value");
    t.check(j["bool_f"].get<bool>() == false,        "bool false value");
}

static void test_json_value_with_default(TestRunner& t) {
    printf("\n[nlohmann/json] value() with default...\n");

    json j = json::parse(R"({"name":"test","age":25})");

    std::string name = j.value("name", "fallback");
    int         age  = j.value("age", -1);

    t.check(name == "test", "existing key returns value");
    t.check(age == 25,      "existing key returns int value");

    std::string missing = j.value("no_such_key", "fallback");
    t.check(missing == "fallback", "missing key returns default");

    int missingInt = j.value("ghost", 999);
    t.check(missingInt == 999, "missing key returns int default");

    std::string typed = j.value("age", std::string("nope"));
    t.check(typed == "nope", "type mismatch → default");
}

static void test_json_parse_serialize(TestRunner& t) {
    printf("\n[nlohmann/json] parse / serialize...\n");

    std::string input = R"({
        "product": "UEMPPlugin",
        "version": "1.0.0",
        "features": ["logging", "yaml", "https"]
    })";

    auto j = json::parse(input);
    t.check(!j.is_null(),             "parse succeeds");
    t.check(j.is_object(),            "parsed result is object");
    t.check(j["product"].is_string(), "product is string");
    t.check(j["features"].is_array(), "features is array");
    t.check(j["features"].size() == 3,"features has 3 items");

    std::string dumped = j.dump();
    auto        j2     = json::parse(dumped);
    t.check(j2["product"] == "UEMPPlugin", "round-trip product");
    t.check(j2["features"].size() == 3,    "round-trip feature count");
}

static void test_json_array_operations(TestRunner& t) {
    printf("\n[nlohmann/json] array operations...\n");

    json arr = json::array();
    t.check(arr.is_array(), "empty array is array type");
    t.check(arr.empty(),    "empty array is empty");
    t.check(arr.size() == 0,"empty array size == 0");

    arr.push_back(10);
    arr.push_back("hello");
    arr.push_back(true);
    arr.push_back({{"key", "value"}});

    t.check(arr.size() == 4,              "array size after push_back");
    t.check(arr[0].get<int>() == 10,      "array element int");
    t.check(arr[1].get<std::string>() == "hello", "array element string");
    t.check(arr[2].get<bool>() == true,   "array element bool");
    t.check(arr[3].is_object() && arr[3]["key"] == "value", "array element object");

    int sum = 0;
    for (auto& el : arr) {
        if (el.is_number_integer()) sum += el.get<int>();
    }
    t.check(sum == 10, "array iteration works");
}

static void test_json_stl_interop(TestRunner& t) {
    printf("\n[nlohmann/json] STL container interop...\n");

    std::vector<int> ivec = {1, 2, 3, 4, 5};
    json jVec(ivec);
    t.check(jVec.is_array() && jVec.size() == 5, "json from vector");
    auto back = jVec.get<std::vector<int>>();
    t.check(back == ivec, "round-trip vector<int>");

    std::map<std::string, int> imap = {{"a", 1}, {"b", 2}, {"c", 3}};
    json jMap(imap);
    t.check(jMap.is_object(),               "json from map is object");
    t.check(jMap["b"].get<int>() == 2,      "map element access");
    t.check(jMap.size() == 3,               "map size preserved");
    auto backMap = jMap.get<std::map<std::string, int>>();
    t.check(backMap == imap,                "round-trip map<string,int>");
}

static void test_json_nested_and_paths(TestRunner& t) {
    printf("\n[nlohmann/json] nested objects and paths...\n");

    json j;
    j["servers"]["primary"]["host"] = "10.0.0.1";
    j["servers"]["primary"]["port"] = 8080;
    j["servers"]["backup"]["host"]  = "10.0.0.2";
    j["servers"]["backup"]["port"]  = 8081;

    t.check(j["servers"].is_object(),                      "auto-created nested object");
    t.check(j["servers"]["primary"]["host"] == "10.0.0.1", "nested string access");
    t.check(j["servers"]["primary"]["port"] == 8080,       "nested int access");
    t.check(j["servers"].size() == 2,                      "two server entries");

    t.check(j.contains("servers"),               "contains existing key");
    t.check(!j.contains("nonexistent"),          "contains returns false for missing");

    std::string host = j["/servers/primary/host"_json_pointer].get<std::string>();
    t.check(host == "10.0.0.1", "json_pointer access");

    int port = j.value("/servers/backup/port"_json_pointer, -1);
    t.check(port == 8081, "json_pointer with value()");

    int missing = j.value("/servers/ghost/port"_json_pointer, -1);
    t.check(missing == -1, "json_pointer missing → default");
}

static void test_json_dump_formatting(TestRunner& t) {
    printf("\n[nlohmann/json] dump formatting...\n");

    json j;
    j["name"] = "test";
    j["enabled"] = true;

    std::string compact = j.dump();
    t.check(compact.find("\n") == std::string::npos, "dump() produces single line");

    std::string pretty = j.dump(4);
    t.check(pretty.find("    ") != std::string::npos ||
            pretty.find("\n")  != std::string::npos,
            "dump(4) produces indented output");
}

static void test_json_parse_error_handling(TestRunner& t) {
    printf("\n[nlohmann/json] parse error handling...\n");

    bool caught = false;
    try {
        auto j = json::parse("{bad json!!!!}");
    } catch (const json::parse_error& e) {
        caught = true;
        printf("  parse_error: %s\n", e.what());
    }
    t.check(caught, "malformed JSON throws parse_error");

    bool caughtEmpty = false;
    try {
        auto j = json::parse("");
    } catch (const json::parse_error& e) {
        caughtEmpty = true;
    }
    t.check(caughtEmpty, "empty string throws parse_error");
}

static void test_json_null_and_empty(TestRunner& t) {
    printf("\n[nlohmann/json] null / empty...\n");

    json j;
    t.check(j.is_null(),  "default-constructed json is null");
    t.check(j.empty(),    "null json is empty");

    json jn = nullptr;
    t.check(jn.is_null(), "explicit nullptr json is null");

    json obj = json::object();
    t.check(obj.is_object(), "empty object is object type");
    t.check(obj.empty(),     "empty object is empty");
    t.check(obj.size() == 0, "empty object size == 0");

    json arr = json::array();
    t.check(arr.is_array(),  "empty array is array type");
    t.check(arr.empty(),     "empty array is empty");

    int v = j.value("anything", 42);
    t.check(v == 42, "value() on null json returns default safely");
}

static void test_json_update_and_merge(TestRunner& t) {
    printf("\n[nlohmann/json] update / merge...\n");

    json config;
    config["host"] = "localhost";
    config["port"] = 3000;

    config["host"] = "0.0.0.0";                      // overwrite
    config.emplace("debug", true);                   // insert if not present
    config.emplace("host", std::string("ignored"));   // no-op because "host" exists

    t.check(config["host"] == "0.0.0.0", "overwrite via operator[]");
    t.check(config["port"] == 3000,       "unrelated key untouched");
    t.check(config["debug"] == true,      "emplace inserted new key");

    json defaults;
    defaults["timeout"] = 30;
    defaults["retries"] = 3;
    defaults["port"]    = 8080;  // should NOT overwrite existing "port"

    config.update(defaults);  // keys present in config are kept

    t.check(config["timeout"] == 30,  "update added missing timeout");
    t.check(config["retries"] == 3,   "update added missing retries");
    t.check(config["port"] == 3000,   "update preserved existing port");
}

static void test_json_comparison(TestRunner& t) {
    printf("\n[nlohmann/json] comparison...\n");

    json a = {{"x", 1}, {"y", 2}};
    json b = {{"x", 1}, {"y", 2}};
    json c = {{"x", 1}, {"y", 3}};

    t.check(a == b, "identical objects compare equal");
    t.check(a != c, "different objects compare unequal");

    json n1(42);
    json n2(42);
    json n3(99);
    t.check(n1 == n2, "identical scalars compare equal");
    t.check(n1 != n3, "different scalars compare unequal");
}

int main() {
    printf("=== nlohmann/json Tests ===\n");
    TestRunner t;
    test_json_basic_types(t);
    test_json_value_with_default(t);
    test_json_parse_serialize(t);
    test_json_array_operations(t);
    test_json_stl_interop(t);
    test_json_nested_and_paths(t);
    test_json_dump_formatting(t);
    test_json_parse_error_handling(t);
    test_json_null_and_empty(t);
    test_json_update_and_merge(t);
    test_json_comparison(t);
    return t.finish();
}
