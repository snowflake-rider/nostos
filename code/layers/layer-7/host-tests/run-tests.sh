#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd -P)"
BUILD_DIR="$SCRIPT_DIR/build"

mkdir -p "$BUILD_DIR"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$PROJECT_DIR/main" \
  "$PROJECT_DIR/main/serial_command.c" \
  "$SCRIPT_DIR/test_serial_command.c" \
  -o "$BUILD_DIR/test_serial_command"

"$BUILD_DIR/test_serial_command"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$PROJECT_DIR/main" \
  "$PROJECT_DIR/main/serial_command.c" \
  "$SCRIPT_DIR/test_serial_command.c" \
  -o "$BUILD_DIR/test_serial_command_sanitized"

"$BUILD_DIR/test_serial_command_sanitized"
