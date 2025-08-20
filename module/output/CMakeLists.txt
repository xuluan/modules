cmake_minimum_required(VERSION 3.16)

project(output)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

add_library(output STATIC 
  ${CMAKE_CURRENT_LIST_DIR}/src/output.c 
)

target_compile_features(output PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(output PRIVATE 
  moduleconfig_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(output PRIVATE 
  geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(output PRIVATE 
  vdsstore_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(output PRIVATE 
  gdlogger_libs)

install(TARGETS
  output
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
  src/output.json
  README.md
  CHANGELOG.md
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)

