cmake_minimum_required(VERSION 3.16)

project(input)

add_library(input STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/input.cpp
)

target_compile_features(input PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(input PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(input PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(input PRIVATE
            vdsstore_libs)
include(openvds_libs)
target_link_libraries(input PRIVATE
            openvds_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{libfort_ROOT}/cmake)
find_package(libfort REQUIRED)
target_link_libraries(input PRIVATE
            libfort::fort)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(input PRIVATE
            gdlogger_libs)

install(TARGETS
  input
  RUNTIME
    COMPONENT Runtime
  LIBRARY
    COMPONENT Runtime
    NAMELINK_COMPONENT Development
  ARCHIVE
    COMPONENT Development
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(FILES
  src/input.json
  README.md
  CHANGELOG.md
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
