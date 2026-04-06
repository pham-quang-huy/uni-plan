#!/bin/bash
# PreToolUse guard: blocks "using namespace" in C++ files.
# CODING.md policy: "No using namespace; always use full UniPlan:: paths"

INPUT=$(cat)
FILE_PATH=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty')

case "$FILE_PATH" in
    *.h|*.hpp|*.cpp|*.cc|*.cxx|*.mm) ;;
    *) exit 0 ;;
esac

CONTENT=$(echo "$INPUT" | jq -r '.tool_input.new_string // .tool_input.content // empty')
[ -z "$CONTENT" ] && exit 0

if echo "$CONTENT" | grep -qE '^\s*using\s+namespace\s+'; then
    BASENAME=$(basename "$FILE_PATH")
    echo "Blocked: 'using namespace' in $BASENAME violates CODING.md policy." >&2
    echo "Use fully qualified namespace paths instead (e.g., UniPlan::Foo instead of 'using namespace UniPlan')." >&2
    exit 2
fi
exit 0
