#!/bin/sh
#
# rules-conform.sh: report what keeps this project from conforming to the rules
# it pins, grouped by the action that fixes it.
#
# Read-only. Never modifies a file, never reads standard input.
#
# Usage: rules-conform.sh [PROJECT_DIR] [--verbose]
#
# Why this exists: the validator states one finding per line, in file order. On a
# project that has never run it, that is a hundred lines of near-identical text,
# and the shape of the work -- "twelve tasks need the same four sections" -- is
# invisible. This collapses the lines into actions, then names the files each
# action touches.
#
# Exit status is 0 when the project conforms, 1 when it does not, 2 on a usage or
# environment error. It reports; it does not repair. A prompt is not a lockfile,
# and a tool that rewrote requirement identifiers would be inventing the
# requirements it claims to be checking.
set -eu
LC_ALL=C
export LC_ALL

usage() {
    echo 'usage: rules-conform.sh [PROJECT_DIR] [--verbose]' >&2
    exit 2
}

project_arg=""
verbose=0
for arg in "$@"; do
    case "$arg" in
        --verbose) verbose=1 ;;
        -h|--help) usage ;;
        -*) usage ;;
        *)
            [ -z "$project_arg" ] || usage
            project_arg=$arg
            ;;
    esac
done

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "${project_arg:-"$DIRECTORY_SCRIPT/.."}" && pwd)
SCRIPT_VALIDATOR="$PROJECT_ROOT/tests/09-prompt-contract.sh"

if [ ! -f "$SCRIPT_VALIDATOR" ]; then
    echo "rules-conform: no tests/09-prompt-contract.sh in $PROJECT_ROOT" >&2
    echo 'rules-conform: the validator is vendored; upgrade the project to a version that carries it' >&2
    exit 2
fi

command -v python3 >/dev/null 2>&1 || {
    echo 'rules-conform: python3 is required (the validator is written in it)' >&2
    exit 2
}

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT HUP INT TERM

# Findings are written to standard error; standard output carries only the
# success line, so the two are captured separately.
status_validator=0
sh "$SCRIPT_VALIDATOR" "$PROJECT_ROOT" 2>"$DIRECTORY_TEMP/findings" >/dev/null \
    || status_validator=$?

version=""
if [ -f "$PROJECT_ROOT/rules.lock" ]; then
    version=$(sed -n 's/^version = //p' "$PROJECT_ROOT/rules.lock" | head -n 1)
fi

python3 - "$verbose" "$version" "$status_validator" "$DIRECTORY_TEMP/findings" <<'PY'
import collections
import re
import sys

verbose = sys.argv[1] == "1"
version = sys.argv[2]
status_validator = int(sys.argv[3])
findings_path = sys.argv[4]

raw = open(findings_path, encoding="utf-8", errors="replace").read().splitlines()

PREFIX = "09-prompt-contract: "
findings = []
noise = []
for line in raw:
    if not line.strip():
        continue
    if line.startswith(PREFIX):
        findings.append(line[len(PREFIX):])
    else:
        noise.append(line)

# One action per category. The order is deliberate: a structural finding is
# rarely about a task, and "TASK-" appears in several messages, so the specific
# checks come before the general one.
CATEGORIES = (
    ("entry point",
     r"^AGENTS\.md (is missing|does not )",
     "write AGENTS.md: it points at prompts/01-contract.md, prompts/02-workflow.md, "
     "prompts/03-conventions.md and prompts/how-to-write-tasks.md, and names `make test`"),
    ("structure declaration",
     r"(root file|top-level directory|tool directory) is not permitted",
     "declare the path in `## Project Structure` in prompts/10-project.md "
     "(a directory takes a trailing slash)"),
    ("shared registry",
     r"not in the contract section 2 registry|contract section 2 \(authoritative rules\) not found",
     "a local rule file belongs at prompts/1x-...; anything under prompts/common, "
     "flavors or capabilities is shared and cannot be local"),
    ("single home",
     r"rule family .*duplicated|duplicate requirement identifier",
     "keep the rule in its authoritative file and make the copy a pointer"),
    ("requirement identifiers",
     r"no requirement identifiers|requirement without an identifier",
     "give each requirement bullet a unique identifier of the form `PREFIX-R001`; "
     "identifiers must be unique across all features"),
    ("unresolved reference",
     r"reference does not exist|registry path does not exist|dependency does not resolve|"
     r"names a missing feature|must list feature files",
     "fix the path, or create the file the prompt names"),
    ("task format",
     r"TASK-|OUTPUT: restatement|operation|repository-relative|unknown identifier",
     "add operation lines such as `- Create: documents/x.md`, a TASK-FILES table, "
     "TASK-VERIFY, and the closing OUTPUT: restatement"),
    ("unattended and safety",
     r"blocks on standard input|reads the terminal|protected path|absolute home path",
     "a script must complete unattended; keep configuration repository-relative"),
    ("capability listing",
     r"capability convention is not listed",
     "list the capability file in the Applicability section of prompts/03-conventions.md"),
    ("feature format",
     r"missing ## (Purpose|Requirements|Behavior|Dependencies)",
     "give the feature its four sections: Purpose, Requirements with identifiers, "
     "Behavior, Dependencies"),
    ("numbering",
     r"duplicate (feature|task) number",
     "renumber one of them, and update every reference to the file you renamed"),
)


def plural(count, noun):
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


def classify(message):
    for name, pattern, action in CATEGORIES:
        if re.search(pattern, message):
            return name, action
    return "other", "read the finding: no action hint covers it yet"


def place(message):
    tokens = re.findall(r"[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)+\.(?:md|sh)", message)
    if tokens:
        return tokens[0]
    tail = message.rsplit(": ", 1)[-1].strip().strip("`")
    if tail and ("not permitted" in message or "registry" in message):
        return tail
    if "AGENTS.md" in message:
        return "AGENTS.md"
    return "(project)"


grouped = collections.OrderedDict()
for message in findings:
    name, action = classify(message)
    entry = grouped.setdefault(name, {"action": action, "messages": [], "places": collections.Counter()})
    entry["messages"].append(message)
    entry["places"][place(message)] += 1

label = f", pinned {version}" if version else ""

if noise:
    print("rules-conform: the validator did not run to completion:", file=sys.stderr)
    for line in noise:
        print(f"  {line}", file=sys.stderr)
    print("", file=sys.stderr)

if not findings:
    if status_validator != 0:
        print("rules-conform: the validator failed without stating a finding; see above", file=sys.stderr)
        sys.exit(1)
    print(f"rules-conform: ok (nothing to conform{label})")
    sys.exit(0)

places = {place(m) for m in findings}
print(f"rules-conform: {plural(len(findings), 'finding')} in "
          f"{plural(len(places), 'place')}{label}")
print()

ordered = sorted(grouped.items(), key=lambda item: (-len(item[1]["messages"]), item[0]))
for index, (name, entry) in enumerate(ordered, 1):
    count = len(entry["messages"])
    print(f"{index}. {name}: {plural(count, 'finding')} in "
          f"{plural(len(entry['places']), 'file')}")
    print(f"   {entry['action']}")
    for place_name, place_count in entry["places"].most_common(8):
        print(f"   - {place_name} ({place_count})")
    if len(entry["places"]) > 8:
        print(f"   - and {len(entry['places']) - 8} more")
    print()

if verbose:
    print("Every finding, by action:")
    print()
    for name, entry in ordered:
        print(f"[{name}]")
        for message in entry["messages"]:
            print(f"  {message}")
        print()
else:
    print("Re-run with --verbose for every finding, or fix one action and re-run.")

sys.exit(1 if status_validator != 0 else 0)
PY
