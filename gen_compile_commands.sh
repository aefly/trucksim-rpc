#!/usr/bin/env bash
set -euo pipefail

MINGW_CXX=x86_64-w64-mingw32-g++
PROJECT_ROOT=$(dirname "$(readlink -f "$0")")

INCLUDE_PATHS=$(
  LANG=C "$MINGW_CXX" -E -x c++ -v /dev/null 2>&1 \
    | sed -n '/^#include <...> search starts here:/,/^End of search list/p' \
    | grep '^ ' \
    | sed 's/^ *//'
)

FLAGS=()
FLAGS+=("-x" "c++")
FLAGS+=("-std=c++23")
FLAGS+=("--target=x86_64-w64-mingw32")
FLAGS+=("-DWIN32" "-D_WIN32" "-D_WIN64")
FLAGS+=("-DDISCORD_APP_ID=0")
FLAGS+=("-include" "src/sdk/discord/compat/preinclude.h")

while IFS= read -r path; do
  [ -n "$path" ] && FLAGS+=("-isystem" "$path")
done <<< "$INCLUDE_PATHS"

FLAGS+=("-I" "src/sdk/scs/include")
FLAGS+=("-I" "src/sdk/discord/cpp")
FLAGS+=("-I" "src/sdk/discord/compat")
FLAGS+=("-I" "src/telemetry")
FLAGS+=("-I" "src/discord")
FLAGS+=("-I" "src/presence")
FLAGS+=("-I" "src/config")
FLAGS+=("-I" "src/log")
FLAGS+=("-I" "src/core")

SOURCES=(
  "src/log/log.cpp"
  "src/telemetry/telemetry.cpp"
  "src/discord/discord_client.cpp"
  "src/presence/presence.cpp"
  "src/core/plugin.cpp"
)

# Add Discord SDK C++ sources
for f in src/sdk/discord/cpp/*.cpp; do
  SOURCES+=("$f")
done

json_escape() {
  printf '%s' "$1" | python3 -c 'import sys,json; print(json.dumps(sys.stdin.read()))'
}

build_args() {
  local out='"clang++"'
  local f
  for f in "${FLAGS[@]}"; do
    out+=", $(json_escape "$f")"
  done
  printf '%s' "$out"
}

ARGS_JSON=$(build_args)

OUTPUT="$PROJECT_ROOT/compile_commands.json"
{
  printf '['
  for i in "${!SOURCES[@]}"; do
    comma=","
    [ $((i + 1)) -eq ${#SOURCES[@]} ] && comma=""
    file="${SOURCES[$i]}"
    printf '
  {
    "directory": %s,
    "file": %s,
    "arguments": [%s]
  }%s' "$(json_escape "$PROJECT_ROOT")" "$(json_escape "$file")" "$ARGS_JSON" "$comma"
  done
  printf '
]
'
} > "$OUTPUT"

echo "Generated $OUTPUT ($(wc -c < "$OUTPUT") bytes)"
