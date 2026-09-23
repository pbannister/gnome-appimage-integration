#!/usr/bin/env python3
"""Graphical AppImage handler.

Invoked as:  appimage_handler_ui.py --tool <appimage-integrate> <AppImage>

The first window shows what the AppImage is (name, version, generic name, comment)
and offers Run once, Integrate, Inspect, and Close.  Every window remembers its
size, and its position where the platform allows it.

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
from gi.repository import GLib, Gtk  # noqa: E402

MINIMUM_WIDTH = 360
MINIMUM_HEIGHT = 280


def state_directory():
    data_home = os.environ.get("XDG_DATA_HOME") or os.path.join(
        os.path.expanduser("~"), ".local", "share"
    )
    directory = os.path.join(data_home, "gnome-appimage-integration")
    os.makedirs(directory, exist_ok=True)
    return directory


def work_area():
    """The usable screen rectangle as (x, y, width, height)."""
    try:
        from gi.repository import Gdk

        display = Gdk.Display.get_default()
        if display is None:
            raise RuntimeError("no display")
        monitors = display.get_monitors()
        monitor = monitors.get_item(0)
        rectangle = monitor.get_workarea()
        return rectangle.x, rectangle.y, rectangle.width, rectangle.height
    except Exception:
        return 0, 0, 1920, 1080


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


class Geometry:
    """Remembers each window's size, and its position where the platform allows."""

    def __init__(self):
        self.path = os.path.join(state_directory(), "ui.json")
        self.data = self._load()

    def _load(self):
        try:
            with open(self.path, encoding="utf-8") as handle:
                loaded = json.load(handle)
            return loaded if isinstance(loaded, dict) else {}
        except Exception:
            return {}

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
        entry = self.entry(key)
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
        self.data[key] = entry
        self._store()

    def apply(self, key, window):
        entry = self.entry(key)
        area_x, area_y, area_width, area_height = work_area()
        try:
            width = int(entry.get("width", 0))
            height = int(entry.get("height", 0))
        except Exception:
            width, height = 0, 0
        if width >= MINIMUM_WIDTH and height >= MINIMUM_HEIGHT:
            # Keep the whole window on the screen.
            width = max(MINIMUM_WIDTH, min(width, area_width))
            height = max(MINIMUM_HEIGHT, min(height, area_height))
            window.set_default_size(width, height)
        x = entry.get("x")
        y = entry.get("y")
        if isinstance(x, int) and isinstance(y, int):
            x = max(area_x, min(x, area_x + area_width - max(width, MINIMUM_WIDTH)))
            y = max(area_y, min(y, area_y + area_height - max(height, MINIMUM_HEIGHT)))
            GLib.idle_add(lambda: (x11_move(window, x, y), False)[1])

    def watch(self, key, window):
        self.apply(key, window)

        def on_change(_window, _parameter):
            self.save(key, window)
            return False

        for parameter in ("default-width", "default-height"):
            try:
                window.connect("notify::%s" % parameter, on_change)
            except Exception:
                pass
        window.connect("close-request", lambda _window: (self.save(key, window), False)[1])


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
        self.windows = []
        self.window = Gtk.ApplicationWindow(application=application, title="AppImage")
        self.window.set_child(self.build_content())
        self.geometry.watch("main", self.window)
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

    def real_conflicts(self):
        return [item for item in self.data.get("conflicts", []) if not item.get("upgrade")]

    def is_missing(self):
        return not os.path.exists(self.path)

    # -- rendering ----------------------------------------------------------
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
        box.append(details)

        conflicts = self.real_conflicts()
        if conflicts:
            box.append(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL))
            notice = Gtk.Label()
            notice.set_markup(
                "<b>This application is already installed.</b>\n"
                "Integrate will ask whether to replace or add alongside."
            )
            notice.set_xalign(0)
            notice.set_wrap(True)
            box.append(notice)

        if self.data.get("error"):
            error_label = Gtk.Label(label=self.data["error"])
            error_label.set_xalign(0)
            error_label.set_wrap(True)
            box.append(error_label)

        spacer = Gtk.Box()
        spacer.set_vexpand(True)
        box.append(spacer)

        buttons = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        buttons.set_halign(Gtk.Align.END)
        for label, callback in (
            ("Close", self.on_close),
            ("Inspect", self.on_inspect),
            ("Integrate", self.on_integrate),
            ("Run once", self.on_run_once),
        ):
            button = Gtk.Button(label=label)
            button.connect("clicked", callback)
            buttons.append(button)
        box.append(buttons)
        return box

    def detail_rows(self):
        rows = [
            ("File", self.path),
            ("Type", self.data.get("detection") or "(not an AppImage)"),
            ("Size", human_size(self.data.get("file_size", 0))),
        ]
        if self.data.get("compression"):
            rows.append(
                (
                    "Payload",
                    "%s, %s"
                    % (human_size(self.data.get("payload_size", 0)), self.data["compression"]),
                )
            )
        if self.data.get("embedded_desktop"):
            rows.append(("Embedded entry", self.data["embedded_desktop"]))
        if self.data.get("desktop_id"):
            rows.append(("Will install as", self.data["desktop_id"]))
        if self.data.get("update_information"):
            rows.append(("Updates", self.data["update_information"]))
        if self.is_missing():
            rows.append(("Note", "this file is no longer at that path"))
        return rows

    # -- window helpers -----------------------------------------------------
    def show_text_window(self, key, title, text, extra_buttons=None):
        window = Gtk.Window(transient_for=self.window, title=title)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(12)
        box.set_margin_bottom(12)
        box.set_margin_start(12)
        box.set_margin_end(12)

        scroller = Gtk.ScrolledWindow()
        scroller.set_vexpand(True)
        view = Gtk.TextView()
        view.set_editable(False)
        view.set_monospace(True)
        view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        view.get_buffer().set_text(text)
        scroller.set_child(view)
        box.append(scroller)

        buttons = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        buttons.set_halign(Gtk.Align.END)
        for label, callback in extra_buttons or []:
            button = Gtk.Button(label=label)
            button.connect("clicked", callback)
            buttons.append(button)
        close = Gtk.Button(label="Close")
        close.connect("clicked", lambda _button: window.close())
        buttons.append(close)
        box.append(buttons)

        window.set_child(box)
        self.geometry.watch(key, window)
        window.present()
        self.windows.append(window)
        return window

    # -- actions ------------------------------------------------------------
    def on_close(self, _button):
        self.geometry.save("main", self.window)
        self.window.close()

    def start_appimage(self):
        try:
            subprocess.Popen(
                [self.tool, "run", "--detached", self.path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True,
            )
        except Exception:
            pass

    def on_run_once(self, _button):
        if self.is_missing():
            self.show_missing_window()
            return
        self.geometry.save("main", self.window)
        self.start_appimage()
        self.window.close()

    def show_missing_window(self):
        self.show_text_window(
            "result",
            "AppImage moved",
            "This AppImage is no longer at\n  %s\n\n"
            "It has probably been integrated already.\n"
            "Look for \"%s\" in the application menu.\n\n"
            "Close returns to the AppImage window."
            % (self.path, self.data.get("name") or "the application"),
        )

    def on_inspect(self, _button):
        _code, out, err = run_tool(self.tool, ["explain", self.path])
        self.show_text_window(
            "inspect",
            "Inspect — %s" % (self.data.get("name") or "AppImage"),
            combined_output(out, err),
        )

    def on_integrate(self, _button):
        if self.is_missing():
            self.show_missing_window()
            return
        policy = []
        conflicts = self.real_conflicts()
        if conflicts:
            lines = [
                "%s is already represented by:" % (self.data.get("name") or "This AppImage"),
                "",
            ]
            for index, conflict in enumerate(conflicts, start=1):
                lines.append(describe_conflict(index, conflict))
                lines.append("")
            if self.data.get("version"):
                lines.append("This AppImage is version %s." % self.data["version"])
                lines.append("")
            lines.append(
                "Replace existing backs those launchers up and installs this version in their place."
            )
            lines.append("Add alongside keeps them and installs this version under a new id.")
            choice = self.ask_conflict("\n".join(lines))
            if choice == "replace":
                policy = ["--replace"]
            elif choice == "add":
                policy = ["--add"]
            else:
                return

        code, out, err = run_tool(self.tool, ["install", "--yes"] + policy + [self.path])
        report = combined_output(out, err)
        if code == 0:
            self.data = self.load_description()
            self.show_text_window(
                "result",
                "Integrated — %s" % (self.data.get("name") or "AppImage"),
                report + "\n\nClose returns to the AppImage window.",
                extra_buttons=[("Run now", self.on_run_now)],
            )
        else:
            self.show_text_window(
                "result",
                "Could not integrate — %s" % (self.data.get("name") or "AppImage"),
                "The integration did not complete.\n\n" + report,
            )

    def on_run_now(self, button):
        if not self.is_missing():
            self.start_appimage()
        window = button.get_ancestor(Gtk.Window)
        if window is not None:
            window.close()
        self.window.close()

    def ask_conflict(self, text):
        """Return 'replace', 'add', or None."""
        dialog = Gtk.Window(transient_for=self.window, modal=True, title="Integrate AppImage")
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(12)
        box.set_margin_bottom(12)
        box.set_margin_start(12)
        box.set_margin_end(12)

        scroller = Gtk.ScrolledWindow()
        scroller.set_vexpand(True)
        view = Gtk.TextView()
        view.set_editable(False)
        view.set_monospace(True)
        view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        view.get_buffer().set_text(text)
        scroller.set_child(view)
        box.append(scroller)

        answer = {"value": None}

        def choose(value):
            answer["value"] = value
            dialog.close()

        buttons = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        buttons.set_halign(Gtk.Align.END)
        for label, value in (
            ("Cancel", None),
            ("Add alongside", "add"),
            ("Replace existing", "replace"),
        ):
            button = Gtk.Button(label=label)
            button.connect("clicked", lambda _button, v=value: choose(v))
            buttons.append(button)
        box.append(buttons)
        dialog.set_child(box)
        self.geometry.watch("conflict", dialog)
        dialog.present()
        self.windows.append(dialog)

        loop = GLib.MainLoop()
        dialog.connect("close-request", lambda _window: (loop.quit(), False)[1])
        loop.run()
        self.windows.remove(dialog)
        return answer["value"]


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

    application = Gtk.Application(application_id="us.bannister.appimage-handler")
    holder = {}

    def on_activate(app):
        holder["handler"] = Handler(app, tool, path)

    application.connect("activate", on_activate)
    return application.run([argv[0]])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
