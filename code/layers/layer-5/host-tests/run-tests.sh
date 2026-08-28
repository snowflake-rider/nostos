#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${script_dir}/build"

mkdir -p "${build_dir}"

cc \
    -std=c11 \
    -Wall \
    -Wextra \
    -Werror \
    -pedantic \
    -I"${script_dir}/../main" \
    "${script_dir}/test_layer_packet.c" \
    "${script_dir}/../main/layer_packet.c" \
    -o "${build_dir}/test_layer_packet"

"${build_dir}/test_layer_packet"
