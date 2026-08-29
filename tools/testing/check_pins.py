#!/usr/bin/env python3
"""NOSTOS source configuration check, NOT an electrical/hardware certification.

Understands the current CubeMX GPIO assignment form only. Unsupported pin
expressions / new initializers block the check instead of silently passing.
"""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


class Unsupported(ValueError):
    pass


def clean(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def body(source, name):
    match = re.search(r"\bvoid\s+" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", source)
    if not match:
        raise Unsupported(f"함수 없음: {name}")
    depth, end = 1, match.end()
    while depth and end < len(source):
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    if depth:
        raise Unsupported(f"함수 범위 해석 실패: {name}")
    return source[match.end():end - 1]


def definitions(source):
    pairs = re.findall(r"^\s*#define[ \t]+(\w+)[ \t]+([^\n]+)", clean(source), re.M)
    return {k: v.strip() for k, v in pairs}


def resolve(token, defines):
    token = token.strip()
    for _ in range(10):
        if token not in defines:
            return token
        token = defines[token]
    raise Unsupported("매크로 순환: " + token)


def parse_gpio(source, name, defines):
    block = body(clean(source), name)
    if re.search(r"#\s*(?:if|else)|\b(?:for|while|switch)\s*\(", block):
        raise Unsupported(f"조건부/반복 초기화 수동 검토 필요: {name}")
    if name == "MX_GPIO_Init" and re.search(r"\bif\s*\(", block):
        raise Unsupported("조건부 GPIO 초기화 수동 검토 필요")
    # MSP branches must be the known per-instance guards, never nested logic.
    guards = re.findall(r"\bif\s*\(([^)]*)\)", block)
    allowed = {"HAL_I2C_MspInit": ["hi2c->Instance==I2C1"],
               "HAL_SPI_MspInit": ["hspi->Instance==SPI2"],
               "HAL_UART_MspInit": ["huart->Instance==USART1", "huart->Instance==USART2"]}
    if name in allowed and [re.sub(r"\s", "", g) for g in guards] != allowed[name]:
        raise Unsupported(f"MSP 분기 변경 수동 검토 필요: {name}")
    pattern = r"GPIO_InitStruct\.(\w+)\s*=\s*([^;]+);|HAL_GPIO_Init\s*\(\s*(\w+)\s*,\s*&GPIO_InitStruct\s*\);"
    config, result, calls = {}, [], 0
    for match in re.finditer(pattern, block):
        field, value, port = match.groups()
        if field:
            config[field] = value.strip()
            continue
        calls += 1
        port = resolve(port, defines)
        if not re.fullmatch(r"GPIO[A-H]", port) or not {"Pin", "Mode", "Pull"} <= config.keys():
            raise Unsupported(f"GPIO 초기화 형식 해석 실패: {name}")
        for token in config["Pin"].split("|"):
            pin = resolve(token, defines)
            if not re.fullmatch(r"GPIO_PIN_(?:[0-9]|1[0-5])", pin):
                raise Unsupported("핀 표현식 해석 실패: " + token)
            result.append({"pin": port.replace("GPIO", "P") + pin[9:],
                           "mode": config["Mode"], "pull": config["Pull"],
                           "af": config.get("Alternate"), "owner": name})
    if calls != len(re.findall(r"\bHAL_GPIO_Init\s*\(", block)) or calls == 0:
        raise Unsupported("지원하지 않는 GPIO 초기화 호출: " + name)
    return result


def inspect(root=ROOT):
    stm = root / "firmware/stm32"
    esp = root / "firmware/esp32"
    ioc = dict(line.split("=", 1) for line in (stm / "nostos_stm32.ioc").read_text().splitlines() if "=" in line)
    defines = definitions((stm / "Core/Inc/main.h").read_text())
    main = clean((stm / "Core/Src/main.c").read_text())
    msp = clean((stm / "Core/Src/stm32f4xx_hal_msp.c").read_text())
    expected = {}
    for key, signal in ioc.items():
        match = re.fullmatch(r"(P[A-H]\d+)(?:-WKUP)?\.Signal", key)
        if match:
            expected[match[1]] = (signal, ioc.get(key.replace("Signal", "GPIO_Label")), key)
    if not expected:
        raise Unsupported("IOC 핀 정보 없음")
    actual = parse_gpio(main, "MX_GPIO_Init", defines)
    for name in ("HAL_I2C_MspInit", "HAL_SPI_MspInit", "HAL_UART_MspInit"):
        actual += parse_gpio(msp, name, defines)
    known_calls = sum(len(re.findall(r"\bHAL_GPIO_Init\s*\(", body(source, name)))
                      for source, names in ((main, ["MX_GPIO_Init"]),
                                            (msp, ["HAL_I2C_MspInit", "HAL_SPI_MspInit", "HAL_UART_MspInit"]))
                      for name in names)
    if known_calls != len(re.findall(r"\bHAL_GPIO_Init\s*\(", main + msp)):
        raise Unsupported("추가 HAL_GPIO_Init 경로 수동 검토 필요")
    if re.search(r"->\s*(?:MODER|AFR|PUPDR)\b|\bLL_GPIO_Init\s*\(", main + msp):
        raise Unsupported("GPIO 직접 레지스터/LL 설정 수동 검토 필요")
    for directory in (stm / "MyApp", stm / "Core"):
        for path in directory.rglob("*.c"):
            if path.name in ("main.c", "stm32f4xx_hal_msp.c", "system_stm32f4xx.c"):
                continue
            if re.search(r"\b(?:HAL_GPIO_Init|LL_GPIO_Init)\s*\(|->\s*(?:MODER|AFR|PUPDR)\b", clean(path.read_text())):
                raise Unsupported(f"추가 핀 설정 경로 수동 검토 필요: {path.relative_to(root)}")
    errors, seen = [], set()
    board_contract = {"I2C1_SCL": "PB8", "I2C1_SDA": "PB9", "SPI2_SCK": "PB13",
                      "SPI2_MISO": "PB14", "SPI2_MOSI": "PB15", "USART1_TX": "PA9",
                      "USART1_RX": "PA10", "USART2_TX": "PA2", "USART2_RX": "PA3"}
    declared = {signal: pin for pin, (signal, _, _) in expected.items()}
    for signal, pin in board_contract.items():
        if declared.get(signal) != pin:
            errors.append(f"보드 핀 계약 불일치: {signal}={pin}")
    afs = {"I2C1": ("GPIO_AF4_I2C1", "GPIO_MODE_AF_OD"),
           "SPI2": ("GPIO_AF5_SPI2", "GPIO_MODE_AF_PP"),
           "USART1": ("GPIO_AF7_USART1", "GPIO_MODE_AF_PP"),
           "USART2": ("GPIO_AF7_USART2", "GPIO_MODE_AF_PP")}
    for item in actual:
        pin = item["pin"]
        if pin in ("PA13", "PA14"):
            errors.append("SWD 예약핀 초기화: " + pin)
        if pin in seen:
            errors.append(f"중복 초기화: {pin}")
        seen.add(pin)
        if pin not in expected:
            errors.append(f"IOC에 없는 핀 초기화: {pin}")
            continue
        signal, label, key = expected[pin]
        item["signal"] = signal
        if signal in ("GPIO_Input", "GPIO_Output"):
            mode = "GPIO_MODE_INPUT" if signal == "GPIO_Input" else "GPIO_MODE_OUTPUT_PP"
            if item["mode"] != mode:
                errors.append(f"모드 불일치: {pin}")
        else:
            af, mode = afs.get(signal.split("_")[0], (None, None))
            if af is None:
                raise Unsupported("지원하지 않는 주변장치: " + signal)
            if item["af"] != af or item["mode"] != mode:
                errors.append(f"AF/모드 불일치: {pin}")
        # CubeMX omits peripheral defaults (e.g. I2C pull-up); absence is
        # not evidence of GPIO_NOPULL. Compare only an explicit IOC value.
        pull = ioc.get(key.replace("Signal", "GPIO_PuPd"), ioc.get(key.replace("Signal", "GPIO_Pu")))
        if pull is not None and item["pull"] != pull:
            errors.append(f"Pull 불일치: {pin} ({item['pull']} / {pull})")
        if label:
            p = resolve(label + "_Pin", defines).replace("GPIO_PIN_", "")
            port = resolve(label + "_GPIO_Port", defines).replace("GPIO", "P")
            if port + p != pin:
                errors.append(f"main.h 라벨 불일치: {label}")
    for pin in expected.keys() - seen:
        errors.append(f"초기화 누락: {pin}")
    # Verify the board's declared UART contract, not just equality of two typos.
    for number in (1, 2):
        for field, value in {"BaudRate": "115200", "WordLength": "UART_WORDLENGTH_8B",
                             "StopBits": "UART_STOPBITS_1", "Parity": "UART_PARITY_NONE",
                             "HwFlowCtl": "UART_HWCONTROL_NONE"}.items():
            values = re.findall(rf"huart{number}\.Init\.{field}\s*=\s*([^;]+);", main)
            if values != [value]:
                errors.append(f"STM32 USART{number} {field}: 115200/8N1 계약 불일치")
    sdk = (esp / "sdkconfig").read_text()
    for setting in ('CONFIG_IDF_TARGET="esp32s3"', "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y"):
        if setting not in sdk.splitlines():
            errors.append("ESP32 설정 불일치: " + setting)
    v2 = "CONFIG_NOSTOS_PROTOCOL_V2=y" in sdk.splitlines()
    selected = "bridge_runtime_v2.c" if v2 else "bridge_runtime.c"
    runtime = clean((esp / "main" / selected).read_text())
    macros = definitions(runtime)
    calls = re.findall(r"\buart_set_pin\s*\(([^;]+)\)", runtime)
    if len(calls) != 1:
        raise Unsupported("ESP32 UART 핀 초기화 개수 변경")
    params = [resolve(value, macros) for value in calls[0].split(",")]
    if params != ["UART_NUM_1", "17", "18", "UART_PIN_NO_CHANGE", "UART_PIN_NO_CHANGE"]:
        errors.append("ESP32 UART1 TX17/RX18 계약 불일치 (USB19/20·예약핀 사용 금지)")
    for field, value in {"baud_rate": "115200", "data_bits": "UART_DATA_8_BITS",
                         "parity": "UART_PARITY_DISABLE", "stop_bits": "UART_STOP_BITS_1",
                         "flow_ctrl": "UART_HW_FLOWCTRL_DISABLE"}.items():
        values = re.findall(r"\." + field + r"\s*=\s*(\w+)", runtime)
        if values != [value]:
            errors.append("ESP32 UART 설정 불일치: " + field)
    for path in (esp / "main").glob("*.c"):
        if path.name in ("bridge_runtime.c", "bridge_runtime_v2.c"):
            continue
        if re.search(r"\b(?:uart_set_pin|gpio_config|gpio_set_direction|gpio_reset_pin|gpio_iomux_out)\s*\(", clean(path.read_text())):
            raise Unsupported(f"추가 ESP32 핀 설정 수동 검토 필요: {path.name}")
    return {"status": "FAIL" if errors else "PASS", "scope": "source GPIO initialization / IOC / UART contract",
            "stm32_pins": len(actual), "map": actual, "esp32_runtime": selected, "issues": errors,
            "limitations": ["일반 C 정적 분석기가 아닌 현재 CubeMX 형식 대조 검사",
                            "system_stm32f4xx.c의 외부 메모리 조건부 코드 및 HAL/vendor 내부는 검사 제외",
                            "실제 배선·전압·클럭·핀 파형·실행 경로·동적 재설정 미검증"]}


def main():
    try:
        result = inspect()
    except (Unsupported, OSError) as exc:
        result = {"status": "BLOCKED", "issues": [str(exc)]}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return {"PASS": 0, "FAIL": 1, "BLOCKED": 2}[result["status"]]


if __name__ == "__main__":
    sys.exit(main())
