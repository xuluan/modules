cmake_minimum_required(VERSION 3.16)

project(segyoutput)

add_library(segyoutput STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/segyoutput.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/segy_writer.cpp

)

target_compile_features(segyoutput PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(segyoutput PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(segyoutput PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(segyoutput PRIVATE
            vdsstore_libs)
include(openvds_libs)
target_link_libraries(segyoutput PRIVATE
            openvds_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{libfort_ROOT}/cmake)
find_package(libfort REQUIRED)
target_link_libraries(segyoutput PRIVATE
            libfort::fort)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(segyoutput PRIVATE
            gdlogger_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDUTILITY_DIR}/cmake)
include(gdutility_libs)
target_link_libraries(segyoutput PRIVATE
            gdutility_libs)                

install(TARGETS
  segyoutput
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
  src/segyoutput.json
  README.md
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
