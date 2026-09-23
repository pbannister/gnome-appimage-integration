//
//	appimage_activator_ui.cpp: the graphical AppImage activator.
//
//	Invoked as:  appimage-activator --tool <appimage-integrate> <AppImage>
//
//	The right-click "Open With" item for an AppImage is named AppImage Activator.
//	This is a single-window GTK4 program, sized like a dialog: it shows what the
//	AppImage is, a row of actions, and one large text area that Inspect and
//	Integrate write into.  Every window remembers its size, and its position where
//	the platform allows it.  All real work is delegated to the appimage-integrate
//	command line, so this program only presents information and choices.
//
//	Two options exist only so the test can drive the window without a person:
//	--activate runs an action as if its button had been clicked, and --set-name
//	types into the Name field.  Neither is used by the desktop entry.
//
#include <gtk/gtk.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(GDK_WINDOWING_X11)
#include <gdk/x11/gdkx.h>
#endif

#include "json/json_reader.h"
#include "version/version.h"

namespace {

namespace fs = std::filesystem;
using gnome_appimage::json::value_c;

constexpr int MINIMUM_WIDTH = 420;
constexpr int MINIMUM_HEIGHT = 320;
constexpr int DEFAULT_WIDTH = 660;
constexpr int DEFAULT_HEIGHT = 720;
constexpr int MAXIMUM_REMEMBERED_SIZE = 16384;
// Autosave, so a size is never lost when the close path does not run.
constexpr unsigned int AUTOSAVE_SECONDS = 2;

std::string environment_value(const char *s_name) {
    const char *s_value = std::getenv(s_name);
    return nullptr == s_value ? std::string() : std::string(s_value);
}

std::string join_path(const std::string &s_directory, const std::string &s_name) {
    if (s_directory.empty()) {
        return s_name;
    }
    if ('/' == s_directory.back()) {
        return s_directory + s_name;
    }
    return s_directory + "/" + s_name;
}

// Where the activator keeps its own state, beside the tool's records.
std::string state_directory() {
    std::string s_data_home = environment_value("XDG_DATA_HOME");
    if (s_data_home.empty()) {
        const std::string s_home = environment_value("HOME");
        s_data_home = s_home.empty() ? std::string(".") : join_path(s_home, ".local/share");
    }
    const std::string s_directory = join_path(s_data_home, "gnome-appimage-integration");
    std::error_code o_error;
    fs::create_directories(s_directory, o_error);
    return s_directory;
}

std::string read_text_file(const std::string &s_path) {
    std::ifstream o_input(s_path);
    if (!o_input) {
        return {};
    }
    std::ostringstream o_buffer;
    o_buffer << o_input.rdbuf();
    return o_buffer.str();
}

bool write_text_file(const std::string &s_path, const std::string &s_text) {
    std::ofstream o_output(s_path, std::ios::trunc);
    if (!o_output) {
        return false;
    }
    o_output << s_text;
    return o_output.good();
}

bool command_exists(const std::string &s_command) {
    if (s_command.empty()) {
        return false;
    }
    if (std::string::npos != s_command.find('/')) {
        return 0 == access(s_command.c_str(), X_OK);
    }
    const char *s_path = std::getenv("PATH");
    if (nullptr == s_path) {
        return false;
    }
    std::string s_remaining = s_path;
    while (!s_remaining.empty()) {
        const std::size_t i_separator = s_remaining.find(':');
        const std::string s_directory = std::string::npos == i_separator
                                            ? s_remaining
                                            : s_remaining.substr(0, i_separator);
        const std::string s_candidate =
            join_path(s_directory.empty() ? "." : s_directory, s_command);
        if (0 == access(s_candidate.c_str(), X_OK)) {
            return true;
        }
        if (std::string::npos == i_separator) {
            break;
        }
        s_remaining = s_remaining.substr(i_separator + 1);
    }
    return false;
}

std::string trim_spaces(const std::string &s_text) {
    std::size_t i_begin = 0;
    std::size_t i_end = s_text.size();
    while (i_begin < i_end && ' ' == s_text[i_begin]) {
        i_begin++;
    }
    while (i_begin < i_end && ' ' == s_text[i_end - 1]) {
        i_end--;
    }
    return s_text.substr(i_begin, i_end - i_begin);
}

// The result of running the command-line tool.
struct process_result_o {
    int exit_code = -1;
    std::string out;
    std::string err;
};

// Run a program without a shell, capturing stdout and stderr separately.  Both
// pipes are drained together, so a large report on one stream cannot deadlock the
// other.
process_result_o run_tool(const std::string &s_tool, const std::vector<std::string> &o_arguments) {
    process_result_o o_result;
    int i_out_pipe[2] = {-1, -1};
    int i_err_pipe[2] = {-1, -1};
    if (0 != pipe(i_out_pipe) || 0 != pipe(i_err_pipe)) {
        o_result.err = std::strerror(errno);
        return o_result;
    }
    const pid_t i_child = fork();
    if (0 > i_child) {
        o_result.err = std::strerror(errno);
        close(i_out_pipe[0]);
        close(i_out_pipe[1]);
        close(i_err_pipe[0]);
        close(i_err_pipe[1]);
        return o_result;
    }
    if (0 == i_child) {
        dup2(i_out_pipe[1], STDOUT_FILENO);
        dup2(i_err_pipe[1], STDERR_FILENO);
        close(i_out_pipe[0]);
        close(i_out_pipe[1]);
        close(i_err_pipe[0]);
        close(i_err_pipe[1]);
        std::vector<char *> o_raw;
        o_raw.reserve(o_arguments.size() + 2);
        o_raw.push_back(const_cast<char *>(s_tool.c_str()));
        for (const std::string &s_argument : o_arguments) {
            o_raw.push_back(const_cast<char *>(s_argument.c_str()));
        }
        o_raw.push_back(nullptr);
        execvp(o_raw[0], o_raw.data());
        _exit(127);
    }
    close(i_out_pipe[1]);
    close(i_err_pipe[1]);
    struct pollfd o_poll[2];
    o_poll[0].fd = i_out_pipe[0];
    o_poll[0].events = POLLIN;
    o_poll[1].fd = i_err_pipe[0];
    o_poll[1].events = POLLIN;
    bool b_out_open = true;
    bool b_err_open = true;
    char s_buffer[4096];
    while (b_out_open || b_err_open) {
        o_poll[0].revents = 0;
        o_poll[1].revents = 0;
        if (0 > poll(o_poll, 2, -1)) {
            if (EINTR == errno) {
                continue;
            }
            break;
        }
        for (int i_index = 0; i_index < 2; i_index++) {
            const bool b_is_out = 0 == i_index;
            if ((b_is_out ? !b_out_open : !b_err_open) || 0 == o_poll[i_index].revents) {
                continue;
            }
            if (0 != (o_poll[i_index].revents & (POLLIN | POLLHUP))) {
                const ssize_t i_count = read(o_poll[i_index].fd, s_buffer, sizeof(s_buffer));
                if (0 < i_count) {
                    (b_is_out ? o_result.out : o_result.err)
                        .append(s_buffer, static_cast<std::size_t>(i_count));
                    continue;
                }
                close(o_poll[i_index].fd);
                o_poll[i_index].fd = -1;
                (b_is_out ? b_out_open : b_err_open) = false;
            }
        }
    }
    int i_status = 0;
    while (0 > waitpid(i_child, &i_status, 0) && EINTR == errno) {
    }
    if (WIFEXITED(i_status)) {
        o_result.exit_code = WEXITSTATUS(i_status);
    } else if (WIFSIGNALED(i_status)) {
        o_result.exit_code = 128 + WTERMSIG(i_status);
    }
    return o_result;
}

std::string combined_output(const process_result_o &o_result) {
    const std::string s_out = trim_spaces(o_result.out);
    const std::string s_err = trim_spaces(o_result.err);
    std::string s_combined;
    if (!s_out.empty()) {
        s_combined = s_out;
    }
    if (!s_err.empty()) {
        if (!s_combined.empty()) {
            s_combined += "\n\n";
        }
        s_combined += "--- error output ---\n" + s_err;
    }
    return s_combined.empty() ? "(no output)" : s_combined;
}

std::string human_size(double d_byte_count) {
    static const char *const s_units[] = {"B", "KiB", "MiB", "GiB"};
    double d_value = 0 > d_byte_count ? 0 : d_byte_count;
    for (int i_unit = 0; i_unit < 4; i_unit++) {
        if (1024.0 > d_value || 3 == i_unit) {
            char s_buffer[64];
            std::snprintf(s_buffer, sizeof(s_buffer), "%.1f %s", d_value, s_units[i_unit]);
            return s_buffer;
        }
        d_value /= 1024.0;
    }
    return "0.0 B";
}

// A label's markup may not contain raw &, <, or >.
std::string escape_markup(const std::string &s_text) {
    std::string s_result;
    for (const char c_character : s_text) {
        switch (c_character) {
            case '&':
                s_result += "&amp;";
                break;
            case '<':
                s_result += "&lt;";
                break;
            case '>':
                s_result += "&gt;";
                break;
            default:
                s_result += c_character;
                break;
        }
    }
    return s_result;
}

std::string describe_conflict(int i_index, const value_c &o_conflict) {
    std::ostringstream o_text;
    o_text << "Existing launcher " << i_index << '\n';
    const std::string s_id = o_conflict.string_or("desktop_id");
    const std::string s_name = o_conflict.string_or("name");
    const std::string s_origin = o_conflict.string_or("origin");
    o_text << "  id:        " << (s_id.empty() ? "(unknown)" : s_id) << '\n';
    o_text << "  name:      " << (s_name.empty() ? "(unknown)" : s_name) << '\n';
    o_text << "  origin:    " << (s_origin.empty() ? "unknown" : s_origin) << '\n';
    if (o_conflict.boolean_or("repair", false)) {
        o_text << "  state:     the same AppImage, not where this launcher expects it\n";
    } else if (o_conflict.boolean_or("upgrade", false)) {
        o_text << "  state:     the same AppImage, already integrated here\n";
    } else {
        o_text << "  state:     a different AppImage for this application\n";
    }
    const std::string s_version = o_conflict.string_or("version");
    if (!s_version.empty()) {
        o_text << "  version:   " << s_version << '\n';
    }
    const std::string s_appimage = o_conflict.string_or("appimage");
    if (!s_appimage.empty()) {
        o_text << "  appimage:  " << s_appimage
               << (o_conflict.boolean_or("exec_exists", false) ? "  [present]" : "  [MISSING]")
               << '\n';
    }
    const std::string s_icon = o_conflict.string_or("icon");
    if (!s_icon.empty()) {
        o_text << "  icon:      " << s_icon << '\n';
    }
    const std::string s_wm_class = o_conflict.string_or("wm_class");
    if (!s_wm_class.empty()) {
        o_text << "  wm class:  " << s_wm_class << '\n';
    }
    const std::string s_path = o_conflict.string_or("path");
    o_text << "  file:      " << (s_path.empty() ? "(unknown)" : s_path) << '\n';
    return o_text.str();
}

// The tool's mode for an AppImage that is already where it belongs, with a record
// whose launcher runs it.  It mirrors integration_plan_o::mode.
constexpr const char *MODE_INTEGRATED = "properly integrated";

// The union of the monitor work areas, or no value when the platform does not
// report a usable area, in which case the caller must not clamp anything.
std::optional<GdkRectangle> work_area() {
    GdkDisplay *p_display = gdk_display_get_default();
    if (nullptr == p_display) {
        return std::nullopt;
    }
    GListModel *p_monitors = gdk_display_get_monitors(p_display);
    const guint u_count = nullptr == p_monitors ? 0 : g_list_model_get_n_items(p_monitors);
    if (0 == u_count) {
        return std::nullopt;
    }
    GdkRectangle o_union{};
    for (guint u_index = 0; u_index < u_count; u_index++) {
        GdkMonitor *p_monitor =
            static_cast<GdkMonitor *>(g_list_model_get_item(p_monitors, u_index));
        if (nullptr == p_monitor) {
            continue;
        }
        GdkRectangle o_area{};
        gdk_monitor_get_geometry(p_monitor, &o_area);
        if (0 == u_index) {
            o_union = o_area;
        } else {
            const int i_left = std::min(o_union.x, o_area.x);
            const int i_top = std::min(o_union.y, o_area.y);
            const int i_right = std::max(o_union.x + o_union.width, o_area.x + o_area.width);
            const int i_bottom = std::max(o_union.y + o_union.height, o_area.y + o_area.height);
            o_union.x = i_left;
            o_union.y = i_top;
            o_union.width = i_right - i_left;
            o_union.height = i_bottom - i_top;
        }
        g_object_unref(p_monitor);
    }
    return o_union;
}

// The X11 window id, or 0 outside X11.
unsigned long x11_window_id(GtkWindow *p_window) {
#if defined(GDK_WINDOWING_X11)
    GdkSurface *p_surface = gtk_native_get_surface(GTK_NATIVE(p_window));
    if (nullptr == p_surface || !GDK_IS_X11_SURFACE(p_surface)) {
        return 0;
    }
    return gdk_x11_surface_get_xid(p_surface);
#else
    static_cast<void>(p_window);
    return 0;
#endif
}

std::optional<std::pair<int, int>> x11_position(GtkWindow *p_window) {
    const unsigned long u_window_id = x11_window_id(p_window);
    if (0 == u_window_id || !command_exists("xdotool")) {
        return std::nullopt;
    }
    std::ostringstream o_command;
    o_command << "xdotool getwindowgeometry --shell " << u_window_id << " 2>/dev/null";
    FILE *p_pipe = popen(o_command.str().c_str(), "r");
    if (nullptr == p_pipe) {
        return std::nullopt;
    }
    std::map<std::string, std::string> o_values;
    char s_line[512];
    while (nullptr != std::fgets(s_line, sizeof(s_line), p_pipe)) {
        std::string s_text(s_line);
        while (!s_text.empty() && ('\n' == s_text.back() || '\r' == s_text.back())) {
            s_text.pop_back();
        }
        const std::size_t i_equals = s_text.find('=');
        if (std::string::npos == i_equals) {
            continue;
        }
        o_values[trim_spaces(s_text.substr(0, i_equals))] =
            trim_spaces(s_text.substr(i_equals + 1));
    }
    pclose(p_pipe);
    const auto o_x = o_values.find("X");
    const auto o_y = o_values.find("Y");
    if (o_values.end() == o_x || o_values.end() == o_y) {
        return std::nullopt;
    }
    return std::make_pair(std::atoi(o_x->second.c_str()), std::atoi(o_y->second.c_str()));
}

bool x11_move(GtkWindow *p_window, int i_x, int i_y) {
    const unsigned long u_window_id = x11_window_id(p_window);
    if (0 == u_window_id || !command_exists("xdotool")) {
        return false;
    }
    std::ostringstream o_command;
    o_command << "xdotool windowmove " << u_window_id << ' ' << i_x << ' ' << i_y << " 2>/dev/null";
    return 0 == std::system(o_command.str().c_str());
}

// Centre the window where the platform allows it.  Wayland does not let a client
// choose its position, so there the compositor places the window.
bool center_window(GtkWindow *p_window) {
    if (!command_exists("xdotool") || 0 == x11_window_id(p_window)) {
        return false;
    }
    const std::optional<GdkRectangle> o_area = work_area();
    if (!o_area.has_value()) {
        return false;
    }
    const int i_width = 0 < gtk_widget_get_width(GTK_WIDGET(p_window))
                            ? gtk_widget_get_width(GTK_WIDGET(p_window))
                            : DEFAULT_WIDTH;
    const int i_height = 0 < gtk_widget_get_height(GTK_WIDGET(p_window))
                             ? gtk_widget_get_height(GTK_WIDGET(p_window))
                             : DEFAULT_HEIGHT;
    const int i_x = o_area->x + std::max(0, (o_area->width - i_width) / 2);
    const int i_y = o_area->y + std::max(0, (o_area->height - i_height) / 2);
    return x11_move(p_window, i_x, i_y);
}

// Remembers the window's size, and its position where the platform allows it.
class geometry_c {
public:
    geometry_c() : s_path_(join_path(state_directory(), "ui.json")) {
        std::string s_error;
        o_data_ = value_c::parse(read_text_file(s_path_), s_error);
        if (!s_error.empty() || !o_data_.is_object()) {
            o_data_ = value_c::make_object();
        }
        // Migrate the first format, which stored one size at the top level.
        const value_c *p_width = o_data_.member("width");
        if (nullptr != p_width && nullptr == o_data_.member("main")) {
            value_c o_entry = value_c::make_object();
            const value_c *p_height = o_data_.member("height");
            o_entry.set("width", value_c::make_number(p_width->as_number()));
            o_entry.set("height",
                        value_c::make_number(nullptr == p_height ? 0 : p_height->as_number()));
            value_c o_migrated = value_c::make_object();
            o_migrated.set("main", o_entry);
            o_data_ = o_migrated;
            store();
        }
    }

    value_c entry(const std::string &s_key) const {
        const value_c *p_entry = o_data_.member(s_key);
        return nullptr != p_entry && p_entry->is_object() ? *p_entry : value_c::make_object();
    }

    void save(const std::string &s_key, GtkWindow *p_window) {
        value_c o_entry = entry(s_key);
        const int i_width = gtk_widget_get_width(GTK_WIDGET(p_window));
        const int i_height = gtk_widget_get_height(GTK_WIDGET(p_window));
        if (MINIMUM_WIDTH <= i_width && MINIMUM_HEIGHT <= i_height) {
            o_entry.set("width", value_c::make_number(i_width));
            o_entry.set("height", value_c::make_number(i_height));
        }
        const std::optional<std::pair<int, int>> o_position = x11_position(p_window);
        if (o_position.has_value()) {
            o_entry.set("x", value_c::make_number(o_position->first));
            o_entry.set("y", value_c::make_number(o_position->second));
        }
        set_entry(s_key, o_entry);
    }

    void apply(const std::string &s_key, GtkWindow *p_window) {
        value_c o_entry = entry(s_key);
        int i_width = static_cast<int>(o_entry.number_or("width", 0));
        int i_height = static_cast<int>(o_entry.number_or("height", 0));
        if (0 >= i_width) {
            i_width = DEFAULT_WIDTH;
        }
        if (0 >= i_height) {
            i_height = DEFAULT_HEIGHT;
        }
        // The remembered size is honoured; only a corrupt value is bounded.
        i_width = std::max(MINIMUM_WIDTH, std::min(i_width, MAXIMUM_REMEMBERED_SIZE));
        i_height = std::max(MINIMUM_HEIGHT, std::min(i_height, MAXIMUM_REMEMBERED_SIZE));
        gtk_window_set_default_size(p_window, i_width, i_height);

        const std::optional<GdkRectangle> o_area = work_area();
        if (o_area.has_value()) {
            // Record what the platform reported, so the clamp is never a mystery.
            value_c o_screen = value_c::make_object();
            o_screen.set("x", value_c::make_number(o_area->x));
            o_screen.set("y", value_c::make_number(o_area->y));
            o_screen.set("width", value_c::make_number(o_area->width));
            o_screen.set("height", value_c::make_number(o_area->height));
            o_entry.set("screen", o_screen);
            set_entry(s_key, o_entry);
        }

        const value_c *p_x = o_entry.member("x");
        const value_c *p_y = o_entry.member("y");
        if (o_area.has_value() && nullptr != p_x && p_x->is_number() && nullptr != p_y
            && p_y->is_number()) {
            // Keep the whole window on the screen.
            move_request_o *p_request = new move_request_o();
            p_request->p_window = p_window;
            p_request->i_x = std::max(o_area->x, std::min(static_cast<int>(p_x->as_number()),
                                                          o_area->x + o_area->width - i_width));
            p_request->i_y = std::max(o_area->y, std::min(static_cast<int>(p_y->as_number()),
                                                          o_area->y + o_area->height - i_height));
            g_idle_add(&move_window_idle, p_request);
        } else {
            // No usable position: behave like a dialog and ask to be centred.
            g_idle_add(&centre_window_idle, p_window);
        }
    }

private:
    struct move_request_o {
        GtkWindow *p_window = nullptr;
        int i_x = 0;
        int i_y = 0;
    };

    static gboolean move_window_idle(gpointer p_data) {
        move_request_o *p_request = static_cast<move_request_o *>(p_data);
        x11_move(p_request->p_window, p_request->i_x, p_request->i_y);
        delete p_request;
        return G_SOURCE_REMOVE;
    }

    static gboolean centre_window_idle(gpointer p_data) {
        center_window(GTK_WINDOW(p_data));
        return G_SOURCE_REMOVE;
    }

    void set_entry(const std::string &s_key, value_c o_entry) {
        const value_c *p_current = o_data_.member(s_key);
        if (nullptr != p_current && p_current->to_text() == o_entry.to_text()) {
            return;
        }
        o_data_.set(s_key, std::move(o_entry));
        store();
    }

    void store() const {
        write_text_file(s_path_, o_data_.to_text());
    }

    std::string s_path_;
    value_c o_data_ = value_c::make_object();
};

enum class action_e {
    run_once,
    integrate,
    inspect,
    close,
    back,
    add_alongside,
    replace_existing,
    run_now,
};

std::optional<action_e> action_from_name(const std::string &s_name) {
    if ("run-once" == s_name) {
        return action_e::run_once;
    }
    if ("integrate" == s_name) {
        return action_e::integrate;
    }
    if ("inspect" == s_name) {
        return action_e::inspect;
    }
    if ("close" == s_name) {
        return action_e::close;
    }
    if ("back" == s_name) {
        return action_e::back;
    }
    if ("add-alongside" == s_name) {
        return action_e::add_alongside;
    }
    if ("replace-existing" == s_name) {
        return action_e::replace_existing;
    }
    if ("run-now" == s_name) {
        return action_e::run_now;
    }
    return std::nullopt;
}

// The window: its widgets, the description it renders, and every action a button
// can take.
class activator_c {
public:
    // The three logs, in notebook order.
    enum class tab_e { status = 0, discovered = 1, actions = 2 };

    activator_c(GtkApplication *p_application, std::string s_tool, std::string s_path)
        : p_application_(p_application), s_tool_(std::move(s_tool)), s_path_(std::move(s_path)) {
        load_description();
        p_window_ = gtk_application_window_new(p_application_);
        gtk_window_set_title(GTK_WINDOW(p_window_), "AppImage");
        build_content();
        watch_geometry();
        show_initial_buttons();
        refresh_status();
        refresh_discovered();
        set_tab(tab_e::actions, "No action has changed the system yet.\n");
    }

    void present() {
        gtk_window_present(GTK_WINDOW(p_window_));
    }

    // Test hooks: act as if a button had been clicked, and type into Name.
    void perform(action_e e_action) {
        switch (e_action) {
            case action_e::run_once:
                on_run_once();
                return;
            case action_e::integrate:
                on_integrate();
                return;
            case action_e::inspect:
                on_inspect();
                return;
            case action_e::close:
                on_close();
                return;
            case action_e::back:
                on_back();
                return;
            case action_e::add_alongside:
                run_install({"--add"});
                return;
            case action_e::replace_existing:
                run_install({"--replace"});
                return;
            case action_e::run_now:
                on_run_now();
                return;
        }
    }

    void set_name_text(const std::string &s_text) {
        gtk_editable_set_text(GTK_EDITABLE(p_name_entry_), s_text.c_str());
    }

    // What the window holds, for a driven run: the test reads the logs rather than
    // the screen.
    void print_tabs(std::ostream &o_out) const {
        // Which page is up says what the window decided to show after an action.
        o_out << "=== showing: " << tab_name(static_cast<tab_e>(
                                         gtk_notebook_get_current_page(GTK_NOTEBOOK(p_notebook_))))
              << " ===\n";
        // The row as built, with * on the suggested action.
        o_out << "=== buttons:";
        for (GtkWidget *p_child = gtk_widget_get_first_child(p_button_box_);
             nullptr != p_child; p_child = gtk_widget_get_next_sibling(p_child)) {
            const char *s_label = gtk_button_get_label(GTK_BUTTON(p_child));
            o_out << ' ' << (nullptr == s_label ? "?" : s_label)
                  << (gtk_widget_has_css_class(p_child, "suggested-action") ? "*" : "");
        }
        o_out << " ===\n";
        o_out << "=== Status ===\n" << tab_text(tab_e::status);
        o_out << "=== Discovered ===\n" << tab_text(tab_e::discovered);
        o_out << "=== Actions ===\n" << tab_text(tab_e::actions);
    }

    static const char *tab_name(tab_e e_tab) {
        switch (e_tab) {
            case tab_e::status:
                return "Status";
            case tab_e::discovered:
                return "Discovered";
            case tab_e::actions:
                return "Actions";
        }
        return "unknown";
    }

private:
    // -- data ---------------------------------------------------------------
    void load_description() {
        const process_result_o o_result = run_tool(s_tool_, {"explain", "--json", s_path_});
        std::string s_error;
        if (!trim_spaces(o_result.out).empty()) {
            o_data_ = value_c::parse(o_result.out, s_error);
        }
        if (!s_error.empty() || !o_data_.is_object()) {
            o_data_ = value_c::make_object();
        }
        if (o_data_.members().empty()) {
            // Say something useful when the tool could not describe the file.
            const std::string s_message =
                trim_spaces(o_result.err).empty() ? "cannot read this AppImage"
                                                  : trim_spaces(o_result.err);
            o_data_.set("path", value_c::make_string(s_path_));
            o_data_.set("name", value_c::make_string(fs::path(s_path_).filename().string()));
            o_data_.set("error", value_c::make_string(s_message));
            o_data_.set("conflicts", value_c::make_array());
            o_data_.set("valid", value_c::make_boolean(false));
        }
    }

    std::vector<value_c> conflicts() const {
        return o_data_.array_or_empty("conflicts");
    }

    bool is_missing() const {
        std::error_code o_error;
        return !fs::exists(s_path_, o_error);
    }

    std::string display_name() const {
        const std::string s_name = o_data_.string_or("name");
        return s_name.empty() ? fs::path(s_path_).filename().string() : s_name;
    }

    // -- widgets ------------------------------------------------------------
    static GtkWidget *make_label(const std::string &s_text, bool b_wrap) {
        GtkWidget *p_label = gtk_label_new(nullptr);
        gtk_label_set_text(GTK_LABEL(p_label), s_text.c_str());
        gtk_label_set_xalign(GTK_LABEL(p_label), 0.0F);
        gtk_label_set_wrap(GTK_LABEL(p_label), b_wrap ? TRUE : FALSE);
        return p_label;
    }

    void build_content() {
        GtkWidget *p_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_margin_top(p_box, 16);
        gtk_widget_set_margin_bottom(p_box, 16);
        gtk_widget_set_margin_start(p_box, 16);
        gtk_widget_set_margin_end(p_box, 16);

        GtkWidget *p_title = gtk_label_new(nullptr);
        const std::string s_markup =
            "<span size='x-large' weight='bold'>" + escape_markup(display_name()) + "</span>";
        gtk_label_set_markup(GTK_LABEL(p_title), s_markup.c_str());
        gtk_label_set_xalign(GTK_LABEL(p_title), 0.0F);
        gtk_label_set_wrap(GTK_LABEL(p_title), TRUE);
        gtk_box_append(GTK_BOX(p_box), p_title);

        std::string s_subtitle;
        const std::string s_version = o_data_.string_or("version");
        if (!s_version.empty()) {
            std::string s_source = o_data_.string_or("version_source");
            if (s_source.empty()) {
                s_source = "?";
            }
            s_subtitle = "Version " + s_version + "  (" + s_source + ")";
        }
        const std::string s_generic_name = o_data_.string_or("generic_name");
        if (!s_generic_name.empty()) {
            if (!s_subtitle.empty()) {
                s_subtitle += "\n";
            }
            s_subtitle += s_generic_name;
        }
        if (!s_subtitle.empty()) {
            gtk_box_append(GTK_BOX(p_box), make_label(s_subtitle, true));
        }

        const std::string s_comment = o_data_.string_or("comment");
        if (!s_comment.empty()) {
            gtk_box_append(GTK_BOX(p_box), make_label(s_comment, true));
        }

        gtk_box_append(GTK_BOX(p_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

        p_details_container_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_append(GTK_BOX(p_box), p_details_container_);
        update_details();

        // The tool's own report, and the already-installed notice, are the Status
        // tab's business, not separate labels above the buttons.
        // The name the launcher will carry sits to the left of the buttons, and is
        // only shown when a choice is offered, because it only matters when another
        // launcher for the same application already exists.
        p_name_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(p_name_box_), gtk_label_new("Name:"));
        p_name_entry_ = gtk_entry_new();
        gtk_editable_set_width_chars(GTK_EDITABLE(p_name_entry_), 28);
        gtk_box_append(GTK_BOX(p_name_box_), p_name_entry_);
        gtk_widget_set_visible(p_name_box_, FALSE);

        p_button_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_halign(p_button_box_, GTK_ALIGN_END);

        GtkWidget *p_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand(p_spacer, TRUE);
        GtkWidget *p_action_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(p_action_row), p_name_box_);
        gtk_box_append(GTK_BOX(p_action_row), p_spacer);
        gtk_box_append(GTK_BOX(p_action_row), p_button_box_);
        gtk_box_append(GTK_BOX(p_box), p_action_row);

        // Three tabs of plain text: what the current state is, what was discovered
        // to deduce it, and what has been done to the system.  Status is the page
        // shown when the window opens.
        p_notebook_ = gtk_notebook_new();
        gtk_widget_set_vexpand(p_notebook_, TRUE);
        gtk_widget_set_hexpand(p_notebook_, TRUE);
        gtk_widget_set_size_request(p_notebook_, -1, MINIMUM_HEIGHT);
        add_text_page("Status", &p_status_view_, &p_status_buffer_);
        add_text_page("Discovered", &p_discovered_view_, &p_discovered_buffer_);
        add_text_page("Actions", &p_actions_view_, &p_actions_buffer_);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(p_notebook_), 0);
        gtk_box_append(GTK_BOX(p_box), p_notebook_);

        gtk_window_set_child(GTK_WINDOW(p_window_), p_box);
    }

    void add_text_page(const char *s_title, GtkWidget **p_view, GtkTextBuffer **p_buffer) {
        GtkWidget *p_scroller = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(p_scroller, TRUE);
        gtk_widget_set_hexpand(p_scroller, TRUE);
        *p_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(*p_view), FALSE);
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(*p_view), TRUE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(*p_view), GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_top_margin(GTK_TEXT_VIEW(*p_view), 6);
        gtk_text_view_set_left_margin(GTK_TEXT_VIEW(*p_view), 6);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(p_scroller), *p_view);
        *p_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(*p_view));
        gtk_notebook_append_page(GTK_NOTEBOOK(p_notebook_), p_scroller,
                                 gtk_label_new(s_title));
    }

    void clear_container(GtkWidget *p_container) {
        GtkWidget *p_child = gtk_widget_get_first_child(p_container);
        while (nullptr != p_child) {
            GtkWidget *p_following = gtk_widget_get_next_sibling(p_child);
            gtk_box_remove(GTK_BOX(p_container), p_child);
            p_child = p_following;
        }
    }

    std::vector<std::pair<std::string, std::string>> detail_rows() const {
        std::vector<std::pair<std::string, std::string>> o_rows;
        o_rows.emplace_back("File", s_path_);
        o_rows.emplace_back("Size", human_size(o_data_.number_or("file_size", 0)));
        // What Integrate would do with this file, in the tool's own words.
        const std::string s_mode = o_data_.string_or("mode");
        if (!s_mode.empty()) {
            o_rows.emplace_back("Integrate will", s_mode);
        }
        const std::string s_desktop_id = o_data_.string_or("desktop_id");
        if (!s_desktop_id.empty()) {
            o_rows.emplace_back("Will install as", s_desktop_id);
        }
        if (is_missing()) {
            o_rows.emplace_back("Note", "this file is no longer at that path");
        }
        return o_rows;
    }

    void update_details() {
        clear_container(p_details_container_);
        GtkWidget *p_grid = gtk_grid_new();
        gtk_grid_set_column_spacing(GTK_GRID(p_grid), 12);
        gtk_grid_set_row_spacing(GTK_GRID(p_grid), 4);
        int i_row = 0;
        for (const std::pair<std::string, std::string> &o_row : detail_rows()) {
            GtkWidget *p_key = make_label(o_row.first, false);
            gtk_widget_add_css_class(p_key, "dim-label");
            GtkWidget *p_value = make_label(o_row.second, true);
            gtk_label_set_selectable(GTK_LABEL(p_value), TRUE);
            gtk_grid_attach(GTK_GRID(p_grid), p_key, 0, i_row, 1, 1);
            gtk_grid_attach(GTK_GRID(p_grid), p_value, 1, i_row, 1, 1);
            i_row++;
        }
        gtk_box_append(GTK_BOX(p_details_container_), p_grid);
    }

    // One button: its label, what it does, and whether it is the suggested action.
    // The buttons are ordered by how likely the owner is to use them, and the GNOME
    // HIG's suggested-action style marks the next step in the process; it allows
    // only one per view.
    struct button_spec_o {
        std::string s_label;
        action_e e_action;
        bool b_suggested = false;
    };

    void set_buttons(const std::vector<button_spec_o> &o_specs) {
        clear_container(p_button_box_);
        for (const button_spec_o &o_spec : o_specs) {
            GtkWidget *p_button = gtk_button_new_with_label(o_spec.s_label.c_str());
            g_signal_connect(p_button, "clicked", G_CALLBACK(on_button_clicked), this);
            g_object_set_data(G_OBJECT(p_button), "activator-action",
                              GINT_TO_POINTER(static_cast<int>(o_spec.e_action)));
            if (o_spec.b_suggested) {
                gtk_widget_add_css_class(p_button, "suggested-action");
            }
            gtk_box_append(GTK_BOX(p_button_box_), p_button);
        }
    }

    static void on_button_clicked(GtkButton *p_button, gpointer p_data) {
        activator_c *p_activator = static_cast<activator_c *>(p_data);
        const int i_action =
            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(p_button), "activator-action"));
        p_activator->perform(static_cast<action_e>(i_action));
    }

    void show_initial_buttons() {
        set_buttons({{"Integrate", action_e::integrate, true},
                     {"Run once", action_e::run_once, false},
                     {"Inspect", action_e::inspect, false},
                     {"Close", action_e::close, false}});
    }

    // -- the three logs -----------------------------------------------------
    GtkTextBuffer *buffer_for(tab_e e_tab) const {
        switch (e_tab) {
            case tab_e::status:
                return p_status_buffer_;
            case tab_e::discovered:
                return p_discovered_buffer_;
            case tab_e::actions:
                return p_actions_buffer_;
        }
        return nullptr;
    }

    GtkWidget *view_for(tab_e e_tab) const {
        switch (e_tab) {
            case tab_e::status:
                return p_status_view_;
            case tab_e::discovered:
                return p_discovered_view_;
            case tab_e::actions:
                return p_actions_view_;
        }
        return nullptr;
    }

    std::string tab_text(tab_e e_tab) const {
        GtkTextBuffer *p_buffer = buffer_for(e_tab);
        if (nullptr == p_buffer) {
            return {};
        }
        GtkTextIter o_start;
        GtkTextIter o_end;
        gtk_text_buffer_get_bounds(p_buffer, &o_start, &o_end);
        char *s_text = gtk_text_buffer_get_text(p_buffer, &o_start, &o_end, FALSE);
        const std::string s_result = nullptr == s_text ? std::string() : std::string(s_text);
        g_free(s_text);
        return s_result;
    }

    void set_tab(tab_e e_tab, const std::string &s_text) {
        GtkTextBuffer *p_buffer = buffer_for(e_tab);
        if (nullptr != p_buffer) {
            gtk_text_buffer_set_text(p_buffer, s_text.c_str(), -1);
        }
    }

    void append_tab(tab_e e_tab, const std::string &s_text) {
        GtkTextBuffer *p_buffer = buffer_for(e_tab);
        GtkWidget *p_view = view_for(e_tab);
        if (nullptr == p_buffer) {
            return;
        }
        GtkTextIter o_end;
        gtk_text_buffer_get_end_iter(p_buffer, &o_end);
        gtk_text_buffer_insert(p_buffer, &o_end, s_text.c_str(), -1);
        if (nullptr != p_view) {
            // Keep the newest line in view.
            gtk_text_buffer_get_end_iter(p_buffer, &o_end);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(p_view), &o_end, 0.0, FALSE, 0.0, 1.0);
        }
    }

    void show_tab(tab_e e_tab) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(p_notebook_), static_cast<int>(e_tab));
    }

    void show_name_field() {
        gtk_editable_set_text(GTK_EDITABLE(p_name_entry_), display_name().c_str());
        gtk_widget_set_visible(p_name_box_, TRUE);
    }

    void hide_name_field() {
        gtk_widget_set_visible(p_name_box_, FALSE);
    }

    std::string typed_name() const {
        if (!gtk_widget_get_visible(p_name_box_)) {
            return {};
        }
        return trim_spaces(gtk_editable_get_text(GTK_EDITABLE(p_name_entry_)));
    }

    // -- geometry -----------------------------------------------------------
    void watch_geometry() {
        o_geometry_.apply("main", GTK_WINDOW(p_window_));
        for (const char *s_parameter :
             {"notify::default-width", "notify::default-height", "notify::maximized"}) {
            g_signal_connect(p_window_, s_parameter, G_CALLBACK(on_geometry_changed), this);
        }
        g_signal_connect(p_window_, "close-request", G_CALLBACK(on_close_request), this);
        g_timeout_add_seconds(AUTOSAVE_SECONDS, &on_autosave, this);
    }

    static void on_geometry_changed(GObject *p_object, GParamSpec *p_spec, gpointer p_data) {
        static_cast<void>(p_object);
        static_cast<void>(p_spec);
        activator_c *p_activator = static_cast<activator_c *>(p_data);
        p_activator->o_geometry_.save("main", GTK_WINDOW(p_activator->p_window_));
    }

    static gboolean on_close_request(GtkWindow *p_window, gpointer p_data) {
        activator_c *p_activator = static_cast<activator_c *>(p_data);
        p_activator->o_geometry_.save("main", p_window);
        return FALSE;
    }

    static gboolean on_autosave(gpointer p_data) {
        activator_c *p_activator = static_cast<activator_c *>(p_data);
        p_activator->o_geometry_.save("main", GTK_WINDOW(p_activator->p_window_));
        return G_SOURCE_CONTINUE;
    }

    // -- the logs -----------------------------------------------------------
    static std::string timestamp_now() {
        const std::time_t i_now = std::time(nullptr);
        std::tm o_local{};
        localtime_r(&i_now, &o_local);
        char s_buffer[32];
        std::strftime(s_buffer, sizeof(s_buffer), "%H:%M:%S", &o_local);
        return s_buffer;
    }

    // What the current state is, in the tool's own words where it has them.
    std::string compose_status() const {
        std::ostringstream o_text;
        if (!s_status_note_.empty()) {
            o_text << s_status_note_ << "\n\n";
        }
        const std::string s_mode = o_data_.string_or("mode");
        const std::string s_desktop_id = o_data_.string_or("desktop_id");
        o_text << "State:     " << (s_mode.empty() ? "unknown" : s_mode) << '\n';
        o_text << "Launcher:  " << (s_desktop_id.empty() ? "(none yet)" : s_desktop_id) << '\n';
        o_text << "File:      " << s_path_ << (is_missing() ? "  [MISSING]" : "") << '\n';

        const std::vector<value_c> o_conflicts = conflicts();
        if (MODE_INTEGRATED == s_mode) {
            // Nothing is wrong and nothing is left to do; do not offer to replace
            // what is already correct.
            o_text << "\nThis AppImage is properly integrated.\n"
                      "Nothing more to do: the launcher and its record already match it.\n";
        } else if (!o_conflicts.empty()) {
            o_text << "\nThis application is already installed.\n"
                      "Integrate will show details, then offer to replace or add alongside.\n";
        }
        // Whatever the tool reported about this file, in its own words: the state it
        // could not reach, or why it stopped.
        const std::string s_error = o_data_.string_or("error");
        if (!s_error.empty()) {
            o_text << "\n" << s_error << '\n';
        }
        if (!o_conflicts.empty()) {
            if (b_prompt_active_) {
                o_text << '\n';
                bool b_all_upgrade = true;
                bool b_any_repair = false;
                for (std::size_t i_index = 0; i_index < o_conflicts.size(); i_index++) {
                    o_text << describe_conflict(static_cast<int>(i_index) + 1, o_conflicts[i_index])
                           << '\n';
                    if (!o_conflicts[i_index].boolean_or("upgrade", false)) {
                        b_all_upgrade = false;
                    }
                    if (o_conflicts[i_index].boolean_or("repair", false)) {
                        b_any_repair = true;
                    }
                }
                const std::string s_version = o_data_.string_or("version");
                if (!s_version.empty()) {
                    o_text << "This AppImage is version " << s_version << ".\n\n";
                }
                if (b_all_upgrade) {
                    if (b_any_repair) {
                        o_text << "Replace existing repairs that launcher: the AppImage is not "
                                  "where the launcher expects it, so its Exec and TryExec are "
                                  "rewritten.\n";
                    } else if (MODE_INTEGRATED == s_mode) {
                        o_text << "Replace existing rewrites that launcher and its record in "
                                  "place; nothing is missing.\n";
                    } else {
                        o_text << "Replace existing updates that launcher in place.\n";
                    }
                    o_text << "Add alongside keeps it and installs this version under a new "
                              "identifier.\n";
                } else {
                    o_text << "Replace existing backs those launchers up and installs this "
                              "version in their place.\n";
                    o_text << "Add alongside keeps them and installs this version under a new "
                              "identifier.\n";
                }
            }
        }
        const std::string s_installed = o_data_.string_or("installed");
        if (!s_installed.empty() && s_installed != s_path_) {
            o_text << "\nThe managed copy lives at:\n  " << s_installed << '\n';
        }
        return o_text.str();
    }

    // What was discovered, and which command discovered it.
    std::string compose_discovered() const {
        std::ostringstream o_text;
        o_text << "appimage-integrate explain --json " << s_path_ << "\n\n";
        const auto add_fact = [&o_text](const std::string &s_label, const std::string &s_value) {
            o_text << "  " << s_label;
            for (std::size_t i_index = s_label.size(); i_index < 18; i_index++) {
                o_text << ' ';
            }
            o_text << s_value << '\n';
        };
        const auto number_text = [](const value_c &o_value) {
            std::ostringstream o_number;
            o_number << static_cast<long long>(o_value.as_number());
            return o_number.str();
        };
        add_fact("AppImage", s_path_);
        add_fact("Detection", o_data_.string_or("detection", "(unknown)"));
        const value_c *p_size = o_data_.member("file_size");
        if (nullptr != p_size && p_size->is_number()) {
            add_fact("Size", human_size(p_size->as_number()) + "  (" + number_text(*p_size)
                                 + " bytes)");
        }
        const value_c *p_payload = o_data_.member("payload_size");
        const value_c *p_offset = o_data_.member("payload_offset");
        if (nullptr != p_payload && p_payload->is_number()) {
            std::string s_payload = number_text(*p_payload) + " bytes";
            if (nullptr != p_offset && p_offset->is_number()) {
                s_payload = "offset " + number_text(*p_offset) + " bytes, size " + s_payload;
            }
            add_fact("Payload", s_payload);
        }
        const std::string s_compression = o_data_.string_or("compression");
        if (!s_compression.empty()) {
            add_fact("Compression", s_compression);
        }
        const std::string s_version = o_data_.string_or("version");
        if (!s_version.empty()) {
            add_fact("Version", s_version + "  (from " + o_data_.string_or("version_source", "?")
                                     + ")");
        }
        const std::string s_signature = o_data_.string_or("signature");
        add_fact("Signature", s_signature.empty() ? "(absent)" : s_signature);
        const std::string s_update = o_data_.string_or("update_information");
        if (!s_update.empty()) {
            add_fact("Update info", s_update);
        }
        add_fact("Identifier", o_data_.string_or("identifier", "(unknown)"));
        add_fact("Embedded entry", o_data_.string_or("embedded_desktop", "(unknown)"));
        add_fact("Desktop id", o_data_.string_or("desktop_id", "(unknown)"));
        add_fact("Installed path", o_data_.string_or("installed", "(unknown)"));
        add_fact("Icon", o_data_.string_or("icon_name", "(none)"));
        add_fact("Exec", o_data_.string_or("exec", "(none)"));
        add_fact("Window class", o_data_.string_or("startup_wm_class", "(none)"));
        const std::string s_mode = o_data_.string_or("mode");
        if (!s_mode.empty()) {
            add_fact("This run", s_mode);
        }
        const std::string s_error = o_data_.string_or("error");
        if (!s_error.empty()) {
            add_fact("Error", s_error);
        }

        const std::string s_entry = o_data_.string_or("desktop_entry");
        if (!s_entry.empty()) {
            o_text << "\nEmbedded desktop entry:\n";
            std::istringstream o_lines(s_entry);
            std::string s_line;
            while (std::getline(o_lines, s_line)) {
                o_text << "  " << s_line << '\n';
            }
        }

        const std::vector<value_c> o_conflicts = conflicts();
        o_text << "\nExisting launchers for this application:\n";
        if (o_conflicts.empty()) {
            o_text << "  (none found)\n";
        }
        for (std::size_t i_index = 0; i_index < o_conflicts.size(); i_index++) {
            std::istringstream o_lines(
                describe_conflict(static_cast<int>(i_index) + 1, o_conflicts[i_index]));
            std::string s_line;
            while (std::getline(o_lines, s_line)) {
                o_text << "  " << s_line << '\n';
            }
        }
        return o_text.str();
    }

    void refresh_status() {
        set_tab(tab_e::status, compose_status());
    }

    void refresh_discovered() {
        set_tab(tab_e::discovered, compose_discovered());
    }

    // Every entry in the Actions tab is a command that changed the system.
    void log_action(const std::string &s_command, const std::string &s_output) {
        if (b_actions_empty_) {
            set_tab(tab_e::actions, "");
            b_actions_empty_ = false;
        }
        std::ostringstream o_entry;
        o_entry << timestamp_now() << "  " << s_command << '\n';
        const std::string s_trimmed = trim_spaces(s_output);
        if (!s_trimmed.empty()) {
            std::istringstream o_lines(s_trimmed);
            std::string s_line;
            while (std::getline(o_lines, s_line)) {
                o_entry << "  " << s_line << '\n';
            }
        }
        o_entry << '\n';
        append_tab(tab_e::actions, o_entry.str());
        show_tab(tab_e::actions);
    }

    static std::string command_line(const std::string &s_tool,
                                    const std::vector<std::string> &o_arguments) {
        std::string s_result = s_tool;
        for (const std::string &s_argument : o_arguments) {
            s_result += ' ';
            if (std::string::npos != s_argument.find(' ')) {
                s_result += '"' + s_argument + '"';
            } else {
                s_result += s_argument;
            }
        }
        return s_result;
    }

    // -- actions ------------------------------------------------------------
    void on_close() {
        o_geometry_.save("main", GTK_WINDOW(p_window_));
        gtk_window_close(GTK_WINDOW(p_window_));
    }

    std::string start_appimage() {
        return combined_output(run_tool(s_tool_, {"run", "--detached", s_path_}));
    }

    std::string missing_message() const {
        return "This AppImage is no longer at\n  " + s_path_
               + "\nIt has probably been integrated already.\nLook for \"" + display_name()
               + "\" in the application menu.";
    }

    void show_missing_status() {
        s_status_note_ = missing_message();
        refresh_status();
        show_tab(tab_e::status);
    }

    void run_the_appimage() {
        const std::vector<std::string> o_arguments = {"run", "--detached", s_path_};
        const std::string s_report = combined_output(run_tool(s_tool_, o_arguments));
        log_action(command_line("appimage-integrate", o_arguments), s_report);
    }

    void on_run_once() {
        if (is_missing()) {
            show_missing_status();
            return;
        }
        run_the_appimage();
    }

    void on_run_now() {
        run_the_appimage();
    }

    void on_inspect() {
        const std::vector<std::string> o_arguments = {"explain", s_path_};
        const std::string s_report = combined_output(run_tool(s_tool_, o_arguments));
        append_tab(tab_e::discovered,
                   "\n" + timestamp_now() + "  " + command_line("appimage-integrate", o_arguments)
                       + "\n\n" + s_report + "\n");
        show_tab(tab_e::discovered);
    }

    void on_integrate() {
        if (is_missing()) {
            show_missing_status();
            return;
        }
        if (conflicts().empty()) {
            run_install({});
            return;
        }
        // The choice, and what each choice does, is status: it is what the window is
        // waiting for.
        b_prompt_active_ = true;
        s_status_note_.clear();
        refresh_status();
        show_tab(tab_e::status);
        show_name_field();
        // No suggested choice here: which of the two is likely depends on what the
        // launcher is, and Status explains both.
        set_buttons({{"Back", action_e::back, false},
                     {"Add alongside", action_e::add_alongside, false},
                     {"Replace existing", action_e::replace_existing, false}});
    }

    void on_back() {
        b_prompt_active_ = false;
        hide_name_field();
        refresh_status();
        show_initial_buttons();
        show_tab(tab_e::status);
    }

    void run_install(const std::vector<std::string> &o_policy) {
        // The typed name decides Name= in the launcher, so two launchers for one
        // application can be told apart in the menu.
        std::vector<std::string> o_arguments = {"install", "--yes"};
        o_arguments.insert(o_arguments.end(), o_policy.begin(), o_policy.end());
        const std::string s_typed = typed_name();
        if (!s_typed.empty()) {
            o_arguments.push_back("--name");
            o_arguments.push_back(s_typed);
        }
        o_arguments.push_back(s_path_);
        hide_name_field();
        const process_result_o o_result = run_tool(s_tool_, o_arguments);
        const std::string s_report = combined_output(o_result);
        log_action(command_line("appimage-integrate", o_arguments), s_report);
        // The log stays in Actions; what the owner needs next is the resulting state.
        show_tab(tab_e::status);
        if (0 == o_result.exit_code) {
            // Integrate moves the AppImage; follow it so Run now and Inspect work.
            const std::string s_installed = o_data_.string_or("installed");
            load_description();
            std::error_code o_error;
            if (!s_installed.empty() && fs::exists(s_installed, o_error)) {
                s_path_ = s_installed;
                load_description();
            }
            b_prompt_active_ = false;
            s_status_note_.clear();
            refresh_status();
            refresh_discovered();
            update_details();
            set_buttons({{"Run now", action_e::run_now, true},
                         {"Inspect", action_e::inspect, false},
                         {"Close", action_e::close, false}});
            return;
        }
        s_status_note_ = "The integration did not complete.";
        refresh_status();
        show_initial_buttons();
    }

    GtkApplication *p_application_ = nullptr;
    std::string s_tool_;
    std::string s_path_;
    value_c o_data_ = value_c::make_object();
    geometry_c o_geometry_;
    GtkWidget *p_window_ = nullptr;
    GtkWidget *p_details_container_ = nullptr;
    GtkWidget *p_name_box_ = nullptr;
    GtkWidget *p_name_entry_ = nullptr;
    GtkWidget *p_button_box_ = nullptr;
    GtkWidget *p_notebook_ = nullptr;
    GtkWidget *p_status_view_ = nullptr;
    GtkWidget *p_discovered_view_ = nullptr;
    GtkWidget *p_actions_view_ = nullptr;
    GtkTextBuffer *p_status_buffer_ = nullptr;
    GtkTextBuffer *p_discovered_buffer_ = nullptr;
    GtkTextBuffer *p_actions_buffer_ = nullptr;
    // Whether the conflict choice is on screen, and whether anything has been done
    // yet; both decide what the Status and Actions tabs say.
    bool b_prompt_active_ = false;
    bool b_actions_empty_ = true;
    std::string s_status_note_;
};

// What the activate handler needs to build the window, and what the test hooks
// need to drive it.
struct context_o {
    GtkApplication *p_application = nullptr;
    std::string s_tool;
    std::string s_path;
    std::string s_set_name;
    std::vector<action_e> o_actions;
    activator_c *p_activator = nullptr;
};

void on_activate(GtkApplication *p_application, gpointer p_data) {
    context_o *p_context = static_cast<context_o *>(p_data);
    p_context->p_activator = new activator_c(p_application, p_context->s_tool, p_context->s_path);
    p_context->p_activator->present();
    // A driven run types the name after each action, because the action is what
    // reveals the field: Integrate shows it prefilled, --set-name then replaces the
    // prefill with the value a person would have typed, and the next action uses it.
    for (const action_e e_action : p_context->o_actions) {
        p_context->p_activator->perform(e_action);
        if (action_e::close == e_action) {
            return;
        }
        if (!p_context->s_set_name.empty()) {
            p_context->p_activator->set_name_text(p_context->s_set_name);
        }
    }
    if (!p_context->o_actions.empty()) {
        // A driven run is a test: report what the window holds, then quit rather than
        // wait for a person to close it.
        p_context->p_activator->print_tabs(std::cout);
        g_application_quit(G_APPLICATION(p_application));
    }
}

void print_usage(std::ostream &o_out) {
    o_out << "usage: appimage-activator [--set-name TEXT] [--activate ACTION[,ACTION...]]\n"
          << "                           --tool <appimage-integrate> <AppImage>\n"
          << "\n"
          << "actions: run-once, integrate, inspect, close, back, add-alongside,\n"
          << "         replace-existing, run-now\n";
}

}  // namespace

int main(int i_argument_count, char **p_arguments) {
    std::string s_tool;
    std::string s_path;
    std::string s_set_name;
    std::string s_activate;
    for (int i_index = 1; i_index < i_argument_count; i_index++) {
        const std::string s_argument = p_arguments[i_index];
        if ("--tool" == s_argument && i_argument_count > i_index + 1) {
            s_tool = p_arguments[++i_index];
            continue;
        }
        if ("--set-name" == s_argument && i_argument_count > i_index + 1) {
            s_set_name = p_arguments[++i_index];
            continue;
        }
        if ("--activate" == s_argument && i_argument_count > i_index + 1) {
            s_activate = p_arguments[++i_index];
            continue;
        }
        if ("--help" == s_argument || "-h" == s_argument) {
            print_usage(std::cout);
            return 0;
        }
        s_path = s_argument;
    }
    if (s_tool.empty() || s_path.empty()) {
        print_usage(std::cerr);
        return 2;
    }

    // The window's Wayland application id is this program name, because no
    // Gtk.Application id is set: GTK uses the application id when there is one and
    // g_get_prgname() otherwise (gtk 4.14, gdk/wayland/gdktoplevel-wayland.c:874).
    // It must equal the desktop entry's file name, or GNOME cannot match the window
    // to its launcher and the dock shows a generic icon.
    g_set_prgname("appimage-activator");
    g_set_application_name("AppImage Activator");

    context_o o_context;
    o_context.s_tool = s_tool;
    o_context.s_path = s_path;
    o_context.s_set_name = s_set_name;
    std::size_t i_begin = 0;
    while (i_begin <= s_activate.size() && !s_activate.empty()) {
        const std::size_t i_comma = s_activate.find(',', i_begin);
        const std::string s_name = s_activate.substr(
            i_begin, std::string::npos == i_comma ? std::string::npos : i_comma - i_begin);
        if (!s_name.empty()) {
            const std::optional<action_e> e_action = action_from_name(s_name);
            if (!e_action.has_value()) {
                std::cerr << "error: unknown action: " << s_name << '\n';
                print_usage(std::cerr);
                return 2;
            }
            o_context.o_actions.push_back(*e_action);
        }
        if (std::string::npos == i_comma) {
            break;
        }
        i_begin = i_comma + 1;
    }

    // NON_UNIQUE: a second launch opens its own window for its own AppImage,
    // instead of activating the already-running instance with the first path.
    // No application id: a dotted id here would become the window's Wayland app id
    // and would no longer match appimage-activator.desktop.
    GtkApplication *p_application = gtk_application_new(nullptr, G_APPLICATION_NON_UNIQUE);
    o_context.p_application = p_application;
    g_signal_connect(p_application, "activate", G_CALLBACK(on_activate), &o_context);
    char *p_argv[] = {p_arguments[0], nullptr};
    const int i_status = g_application_run(G_APPLICATION(p_application), 1, p_argv);
    delete o_context.p_activator;
    g_object_unref(p_application);
    return i_status;
}
