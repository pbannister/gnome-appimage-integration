// Portable unit tests for the desktop entry reader.
// Usage: desktop-entry-reader-test [repository-root]
#include "desktop/desktop_entry_reader.h"

#include <cstdio>
#include <string>
#include <vector>

using gnome_appimage::desktop::desktop_entry_diagnostic_o;
using gnome_appimage::desktop::desktop_entry_file_o;
using gnome_appimage::desktop::desktop_entry_reader_c;
using gnome_appimage::desktop::desktop_entry_severity_e;
using gnome_appimage::desktop::exec_deprecated_field_codes;
using gnome_appimage::desktop::exec_field_codes;
using gnome_appimage::desktop::locale_match_candidates;

namespace {

int g_count_failure = 0;

void check(bool b_condition, const char *s_expression, int i_line) {
    if (!b_condition) {
        std::fprintf(stderr, "FAIL line %d: %s\n", i_line, s_expression);
        g_count_failure++;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

bool has_error(const std::vector<desktop_entry_diagnostic_o> &o_diagnostics) {
    for (const desktop_entry_diagnostic_o &o_diagnostic : o_diagnostics) {
        if (desktop_entry_severity_e::error == o_diagnostic.severity) {
            return true;
        }
    }
    return false;
}

bool has_warning(const std::vector<desktop_entry_diagnostic_o> &o_diagnostics) {
    for (const desktop_entry_diagnostic_o &o_diagnostic : o_diagnostics) {
        if (desktop_entry_severity_e::warning == o_diagnostic.severity) {
            return true;
        }
    }
    return false;
}

const std::string SPEC_EXAMPLE =
    "[Desktop Entry]\n"
    "Version=1.0\n"
    "Type=Application\n"
    "Name=Foo Viewer\n"
    "Comment=The best viewer for Foo objects available!\n"
    "TryExec=fooview\n"
    "Exec=fooview %F\n"
    "Icon=fooview\n"
    "MimeType=image/x-foo;\n"
    "Actions=Gallery;Create;\n"
    "\n"
    "[Desktop Action Gallery]\n"
    "Exec=fooview --gallery\n"
    "Name=Browse Gallery\n"
    "\n"
    "[Desktop Action Create]\n"
    "Exec=fooview --create-new\n"
    "Name=Create a new Foo!\n"
    "Icon=fooview-new\n";

const std::string LOCALIZED =
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Foo\n"
    "Name[sr_YU]=Foo YU\n"
    "Name[sr@Latn]=Foo Latn\n"
    "Name[sr]=Foo SR\n"
    "Name[fr]=Foo FR\n"
    "Exec=foo\n";

const std::string ESCAPES =
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Tab\\tNew\\nLine\n"
    "Exec=foo\n"
    "Keywords=a\\;b;c;d;\n"
    "X-Empty=;\n"
    "NoDisplay=1\n";

void test_spec_example() {
    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;
    CHECK(desktop_entry_reader_c::parse_text(SPEC_EXAMPLE, o_file, o_diagnostics));
    CHECK(!has_error(o_diagnostics));
    CHECK(3 == o_file.groups.size());
    CHECK("Desktop Entry" == o_file.groups[0].name);
    CHECK("Desktop Action Gallery" == o_file.groups[1].name);
    CHECK("Desktop Action Create" == o_file.groups[2].name);
    CHECK("Application" == o_file.type());
    CHECK("Foo Viewer" == o_file.value("Desktop Entry", "Name").value_or(""));
    CHECK("fooview %F" == o_file.value("Desktop Entry", "Exec").value_or(""));

    const std::vector<std::string> o_actions = o_file.list_value("Desktop Entry", "Actions");
    CHECK(2 == o_actions.size());
    CHECK("Gallery" == o_actions[0]);
    CHECK("Create" == o_actions[1]);

    const std::vector<std::string> o_mime_types = o_file.list_value("Desktop Entry", "MimeType");
    CHECK(1 == o_mime_types.size());
    CHECK("image/x-foo" == o_mime_types[0]);

    CHECK(!has_error(desktop_entry_validate(o_file)));
}

void test_localized_selection() {
    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;
    CHECK(desktop_entry_reader_c::parse_text(LOCALIZED, o_file, o_diagnostics));
    CHECK("Foo YU" == o_file.value("Desktop Entry", "Name", "sr_YU").value_or(""));
    CHECK("Foo YU" == o_file.value("Desktop Entry", "Name", "sr_YU@Latn").value_or(""));
    CHECK("Foo SR" == o_file.value("Desktop Entry", "Name", "sr_RS").value_or(""));
    CHECK("Foo FR" == o_file.value("Desktop Entry", "Name", "fr").value_or(""));
    CHECK("Foo Latn" == o_file.value("Desktop Entry", "Name", "sr@Latn").value_or(""));
    CHECK("Foo" == o_file.value("Desktop Entry", "Name", "de").value_or(""));

    const std::vector<std::string> o_candidates = locale_match_candidates("sr_YU@Latn");
    CHECK(4 == o_candidates.size());
    CHECK("sr_YU@Latn" == o_candidates[0]);
    CHECK("sr_YU" == o_candidates[1]);
    CHECK("sr@Latn" == o_candidates[2]);
    CHECK("sr" == o_candidates[3]);
}

void test_escapes_and_lists() {
    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;
    CHECK(desktop_entry_reader_c::parse_text(ESCAPES, o_file, o_diagnostics));
    CHECK("Tab\tNew\nLine" == o_file.value("Desktop Entry", "Name").value_or(""));

    const std::vector<std::string> o_keywords = o_file.list_value("Desktop Entry", "Keywords");
    CHECK(3 == o_keywords.size());
    CHECK("a;b" == o_keywords[0]);
    CHECK("c" == o_keywords[1]);
    CHECK("d" == o_keywords[2]);

    const std::vector<std::string> o_empty = o_file.list_value("Desktop Entry", "X-Empty");
    CHECK(1 == o_empty.size());
    CHECK("" == o_empty[0]);

    CHECK(o_file.bool_value("Desktop Entry", "NoDisplay").value_or(false));
}

void test_validation() {
    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;

    CHECK(desktop_entry_reader_c::parse_text(
        "[Desktop Entry]\nType=Application\nExec=foo\n", o_file, o_diagnostics));
    CHECK(has_error(desktop_entry_validate(o_file)));

    CHECK(desktop_entry_reader_c::parse_text(
        "[Desktop Entry]\nType=Application\nName=X\n", o_file, o_diagnostics));
    CHECK(has_error(desktop_entry_validate(o_file)));

    CHECK(desktop_entry_reader_c::parse_text(
        "[Desktop Entry]\nType=Application\nName=X\nDBusActivatable=true\n", o_file,
        o_diagnostics));
    CHECK(!has_error(desktop_entry_validate(o_file)));

    CHECK(desktop_entry_reader_c::parse_text(
        "[Desktop Entry]\nType=Link\nName=X\n", o_file, o_diagnostics));
    CHECK(has_error(desktop_entry_validate(o_file)));

    CHECK(desktop_entry_reader_c::parse_text(
        "[Desktop Entry]\nType=Application\nName=X\nExec=x\n\n[Desktop Action Orphan]\nName=O\n",
        o_file, o_diagnostics));
    CHECK(has_warning(desktop_entry_validate(o_file)));

    CHECK(desktop_entry_reader_c::parse_text(
        "[Desktop Entry]\nType=Application\nName=X\nName=Y\nExec=x\n", o_file, o_diagnostics));
    CHECK(has_warning(o_diagnostics));
}

void test_malformed_input() {
    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;
    CHECK(!desktop_entry_reader_c::parse_text(
        "[Desktop Entry]\nType=Application\nthis line has no equals\n", o_file, o_diagnostics));
    CHECK(!o_diagnostics.empty());
    CHECK(3 == o_diagnostics[0].line_number);

    CHECK(!desktop_entry_reader_c::parse_text("Name=X\n", o_file, o_diagnostics));

    CHECK(!desktop_entry_reader_c::parse_text(
        std::string("[Desktop Entry]\nName=\xff\xfe\n"), o_file, o_diagnostics));
}

void test_comments_and_bom() {
    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;
    CHECK(desktop_entry_reader_c::parse_text(
        "# a comment\n[Desktop Entry]\n\nType=Application\nName=X\nExec=x\n", o_file,
        o_diagnostics));
    CHECK(6 == o_file.lines.size());
    CHECK("# a comment" == o_file.lines[0]);

    CHECK(desktop_entry_reader_c::parse_text(
        std::string("\xef\xbb\xbf[Desktop Entry]\nType=Application\nName=X\nExec=x\n"), o_file,
        o_diagnostics));
    CHECK(nullptr != o_file.desktop_entry_group());
}

void test_exec_field_codes() {
    const std::vector<std::string> o_codes = exec_field_codes("fooview %F --icon %i %%");
    CHECK(3 == o_codes.size());
    CHECK("%F" == o_codes[0]);
    CHECK("%i" == o_codes[1]);
    CHECK("%%" == o_codes[2]);

    const std::vector<std::string> o_deprecated = exec_deprecated_field_codes("foo %d %n %F");
    CHECK(2 == o_deprecated.size());
    CHECK("%d" == o_deprecated[0]);
    CHECK("%n" == o_deprecated[1]);
}

void test_sample_files(const std::string &s_repository_root) {
    desktop_entry_file_o o_file;
    std::vector<desktop_entry_diagnostic_o> o_diagnostics;
    const std::string s_path =
        s_repository_root + "/dataflow.in/desktop/spec-example.desktop";
    if (!desktop_entry_reader_c::parse_file(s_path, o_file, o_diagnostics)) {
        std::fprintf(stderr, "FAIL cannot read %s\n", s_path.c_str());
        g_count_failure++;
        return;
    }
    CHECK("Foo Viewer" == o_file.value("Desktop Entry", "Name").value_or(""));
    CHECK(3 == o_file.groups.size());
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    test_spec_example();
    test_localized_selection();
    test_escapes_and_lists();
    test_validation();
    test_malformed_input();
    test_comments_and_bom();
    test_exec_field_codes();
    if (2 <= i_argument_count) {
        test_sample_files(p_arguments[1]);
    }

    if (0 != g_count_failure) {
        std::fprintf(stderr, "desktop-entry-reader-test: %d failure(s)\n", g_count_failure);
        return 1;
    }
    std::printf("desktop-entry-reader-test: all checks passed\n");
    return 0;
}
