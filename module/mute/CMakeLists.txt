cmake_minimum_required(VERSION 3.16)

project(mute)

add_library(mute STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/mute.cpp
)

target_compile_features(mute PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(mute PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(mute PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(mute PRIVATE
            vdsstore_libs
)

list(APPEND CMAKE_MODULE_PATH $ENV{libfort_ROOT}/cmake)
find_package(libfort REQUIRED)
target_link_libraries(mute PRIVATE
            libfort::fort)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(mute PRIVATE
            gdlogger_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDUTILITY_DIR}/cmake)
include(gdutility_libs)
target_link_libraries(mute PRIVATE
            gdutility_libs)


install(TARGETS
  mute
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
  src/mute.json
  README.md
  CHANGELOG.md  
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
