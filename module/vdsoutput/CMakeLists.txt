cmake_minimum_required(VERSION 3.16)

project(vdsoutput)

add_library(vdsoutput STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/vdsoutput.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/ChannelChunkWriter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/Converter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/VDSHandler.cpp
)

target_compile_features(vdsoutput PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(vdsoutput PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(vdsoutput PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(vdsoutput PRIVATE
            vdsstore_libs)
include(openvds_libs)
target_link_libraries(vdsoutput PRIVATE
            openvds_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{libfort_ROOT}/cmake)
find_package(libfort REQUIRED)
target_link_libraries(vdsoutput PRIVATE
            libfort::fort)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(vdsoutput PRIVATE
            gdlogger_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDUTILITY_DIR}/cmake)
include(gdutility_libs)
target_link_libraries(vdsoutput PRIVATE
            gdutility_libs)                

install(TARGETS
  vdsoutput
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
  src/vdsoutput.json
  README.md
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
