cmake_minimum_required(VERSION 3.16)

project(testexpect)

add_library(testexpect STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/testexpect.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/common_expect.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/attrcalc_expect.cpp
)

target_compile_features(testexpect PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(testexpect PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(testexpect PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(testexpect PRIVATE
            vdsstore_libs
)

list(APPEND CMAKE_MODULE_PATH $ENV{libfort_ROOT}/cmake)
find_package(libfort REQUIRED)
target_link_libraries(testexpect PRIVATE
            libfort::fort)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(testexpect PRIVATE
            gdlogger_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDUTILITY_DIR}/cmake)
include(gdutility_libs)
target_link_libraries(testexpect PRIVATE
            gdutility_libs)

install(TARGETS
  testexpect
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
  src/testexpect.json
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
