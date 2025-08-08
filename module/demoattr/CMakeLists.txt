cmake_minimum_required(VERSION 3.16)

project(demoattr)

add_library(demoattr STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/demoattr.cpp
)

target_compile_features(demoattr PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(demoattr PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(demoattr PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(demoattr PRIVATE
            vdsstore_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(demoattr PRIVATE
            gdlogger_libs)

install(TARGETS
  demoattr
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
  src/demoattr.json
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)