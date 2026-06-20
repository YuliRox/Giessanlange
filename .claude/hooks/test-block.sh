#!/bin/bash
# Verifies block-secrets.sh: feeds it staged JSON inputs and reports
# which got denied. Output JSON deny = correct deny; no output = pass-through.

set -e
HOOK="$(dirname "$0")/block-secrets.sh"
S="secrets"
ext=".ini"
target="${S}${ext}"

run() {
  local label="$1"; shift
  local payload="$1"; shift
  echo "=== $label ==="
  printf '%s\n' "$payload" | bash "$HOOK"
  echo "(end)"
  echo
}

run "Read target -> DENY" \
  "{\"tool_name\":\"Read\",\"tool_input\":{\"file_path\":\"D:/x/${target}\"}}"

run "Read target.example -> PASS" \
  "{\"tool_name\":\"Read\",\"tool_input\":{\"file_path\":\"D:/x/${target}.example\"}}"

run "Bash cat target -> DENY" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"cat ${target}\"}}"

run "Bash unrelated -> PASS" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"git status\"}}"

run "Grep on target path -> DENY" \
  "{\"tool_name\":\"Grep\",\"tool_input\":{\"pattern\":\"foo\",\"path\":\"${target}\"}}"

run "Glob target -> DENY" \
  "{\"tool_name\":\"Glob\",\"tool_input\":{\"pattern\":\"**/${target}\"}}"

run "Glob unrelated -> PASS" \
  "{\"tool_name\":\"Glob\",\"tool_input\":{\"pattern\":\"**/*.md\"}}"

run "Bash cat target.example -> PASS (was a false-positive before)" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"cat ${target}.example\"}}"

run "Bash ./target -> DENY" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"cat ./${target}\"}}"

run "Bash word containing target as substring -> PASS" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"echo mysecrets.ini.bak\"}}"

run "Bash git commit mentioning target in msg -> PASS" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"git commit -m 'fix ${target} leak'\"}}"

run "Bash gh issue body mentioning target -> PASS" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"gh issue create --body 'about ${target}'\"}}"

run "Bash echo target -> PASS" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"echo ${target}\"}}"

run "Bash less target -> DENY" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"less ${target}\"}}"

run "Bash xxd target -> DENY" \
  "{\"tool_name\":\"Bash\",\"tool_input\":{\"command\":\"xxd ${target}\"}}"
