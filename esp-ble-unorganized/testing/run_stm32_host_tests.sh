#!/bin/sh
set -eu
testing_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
stm32_dir="$testing_dir/../stm32-project/integration/stm32"
cmake -S "$stm32_dir/host-tests" -B "$stm32_dir/build/host-tests" -G Ninja -DENABLE_SANITIZERS=ON
cmake --build "$stm32_dir/build/host-tests"
ctest --test-dir "$stm32_dir/build/host-tests" --output-on-failure
