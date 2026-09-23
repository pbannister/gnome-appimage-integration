#!/usr/bin/env python3
"""Graphical AppImage handler.

Invoked as:  appimage_handler_ui.py --tool <appimage-integrate> <AppImage>

A single-window application, sized like a dialog.  The window shows what the
AppImage is, a row of actions, and one large text area that Inspect and Integrate
write into.  Every window remembers its size, and its position where the platform
allows it.

All real work is delegated to the `appimage-integrate` CLI, so this file only
presents information and choices.
"""

import json
import os
import shutil
import subprocess
import sys

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import Gio, GLib, Gtk  # noqa: E402

MINIMUM_WIDTH = 420
MINIMUM_HEIGHT = 320
DEFAULT_WIDTH = 660
DEFAULT_HEIGHT = 720


def state_directory():
    data_home = os.environ.get("XDG_DATA_HOME") or os.path.join(
        os.path.expanduser("~"), ".local", "share"
    )
    directory = os.path.join(data_home, "gnome-appimage-integration")
    os.makedirs(directory, exist_ok=True)
    return directory


def work_area():
    """The union of the monitor work areas as (x, y, width, height), or None.

    None means the platform did not report a usable area, in which case the
    caller must not clamp anything.
    """
    try:
        from gi.repository import Gdk

        display = Gdk.Display.get_default()
        if display is None:
            return None
        monitors = display.get_monitors()
        count = monitors.get_n_items()
        if count <= 0:
            return None
        left = top = right = bottom = None
        for index in range(count):
            rectangle = monitors.get_item(index).get_workarea()
            if left is None:
                left, top = rectangle.x, rectangle.y
                right = rectangle.x + rectangle.width
                bottom = rectangle.y + rectangle.height
            else:
                left = min(left, rectangle.x)
                top = min(top, rectangle.y)
                right = max(right, rectangle.x + rectangle.width)
                bottom = max(bottom, rectangle.y + rectangle.height)
        if left is None:
            return None
        return left, top, right - left, bottom - top
    except Exception:
        return None


def x11_window_id(window):
    """The X11 window id, or None when the platform does not expose one."""
    try:
        gi.require_version("GdkX11", "4.0")
        from gi.repository import GdkX11

        surface = window.get_surface()
        if surface is None:
            return None
        return GdkX11.X11Surface.get_xid(surface)
    except Exception:
        return None


def x11_position(window):
    """Read the window position on X11 when xdotool is available."""
    window_id = x11_window_id(window)
    if window_id is None or shutil.which("xdotool") is None:
        return None
    try:
        completed = subprocess.run(
            ["xdotool", "getwindowgeometry", "--shell", str(window_id)],
            capture_output=True,
            text=True,
            check=False,
        )
        values = {}
        for line in completed.stdout.splitlines():
            if "=" in line:
                key, _, value = line.partition("=")
                values[key.strip()] = value.strip()
        if "X" in values and "Y" in values:
            return int(values["X"]), int(values["Y"])
    except Exception:
        pass
    return None


def x11_move(window, x, y):
    """Move the window on X11 when xdotool is available."""
    window_id = x11_window_id(window)
    if window_id is None or shutil.which("xdotool") is None:
        return False
    try:
        subprocess.run(
            ["xdotool", "windowmove", str(window_id), str(x), str(y)],
            capture_output=True,
            check=False,
        )
        return True
    except Exception:
        return False


def center_window(window):
    """Centre the window where the platform allows it.

    Wayland does not let a client choose its position, so there this does
    nothing and the compositor places the window.
    """
    if shutil.which("xdotool") is None or x11_window_id(window) is None:
        return False
    area = work_area()
    if area is None:
        return False
    area_x, area_y, area_width, area_height = area
    try:
        width = window.get_width() or DEFAULT_WIDTH
        height = window.get_height() or DEFAULT_HEIGHT
    except Exception:
        width, height = DEFAULT_WIDTH, DEFAULT_HEIGHT
    x = area_x + max(0, (area_width - width) // 2)
    y = area_y + max(0, (area_height - height) // 2)
    return x11_move(window, x, y)


class Geometry:
    """Remembers the window's size, and its position where the platform allows."""

    def __init__(self):
        self.path = os.path.join(state_directory(), "ui.json")
        self.data = self._load()

    def _load(self):
        try:
            with open(self.path, encoding="utf-8") as handle:
                loaded = json.load(handle)
        except Exception:
            return {}
        if not isinstance(loaded, dict):
            return {}
        # Migrate the first format, which stored one size at the top level.
        if "width" in loaded and "main" not in loaded:
            return {"main": {"width": loaded.get("width"), "height": loaded.get("height")}}
        return loaded

    def _store(self):
        try:
            with open(self.path, "w", encoding="utf-8") as handle:
                json.dump(self.data, handle, indent=2, sort_keys=True)
        except Exception:
            pass

    def entry(self, key):
        value = self.data.get(key)
        return value if isinstance(value, dict) else {}

    def save(self, key, window):
        entry = dict(self.entry(key))
        try:
            width = window.get_width()
            height = window.get_height()
            if width >= MINIMUM_WIDTH and height >= MINIMUM_HEIGHT:
                entry["width"] = width
                entry["height"] = height
        except Exception:
            pass
        position = x11_position(window)
        if position is not None:
            entry["x"], entry["y"] = position
        if entry != self.entry(key):
            self.data[key] = entry
            self._store()

    def apply(self, key, window):
        entry = self.entry(key)
        width, height = DEFAULT_WIDTH, DEFAULT_HEIGHT
        try:
            width = int(entry.get("width", 0)) or DEFAULT_WIDTH
            height = int(entry.get("height", 0)) or DEFAULT_HEIGHT
        except Exception:
            width, height = DEFAULT_WIDTH, DEFAULT_HEIGHT
        # The remembered size is honoured; only a corrupt value is bounded.
        width = max(MINIMUM_WIDTH, min(width, 16384))
        height = max(MINIMUM_HEIGHT, min(height, 16384))
        window.set_default_size(width, height)

        area = work_area()
        if area is not None:
            # Record what the platform reported, so the clamp is never a mystery.
            entry["screen"] = {
                "x": area[0],
                "y": area[1],
                "width": area[2],
                "height": area[3],
            }
            if entry != self.entry(key):
                self.data[key] = entry
                self._store()

        x = entry.get("x")
        y = entry.get("y")
        if area is not None and isinstance(x, int) and isinstance(y, int):
            # Keep the whole window on the screen.
            x = max(area[0], min(x, area[0] + area[2] - width))
            y = max(area[1], min(y, area[1] + area[3] - height))
            GLib.idle_add(lambda: (x11_move(window, x, y), False)[1])
        else:
            # No usable position: behave like a dialog and ask to be centred.
            GLib.idle_add(lambda: (center_window(window), False)[1])

    def watch(self, key, window):
        self.apply(key, window)

        def on_change(_window, _parameter):
            self.save(key, window)
            return False

        for parameter in ("default-width", "default-height", "maximized"):
            try:
                window.connect("notify::%s" % parameter, on_change)
            except Exception:
                pass
        window.connect("close-request", lambda _window: (self.save(key, window), False)[1])

        # Autosave, so a size is never lost when the close path does not run.
        GLib.timeout_add_seconds(2, lambda: (self.save(key, window), True)[1])


def run_tool(tool, arguments):
    """Run the CLI and return (returncode, stdout, stderr)."""
    try:
        completed = subprocess.run(
            [tool] + arguments, capture_output=True, text=True, check=False
        )
        return completed.returncode, completed.stdout, completed.stderr
    except Exception as error:  # pragma: no cover - defensive
        return 1, "", str(error)


def combined_output(out, err):
    parts = []
    if out.strip():
        parts.append(out.strip())
    if err.strip():
        parts.append("--- error output ---\n" + err.strip())
    return "\n\n".join(parts) if parts else "(no output)"


def human_size(byte_count):
    value = float(byte_count or 0)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if value < 1024.0 or unit == "GiB":
            return "%.1f %s" % (value, unit)
        value /= 1024.0
    return "%d B" % byte_count


def escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def describe_conflict(index, conflict):
    lines = ["Existing launcher %d" % index]
    lines.append("  id:        %s" % (conflict.get("desktop_id") or "(unknown)"))
    lines.append("  name:      %s" % (conflict.get("name") or "(unknown)"))
    lines.append("  origin:    %s" % (conflict.get("origin") or "unknown"))
    if conflict.get("version"):
        lines.append("  version:   %s" % conflict["version"])
    if conflict.get("appimage"):
        presence = "present" if conflict.get("exec_exists") else "MISSING"
        lines.append("  appimage:  %s  [%s]" % (conflict["appimage"], presence))
    if conflict.get("icon"):
        lines.append("  icon:      %s" % conflict["icon"])
    if conflict.get("wm_class"):
        lines.append("  wm class:  %s" % conflict["wm_class"])
    lines.append("  file:      %s" % (conflict.get("path") or "(unknown)"))
    return "\n".join(lines)


class Handler:
    def __init__(self, application, tool, path):
        self.application = application
        self.tool = tool
        self.path = path
        self.geometry = Geometry()
        self.data = self.load_description()
        self.window = Gtk.ApplicationWindow(application=application, title="AppImage")
        self.build_content()
        self.geometry.watch("main", self.window)
        self.show_initial_buttons()
        self.window.present()

    # -- data ---------------------------------------------------------------
    def load_description(self):
        _code, out, err = run_tool(self.tool, ["explain", "--json", self.path])
        data = {}
        if out.strip():
            try:
                data = json.loads(out)
            except Exception:
                data = {}
        if not data:
            data = {
                "path": self.path,
                "name": os.path.basename(self.path),
                "error": (err or "cannot read this AppImage").strip(),
                "conflicts": [],
                "valid": False,
            }
        return data

    def conflicts(self):
        """Every existing launcher for this application, upgrades included."""
        return list(self.data.get("conflicts", []))

    def is_missing(self):
        return not os.path.exists(self.path)

    # -- widgets ------------------------------------------------------------
    def build_content(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(16)
        box.set_margin_bottom(16)
        box.set_margin_start(16)
        box.set_margin_end(16)

        name = self.data.get("name") or os.path.basename(self.path)
        title = Gtk.Label()
        title.set_markup("<span size='x-large' weight='bold'>%s</span>" % escape(name))
        title.set_xalign(0)
        title.set_wrap(True)
        box.append(title)

        subtitle = []
        if self.data.get("version"):
            subtitle.append(
                "Version %s  (%s)" % (self.data["version"], self.data.get("version_source", "?"))
            )
        if self.data.get("generic_name"):
            subtitle.append(self.data["generic_name"])
        if subtitle:
            label = Gtk.Label(label="\n".join(subtitle))
            label.set_xalign(0)
            label.set_wrap(True)
            box.append(label)

        if self.data.get("comment"):
            comment = Gtk.Label(label=self.data["comment"])
            comment.set_xalign(0)
            comment.set_wrap(True)
            box.append(comment)

        box.append(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL))

        self.details_container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        box.append(self.details_container)
        self.update_details()

        if self.conflicts():
            notice = Gtk.Label()
            notice.set_markup(
                "<b>This application is already installed.</b>\n"
                "Integrate will show details, then offer to replace or add alongside."
            )
            notice.set_xalign(0)
            notice.set_wrap(True)
            box.append(notice)

        if self.data.get("error"):
            error_label = Gtk.Label(label=self.data["error"])
            error_label.set_xalign(0)
            error_label.set_wrap(True)
            box.append(error_label)

        self.button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        self.button_box.set_halign(Gtk.Align.END)
        box.append(self.button_box)

        scroller = Gtk.ScrolledWindow()
        scroller.set_vexpand(True)
        scroller.set_hexpand(True)
        scroller.set_size_request(-1, MINIMUM_HEIGHT)
        self.text_view = Gtk.TextView()
        self.text_view.set_editable(False)
        self.text_view.set_monospace(True)
        self.text_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        scroller.set_child(self.text_view)
        box.append(scroller)

        self.window.set_child(box)

    def update_details(self):
        """Rebuild the details rows, so File follows the AppImage when it moves."""
        child = self.details_container.get_first_child()
        while child is not None:
            following = child.get_next_sibling()
            self.details_container.remove(child)
            child = following
        details = Gtk.Grid(column_spacing=12, row_spacing=4)
        row = 0
        for key, value in self.detail_rows():
            key_label = Gtk.Label(label=key)
            key_label.set_xalign(0)
            key_label.add_css_class("dim-label")
            value_label = Gtk.Label(label=value)
            value_label.set_xalign(0)
            value_label.set_wrap(True)
            value_label.set_selectable(True)
            details.attach(key_label, 0, row, 1, 1)
            details.attach(value_label, 1, row, 1, 1)
            row += 1
        self.details_container.append(details)

    def detail_rows(self):
        rows = [
            ("File", self.path),
            ("Size", human_size(self.data.get("file_size", 0))),
        ]
        if self.data.get("desktop_id"):
            rows.append(("Will install as", self.data["desktop_id"]))
        if self.is_missing():
            rows.append(("Note", "this file is no longer at that path"))
        return rows

    def set_buttons(self, specs):
        child = self.button_box.get_first_child()
        while child is not None:
            following = child.get_next_sibling()
            self.button_box.remove(child)
            child = following
        for label, callback in specs:
            button = Gtk.Button(label=label)
            button.connect("clicked", callback)
            self.button_box.append(button)

    def show_initial_buttons(self):
        self.set_buttons(
            [
                ("Run once", self.on_run_once),
                ("Integrate", self.on_integrate),
                ("Inspect", self.on_inspect),
                ("Close", self.on_close),
            ]
        )

    def set_text(self, text):
        self.text_view.get_buffer().set_text(text or "")

    # -- actions ------------------------------------------------------------
    def on_close(self, _button):
        self.geometry.save("main", self.window)
        self.window.close()

    def start_appimage(self):
        _code, out, err = run_tool(self.tool, ["run", "--detached", self.path])
        return combined_output(out, err)

    def on_run_once(self, _button):
        if self.is_missing():
            self.set_text(
                "This AppImage is no longer at\n  %s\n\n"
                "It has probably been integrated already.\n"
                "Look for \"%s\" in the application menu."
                % (self.path, self.data.get("name") or "the application")
            )
            return
        self.set_text(self.start_appimage())

    def on_inspect(self, _button):
        _code, out, err = run_tool(self.tool, ["explain", self.path])
        self.set_text(combined_output(out, err))

    def on_integrate(self, _button):
        if self.is_missing():
            self.set_text(
                "This AppImage is no longer at\n  %s\n\n"
                "It has probably been integrated already.\n"
                "Look for \"%s\" in the application menu."
                % (self.path, self.data.get("name") or "the application")
            )
            return
        conflicts = self.conflicts()
        if conflicts:
            lines = [
                "%s is already installed:" % (self.data.get("name") or "This AppImage"),
                "",
            ]
            for index, conflict in enumerate(conflicts, start=1):
                lines.append(describe_conflict(index, conflict))
                lines.append("")
            if self.data.get("version"):
                lines.append("This AppImage is version %s." % self.data["version"])
                lines.append("")
            if all(conflict.get("upgrade") for conflict in conflicts):
                lines.append("Replace existing upgrades that launcher in place.")
                lines.append(
                    "Add alongside keeps it and installs this version under a new identifier."
                )
            else:
                lines.append(
                    "Replace existing backs those launchers up and installs this version in their place."
                )
                lines.append(
                    "Add alongside keeps them and installs this version under a new identifier."
                )
            self.set_text("\n".join(lines))
            self.set_buttons(
                [
                    ("Back", self.on_back),
                    ("Add alongside", self.on_add_alongside),
                    ("Replace existing", self.on_replace_existing),
                ]
            )
            return
        self.run_install([])

    def on_back(self, _button):
        self.set_text("")
        self.show_initial_buttons()

    def on_add_alongside(self, _button):
        self.run_install(["--add"])

    def on_replace_existing(self, _button):
        self.run_install(["--replace"])

    def run_install(self, policy):
        code, out, err = run_tool(self.tool, ["install", "--yes"] + policy + [self.path])
        report = combined_output(out, err)
        if code == 0:
            # Integrate moves the AppImage; follow it so Run now and Inspect work.
            installed_path = self.data.get("installed") or ""
            self.data = self.load_description()
            if installed_path and os.path.exists(installed_path):
                self.path = installed_path
                self.data = self.load_description()
            self.update_details()
            self.set_text(report + "\n\nFile is now:\n  " + self.path)
            self.set_buttons(
                [
                    ("Run now", self.on_run_now),
                    ("Inspect", self.on_inspect),
                    ("Close", self.on_close),
                ]
            )
        else:
            self.set_text("The integration did not complete.\n\n" + report)
            self.show_initial_buttons()

    def on_run_now(self, _button):
        self.set_text(self.start_appimage())


def main(argv):
    tool = None
    path = None
    index = 1
    while index < len(argv):
        if argv[index] == "--tool" and index + 1 < len(argv):
            tool = argv[index + 1]
            index += 2
            continue
        path = argv[index]
        index += 1
    if tool is None or path is None:
        print(
            "usage: appimage_handler_ui.py --tool <appimage-integrate> <AppImage>",
            file=sys.stderr,
        )
        return 2

    # GNOME matches a running window to appimage-handler.desktop by this name,
    # so the dock shows the handler icon instead of a generic one.
    GLib.set_prgname("appimage-handler")
    GLib.set_application_name("AppImage Handler")

    # NON_UNIQUE: a second launch opens its own window for its own AppImage,
    # instead of activating the already-running instance with the first path.
    application = Gtk.Application(
        application_id="us.bannister.appimage-handler",
        flags=Gio.ApplicationFlags.NON_UNIQUE,
    )
    holder = {}

    def on_activate(app):
        holder["handler"] = Handler(app, tool, path)

    application.connect("activate", on_activate)
    return application.run([argv[0]])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
