#!/bin/bash
# PreToolUse hook: deny any tool call that would access secrets.ini.
#
# Fires before Read / Edit / Write / Bash / Grep / Glob. Inspects the
# tool input on stdin (JSON) and prints a deny decision if the call
# targets secrets.ini in any form. Exits 0 with no output otherwise.
#
# Invoked via .claude/settings.json hooks.PreToolUse entry.

set -e

input=$(cat)

tool_name=$(printf '%s' "$input" | jq -r '.tool_name // ""')
tool_input=$(printf '%s' "$input" | jq -c '.tool_input // {}')

should_block=false
reason="Access to secrets.ini is blocked. It contains WiFi/MQTT credentials and must not be read, edited, or displayed. Point the user at secrets.ini.example instead."

# Match secrets.ini as a whole filename (not as a prefix of e.g.
# secrets.ini.example). Boundary chars on both sides: anything outside
# [a-zA-Z0-9._-] counts as "not a filename character".
PROTECTED_RX='(^|[^a-zA-Z0-9._-])secrets\.ini([^a-zA-Z0-9._-]|$)'

# Bash/PowerShell commands that would leak the file's contents if pointed
# at it. The hook only denies a shell command when the command line
# starts with one of these AND mentions the protected filename. This
# lets `git commit -m "..." `, `gh issue create --body "..."`, echo etc.
# mention the name freely (they don't read file contents).
LEAK_CMDS_RX='^[[:space:]]*(cat|head|tail|grep|rg|egrep|fgrep|sed|awk|less|more|cp|mv|tee|xxd|od|hexdump|file|strings|nl|tac|column|pr|fold|expand|unexpand|cut|paste|sort|uniq|wc|view|nano|vim|vi|emacs|code|notepad)([[:space:]]|$)'

matches() {
  [[ "$1" =~ $PROTECTED_RX ]]
}

case "$tool_name" in
  Read|Edit|Write|NotebookEdit)
    file_path=$(printf '%s' "$tool_input" | jq -r '.file_path // ""')
    matches "$file_path" && should_block=true
    ;;
  Bash|PowerShell)
    command=$(printf '%s' "$tool_input" | jq -r '.command // ""')
    if [[ "$command" =~ $LEAK_CMDS_RX ]] && matches "$command"; then
      should_block=true
    fi
    ;;
  Grep)
    path=$(printf '%s' "$tool_input" | jq -r '.path // ""')
    glob=$(printf '%s' "$tool_input" | jq -r '.glob // ""')
    if matches "$path" || matches "$glob"; then should_block=true; fi
    ;;
  Glob)
    pattern=$(printf '%s' "$tool_input" | jq -r '.pattern // ""')
    path=$(printf '%s' "$tool_input" | jq -r '.path // ""')
    if matches "$pattern" || matches "$path"; then should_block=true; fi
    ;;
esac

if [ "$should_block" = true ]; then
  jq -n --arg r "$reason" '{
    hookSpecificOutput: {
      hookEventName: "PreToolUse",
      permissionDecision: "deny",
      permissionDecisionReason: $r
    }
  }'
  exit 0
fi

# No decision — let normal permission flow proceed.
exit 0
