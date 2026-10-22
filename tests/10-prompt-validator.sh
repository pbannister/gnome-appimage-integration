#!/bin/sh
#
# Prompt-validator test: the adversarial corpus.
#
# Tier: tool-gated (python3 and git, through tests/09). Builds a throwaway copy
# of the repository, commits it to a scratch git repository, injects one
# malformed fixture at a time, and asserts that tests/09-prompt-contract.sh
# rejects each one for the expected reason. This tests the validator itself,
# not only the clean corpus.
#
# Every rejection declares the numbered check it exercises:
#
#     expect_reject <check-id> '<name>' '<expected message fragment>'
#
# tests/13-corpus-coverage.sh requires every numbered check in tests/09 to
# appear at least once here, so a new check cannot land unexercised. An
# acceptance case takes no check id: it guards against over-rejection.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
SCRIPT_CHECK="$REPOSITORY_ROOT/tests/09-prompt-contract.sh"

. "$DIRECTORY_SCRIPT/lib/test_helpers.sh"
skip_unless_tool python3
skip_unless_tool git

DIRECTORY_TEST=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEST"' EXIT HUP INT TERM
DIRECTORY_ROOT="$DIRECTORY_TEST/root"

fail() {
    echo "10-prompt-validator: $1" >&2
    exit 1
}

# reset: a clean, committed copy of the repository to mutate. Committing it
# lets the validator's tracked-root and tool-config checks run.
#
# The copy is built from `git ls-files`, not from the working tree, because the
# validator only ever reads tracked files and a working tree can be enormous:
# one project carries 3.9 GB of ignored research evidence, and copying it once
# per case (54 times) made a ten-minute suite out of a half-megabyte fixture.
# `--ignore-failed-read` covers a path that is tracked but absent locally, which
# must not abort the fixture build.
reset() {
    rm -rf "$DIRECTORY_ROOT"
    mkdir -p "$DIRECTORY_ROOT"
    if [ -z "$(git -C "$REPOSITORY_ROOT" ls-files 2>/dev/null | head -n 1)" ]; then
        # The repository has no index: it has never been committed. The corpus
        # still has to be able to test the rules, so it falls back to the working
        # tree. A project in this state cannot pin a version, which the pin test
        # reports separately; silently failing 54 cases would say the rules were
        # wrong when the repository was.
        (cd "$REPOSITORY_ROOT" && tar --exclude=./.git --exclude=./site.out \
            --exclude=./dataflow.out --exclude=./logs -cf - .) \
            | (cd "$DIRECTORY_ROOT" && tar -xf -)
    else
        (cd "$REPOSITORY_ROOT" && git ls-files -z \
            | tar --null -T - --ignore-failed-read -cf -) \
            | (cd "$DIRECTORY_ROOT" && tar -xf -)
    fi
    # The adversarial cases mutate a feature and a task with known shape. Using
    # the project's own corpus made this corpus non-portable: a project that does
    # not model its site build as a feature failed every case for a reason that
    # had nothing to do with the checks under test. The fixture is synthesized
    # every time, under reserved numbers (89-) and reserved identifiers
    # (FIXTURE-R), so it cannot collide with the project's corpus or drift with it.
    mkdir -p "$DIRECTORY_ROOT/prompts/features" "$DIRECTORY_ROOT/prompts/tasks"
    cat > "$DIRECTORY_ROOT/prompts/features/89-site-build.md" <<'FIXTURE_FEATURE'
# Feature: Site Build

## Purpose

The site build generates the static site.

## Requirements

- `FIXTURE-R001` - `scripts/site-build.sh` must generate `site.out/` from `site.in/`.
- `FIXTURE-R002` - Generated output must be identified as generated.

## Behavior

- Running `make site` recreates `site.out/` from `site.in/`.

## Dependencies

- None.
FIXTURE_FEATURE
    cat > "$DIRECTORY_ROOT/prompts/tasks/89-site-build-implement.md" <<'FIXTURE_TASK'
# Task: Implement the Site Build feature

## TASK-DESCRIPTION
- Create: `scripts/site-build.sh`

Implement the requirements in `prompts/features/89-site-build.md`.

## TASK-OUTPUT
* Report the created file and the `make test` result.
* End with the `VERIFICATION:` line.

## TASK-FILES

| Operation | Path |
|---|---|
| create | `scripts/site-build.sh` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0.

## TASK-FEATURES
- `prompts/features/89-site-build.md`

## TASK-ACCEPTANCE
- `FIXTURE-R001`

OUTPUT: the created file and the `make test` result, ending with the `VERIFICATION:` line.
FIXTURE_TASK
    git -C "$DIRECTORY_ROOT" init -q
    git -C "$DIRECTORY_ROOT" add -A
}

FILE_TASK="$DIRECTORY_ROOT/prompts/tasks/89-site-build-implement.md"
FILE_WORKFLOW="$DIRECTORY_ROOT/prompts/02-workflow.md"
FILE_CONTRACT="$DIRECTORY_ROOT/prompts/01-contract.md"
FILE_FEATURE="$DIRECTORY_ROOT/prompts/features/89-site-build.md"
FILE_FEATURE_02="$DIRECTORY_ROOT/prompts/features/02-project-pages.md"
FILE_EPISODE="$DIRECTORY_ROOT/prompts/episodes/01-episode-template.md"

count_case=0
count_accept=0

expect_reject() {
    id_case=$1
    name_case=$2
    text_expect=$3
    count_case=$((count_case + 1))
    git -C "$DIRECTORY_ROOT" add -A >/dev/null 2>&1 || true
    if output_case=$(sh "$SCRIPT_CHECK" "$DIRECTORY_ROOT" 2>&1); then
        fail "the validator accepted [$id_case] '$name_case'"
    fi
    printf '%s\n' "$output_case" | grep -qF "$text_expect" \
        || fail "the validator rejected [$id_case] '$name_case' for the wrong reason: $output_case"
}

expect_accept() {
    name_case=$1
    count_accept=$((count_accept + 1))
    git -C "$DIRECTORY_ROOT" add -A >/dev/null 2>&1 || true
    if ! output_case=$(sh "$SCRIPT_CHECK" "$DIRECTORY_ROOT" 2>&1); then
        fail "the validator rejected '$name_case': $output_case"
    fi
}

# Baseline: the clean corpus validates.
reset
if ! output_clean=$(sh "$SCRIPT_CHECK" "$DIRECTORY_ROOT" 2>&1); then
    fail "the clean corpus did not validate: $output_clean"
fi

# --- check 4: task structure ---
reset; sed -i '/^## TASK-DESCRIPTION$/d' "$FILE_TASK"
expect_reject 4 'task missing TASK-DESCRIPTION' 'missing ## TASK-DESCRIPTION'

reset; sed -i '/^## TASK-OUTPUT$/d' "$FILE_TASK"
expect_reject 4 'task missing TASK-OUTPUT' 'missing ## TASK-OUTPUT'

reset; sed -i '1i ## TASK-OUTPUT' "$FILE_TASK"
expect_reject 4 'task sections out of order' 'must precede TASK-OUTPUT'

reset; sed -i '/^## TASK-OUTPUT$/i ## TASK-CONTEXT' "$FILE_TASK"
expect_reject 4 'a section placed before TASK-OUTPUT' '## TASK-CONTEXT must follow TASK-OUTPUT'

reset; sed -i '/^OUTPUT:/d' "$FILE_TASK"
expect_reject 4 'task missing the OUTPUT line' 'OUTPUT: restatement'

reset; sed -i '/^- Run:/d' "$FILE_TASK"
expect_reject 4 'TASK-VERIFY without a Run line' "TASK-VERIFY has no 'Run:' line"

reset; sed -i '/^- Expected:/d' "$FILE_TASK"
expect_reject 4 'TASK-VERIFY without an Expected line' "TASK-VERIFY has no 'Expected:' line"

reset; sed -i 's/^- Expected: .*/- Expected: looks good./' "$FILE_TASK"
expect_reject 4 'a judgmental Expected line' 'must describe an observable result'

reset; sed -i '/^- Create: /d; /^- Modify: /d' "$FILE_TASK"
expect_reject 4 'TASK-DESCRIPTION without an operation line' "TASK-DESCRIPTION has no '- <Verb>:"

# --- check 4: scope table and operation lines ---
reset; sed -i '/^| create |/d' "$FILE_TASK"
expect_reject 4 'TASK-FILES without rows' "TASK-FILES has no '| operation |"

reset; sed -i 's/^| create |/| frobnicate |/' "$FILE_TASK"
expect_reject 4 'unknown operation in TASK-FILES' 'unknown operation'

reset; sed -i 's#`scripts/site-build.sh`#`/etc/passwd`#' "$FILE_TASK"
expect_reject 4 'absolute path in TASK-FILES' 'not repository-relative'

reset; sed -i 's#`scripts/site-build.sh`#`../outside.sh`#' "$FILE_TASK"
expect_reject 4 'parent escape in TASK-FILES' 'not repository-relative'

reset; sed -i '/^|---|---|$/a | create | `scripts/undeclared.sh` |' "$FILE_TASK"
expect_reject 4 'TASK-FILES row without an operation line' 'has no operation line in TASK-DESCRIPTION'

reset; sed -i '/^## TASK-DESCRIPTION$/a - Modify: `scripts/undeclared.sh`' "$FILE_TASK"
expect_reject 4 'operation on an undeclared path' 'operation on undeclared path'

reset; sed -i 's#^- Create: `scripts/site-build.sh`$#- Modify: `scripts/site-build.sh`#' "$FILE_TASK"
expect_reject 4 'verb disagrees between sections' "but 'modify' in TASK-DESCRIPTION"

reset; sed -i '/^## TASK-FILES$/d; /^| create |/d' "$FILE_TASK"
expect_reject 4 'task missing TASK-FILES' 'missing ## TASK-FILES'

reset; sed -i '/^## TASK-VERIFY$/d; /^- Run:/d; /^- Expected:/d' "$FILE_TASK"
expect_reject 4 'task missing TASK-VERIFY' 'missing ## TASK-VERIFY'

# --- check 10: traceability ---
reset; sed -i '/^## TASK-ACCEPTANCE$/a - `NOPE-R999`' "$FILE_TASK"
expect_reject 10 'unknown acceptance identifier' 'unknown identifier'

reset; sed -i '/^## TASK-ACCEPTANCE$/a - `PROJECT-PAGES-R001`' "$FILE_TASK"
expect_reject 10 'acceptance from a feature not in TASK-FEATURES' 'not in TASK-FEATURES'

reset; sed -i '/^## Dependencies$/a - `conventions`' "$FILE_FEATURE"
expect_reject 10 'a dependency that is not a feature file' 'feature dependency is not a feature file'

reset; sed -i '/^## Dependencies$/a - `99-missing.md`' "$FILE_FEATURE"
expect_reject 10 'unresolved feature dependency' 'feature dependency does not resolve'

reset; sed -i '/^## Requirements$/a - `FIXTURE-R001` A duplicate of the first requirement.' "$FILE_FEATURE"
expect_reject 10 'duplicate requirement identifier' 'duplicate requirement identifier'

reset; sed -i '/^## Requirements$/a - A new unnamed requirement.' "$FILE_FEATURE"
expect_reject 10 'requirement without an identifier' 'requirement without an identifier'

reset; sed -i '/^## Requirements$/,/^## Behavior$/{/^- `/d}' "$FILE_FEATURE"
expect_reject 10 'feature without requirement identifiers' 'no requirement identifiers'

reset; sed -i '/^## TASK-ACCEPTANCE$/,/^OUTPUT:/{/^- `/d}' "$FILE_TASK"
expect_reject 10 'TASK-ACCEPTANCE without identifiers' 'TASK-ACCEPTANCE has no requirement identifiers'

reset; sed -i '/^## TASK-FEATURES$/d; /^\- `prompts\/features\/89-site-build.md`$/d' "$FILE_TASK"
expect_reject 10 'TASK-ACCEPTANCE without TASK-FEATURES' 'has TASK-ACCEPTANCE but no TASK-FEATURES'

reset; sed -i '/^## TASK-FEATURES$/d; /^\- `prompts\/features\/89-site-build.md`$/d; /^## TASK-ACCEPTANCE$/d; /^- `FIXTURE-R/d' "$FILE_TASK"
expect_reject 10 'feature referenced but no TASK-FEATURES' 'references a feature but has no TASK-FEATURES'

reset; sed -i '/^## TASK-ACCEPTANCE$/d; /^- `FIXTURE-R/d' "$FILE_TASK"
expect_reject 10 'TASK-FEATURES without TASK-ACCEPTANCE' 'has TASK-FEATURES but no TASK-ACCEPTANCE'

reset; sed -i 's#`prompts/features/89-site-build.md`#`prompts/03-conventions.md`#' "$FILE_TASK"
expect_reject 10 'TASK-FEATURES lists a non-feature' 'TASK-FEATURES must list feature files'

reset; sed -i 's#`prompts/features/89-site-build.md`#`prompts/features/99-missing.md`#' "$FILE_TASK"
expect_reject 10 'TASK-FEATURES lists a missing feature' 'TASK-FEATURES names a missing feature'

# --- checks 1, 2, 3, 8, 11: corpus and registry ---
reset; sed -i 's#| `prompts/how-to-write-research.md` |#| `prompts/99-missing.md` |#' "$FILE_CONTRACT"
expect_reject 1 'registry path that does not exist' 'registry path does not exist'

reset; printf '%s\n' 'See `prompts/does-not-exist.md`.' >> "$FILE_WORKFLOW"
expect_reject 2 'unresolved prompt reference' 'reference does not exist'

reset; cp "$FILE_FEATURE" "$DIRECTORY_ROOT/prompts/features/01-dup.md"
expect_reject 3 'duplicate feature number' 'duplicate feature number'

reset; printf '# Temporary rule file\n' > "$DIRECTORY_ROOT/prompts/common/04-temp.md"
expect_reject 8 'unregistered universal rule file' 'not in the contract section 2 registry'

reset; printf '%s\n' '- Change only what the task requires.' >> "$FILE_WORKFLOW"
expect_reject 11 'duplicated authoritative rule' 'duplicated in'

reset; sed -i '/Change only what the task requires/d' "$DIRECTORY_ROOT/prompts/common/01-universal-rules.md"
expect_reject 11 'a rule-family marker removed from its owner' 'marker not found in'

# --- checks 6, 6.1: features, episodes ---
reset; sed -i '/^## Purpose$/d' "$FILE_FEATURE"
expect_reject 6 'feature missing a required section' 'missing ## Purpose'

reset; sed -i '/^## EPISODE-GOAL$/d' "$FILE_EPISODE"
expect_reject 6.1 'episode missing its goal' 'missing ## EPISODE-GOAL'

reset; sed -i '/^## EPISODE-ACCEPTANCE$/d' "$FILE_EPISODE"
expect_reject 6.1 'episode missing acceptance' 'missing ## EPISODE-ACCEPTANCE'

reset; sed -i '/^## EPISODE-ACCEPTANCE$/,/^## EPISODE-RISKS$/{/^- /d}' "$FILE_EPISODE"
expect_reject 6.1 'episode acceptance without criteria' 'EPISODE-ACCEPTANCE has no criteria'

# --- check 9: root structure and portable tool configuration ---
#
# A fixture must be tracked for the structure checks to see it. `git add -f` is
# deliberate: a project may ignore these very names for good reasons -- one
# project ignores `config.json` because it holds a device password -- and an
# ignored fixture would make the case pass for the wrong reason, silently. The
# force applies to the fixture only, never to the project's whole tree.
reset; printf '%s\n' 'x' > "$DIRECTORY_ROOT/NOTES.md"
git -C "$DIRECTORY_ROOT" add -f NOTES.md
expect_reject 9 'stray root file' 'root file is not permitted'

reset; mkdir -p "$DIRECTORY_ROOT/.undocumented"
printf '%s\n' 'x' > "$DIRECTORY_ROOT/.undocumented/config.json"
git -C "$DIRECTORY_ROOT" add -f .undocumented/config.json
expect_reject 9 'undeclared tool directory' 'tool directory is not permitted'

reset; mkdir -p "$DIRECTORY_ROOT/.vscode"
printf '%s\n' '{ "path": "/home/someone/sources" }' > "$DIRECTORY_ROOT/.vscode/settings.json"
git -C "$DIRECTORY_ROOT" add -f .vscode/settings.json
expect_reject 9 'absolute home path in tool configuration' 'absolute home path'

# --- checks 9.1, 9.2, 11: the agent entry point and unattended scripts ---
reset; rm "$DIRECTORY_ROOT/AGENTS.md"
expect_reject 9.1 'missing agent entry point' 'AGENTS.md is missing'

reset; printf '# Agent Entry Point\n\nRun `make test`.\n' > "$DIRECTORY_ROOT/AGENTS.md"
expect_reject 9.1 'agent entry point without pointers' 'AGENTS.md does not point at prompts/01-contract.md'

reset; printf '# Agent Entry Point\n\n- `prompts/01-contract.md`\n- `prompts/02-workflow.md`\n- `prompts/03-conventions.md`\n- `prompts/how-to-write-tasks.md`\n' > "$DIRECTORY_ROOT/AGENTS.md"
expect_reject 9.1 'entry point that omits the check command' 'does not name the `make test` command'

reset; printf '%s\n' '- Change only what the task requires.' >> "$DIRECTORY_ROOT/AGENTS.md"
expect_reject 11 'rule restated in the agent entry point' 'duplicated in AGENTS.md'

reset; printf '#!/bin/sh\nread -p "Continue? " answer\n' > "$DIRECTORY_ROOT/scripts/prompt-user.sh"
expect_reject 9.2 'script that blocks on standard input' 'blocks on standard input'

reset; printf '#!/bin/sh\nread -r answer < /dev/tty\n' > "$DIRECTORY_ROOT/scripts/prompt-tty.sh"
expect_reject 9.2 'script that reads the terminal' 'reads the terminal'

# --- check 12: protected paths ---
reset; sed -i 's#`scripts/site-build.sh`#`.git/config`#' "$FILE_TASK"
expect_reject 12 'a task authorizing a version-control path' 'authorizes a protected path'

reset; sed -i 's#`scripts/site-build.sh`#`logs/session.log`#' "$FILE_TASK"
expect_reject 12 'a task authorizing generated output' 'authorizes a protected path'

# --- check 13: capability registry ---
reset; sed -i '/^    - `06-execution-boundary.md`$/d' "$DIRECTORY_ROOT/prompts/03-conventions.md"
expect_reject 13 'capability convention missing from Applicability' 'not listed in the Applicability section'

# --- check 14: undeclared top-level directories ---
reset; mkdir -p "$DIRECTORY_ROOT/llama.cpp"
printf '%s\n' 'A third-party checkout inside the working tree.' > "$DIRECTORY_ROOT/llama.cpp/README.md"
expect_reject 14 'a third-party checkout inside the working tree' 'top-level directory is not permitted'

# --- acceptance cases: the validator must not reject a valid variation ---
# The fixture feature is made a dependency of feature 02, so a task that lists 02
# may claim a requirement owned by the fixture: dependencies apply transitively.
# The dependency is added here rather than assumed, because the shared feature no
# longer names a project-specific one.
reset; sed -i '/^## Dependencies$/a - `89-site-build.md`' "$FILE_FEATURE_02"
sed -i 's#`prompts/features/89-site-build.md`#`prompts/features/02-project-pages.md`#' "$FILE_TASK"
expect_accept 'acceptance reachable through a feature dependency'

reset; cat > "$DIRECTORY_ROOT/prompts/features/90-eval-sample.md" <<'EOF'
# Feature: Eval Sample

## Purpose

Demonstrate that a feature with no dependencies validates.

## Requirements

- `EVAL-SAMPLE-R001` The sample exists.

## Behavior

- The sample declares no dependencies.

## Dependencies

- None.
EOF
expect_accept 'a feature that declares no dependencies'

reset; sed -i '/^## TASK-CONTEXT$/,/^## TASK-FILES$/{/^## TASK-FILES$/!d}' "$FILE_TASK"
expect_accept 'a task without the optional TASK-CONTEXT section'

reset; printf '\n## How this project does it\n\n- This project appends a project section.\n' >> "$DIRECTORY_ROOT/prompts/03-conventions.md"
expect_accept 'a project appendix on a shared rule file'

reset; printf '#!/bin/sh\nwhile IFS= read -r line; do\n    :\ndone < "$1"\n' > "$DIRECTORY_ROOT/scripts/loop-input.sh"
expect_accept 'a script that reads a file, not standard input'

reset; sed -i 's#`scripts/site-build.sh`#`logs/.gitkeep`#' "$FILE_TASK"
expect_accept 'a tracked placeholder inside a generated directory'

echo "10-prompt-validator: ok ($count_case rejected, $count_accept accepted)"
exit 0
