#include "test_common.h"

#include <yamltool/yamlnode.h>
#include <yamltool/yamltool.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using YT = YamlTool::YamlTool;
using YamlNode = YamlTool::YamlNode;

// ---- YamlTool tests ----
static void test_yamltool_basic(TestRunner& t) {
    printf("\n[YamlTool] load / save / get / set...\n");
    fs::path tmpDir = fs::temp_directory_path() / "uemp_yaml_test";
    fs::create_directories(tmpDir);
    std::string yamlPath = (tmpDir / "test.yaml").string();

    YamlNode root;
    YT::set<int>(root, "answer", 42);
    YT::set<std::string>(root, "name", "uemp");
    YT::set<double>(root, "pi", 3.14159);
    YT::set<bool>(root, "enabled", true);
    YT::saveAsFile(root, yamlPath);
    t.check(fs::exists(yamlPath), "YAML file saved");

    YamlNode loaded;
    bool ok = YT::loadFile(loaded, yamlPath);
    t.check(ok, "YAML file loaded");

    int         answer  = YT::getDef<int>(loaded, "answer", 0);
    std::string name    = YT::getDef<std::string>(loaded, "name", "");
    double      pi      = YT::getDef<double>(loaded, "pi", 0.0);
    bool        enabled = YT::getDef<bool>(loaded, "enabled", false);

    t.check(answer == 42,              "int round-trip");
    t.check(name == "uemp",            "string round-trip");
    t.check(pi > 3.14 && pi < 3.142,  "double round-trip");
    t.check(enabled == true,           "bool round-trip");

    int missing = YT::getDef<int>(loaded, "no_such_key", 999);
    t.check(missing == 999, "getDef returns default for missing key");

    YamlNode nullNode;
    t.check(nullNode.isNull(), "default-constructed node is null");
    int nullDef = YT::getDef<int>(nullNode, "anything", -1);
    t.check(nullDef == -1, "getDef on null node returns default safely");

    bool wrote = YT::setDef<int>(loaded, "new_key", 100);
    t.check(wrote, "setDef returns true when key was absent");
    int newVal = YT::getDef<int>(loaded, "new_key", 0);
    t.check(newVal == 100, "setDef actually wrote the value");

    bool wroteAgain = YT::setDef<int>(loaded, "answer", 99);
    t.check(!wroteAgain, "setDef returns false when key already exists");
    int answer2 = YT::getDef<int>(loaded, "answer", 0);
    t.check(answer2 == 42, "setDef did not overwrite existing value");

    fs::remove_all(tmpDir);
}

static void test_yamltool_node_types(TestRunner& t) {
    printf("\n[YamlTool] node types...\n");

    YamlNode mapNode;
    YT::set<int>(mapNode, "a", 1);
    t.check(mapNode.isMap(),   "node with keys is Map");
    t.check(!mapNode.isNull(), "non-empty node is not null");

    fs::path tmpDir = fs::temp_directory_path() / "uemp_yaml_seq_test";
    fs::create_directories(tmpDir);
    std::string seqPath = (tmpDir / "seq.yaml").string();

    YamlNode root;
    YamlNode item1;
    YT::set<int>(item1, "id", 1);
    YamlNode item2;
    YT::set<int>(item2, "id", 2);
    YT::pushBack(root, item1);
    YT::pushBack(root, item2);
    YT::saveAsFile(root, seqPath);

    YamlNode loadedSeq;
    YT::loadFile(loadedSeq, seqPath);
    t.check(loadedSeq.isSequence(), "loaded sequence is Sequence");
    t.check(loadedSeq.size() == 2,  "sequence has 2 elements");

    YamlNode first = YT::getSequenceNode(loadedSeq, 0);
    int firstId = YT::getDef<int>(first, "id", -1);
    t.check(firstId == 1, "first sequence element id == 1");

    fs::remove_all(tmpDir);
}

static void test_yamltool_set_null(TestRunner& t) {
    printf("\n[YamlTool] setNull / setNullDef...\n");

    YamlNode node;

    bool wrote = YT::setNullDef(node, "optional_field");
    t.check(wrote, "setNullDef writes when key missing");

    bool wroteAgain = YT::setNullDef(node, "optional_field");
    t.check(!wroteAgain, "setNullDef skips when key exists");

    YT::setNull(node, "optional_field");
    t.check(true, "setNull does not crash");
}

int main() {
    printf("=== YamlTool Tests ===\n");
    TestRunner t;
    test_yamltool_basic(t);
    test_yamltool_node_types(t);
    test_yamltool_set_null(t);
    return t.finish();
}
