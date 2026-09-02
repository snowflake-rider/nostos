#!/usr/bin/env bash
set -euo pipefail

FIRMWARE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
target="${1:-all}"

discover_executable() {
    local name="$1"
    local configured_path="${2:-}"
    local search_root="${XDG_DATA_HOME:-${HOME}/.local/share}/nostos-toolchains"
    local found=''

    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
        return
    fi
    if [[ -n "$configured_path" && -x "$configured_path" ]]; then
        printf '%s\n' "$configured_path"
        return
    fi
    if [[ -d "$search_root" ]]; then
        found="$(find "$search_root" -type f -name "$name" -perm -111 -print -quit 2>/dev/null || true)"
    fi
    [[ -n "$found" ]] && printf '%s\n' "$found"
}

activate_stm32_tools() {
    local cached_compiler=''
    local cached_ninja=''
    local compiler_file
    compiler_file="$(find "$FIRMWARE_DIR/stm32/build/Release/CMakeFiles" -name CMakeCCompiler.cmake -print -quit 2>/dev/null || true)"
    if [[ -n "$compiler_file" ]]; then
        cached_compiler="$(sed -n 's/^set(CMAKE_C_COMPILER "\([^"]*\)")$/\1/p' "$compiler_file" | head -1)"
    fi
    if [[ -f "$FIRMWARE_DIR/stm32/build/Release/CMakeCache.txt" ]]; then
        cached_ninja="$(sed -n 's/^CMAKE_MAKE_PROGRAM:FILEPATH=//p' "$FIRMWARE_DIR/stm32/build/Release/CMakeCache.txt" | head -1)"
    fi

    local arm_gcc
    local arm_gcc_dir
    local ninja_bin
    local ninja_dir
    arm_gcc="$(discover_executable arm-none-eabi-gcc "$cached_compiler")"
    [[ -n "$arm_gcc" ]] || {
        printf '%s\n' 'error: arm-none-eabi-gcc not found; set PATH or install the NOSTOS ARM toolchain' >&2
        return 1
    }
    arm_gcc_dir="$(dirname -- "$arm_gcc")"
    export PATH="$arm_gcc_dir:$PATH"

    ninja_bin="$(discover_executable ninja "$cached_ninja")"
    if [[ -n "$ninja_bin" ]]; then
        ninja_dir="$(dirname -- "$ninja_bin")"
        export PATH="$ninja_dir:$PATH"
    elif [[ "${NOSTOS_RELEASE_BUILD:-0}" == 1 ]]; then
        printf '%s\n' 'error: release build requires Ninja; no build was started' >&2
        return 1
    fi
}

activate_esp32_tools() {
    local search_root="${XDG_DATA_HOME:-${HOME}/.local/share}/nostos-toolchains"
    local export_script="${NOSTOS_IDF_EXPORT:-}"
    local standard_export="${HOME}/esp/esp-idf-v5.5.5/export.sh"
    local env_cache="$FIRMWARE_DIR/out/idf-env-cache.sh"
    if [[ -z "$export_script" && -f "$standard_export" ]]; then
        export_script="$standard_export"
    fi
    if [[ -z "$export_script" && -d "$search_root" ]]; then
        export_script="$(find "$search_root" -path '*/esp-idf-v5.5.5/export.sh' -type f -print -quit 2>/dev/null || true)"
    fi

    if ! command -v idf.py >/dev/null 2>&1 && [[ -f "$env_cache" ]] && \
        { [[ -z "$export_script" ]] || [[ "$env_cache" -nt "$export_script" ]]; }; then
        # shellcheck disable=SC1090
        source "$env_cache"
    fi
    if ! command -v idf.py >/dev/null 2>&1 && [[ -n "$export_script" && -f "$export_script" ]]; then
        local original_path="$PATH"
        # shellcheck disable=SC1090
        source "$export_script" >/dev/null 2>&1
        local idf_prefix="$PATH"
        if [[ "$PATH" == *":$original_path" ]]; then
            idf_prefix="${PATH%":$original_path"}"
        fi
        mkdir -p "$(dirname -- "$env_cache")"
        {
            printf '# Generated from %q; delete to refresh.\n' "$export_script"
            for variable_name in ESP_IDF_VERSION ESP_ROM_ELF_DIR IDF_PATH IDF_PYTHON_ENV_PATH \
                IDF_TOOLS_EXPORT_CMD IDF_TOOLS_INSTALL_CMD OPENOCD_SCRIPTS; do
                printf 'export %s=%q\n' "$variable_name" "${!variable_name:-}"
            done
            # Keep $PATH literal so the cache extends the caller's future PATH.
            # shellcheck disable=SC2016
            printf 'export PATH=%q:"$PATH"\n' "$idf_prefix"
        } > "$env_cache"
    fi
    command -v idf.py >/dev/null 2>&1 || {
        printf '%s\n' 'error: ESP-IDF v5.5.5 not found; set NOSTOS_IDF_EXPORT or activate the environment' >&2
        return 1
    }
    local cached_ninja=''
    if [[ -f "$FIRMWARE_DIR/esp32/build/CMakeCache.txt" ]]; then
        cached_ninja="$(sed -n 's/^CMAKE_MAKE_PROGRAM:FILEPATH=//p' "$FIRMWARE_DIR/esp32/build/CMakeCache.txt" | head -1)"
    fi
    local ninja_bin
    local ninja_dir
    ninja_bin="$(discover_executable ninja "$cached_ninja")"
    [[ -n "$ninja_bin" ]] || {
        printf '%s\n' 'error: Ninja not found; ESP32 build was not started' >&2
        return 1
    }
    ninja_dir="$(dirname -- "$ninja_bin")"
    export PATH="$PATH:$ninja_dir"
}

build_stm32() {
    activate_stm32_tools
    local stm32_ssd1306_display="${NOSTOS_STM32_SSD1306_DISPLAY:-ON}"
    local stm32_mpu6050_sensor="${NOSTOS_STM32_MPU6050_SENSOR:-OFF}"
    local stm32_dht11_sensor="${NOSTOS_STM32_DHT11_SENSOR:-OFF}"
    local stm32_feature_value
    for stm32_feature_value in \
        "$stm32_ssd1306_display" \
        "$stm32_mpu6050_sensor" \
        "$stm32_dht11_sensor"; do
        case "$stm32_feature_value" in
            ON|OFF) ;;
            *)
                printf 'error: STM32 feature overrides must be ON or OFF; found: %s\n' \
                    "$stm32_feature_value" >&2
                return 1
                ;;
        esac
    done
    (
        cd "$FIRMWARE_DIR/stm32"
        cmake_args=(
            --preset Release
            -DNOSTOS_FREERTOS=ON
            -DSSD1306_DISPLAY="$stm32_ssd1306_display"
            -DMPU6050_SENSOR="$stm32_mpu6050_sensor"
            -DDHT11_SENSOR="$stm32_dht11_sensor"
        )
        if ! command -v ninja >/dev/null 2>&1; then
            cmake_args+=(-G "Unix Makefiles")
        fi
        cmake "${cmake_args[@]}"
        cmake --build --preset Release --parallel
        arm-none-eabi-objcopy -O binary \
            build/Release/nostos_stm32.elf \
            build/Release/nostos_stm32.bin
    )
}

build_esp32() {
    activate_esp32_tools
    if [[ -n "${IDF_PATH:-}" && -f "$IDF_PATH/version.txt" ]]; then
        idf_version="$(cat "$IDF_PATH/version.txt")"
    else
        idf_version="$(idf.py --version)"
    fi
    printf '%s\n' "$idf_version"
    case "$idf_version" in
        *v5.5.5*|*" 5.5.5"*) ;;
        *)
            printf 'error: ESP-IDF v5.5.5 is required; found: %s\n' "$idf_version" >&2
            return 1
            ;;
    esac
    (
        cd "$FIRMWARE_DIR/esp32"
        if [[ -n "${IDF_PYTHON_ENV_PATH:-}" && -x "$IDF_PYTHON_ENV_PATH/bin/python" && \
            -n "${IDF_PATH:-}" && -f "$IDF_PATH/tools/idf.py" ]]; then
            "$IDF_PYTHON_ENV_PATH/bin/python" "$IDF_PATH/tools/idf.py" build
        else
            idf.py build
        fi
    )
}

case "$target" in
    stm32) build_stm32 ;;
    esp32) build_esp32 ;;
    all)
        build_stm32
        build_esp32
        ;;
    *)
        printf 'usage: %s [all|stm32|esp32]\n' "$0" >&2
        exit 2
        ;;
esac
