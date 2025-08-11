cmake_minimum_required(VERSION 3.16)

project(segyinput)

add_library(segyinput STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/segyinput.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/segy_reader.cpp

)

target_compile_features(segyinput PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(segyinput PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(segyinput PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(segyinput PRIVATE
            vdsstore_libs)
include(openvds_libs)
target_link_libraries(segyinput PRIVATE
            openvds_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{libfort_ROOT}/cmake)
find_package(libfort REQUIRED)
target_link_libraries(segyinput PRIVATE
            libfort::fort)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(segyinput PRIVATE
            gdlogger_libs)

install(TARGETS
  segyinput
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
  src/segyinput.json
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
