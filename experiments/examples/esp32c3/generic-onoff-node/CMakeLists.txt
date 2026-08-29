cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# ESP-BLE-MESH 예제에 필요한 컴포넌트만 빌드한다.
idf_build_set_property(MINIMAL_BUILD ON)

project(bike_mesh_onoff_node)
