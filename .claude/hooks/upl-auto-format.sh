#!/bin/bash
# PostToolUse: auto-format C++ files after Edit/Write using the repo's .clang-format.

CLANG_FMT="$(command -v clang-format)"
[ -z "$CLANG_FMT" ] && exit 0

INPUT=$(cat)
FILE_PATH=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty')

[ -z "$FILE_PATH" ] && exit 0
[ -f "$FILE_PATH" ] || exit 0

case "$FILE_PATH" in
    *.cpp|*.h|*.hpp|*.cc|*.cxx|*.mm) ;;
    *) exit 0 ;;
esac

case "$FILE_PATH" in
    */Build/*|*/ThirdParty/*) exit 0 ;;
esac

"$CLANG_FMT" -i "$FILE_PATH" 2>/dev/null
exit 0
