cmake_minimum_required(VERSION 3.16)

project(segytest)

# Build the static library (simplified version without OpenVDS dependencies)
add_library(segytest STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/segy_reader.cpp
)

# Build the executable
add_executable(segytest_main
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)

# Set C++17 for both targets
target_compile_features(segytest PRIVATE cxx_std_17)
target_compile_features(segytest_main PRIVATE cxx_std_17)

# Link the executable with the library and basic system libraries
target_link_libraries(segytest_main PRIVATE segytest)

# Add pthread if needed
find_package(Threads REQUIRED)
target_link_libraries(segytest_main PRIVATE Threads::Threads)

install(TARGETS
  segytest
  segytest_main
  RUNTIME
    COMPONENT Runtime
  LIBRARY
    COMPONENT Runtime
    NAMELINK_COMPONENT Development
  ARCHIVE
    COMPONENT Development
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)
