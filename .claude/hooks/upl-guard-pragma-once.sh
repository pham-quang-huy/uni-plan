#!/bin/bash
# PreToolUse guard: ensures new header files include "#pragma once".
# CODING.md policy: "Use #pragma once in headers."
# Only triggers on Write (new file creation).

INPUT=$(cat)
FILE_PATH=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty')

case "$FILE_PATH" in
    *.h|*.hpp) ;;
    *) exit 0 ;;
esac

case "$FILE_PATH" in
    */Build/*|*/ThirdParty/*) exit 0 ;;
esac

CONTENT=$(echo "$INPUT" | jq -r '.tool_input.content // empty')
[ -z "$CONTENT" ] && exit 0

if ! echo "$CONTENT" | grep -q '#pragma once'; then
    BASENAME=$(basename "$FILE_PATH")
    echo "Blocked: new header $BASENAME is missing '#pragma once' (CODING.md policy)." >&2
    echo "Add '#pragma once' at the top of the file." >&2
    exit 2
fi
exit 0
