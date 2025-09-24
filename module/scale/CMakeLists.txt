cmake_minimum_required(VERSION 3.16)

project(scale)

add_library(scale STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/scale.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/scale_factor.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/scale_common.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/scale_agc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/scale_diverge.cpp

)

target_compile_features(scale PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(scale PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(scale PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(scale PRIVATE
            vdsstore_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(scale PRIVATE
            gdlogger_libs)

install(TARGETS
  scale
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
  src/scale.json
  README.md
  CHANGELOG.md  
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)