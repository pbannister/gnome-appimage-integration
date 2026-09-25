#!/bin/sh
#
# Leak-gate test (scripts/leak-gate.sh and scripts/sensitive-patterns.sh).
#
# Tier: portable. Checks that the gate refuses private addresses, MAC
# addresses, home paths, and key material, and that it does NOT refuse
# ordinary content that merely resembles a pattern (a public address, or a
# numbered heading like "## 10. Human Override").
#
# It also guards the single source: the pattern is defined in
# scripts/sensitive-patterns.sh and inlined nowhere else in scripts/.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
SCRIPT_GATE="$REPOSITORY_ROOT/scripts/leak-gate.sh"

DIRECTORY_TEST=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEST"' EXIT HUP INT TERM

fail() {
    echo "04-leak-gate: $1" >&2
    exit 1
}

# --- the pattern has one source ---------------------------------------------
files_inline=$(grep -l '192\.168\.' "$REPOSITORY_ROOT"/scripts/*.sh \
    | grep -v 'sensitive-patterns.sh' || true)
if [ -n "$files_inline" ]; then
    fail "the sensitive pattern is duplicated in: $files_inline"
fi

# --- clean content passes ----------------------------------------------------
mkdir -p "$DIRECTORY_TEST/clean"
cat > "$DIRECTORY_TEST/clean/page.html" <<'EOF'
<h1>Findings</h1>
<p>A public resolver at 8.8.8.8, and a block just outside RFC 1918 at
172.32.0.1; the host answered on 10. examples are below.</p>
<h2>10. Human Override</h2>
<p>Nothing private is recorded here.</p>
EOF

if ! output_clean=$(sh "$SCRIPT_GATE" "$DIRECTORY_TEST/clean" 2>&1); then
    fail "the gate refused clean content: $output_clean"
fi

# --- leaky content is refused, with the offending lines shown ---------------
mkdir -p "$DIRECTORY_TEST/leaky"
printf '%s\n' '<p>Host 192.168.1.10 at aa:bb:cc:dd:ee:ff.</p>' \
    > "$DIRECTORY_TEST/leaky/page.html"
printf '%s\n' 'Path /home/example/work/project' \
    > "$DIRECTORY_TEST/leaky/home.txt"
printf '%s\n' '-----BEGIN RSA PRIVATE KEY-----' \
    > "$DIRECTORY_TEST/leaky/key.txt"

if output_leaky=$(sh "$SCRIPT_GATE" "$DIRECTORY_TEST/leaky" 2>&1); then
    fail 'the gate accepted sensitive content'
fi

printf '%s\n' "$output_leaky" | grep -q 'REFUSING' \
    || fail 'the refusal was not reported'
printf '%s\n' "$output_leaky" | grep -q '192\.168\.1\.10' \
    || fail 'the private address was not shown'
printf '%s\n' "$output_leaky" | grep -q 'PRIVATE KEY' \
    || fail 'the key material was not shown'

# --- a missing path is a failure, not a pass --------------------------------
if sh "$SCRIPT_GATE" "$DIRECTORY_TEST/absent" >/dev/null 2>&1; then
    fail 'a missing path was accepted'
fi

echo '04-leak-gate: ok'
exit 0
