//
// json_reader_test.cpp: unit tests for the minimal JSON value used by the
// graphical activator to read `appimage-integrate explain --json` and its own
// geometry file.
//
#include "json/json_reader.h"

#include <iostream>
#include <string>

using gnome_appimage::json::value_c;

namespace {

int i_failures = 0;

void check(bool b_condition, const std::string &s_description) {
    if (!b_condition) {
        std::cerr << "FAIL: " << s_description << '\n';
        i_failures++;
    }
}

value_c parse_ok(const std::string &s_text) {
    std::string s_error;
    const value_c o_value = value_c::parse(s_text, s_error);
    if (!s_error.empty()) {
        std::cerr << "FAIL: unexpected parse error: " << s_error << '\n';
        i_failures++;
    }
    return o_value;
}

void check_parse_error(const std::string &s_text, const std::string &s_description) {
    std::string s_error;
    const value_c o_value = value_c::parse(s_text, s_error);
    check(!s_error.empty(), s_description + " must be rejected");
    check(o_value.is_null(), s_description + " must yield a null value");
}

// The shape `explain --json` produces, trimmed to the fields the activator reads.
const char *const s_explain =
    "{\"name\":\"Probe App\",\"version\":\"9.9.10\",\"version_source\":\"X-AppImage-Version\","
    "\"file_size\":1024,\"installed\":\"/home/u/Applications/Probe.AppImage\","
    "\"desktop_id\":\"org.example.Probe.desktop\",\"mode\":\"update the launcher in place\","
    "\"valid\":true,\"conflicts\":[{\"desktop_id\":\"org.example.Probe.desktop\","
    "\"origin\":\"this tool (upgrade)\",\"upgrade\":true,\"repair\":false,\"exec_exists\":true},"
    "{\"desktop_id\":\"legacy.Probe.desktop\",\"origin\":\"unknown\",\"upgrade\":false,"
    "\"repair\":false,\"exec_exists\":false}]}";

void test_explain_document() {
    const value_c o_value = parse_ok(s_explain);
    check(o_value.is_object(), "the explain document is an object");
    check(o_value.string_or("name") == "Probe App", "name is read");
    check(o_value.string_or("mode") == "update the launcher in place", "mode is read");
    check(o_value.number_or("file_size") == 1024.0, "file_size is read as a number");
    check(o_value.boolean_or("valid", false), "valid is read as a boolean");
    check(o_value.string_or("missing", "fallback") == "fallback", "a missing member falls back");

    const std::vector<value_c> o_conflicts = o_value.array_or_empty("conflicts");
    check(2 == o_conflicts.size(), "both conflicts are read");
    check(o_conflicts[0].string_or("desktop_id") == "org.example.Probe.desktop",
          "the first conflict id is read");
    check(o_conflicts[0].boolean_or("upgrade", false), "the first conflict is an upgrade");
    check(!o_conflicts[0].boolean_or("repair", true), "repair is read as false");
    check(o_conflicts[0].boolean_or("exec_exists", false), "exec_exists is read as true");
    check(!o_conflicts[1].boolean_or("upgrade", true), "the second conflict is not an upgrade");
    check(o_conflicts[1].string_or("origin") == "unknown", "the second origin is read");
}

void test_escapes() {
    const value_c o_value = parse_ok(
        "{\"a\":\"quote \\\" backslash \\\\ newline \\n tab \\t\",\"u\":\"caf\\u00e9\","
        "\"u8\":\"\\u20ac\",\"plain\":\"Grüße\"}");
    check(o_value.string_or("a") == "quote \" backslash \\ newline \n tab \t",
          "the simple escapes are decoded");
    check(o_value.string_or("u") == "café", "a two-byte \\u escape is decoded");
    check(o_value.string_or("u8") == "€", "a three-byte \\u escape is decoded");
    check(o_value.string_or("plain") == "Grüße", "raw UTF-8 passes through");
}

void test_other_kinds() {
    const value_c o_value = parse_ok("{\"n\":null,\"t\":true,\"f\":false,\"d\":-2.5,\"e\":1e3}");
    check(o_value.member("n") != nullptr && o_value.member("n")->is_null(), "null is read");
    check(o_value.boolean_or("t", false), "true is read");
    check(o_value.boolean_or("f", true) == false, "false is read");
    check(o_value.number_or("d") == -2.5, "a negative fraction is read");
    check(o_value.number_or("e") == 1000.0, "an exponent is read");
    check(parse_ok("[]").is_array(), "an empty array is read");
    check(parse_ok("[]").items().empty(), "an empty array has no items");
    check(parse_ok("{}").is_object(), "an empty object is read");
    check(parse_ok("  {\"a\" : [ 1 , 2 ] }  ").array_or_empty("a").size() == 2,
          "whitespace anywhere is allowed");
}

void test_errors() {
    check_parse_error("", "empty input");
    check_parse_error("{", "an unterminated object");
    check_parse_error("{\"a\":}", "a missing value");
    check_parse_error("{\"a\" 1}", "a missing colon");
    check_parse_error("{\"a\":1 \"b\":2}", "a missing comma");
    check_parse_error("{\"a\":1,}", "a trailing comma");
    check_parse_error("[1,]", "a trailing comma in an array");
    check_parse_error("\"unterminated", "an unterminated string");
    check_parse_error("{\"a\":\"\\q\"}", "an unknown escape");
    check_parse_error("{\"a\":\"\\u00zz\"}", "a broken \\u escape");
    check_parse_error("{} extra", "trailing text");
    check_parse_error("nul", "a truncated literal");
    check_parse_error("{\"a\":+1}", "a leading plus");
}

void test_writer_round_trip() {
    // Build the geometry document the way the activator does, write it, read it
    // back, and confirm nothing was lost: an entry it does not understand must
    // survive a save.
    value_c o_root = value_c::make_object();
    value_c o_main = value_c::make_object();
    o_main.set("height", value_c::make_number(720));
    o_main.set("width", value_c::make_number(660));
    o_main.set("x", value_c::make_number(40));
    o_main.set("y", value_c::make_number(60));
    o_root.set("main", o_main);

    value_c o_legacy = value_c::make_object();
    o_legacy.set("width", value_c::make_number(500));
    o_root.set("result", o_legacy);

    const std::string s_text = o_root.to_text();
    check(s_text.find("\"main\"") != std::string::npos, "the writer emits the main entry");
    check(s_text.find("\"height\": 720") != std::string::npos, "a number is written as an integer");
    // Keys are sorted, so "main" precedes "result" and "height" precedes "width".
    check(s_text.find("\"main\"") < s_text.find("\"result\""), "keys are sorted");
    check(s_text.find("\"height\"") < s_text.find("\"width\""), "keys within an entry are sorted");

    const value_c o_read = parse_ok(s_text);
    check(o_read.number_or("missing") == 0.0, "an unknown key falls back to zero");
    const value_c *p_main = o_read.member("main");
    check(nullptr != p_main, "main survives the round trip");
    check(p_main->number_or("width") == 660.0, "width survives the round trip");
    check(o_read.member("result") != nullptr, "an entry the code does not use survives");
}

void test_migration_shape() {
    // The first format stored one size at the top level; the reader must expose it
    // so the caller can migrate it.
    const value_c o_value = parse_ok("{\"width\": 500, \"height\": 400}");
    check(o_value.member("main") == nullptr, "the old format has no main entry");
    check(o_value.number_or("width") == 500.0, "the old width is readable");
    check(o_value.number_or("height") == 400.0, "the old height is readable");
}

}  // namespace

int main() {
    test_explain_document();
    test_escapes();
    test_other_kinds();
    test_errors();
    test_writer_round_trip();
    test_migration_shape();
    if (0 != i_failures) {
        std::cerr << "json-reader-test: " << i_failures << " checks failed\n";
        return 1;
    }
    std::cout << "json-reader-test: all checks passed\n";
    return 0;
}
