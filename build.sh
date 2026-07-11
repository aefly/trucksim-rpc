#!/usr/bin/env bash
set -e
trap 'echo "Build failed."; exit 1' ERR

# Load APP_ID from .env if present
if [ -f .env ]; then
  set -a
  source .env
  set +a
fi

if [ -z "${APP_ID:-}" ]; then
  echo "Error: APP_ID not set."
  echo "Set APP_ID=<id> in .env"
  exit 1
fi

start_time=$(date +%s)

mkdir -p build/bin/win_x64/plugins

x86_64-w64-mingw32-windres trucksim-rpc.rc -O coff -o build/trucksim-rpc_res.o

cp src/sdk/discord/lib/x86_64/discord_game_sdk.dll build/bin/win_x64/

DISCORD_SOURCES=$(ls src/sdk/discord/cpp/*.cpp)

x86_64-w64-mingw32-g++ -shared -m64 -O2 -std=c++23 -o build/bin/win_x64/plugins/trucksim-rpc.dll \
    -DDISCORD_APP_ID=$APP_ID \
    build/trucksim-rpc_res.o \
    src/log/log.cpp \
    src/telemetry/telemetry.cpp \
    src/discord/discord_client.cpp \
    src/presence/presence.cpp \
    src/core/plugin.cpp \
    $DISCORD_SOURCES \
    trucksim-rpc.def \
    -include src/sdk/discord/compat/preinclude.h \
    -I src/sdk/scs/include \
    -I src/sdk/discord/cpp \
    -I src/sdk/discord/compat \
    -I src/telemetry \
    -I src/discord \
    -I src/presence \
    -I src/config \
    -I src/log \
    -I src/core \
    src/sdk/discord/lib/x86_64/discord_game_sdk.dll.lib \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
rm -f build/trucksim-rpc_res.o

end_time=$(date +%s)
elapsed=$((end_time - start_time))
echo "Build successful (${elapsed}s):"
echo "  - build/bin/win_x64/plugins/trucksim-rpc.dll"
echo "  - build/bin/win_x64/discord_game_sdk.dll"
