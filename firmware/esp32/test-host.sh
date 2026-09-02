#!/usr/bin/env bash
set -euo pipefail

LAYER_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
FIRMWARE_DIR="$(cd -- "$LAYER_DIR/.." && pwd -P)"
BUILD_ROOT="${NOSTOS_ESP32_TEST_BUILD_DIR:-$FIRMWARE_DIR/out/host-tests/esp32}"
GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
MODE="${1:-full}"
case "$MODE" in
    --fast) variants=(fast) ;;
    full) variants=(debug release sanitized) ;;
    *) printf 'usage: %s [--fast]\n' "$0" >&2; exit 2 ;;
esac

check_mesh_policy() {
    local defaults="$LAYER_DIR/sdkconfig.defaults"
    local resolved="$LAYER_DIR/sdkconfig"
    local composition="$LAYER_DIR/main/mesh_node.c"
    local failures=0
    local key
    local token

    for key in \
        CONFIG_BLE_MESH \
        CONFIG_BLE_MESH_NODE \
        CONFIG_BLE_MESH_PB_ADV \
        CONFIG_BLE_MESH_PB_GATT \
        CONFIG_BLE_MESH_GATT_PROXY_SERVER \
        CONFIG_BLE_MESH_SETTINGS \
        CONFIG_BLE_MESH_RELAY \
        CONFIG_BLE_MESH_HEALTH_SRV \
        CONFIG_BT_GATTC_ENABLE \
        CONFIG_BLE_MESH_SUPPORT_BLE_SCAN; do
        if ! grep -Fqx -- "$key=y" "$defaults" ||
           ! grep -Fqx -- "$key=y" "$resolved"; then
            printf 'FAIL Mesh policy requires %s=y in defaults and resolved config\n' \
                "$key" >&2
            failures=$((failures + 1))
        fi
    done

    for key in \
        CONFIG_BLE_MESH_V11_SUPPORT \
        CONFIG_BLE_MESH_DEINIT \
        CONFIG_BLE_MESH_GENERIC_SERVER; do
        if ! grep -Fqx -- "$key=n" "$defaults" ||
           ! grep -Fqx -- "# $key is not set" "$resolved"; then
            printf 'FAIL Mesh policy requires %s disabled in defaults and resolved config\n' \
                "$key" >&2
            failures=$((failures + 1))
        fi
    done

    for token in \
        ESP_BLE_MESH_MODEL_CFG_SRV \
        ESP_BLE_MESH_MODEL_HEALTH_SRV \
        ESP_BLE_MESH_VENDOR_MODEL; do
        if ! grep -Fq -- "$token" "$composition"; then
            printf 'FAIL Mesh composition is missing %s\n' "$token" >&2
            failures=$((failures + 1))
        fi
    done
    if grep -Eq 'ESP_BLE_MESH_MODEL_GEN_|esp_ble_mesh_generic_model_api' \
        "$composition"; then
        printf '%s\n' 'FAIL Mesh composition must not include Generic Models' >&2
        failures=$((failures + 1))
    fi
    if [[ "$failures" -ne 0 ]]; then
        return 1
    fi
    printf '%s\n' \
        'ESP32_MESH_POLICY=PASS; foundation servers + one Vendor Model; no Generic/V1.1/deinit'
}

check_mesh_policy
if [[ -z "${CMAKE_GENERATOR:-}" ]] && command -v ninja >/dev/null 2>&1; then GENERATOR=Ninja; fi
for variant in "${variants[@]}"; do
    build_type=Debug
    sanitizers=OFF
    if [[ "$variant" == release ]]; then build_type=Release; fi
    if [[ "$variant" == sanitized ]]; then sanitizers=ON; fi
    build_dir="$BUILD_ROOT/$variant"
    cmake -S "$LAYER_DIR/host-tests" -B "$build_dir" -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE="$build_type" -DENABLE_SANITIZERS="$sanitizers"
    cmake --build "$build_dir"
    ctest --test-dir "$build_dir" --output-on-failure
done
