# Running an AppImage Under Isolation

Integrating an AppImage and running it are separate decisions. Integration writes a launcher;
isolation decides what that launcher may touch when it starts. `firejail` was the first
candidate for the second question, and this document assesses the alternatives that need no
extra package — in particular systemd's own sandboxing and container tools — and records which
of them is the right shape for a launcher on the application grid.

What is implemented now is firejail, offered rather than assumed: `install --sandbox firejail`
and a *Run in firejail* checkbox in the window, shown only where firejail is installed, with the
choice recorded beside the integration so a rewrite keeps it. The rest of this document assesses
the alternatives and is why the option is a named backend rather than a flag.

One finding changed the shape of the question. An AppImage that mounts its payload needs
`/dev/fuse` and a `fusermount` binary whether or not its runtime links FUSE statically, and the
older runtimes additionally load `libfuse.so.2` by name; on this host `/dev/fuse` is absent
altogether, which no sandbox can fix. That is now detected and reported — see
`10-appimage-desktop-integration.md` — and it means a sandbox's job is narrower than it first
looks: keeping FUSE working is a prerequisite, not something a sandbox provides.

## What any backend has to provide

A launcher's `Exec` is one command line that the desktop expands and runs at login and from the
grid. Whatever runs the application has to leave all of this working:

- **The display.** The X11 socket or the Wayland socket of the current session.
- **The GPU.** `/dev/dri`, or the application falls back to software rendering.
- **Audio and session IPC.** The PipeWire or PulseAudio socket, and usually the session bus.
- **The AppImage's own mount.** A type-2 AppImage self-mounts with FUSE. Anything that hides
  `/dev/fuse` or blocks the setuid `fusermount3` breaks the file before the application starts.
  `APPIMAGE_EXTRACT_AND_RUN=1` sidesteps this by extracting to a temporary directory instead of
  mounting, at the cost of startup time and temporary disk — the fallback already described in
  `10-appimage-desktop-integration.md`.
- **The application's real settings.** They live under `$HOME` and `$XDG_CONFIG_HOME`. A sandbox
  that gives the application a private home starts it as if it were new, which is the most
  common way an "isolated" desktop application is reported as broken.

The last two are what separate a backend that works from one that does not, and neither is an
AppImage problem: they are the price of running any GUI application in a box.

## The candidates

| Mechanism | What it is | Privileges | Knows about AppImages | Shape for a launcher |
| --------- | ---------- | ---------- | --------------------- | -------------------- |
| `firejail` | setuid-root sandbox with profiles | setuid binary, one package | **Yes** — `--appimage` exists and disables `private-bin`/`private-lib`, which is what AppImages need | Good, once installed |
| `systemd-run --user` | transient user unit with systemd's sandboxing directives | none; needs the user manager | No | Promising, but all the plumbing is ours |
| `systemd-nspawn` | container booted from an OS tree | root, or user namespaces | No | Wrong shape |
| `bwrap` | unprivileged bind-mount sandbox; what Flatpak itself uses | none, no daemon | No | Workable; the bind set is ours |
| rootless `podman`/`docker` | an image plus a runtime or daemon | none, but a runtime to install | No | Heavy for one application |
| `flatpak` | runs Flatpak applications | none | Not applicable — it does not run arbitrary AppImages | Not applicable |

### firejail

The only candidate that understands the artifact. Its own manual documents the invocation:

```
firejail [OPTIONS] --appimage [OPTIONS] [appimage-file and arguments]
```

and notes that `private-bin` and `private-lib` are disabled by default for AppImages, which is
exactly what makes them run. It also ships profiles (`--profile=krita`), so the AppImage-specific
work is largely done by other people, and it handles the FUSE mount rather than leaving it to us.
Its costs: a package to install, a setuid-root binary, and its own history of privilege bugs —
which is why no wording anywhere should call the result "safe".

### systemd-run --user with sandboxing directives

This is the most interesting of the daemon-free candidates. Every modern desktop session already
runs a systemd user manager, so nothing is installed and no setuid helper is involved, and the
isolation is declarative:

```
systemd-run --user --collect --scope \
    -p NoNewPrivileges=yes -p PrivateTmp=yes -p ProtectSystem=strict \
    -p ReadWritePaths=%h -p RestrictAddressFamilies=AF_UNIX \
    /path/to/Application.AppImage %U
```

What it does not give us is anything AppImage-specific. We would have to write the directive set
ourselves: keep the display and audio sockets and `/dev/dri` reachable, avoid `PrivateDevices`
(which removes `/dev/fuse`), and decide what to do about `$HOME`. Several of the stronger
directives depend on unprivileged user namespaces, which are enabled here but are a distribution
policy rather than a guarantee. And the isolation is only as good as the directive set: a wrong
one silently breaks the application instead of failing loudly, which is the same criticism that
applies to `bwrap`.

### systemd-nspawn

A container with its own operating system tree, its own init and its own package set. It needs an
image, root or user namespaces, and the same display, GPU and audio plumbing as any other option,
on top of the cost of maintaining a second operating system for one application. It answers a
question nobody asked: the goal is to run this file with less reach, not to run a second machine.

### bwrap

Unprivileged, no daemon, no image: it builds a mount namespace and hands the application a chosen
view of the filesystem. It is what Flatpak uses to enforce its sandboxes, so the mechanism is
proven at desktop scale. What it does not ship is a profile — the bind set is ours to write, and
an incomplete one breaks the application quietly long after the launcher was written.

## Observed on this host

- `firejail` is not installed, so its behaviour cannot be verified here.
- `bwrap` and `systemd-nspawn` are installed; `podman` and `distrobox` are not; `flatpak` is.
- systemd is version 259 and `kernel.unprivileged_userns_clone` is `1`, so user namespaces are
  available — but there is **no user systemd bus** in this environment
  (`systemd-run --user` fails with "Failed to connect to user scope bus"), so
  `systemd-run --user` cannot be verified here either, until it is run from a real desktop
  session.

That leaves `bwrap` as the only candidate that can be exercised on this machine today, and it is
the one with the least AppImage-specific help. It is an argument for building the option as a
named backend and for testing the backend by injecting a stand-in on `PATH` rather than by
requiring the real one.

## What this means for the design

- **Model it as a named backend**, not a boolean: `none` (the default), then `firejail` or
  `bwrap` or `systemd-run`. This is what `integration_sandbox_e` does, and the choice is written
  into the record for the integration, so `refresh` and `update` keep it and `audit` reports a
  launcher whose backend has gone.
- **Decide the `Exec` question before writing any of it.** Baking a backend into `Exec` means
  removing that backend later breaks every launcher that used it. The alternatives are: keep
  `Exec` direct and require the backend at install time, with an `audit` finding when it later
  goes missing; or write a small wrapper that falls back to running the file directly. The
  wrapper is more resilient and one more file to own, which is why it is a decision and not a
  default.
- **Prefer the extract-and-run route for a sandboxed launch.** It removes FUSE from the problem
  for every backend except firejail, which handles the mount itself.
- **Default to the real home.** Isolation that costs the application its settings will be turned
  off by the first person who tries it.
- **Never call it safe.** Say "runs under firejail" or "runs in a bwrap sandbox", and nothing
  more. Each of these reduces reach; none of them makes an untrusted download trustworthy.

## Recommendation

1. `firejail` first, and it is what is implemented: `--appimage` exists, its profiles are
   maintained elsewhere, and it keeps `private-bin` and `private-lib` out of the way, which is
   what an AppImage needs. The option is offered only where it is installed, and a record that
   names it is not rewritten when it goes.
2. `bwrap` second, because it needs nothing installed on a GNOME desktop and is the only option
   verifiable on this host; it costs us an AppImage-specific bind set.
3. `systemd-run --user` is worth revisiting once it can be tested from a real session; it is the
   most maintainable of the three on a systemd desktop.
4. `systemd-nspawn` and the container runtimes are the wrong shape for a launcher, and are
   recorded here so the question does not have to be asked again.
