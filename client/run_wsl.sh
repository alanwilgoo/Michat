#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_dir"

if [[ ! -x build/bin/chat_sim ]]; then
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
fi

export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
if [[ -z "${SDL_VIDEODRIVER:-}" ]]; then
    if [[ -S "${XDG_RUNTIME_DIR:-}/wayland-0" ]]; then
        export SDL_VIDEODRIVER=wayland
    else
        export SDL_VIDEODRIVER=x11
    fi
fi

exec "$project_dir/build/bin/chat_sim"
