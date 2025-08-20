cmake_minimum_required(VERSION 3.16)

project(attrcalc)

add_library(attrcalc STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/attrcalc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/vector_operations.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/expression_parser.cpp
)

target_compile_features(attrcalc PRIVATE cxx_std_17)

list(APPEND CMAKE_MODULE_PATH $ENV{MODULECONFIG_DIR}/cmake)
include(moduleconfig_libs)
target_link_libraries(attrcalc PRIVATE
            moduleconfig_libs)
list(APPEND CMAKE_MODULE_PATH $ENV{GEODATAFLOW_DIR}/cmake)
include(geodataflow_libs)
target_link_libraries(attrcalc PRIVATE
            geodataflow_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{VDSSTORE_DIR}/cmake)
include(vdsstore_libs)
target_link_libraries(attrcalc PRIVATE
            vdsstore_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDLOGGER_DIR}/cmake)
include(gdlogger_libs)
target_link_libraries(attrcalc PRIVATE
            gdlogger_libs)

list(APPEND CMAKE_MODULE_PATH $ENV{GDUTILITY_DIR}/cmake)
include(gdutility_libs)
target_link_libraries(attrcalc PRIVATE
            gdutility_libs)

install(TARGETS
  attrcalc
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
  src/attrcalc.json
  README.md
  CHANGELOG.md
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)