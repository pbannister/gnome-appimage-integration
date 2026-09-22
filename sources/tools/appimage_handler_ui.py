#!/usr/bin/env python3
"""Graphical AppImage handler.

Invoked as:  appimage_handler_ui.py --tool <appimage-integrate> <AppImage>

The window shows what the AppImage is (name, version, comment, generic name) and
offers Run once, Integrate, Inspect, and Close.  The window size is remembered
between runs in the tool's state directory.  All real work is delegated to the
`appimage-integrate` CLI, so this file only presents information and choices.
"""

import json
import os
import subprocess
import sys

import gi

gi.require_version("Gtk", "4.0")
from gi.repository import GLib, Gtk  # noqa: E402


def state_directory():
    data_home = os.environ.get("XDG_DATA_HOME") or os.path.join(
        os.path.expanduser("~"), ".local", "share"
    )
    directory = os.path.join(data_home, "gnome-appimage-integration")
    os.makedirs(directory, exist_ok=True)
    return directory


def geometry_file():
    return os.path.join(state_directory(), "ui.json")


def load_geometry():
    try:
        with open(geometry_file(), encoding="utf-8") as handle:
            data = json.load(handle)
        width = int(data.get("width", 0))
        height = int(data.get("height", 0))
        if width > 320 and height > 240:
            return width, height
    except Exception:
        pass
    return 580, 640


def save_geometry(window):
    try:
        width = window.get_width()
        height = window.get_height()
        if width > 320 and height > 240:
            with open(geometry_file(), "w", encoding="utf-8") as handle:
                json.dump({"width": width, "height": height}, handle)
    except Exception:
        pass


def run_tool(tool, arguments):
    """Run the CLI and return (returncode, stdout, stderr)."""
    try:
        completed = subprocess.run(
            [tool] + arguments, capture_output=True, text=True, check=False
        )
        return completed.returncode, completed.stdout, completed.stderr
    except Exception as error:  # pragma: no cover - defensive
        return 1, "", str(error)


def human_size(byte_count):
    value = float(byte_count or 0)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if value < 1024.0 or unit == "GiB":
            return "%.1f %s" % (value, unit)
        value /= 1024.0
    return "%d B" % byte_count


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


class HandlerWindow:
    def __init__(self, application, tool, path):
        self.application = application
        self.tool = tool
        self.path = path
        self.data = self.load_description()
        self.windows = []
        self.window = Gtk.ApplicationWindow(application=application, title="AppImage")
        self.window.set_default_size(*load_geometry())
        self.window.connect("close-request", self.on_close_request)
        self.window.set_child(self.build_content())
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
        details.set_column_homogeneous(False)
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
            rows.append(("Payload", "%s, %s" % (human_size(self.data.get("payload_size", 0)),
                                                self.data["compression"])))
        if self.data.get("embedded_desktop"):
            rows.append(("Embedded entry", self.data["embedded_desktop"]))
        if self.data.get("desktop_id"):
            rows.append(("Will install as", self.data["desktop_id"]))
        if self.data.get("update_information"):
            rows.append(("Updates", self.data["update_information"]))
        return rows

    # -- actions ------------------------------------------------------------
    def on_close_request(self, _window):
        save_geometry(self.window)
        return False

    def on_close(self, _button):
        save_geometry(self.window)
        self.window.close()

    def on_run_once(self, _button):
        save_geometry(self.window)
        try:
            subprocess.Popen(
                [self.tool, "run", "--detached", self.path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True,
            )
        except Exception:
            pass
        self.window.close()

    def show_text_window(self, title, text, extra_buttons=None):
        window = Gtk.Window(transient_for=self.window, title=title)
        window.set_default_size(760, 560)
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
        window.present()
        self.windows.append(window)
        return window

    def on_inspect(self, _button):
        _code, out, err = run_tool(self.tool, ["explain", self.path])
        self.show_text_window("Inspect — %s" % (self.data.get("name") or "AppImage"),
                              out or err or "(no information)")

    def on_integrate(self, _button):
        policy = []
        conflicts = self.real_conflicts()
        if conflicts:
            lines = ["%s is already represented by:" % (self.data.get("name") or "This AppImage"), ""]
            for index, conflict in enumerate(conflicts, start=1):
                lines.append(describe_conflict(index, conflict))
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

        _code, out, err = run_tool(self.tool, ["install", "--yes"] + policy + [self.path])
        report = out or err or "(no output)"
        if _code == 0:
            self.refresh_after_install()
            self.show_text_window(
                "Integrated — %s" % (self.data.get("name") or "AppImage"),
                report + "\n\nClose returns to the AppImage window.",
                extra_buttons=[("Run now", self.on_run_now)],
            )
        else:
            self.show_text_window("Could not integrate", report)

    def on_run_now(self, button):
        try:
            subprocess.Popen(
                [self.tool, "run", "--detached", self.path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True,
            )
        except Exception:
            pass
        button.get_ancestor(Gtk.Window).close()
        self.window.close()

    def refresh_after_install(self):
        self.data = self.load_description()

    def ask_conflict(self, text):
        """Return 'replace', 'add', or None."""
        dialog = Gtk.Window(transient_for=self.window, modal=True, title="Integrate AppImage")
        dialog.set_default_size(720, 520)
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
        dialog.present()
        self.windows.append(dialog)

        # Spin a nested main loop so the function can return the choice.
        loop = GLib.MainLoop()
        dialog.connect("close-request", lambda _window: (loop.quit(), False)[1])
        loop.run()
        self.windows.remove(dialog)
        return answer["value"]


def escape(text):
    return (
        text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    )


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
        print("usage: appimage_handler_ui.py --tool <appimage-integrate> <AppImage>", file=sys.stderr)
        return 2

    application = Gtk.Application(application_id="us.bannister.appimage-handler")
    holder = {}

    def on_activate(app):
        holder["window"] = HandlerWindow(app, tool, path)

    application.connect("activate", on_activate)
    return application.run([argv[0]])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
